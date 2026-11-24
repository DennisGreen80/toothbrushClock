// Renders static screen previews and transition filmstrips:
//   docs/screens/*.svg      English
//   docs/screens/da/*.svg   Danish
//   node preview/render.js
const fs = require('fs');
const path = require('path');
const { BrushCore, BrushScene, OralbTracker, parseOralb, oralbAdvert, renderSVG, LANGS } = require('./sim.js');

const baseDir = path.join(__dirname, '..', 'docs', 'screens');

const date = new Date(2026, 10, 23, 7, 42, 18, 400);
const NOW = 1000000;
const s = sec => sec * 1000;

// Builds a core whose timers were started `elapsed` seconds ago (optionally paused for `pausedFor`).
// `oralb: {hard}` timers are created through the real advertisement path instead.
function coreWith(timers) {
  const core = new BrushCore();
  const tracker = new OralbTracker();
  timers.forEach((t, n) => {
    if (t.oralb) {
      const adv = parseOralb(oralbAdvert({ running: true, pressureHigh: !!t.oralb.hard, seconds: t.elapsed }));
      tracker.onAdv(core, `brush-${n}`, adv, NOW);
      return;
    }
    const i = core.add(NOW - s(t.elapsed) - s(t.pausedFor || 0));
    if (t.pausedFor) {
      core.togglePause(i, NOW - s(t.pausedFor));
    }
  });
  core.tick(NOW);
  return core;
}

const scenes = lang => ({
  '1-idle-clock': { screen: 'main', core: new BrushCore() },
  '2-one-timer': { screen: 'main', core: coreWith([{ elapsed: 47 }]) },
  '3-two-timers': { screen: 'main', core: coreWith([{ elapsed: 78 }, { elapsed: 21 }]) },
  '4-three-timers': { screen: 'main', core: coreWith([{ elapsed: 104 }, { elapsed: 66, pausedFor: 12 }, { elapsed: 9 }]) },
  '5-four-timers': { screen: 'main', core: coreWith([{ elapsed: 123 }, { elapsed: 95 }, { elapsed: 52 }, { elapsed: 14 }]), smileyAgeMs: 3000 },
  '6-done': { screen: 'main', core: coreWith([{ elapsed: 123 }]), smileyAgeMs: 3000 },
  '7-count-up-mode': { screen: 'main', core: coreWith([{ elapsed: 47 }]), countUp: true },
  '8-settings': { screen: 'settings', settings: { hour: 7, minute: 42, countUp: false, sound: true, lang } },
  '9-oralb-brushing': { screen: 'main', core: coreWith([{ elapsed: 38, oralb: {} }]) },
  '10-oralb-too-hard': { screen: 'main', core: coreWith([{ elapsed: 83, oralb: { hard: true } }]) },
  '11-oralb-mixed': { screen: 'main', core: coreWith([{ elapsed: 64, oralb: {} }, { elapsed: 41 }, { elapsed: 12, oralb: { hard: true } }]) },
  '12-screensaver': { screen: 'main', core: new BrushCore(), saverLevel: 1, saverElapsedMs: 22000 },
  '13-birthday': { screen: 'main', core: new BrushCore(), birthdayDates: [[11, 23]] },
  '14-synthwave': { screen: 'main', core: new BrushCore(), clockFace: 'synthwave' },
  '15-flower-power': { screen: 'main', core: new BrushCore(), clockFace: 'flower-power' },
  '16-christmas': { screen: 'main', core: new BrushCore(), clockFace: 'christmas',
    date: new Date(2026, 11, 24, 7, 42, 18, 400) },
  '17-nineties': { screen: 'main', core: new BrushCore(), clockFace: 'nineties' },
  '18-reindeer': { screen: 'main', core: new BrushCore(), clockFace: 'reindeer',
    date: new Date(2026, 11, 24, 7, 42, 18, 400) },
  '19-woven-heart': { screen: 'main', core: new BrushCore(), clockFace: 'heart',
    date: new Date(2026, 11, 24, 7, 42, 18, 400) },
  '20-candy-cane': { screen: 'main', core: new BrushCore(), clockFace: 'candy',
    date: new Date(2026, 11, 24, 7, 42, 18, 400) },
});

// ---- Transition filmstrips -------------------------------------------------------------------
const FRAME = 260;
const LABEL_H = 26;

function filmstrip(frames) {
  const w = frames.length * (FRAME + 12);
  let body = '';
  frames.forEach((f, i) => {
    const x = i * (FRAME + 12);
    const inner = f.svg.replace('<svg ', `<svg x="${x}" y="0" `).replace(/width="\d+" height="\d+"/, `width="${FRAME}" height="${FRAME}"`);
    body += inner;
    body += `<text x="${x + FRAME / 2}" y="${FRAME + 18}" font-family="Segoe UI, Arial" font-size="14" fill="#8E8E93" text-anchor="middle">${f.label}</text>`;
  });
  return `<svg xmlns="http://www.w3.org/2000/svg" width="${w}" height="${FRAME + LABEL_H}" viewBox="0 0 ${w} ${FRAME + LABEL_H}">${body}</svg>`;
}

// Renders `times` (ms after `action` runs) of a transition, starting from a settled `core`.
function capture(lang, core, action, times) {
  const scene = new BrushScene(core, NOW);
  renderSVG({ screen: 'main', core, scene, now: NOW, date, lang }); // caches ring content, like the device
  action(core);
  return times.map(t => ({
    label: `${t} ms`,
    svg: renderSVG({ screen: 'main', core, scene, now: NOW + t, date, lang }),
  }));
}

const steps = [0, 120, 240, 360, 520, 900];
const transitions = lang => ({
  'transition-start': capture(lang, new BrushCore(), c => c.add(NOW), steps),
  'transition-add': capture(lang, coreWith([{ elapsed: 47 }]), c => c.add(NOW), steps),
  'transition-finish': capture(lang, coreWith([{ elapsed: 47 }]), c => c.remove(0), [0, 100, 200, 400, 600, 900]),
  'transition-oralb-start': capture(lang, new BrushCore(), c => new OralbTracker().onAdv(c, 'brush',
    parseOralb(oralbAdvert({ running: true, seconds: 1 })), NOW), steps),
});

for (const lang of LANGS) {
  const outDir = lang === 'en' ? baseDir : path.join(baseDir, lang);
  fs.mkdirSync(outDir, { recursive: true });
  const write = (name, svg) => {
    fs.writeFileSync(path.join(outDir, `${name}.svg`), svg);
    console.log(`wrote ${path.relative(path.join(__dirname, '..'), path.join(outDir, `${name}.svg`))}`);
  };
  for (const [name, scene] of Object.entries(scenes(lang))) {
    write(name, renderSVG(Object.assign({ now: NOW, date, countUp: false, lang }, scene)));
  }
  for (const [name, frames] of Object.entries(transitions(lang))) {
    write(name, filmstrip(frames));
  }
}
