/* Preview renderer for the toothbrush timer UI.
 *
 * Mirrors lib/brush_core (timer logic + layout) and src/ui (colors, segment digit
 * geometry, arc/label placement) so the SVG output matches what the device draws.
 * Used by preview/index.html (interactive) and preview/render.js (static screenshots).
 */
(function (root) {
  'use strict';

  // ---- Constants (keep in sync with src/ui/ui.cpp) --------------------------------------
  const SCREEN = 480;
  const MAX_TIMERS = 4;
  const QUADRANTS = 4;
  const TIMER_COLORS = ['#3DD6F5', '#FF7AB6', '#FFC24B', '#9B8CFF'];
  const COLOR_DONE = '#4ADE80';
  const COLOR_TEXT = '#ECECEC';
  const COLOR_MUTED = '#8E8E93';
  const COLOR_FAINT = '#5A5A5E';
  const COLOR_SURFACE = '#1C1C1E';
  const COLOR_OUTLINE = '#3A3A3C';
  const COLOR_PRESSURE_OK = '#FFFFFF';
  const COLOR_PRESSURE_HIGH = '#FF453A';
  const PRESSURE_OK_OPA = 90 / 255;
  const TRACK_OPA = 40 / 255;
  const OFF_SEGMENT_OPA = 18 / 255;
  const SCREENSAVER_DRIFT_DELAY_MS = 5 * 60 * 1000;
  const CLOCK_FACES = ['classic', 'synthwave', 'flower-power', 'christmas'];
  const SYNTH_GRID_COLORS = ['#A43983', '#7051B1', '#368DA2', '#6357AD'];
  const SYNTH_RING_COLORS = ['#FF53BB', '#A685FF', '#54E8F7', '#FFAB6B'];
  const FLOWER = { ink: '#43223B', paper: '#FFE9BA', coral: '#E66A4E', pink: '#BA435F',
    gold: '#F5B640', green: '#527763', ring: ['#FFC557', '#F78E60', '#F48CA5', '#9CC18E'] };
  const CHRISTMAS = { gold: '#FFD58D', snow: '#FFF9E8', track: '#29443A' };
  const FLOWER_MARKS = Array.from({ length: 60 }, (_, i) => {
    const angle = 2 * Math.PI * i / 60;
    return [240 + Math.round(224 * Math.sin(angle)), 240 - Math.round(224 * Math.cos(angle))];
  });
  // Mirrors lib/brush_core/src/brush_i18n.c
  const STRINGS = {
    en: {
      language: 'English', hint: 'Tap to start brushing', paused: 'Paused', done: 'Done!', tooHard: 'Too hard', settings: 'Settings',
      countdown: 'Countdown', countUp: 'Count up', sound: 'Sound', muted: 'Muted', face: 'Face', classic: 'Classic', christmas: 'Christmas', ok: 'Done',
      quadrants: ['Upper right', 'Lower right', 'Lower left', 'Upper left'],
      weekdays: ['Sunday', 'Monday', 'Tuesday', 'Wednesday', 'Thursday', 'Friday', 'Saturday'],
      months: ['Jan', 'Feb', 'Mar', 'Apr', 'May', 'Jun', 'Jul', 'Aug', 'Sep', 'Oct', 'Nov', 'Dec'],
      date: (s, d) => `${s.weekdays[d.getDay()]}, ${d.getDate()} ${s.months[d.getMonth()]}`,
    },
    da: {
      language: 'Dansk', hint: 'Tryk for at børste', paused: 'Pause', done: 'Færdig!', tooHard: 'For hårdt', settings: 'Indstillinger',
      countdown: 'Nedtælling', countUp: 'Tæl op', sound: 'Lyd', muted: 'Lydløs', face: 'Urskive', classic: 'Klassisk', christmas: 'Jul', ok: 'Færdig',
      quadrants: ['Øverst højre', 'Nederst højre', 'Nederst venstre', 'Øverst venstre'],
      weekdays: ['Søndag', 'Mandag', 'Tirsdag', 'Onsdag', 'Torsdag', 'Fredag', 'Lørdag'],
      months: ['jan.', 'feb.', 'mar.', 'apr.', 'maj', 'jun.', 'jul.', 'aug.', 'sep.', 'okt.', 'nov.', 'dec.'],
      date: (s, d) => `${s.weekdays[d.getDay()]} ${d.getDate()}. ${s.months[d.getMonth()]}`,
    },
  };
  const LANGS = Object.keys(STRINGS);
  const strings = lang => STRINGS[lang] || STRINGS.en;
  const faceName = (face, txt) => face === 'christmas' ? txt.christmas : face === 'flower-power' ? 'Flower Power' :
    face === 'synthwave' ? 'Synthwave' : txt.classic;
  const FONT = "Montserrat, 'Segoe UI', Helvetica, Arial, sans-serif";
  const LINE_H = { 14: 16, 20: 22, 28: 30 };

  const idiv = (a, b) => Math.trunc(a / b);

  // ---- Core logic (port of lib/brush_core/src/brush_core.c) --------------------------------
  class BrushCore {
    constructor(cfg) {
      this.cfg = Object.assign({ durationMs: 120000, markMs: 30000, autoClearMs: 60000 }, cfg || {});
      this.timers = [];
    }
    get count() { return this.timers.length; }

    add(now) { return this.addAt(now, 0, 'manual'); }
    addAt(now, elapsedMs, source) {
      if (this.timers.length >= MAX_TIMERS) return -1;
      let color = 0;
      while (this.timers.some(t => t.color === color)) color++;
      this.nextId = ((this.nextId || 0) % 65535) + 1;
      this.timers.push({
        state: elapsedMs >= this.cfg.durationMs ? 'done' : 'running', start: now - elapsedMs, pausedTotal: 0,
        pauseStart: 0, marks: this.marksReached(elapsedMs), color, source: source || 'manual', held: false,
        pressureHigh: false, id: this.nextId,
      });
      return this.timers.length - 1;
    }
    marksTotal() { return idiv(this.cfg.durationMs - 1, this.cfg.markMs); }
    marksReached(e) { return Math.min(idiv(e, this.cfg.markMs), this.marksTotal()); }
    remove(i) {
      if (i < 0 || i >= this.timers.length) return false;
      this.timers.splice(i, 1);
      return true;
    }
    isPaused(i) { const t = this.timers[i]; return !!t && (t.state === 'paused' || t.held); }
    setPaused(i, paused, now) {
      const t = this.timers[i];
      if (!t || paused === this.isPaused(i)) return false;
      if (paused) {
        t.pauseStart = now;
        if (t.state === 'running') t.state = 'paused'; else t.held = true;
      } else {
        t.pausedTotal += now - t.pauseStart;
        if (t.state === 'paused') t.state = 'running';
        t.held = false;
      }
      return true;
    }
    setElapsed(i, e, now) {
      const t = this.timers[i];
      if (!t) return;
      t.start += this.elapsed(i, now) - e;
      t.marks = Math.min(t.marks, this.marksReached(e));
    }
    indexOfId(id) { return this.timers.findIndex(t => t.id === id); }
    togglePause(i, now) {
      const t = this.timers[i];
      if (!t) return 'done';
      if (t.state !== 'done') this.setPaused(i, t.state === 'running', now);
      return t.state;
    }
    elapsed(i, now) {
      const t = this.timers[i];
      if (!t) return 0;
      const end = this.isPaused(i) ? t.pauseStart : now;
      return end - t.start - t.pausedTotal;
    }
    tick(now) {
      const events = [];
      const { durationMs, autoClearMs } = this.cfg;
      const marksTotal = this.marksTotal();
      this.timers.forEach((t, i) => {
        if (t.state !== 'running') return;
        const e = this.elapsed(i, now);
        if (e >= durationMs) {
          t.state = 'done';
          t.marks = marksTotal;
          events.push({ type: 'done', index: i, color: t.color });
          return;
        }
        const reached = this.marksReached(e);
        if (reached > t.marks) {
          t.marks = reached;
          events.push({ type: 'mark', index: i, mark: reached, color: t.color });
        }
      });
      if (autoClearMs > 0) {
        for (let i = this.timers.length - 1; i >= 0; i--) {
          const t = this.timers[i];
          if (t.state === 'done' && t.source === 'manual' && this.elapsed(i, now) >= durationMs + autoClearMs) {
            const color = this.timers[i].color;
            this.remove(i);
            events.push({ type: 'cleared', index: i, color });
          }
        }
      }
      return events;
    }
    quadrantPermille(i, q, now) {
      const e = this.elapsed(i, now);
      const len = idiv(this.cfg.durationMs, QUADRANTS);
      const start = q * len;
      if (e <= start) return 0;
      if (e >= start + len) return 1000;
      return idiv((e - start) * 1000, len);
    }
    currentQuadrant(i, now) {
      return Math.min(idiv(this.elapsed(i, now), idiv(this.cfg.durationMs, QUADRANTS)), QUADRANTS - 1);
    }
    displaySeconds(i, now, countUp) {
      const e = this.elapsed(i, now);
      const d = this.cfg.durationMs;
      const over = e >= d;
      if (countUp) return { secs: idiv(e, 1000), over };
      if (over) return { secs: idiv(e - d, 1000), over };
      return { secs: idiv(d - e + 999, 1000), over };
    }
  }

  function formatMMSS(seconds) {
    let m = idiv(seconds, 60), s = seconds % 60;
    if (m > 99) { m = 99; s = 59; }
    return `${m}:${String(s).padStart(2, '0')}`;
  }

  // ---- Oral-B passive BLE (port of lib/brush_core/src/brush_oralb.c) -------------------------
  const ORALB = { companyId: 0x00DC, running: 3, advTimeoutMs: 4000, resumeWindowMs: 30000, resyncMs: 1500 };

  // mfg: manufacturer data bytes including the 2 company id bytes.
  function parseOralb(mfg) {
    if (!mfg || mfg.length < 2 || (mfg[0] | (mfg[1] << 8)) !== ORALB.companyId) return null;
    const d = mfg.slice(2);
    if (d.length !== 9 && d.length !== 11) return null;
    return {
      protocol: d[0], model: d[1], state: d[3], running: d[3] === ORALB.running,
      pressureHigh: !!(d[4] & 0x80) && !(d[4] & 0x0C), brushTimeS: d[5] * 60 + d[6], mode: d[7],
    };
  }

  // Synthetic iO advertisement, handy for the simulator.
  function oralbAdvert({ running, pressureHigh, seconds }) {
    const s = Math.max(0, Math.floor(seconds || 0));
    return [0xDC, 0x00, 0x07, 0x34, 0x0C, running ? 3 : 2, pressureHigh ? 0x80 : 0x00,
      Math.min(idiv(s, 60), 255), s % 60, 0x02, 0x01, 0x00, 0x04];
  }

  class OralbTracker {
    constructor() { this.links = []; }

    pauseLink(l, core, now) {
      l.stopped = true;
      l.stoppedMs = now;
      const i = core.indexOfId(l.timerId);
      if (i < 0) return 'none';
      core.timers[i].pressureHigh = false;
      return core.setPaused(i, true, now) ? 'paused' : 'none';
    }

    onAdv(core, addr, adv, now) {
      let l = this.links.find(k => k.addr === addr);
      if (!adv.running) {
        if (l && !l.stopped) { l.lastSeen = now; return this.pauseLink(l, core, now); }
        return 'none';
      }
      const brushMs = adv.brushTimeS * 1000;
      if (!l) {
        if (this.links.length >= MAX_TIMERS) return 'full';
        const i = core.addAt(now, brushMs, 'oralb');
        if (i < 0) return 'full';
        core.timers[i].pressureHigh = adv.pressureHigh;
        this.links.push({ addr, timerId: core.timers[i].id, dismissed: false, stopped: false, lastSeen: now, stoppedMs: 0 });
        return 'started';
      }
      l.lastSeen = now;
      const wasStopped = l.stopped;
      l.stopped = false;
      if (l.dismissed) return 'none';
      const i = core.indexOfId(l.timerId);
      if (i < 0) { l.dismissed = true; return 'none'; }
      core.timers[i].pressureHigh = adv.pressureHigh;
      const action = core.setPaused(i, false, now) || wasStopped ? 'resumed' : 'none';
      const e = core.elapsed(i, now);
      if (e + ORALB.resyncMs < brushMs || e > brushMs + 1000 + ORALB.resyncMs) core.setElapsed(i, brushMs + 500, now);
      return action;
    }

    tick(core, now) {
      let result = 'none';
      for (const l of this.links.slice()) {
        if (!l.stopped && now - l.lastSeen > ORALB.advTimeoutMs) {
          if (this.pauseLink(l, core, l.lastSeen) === 'paused' && result === 'none') result = 'paused';
        }
        if (l.stopped && now - l.stoppedMs > ORALB.resumeWindowMs) {
          const i = core.indexOfId(l.timerId);
          if (!l.dismissed && i >= 0) { core.remove(i); result = 'ended'; }
          this.links.splice(this.links.indexOf(l), 1);
        }
      }
      return result;
    }
  }

  function layout(n) {
    n = Math.max(0, Math.min(MAX_TIMERS, n));
    const c = (cx, cy, r) => ({ cx, cy, r });
    const out = { slots: [], add: c(0, 0, 0), addIsGhost: false, clockCy: 0, clockH: 0 };
    if (n === 0) { out.clockCy = 212; out.clockH = 110; }
    else if (n === 1) {
      out.slots = [c(240, 248, 160)]; out.add = c(240, 440, 26); out.clockCy = 52; out.clockH = 30;
    } else if (n === 2) {
      out.slots = [c(128, 240, 102), c(352, 240, 102)]; out.add = c(240, 392, 30); out.clockCy = 92; out.clockH = 36;
    } else {
      const grid = [[152, 182], [328, 182], [152, 352], [328, 352]];
      out.slots = grid.slice(0, n).map(([x, y]) => c(x, y, 80));
      if (n < MAX_TIMERS) { out.add = c(grid[n][0], grid[n][1], 80); out.addIsGhost = true; }
      out.clockCy = 60; out.clockH = 28;
    }
    return out;
  }

  // ---- Scene / transitions (port of lib/brush_core/src/brush_scene.c) -------------------------
  const EASE = {
    linear: t => t,
    outCubic: t => 1 - (1 - t) ** 3,
    inCubic: t => t ** 3,
    inOutCubic: t => (t < 0.5 ? 4 * t ** 3 : 1 - (-2 * t + 2) ** 3 / 2),
    outBack: t => { const c1 = 1.2, c3 = c1 + 1, u = t - 1; return 1 + c3 * u ** 3 + c1 * u ** 2; },
  };
  const T = { move: 450, appear: 520, appearDelayFromIdle: 200, disappear: 280, clock: 420,
              idleOut: 220, idleIn: 400, idleInDelay: 250, addIn: 300, addInDelay: 380 };
  const PULSE = { mark: [30, 380], done: [80, 650], pressure: [22, 300] }; // [permille, ms] (src/ui/ui.cpp)
  const geom = (cx, cy, r, opa) => ({ cx, cy, r, opa });
  const slotGeom = (c, opa) => geom(c.cx, c.cy, c.r, opa);

  class Tween {
    constructor(g, now) { this.snap(g, now); }
    snap(g, now) { this.from = g; this.to = g; this.start = now; this.delay = 0; this.dur = 0; this.ease = EASE.linear; }
    tweenTo(g, now, delay, dur, ease) {
      this.from = this.value(now); this.to = g; this.start = now; this.delay = delay; this.dur = dur; this.ease = ease;
    }
    value(now) {
      const dt = now - this.start - this.delay;
      if (this.dur === 0 || dt >= this.dur) return this.to;
      if (dt <= 0) return this.from;
      const k = this.ease(dt / this.dur), f = this.from, t = this.to;
      return geom(f.cx + (t.cx - f.cx) * k, f.cy + (t.cy - f.cy) * k, f.r + (t.r - f.r) * k, f.opa + (t.opa - f.opa) * k);
    }
    running(now) { return this.dur > 0 && now - this.start < this.delay + this.dur; }
  }

  function slotsFor(core) {
    const slots = [-1, -1, -1, -1];
    core.timers.forEach((t, i) => { slots[t.color % MAX_TIMERS] = i; });
    return slots;
  }

  class BrushScene {
    constructor(core, now) {
      const l = layout(core.count);
      this.count = core.count;
      this.cardSlot = slotsFor(core);
      this.clock = new Tween(geom(240, l.clockCy, l.clockH, 1), now);
      this.idle = new Tween(geom(0, 0, 0, core.count === 0 ? 1 : 0), now);
      this.cards = this.cardSlot.map(s => new Tween(s >= 0 ? slotGeom(l.slots[s], 1) : geom(240, 240, 0, 0), now));
      this.add = new Tween(slotGeom(l.add, l.add.r > 0 ? 1 : 0), now);
      this.pulses = [null, null, null, null];
      this.content = [null, null, null, null]; // last drawn card content, kept for fade-outs
    }

    sync(core, now) {
      const slotOf = slotsFor(core);
      const n = core.count;
      if (n === this.count && slotOf.every((s, c) => s === this.cardSlot[c])) return false;
      const oldL = layout(this.count), l = layout(n);
      const fromIdle = this.count === 0 && n > 0, toIdle = this.count > 0 && n === 0;
      for (let c = 0; c < MAX_TIMERS; c++) {
        const was = this.cardSlot[c], slot = slotOf[c], tw = this.cards[c];
        if (slot >= 0) {
          const target = slotGeom(l.slots[slot], 1);
          if (was < 0) {
            const start = !fromIdle && oldL.add.r > 0 ? slotGeom(oldL.add, 0) : geom(target.cx, target.cy, target.r * 0.6, 0);
            tw.snap(start, now);
            tw.tweenTo(target, now, fromIdle ? T.appearDelayFromIdle : 0, T.appear, EASE.outBack);
          } else {
            tw.tweenTo(target, now, 0, T.move, EASE.inOutCubic);
          }
        } else if (was >= 0) {
          const cur = tw.value(now);
          tw.tweenTo(geom(cur.cx, cur.cy, cur.r * 0.75, 0), now, 0, T.disappear, EASE.inCubic);
        }
        this.cardSlot[c] = slot;
      }
      this.clock.tweenTo(geom(240, l.clockCy, l.clockH, 1), now, toIdle ? 80 : 0, T.clock, EASE.inOutCubic);
      if (fromIdle) this.idle.tweenTo(geom(0, 0, 0, 0), now, 0, T.idleOut, EASE.outCubic);
      else if (toIdle) this.idle.tweenTo(geom(0, 0, 0, 1), now, T.idleInDelay, T.idleIn, EASE.outCubic);
      this.add.snap(slotGeom(l.add, 0), now);
      if (l.add.r > 0) this.add.tweenTo(slotGeom(l.add, 1), now, T.addInDelay, T.addIn, EASE.outCubic);
      this.count = n;
      return true;
    }

    pulse(color, [amp, dur], now) { this.pulses[color % MAX_TIMERS] = { start: now, amp, dur }; }

    card(color, now) {
      const g = Object.assign({}, this.cards[color].value(now));
      const p = this.pulses[color];
      if (p && now - p.start < p.dur) g.r *= 1 + p.amp / 1000 * Math.sin(Math.PI * (now - p.start) / p.dur);
      return g;
    }

    animating(now) {
      return [this.clock, this.idle, this.add, ...this.cards].some(t => t.running(now)) ||
        this.pulses.some(p => p && now - p.start < p.dur);
    }
  }

  // ---- SVG primitives ------------------------------------------------------------------------
  const esc = s => String(s).replace(/&/g, '&amp;').replace(/</g, '&lt;').replace(/>/g, '&gt;');

  function rrect(x1, y1, x2, y2, fill, opa) {
    const w = x2 - x1, h = y2 - y1, r = Math.min(w, h) / 2;
    return `<rect x="${x1}" y="${y1}" width="${w}" height="${h}" rx="${r}" fill="${fill}" fill-opacity="${opa.toFixed(3)}"/>`;
  }

  function text(x, cy, size, color, str, weight, opa) {
    const o = opa === undefined ? '' : ` fill-opacity="${opa.toFixed(3)}"`;
    return `<text x="${x}" y="${cy}" font-family="${FONT}" font-size="${size}" font-weight="${weight || 500}" fill="${color}"${o} text-anchor="middle" dominant-baseline="central">${esc(str)}</text>`;
  }

  // LVGL angles: 0 deg = 3 o'clock, clockwise. Arc drawn inside a box of radius r with width w.
  function arcPath(cx, cy, r, w, a0, a1, color, opa, cap = 'round') {
    if (a1 <= a0) return '';
    const rr = r - w / 2;
    const p = a => [cx + rr * Math.cos(a * Math.PI / 180), cy + rr * Math.sin(a * Math.PI / 180)];
    if (a1 - a0 >= 359.99) {
      return `<circle cx="${cx}" cy="${cy}" r="${rr}" fill="none" stroke="${color}" stroke-opacity="${opa.toFixed(3)}" stroke-width="${w}"/>`;
    }
    const [x0, y0] = p(a0), [x1, y1] = p(a1);
    const large = a1 - a0 > 180 ? 1 : 0;
    return `<path d="M${x0.toFixed(2)} ${y0.toFixed(2)} A${rr} ${rr} 0 ${large} 1 ${x1.toFixed(2)} ${y1.toFixed(2)}" fill="none" stroke="${color}" stroke-opacity="${opa.toFixed(3)}" stroke-width="${w}" stroke-linecap="${cap}"/>`;
  }

  // ---- Segment digits (port of src/ui/seg_digits.cpp) --------------------------------------
  const SEGMENTS = [0x3F, 0x06, 0x5B, 0x4F, 0x66, 0x6D, 0x7D, 0x07, 0x7F, 0x6F];

  function segGeometry(h) {
    const t = Math.max(idiv(h * 10, 100), 2);
    return { h, w: idiv(h * 52, 100), t, gap: Math.max(idiv(t, 4), 1), spacing: idiv(h * 16, 100) };
  }
  const charWidth = (g, ch) => (ch === ':' ? g.t : g.w);
  function segWidth(h, str) {
    const g = segGeometry(h);
    let w = 0;
    for (const ch of str) w += charWidth(g, ch);
    return w + (str.length > 1 ? g.spacing * (str.length - 1) : 0);
  }

  function segDigits(x, y, h, str, color, opa = 1) {
    const g = segGeometry(h);
    const { w, t, gap } = g;
    let out = '';
    for (const ch of str) {
      if (ch === ':') {
        for (const c of [y + idiv(h * 32, 100), y + idiv(h * 68, 100)]) {
          out += rrect(x, c - idiv(t, 2), x + t, c - idiv(t, 2) + t, color, opa);
        }
      } else {
        const on = ch >= '0' && ch <= '9' ? SEGMENTS[+ch] : ch === '-' ? 0x40 : 0;
        const hx1 = x + idiv(t, 2) + gap, hx2 = x + w - idiv(t, 2) - gap, mid = y + idiv(h, 2);
        const segs = [
          [hx1, y, hx2, y + t],
          [x + w - t, y + idiv(t, 2) + gap, x + w, mid - gap],
          [x + w - t, mid + gap, x + w, y + h - idiv(t, 2) - gap],
          [hx1, y + h - t, hx2, y + h],
          [x, mid + gap, x + t, y + h - idiv(t, 2) - gap],
          [x, y + idiv(t, 2) + gap, x + t, mid - gap],
          [hx1, mid - idiv(t, 2), hx2, mid - idiv(t, 2) + t],
        ];
        segs.forEach((s, i) => { out += rrect(s[0], s[1], s[2], s[3], color, (on & (1 << i) ? 1 : OFF_SEGMENT_OPA) * opa); });
      }
      x += charWidth(g, ch) + g.spacing;
    }
    return out;
  }

  function segCentered(cx, top, h, str, color, opa = 1) {
    return segDigits(cx - idiv(segWidth(h, str), 2), top, h, str, color, opa);
  }

  function plusGlyph(cx, cy, size, color, opa = 1) {
    const s = size / 2;
    return `<path d="M${cx - s} ${cy} H${cx + s} M${cx} ${cy - s} V${cy + s}" stroke="${color}" stroke-opacity="${opa.toFixed(3)}" stroke-width="3.2" stroke-linecap="round"/>`;
  }

  function chevron(cx, cy, up, color) {
    const d = up ? -1 : 1;
    return `<path d="M${cx - 9} ${cy - 4 * d} L${cx} ${cy + 5 * d} L${cx + 9} ${cy - 4 * d}" fill="none" stroke="${color}" stroke-width="3.2" stroke-linecap="round" stroke-linejoin="round"/>`;
  }

  const mix = (hex, other, ratio) => {
    const a = parseInt(hex.slice(1), 16), b = parseInt(other.slice(1), 16);
    const ch = sh => Math.round(((a >> sh) & 255) * ratio + ((b >> sh) & 255) * (1 - ratio));
    return '#' + [16, 8, 0].map(sh => ch(sh).toString(16).padStart(2, '0')).join('');
  };

  function screensaverAccent(elapsedMs) {
    const segmentMs = 16000, phaseMs = elapsedMs % (segmentMs * MAX_TIMERS);
    const i = Math.floor(phaseMs / segmentMs);
    const t = (phaseMs % segmentMs) / segmentMs;
    const eased = t * t * (3 - 2 * t);
    return mix(TIMER_COLORS[i], TIMER_COLORS[(i + 1) % MAX_TIMERS], 1 - eased);
  }

  // ---- Screens -----------------------------------------------------------------------------------
  const pad2 = n => String(n).padStart(2, '0');
  const clockText = d => `${pad2(d.getHours())}:${pad2(d.getMinutes())}`;

  // ---- Main screen (mirrors src/ui/ui.cpp, driven by BrushScene) ---------------------------
  const clamp01 = v => Math.max(0, Math.min(1, v));

  function cardContent(core, i, now, countUp, txt) {
    const t = core.timers[i];
    const quads = [0, 1, 2, 3].map(q => core.quadrantPermille(i, q, now));
    const { secs } = core.displaySeconds(i, now, countUp);
    const oralb = t.source === 'oralb';
    const tooHard = oralb && t.pressureHigh && t.state === 'running';
    const status = t.state === 'paused' ? txt.paused : t.state === 'done' ? txt.done
      : tooHard ? txt.tooHard : txt.quadrants[core.currentQuadrant(i, now)];
    const color = t.state === 'done' ? COLOR_DONE : TIMER_COLORS[t.color % MAX_TIMERS];
    return { state: t.state, color, quads, text: formatMMSS(secs), status, oralb, tooHard };
  }

  // Toothbrush icon (src/ui/brush_icon.cpp): shapes in a 100 x 40 box.
  const ICON_BODY = [[0, 24, 62, 38], [56, 28, 78, 34], [72, 24, 99, 36]];
  const ICON_BRISTLES = [[74, 6, 81, 26], [83, 4, 90, 26], [92, 6, 99, 26]];

  function brushIcon(cx, cy, w, body, bristles, opa) {
    const h = idiv(w * 40, 100);
    const x = cx - idiv(w, 2), y = cy - idiv(h, 2);
    const shape = (s, fill) => rrect(x + idiv(s[0] * w, 100), y + idiv(s[1] * w, 100), x + idiv(s[2] * w, 100), y + idiv(s[3] * w, 100), fill, opa);
    return ICON_BODY.map(s => shape(s, body)).join('') + ICON_BRISTLES.map(s => shape(s, bristles)).join('');
  }

  function renderCard(content, g) {
    const r = Math.round(g.r), cx = Math.round(g.cx), cy = Math.round(g.cy);
    const opa = clamp01(g.opa);
    const { color, state, oralb, tooHard } = content;
    const paused = state === 'paused';
    const dimmed = paused ? mix(color, '#000000', 0.5) : color;
    const arcW = Math.max(6, idiv(r * 85, 1000));
    const halfGap = 2 + idiv(idiv(arcW * 2865, 100) + idiv(r, 2), r);
    let s = '';
    for (let q = 0; q < QUADRANTS; q++) {
      const a0 = 270 + q * 90 + halfGap, a1 = 270 + (q + 1) * 90 - halfGap;
      s += arcPath(cx, cy, r, arcW, a0, a1, color, TRACK_OPA * opa);
      const v = content.quads[q];
      if (v > 0) s += arcPath(cx, cy, r, arcW, a0, a0 + (a1 - a0) * v / 1000, color, (paused ? 0.5 : 1) * opa);
    }
    if (oralb) {
      if (state !== 'done') {
        const pr = r - arcW - Math.max(4, idiv(r * 5, 100));
        s += arcPath(cx, cy, pr, Math.max(3, idiv(r * 25, 1000)), 0, 360,
          tooHard ? COLOR_PRESSURE_HIGH : COLOR_PRESSURE_OK, (tooHard ? 1 : PRESSURE_OK_OPA) * opa);
      }
      s += brushIcon(cx, cy - idiv(r * 48, 100), idiv(r * 46, 100), dimmed, tooHard ? COLOR_PRESSURE_HIGH : '#FFFFFF', opa);
    }
    const digitH = idiv(r * 46, 100);
    const digitsCy = cy - idiv(r * 6, 100);
    s += segCentered(cx, digitsCy - idiv(digitH, 2), digitH, content.text, dimmed, opa);
    const fontSize = r >= 120 ? 20 : 14;
    const statusColor = tooHard ? COLOR_PRESSURE_HIGH : state === 'running' ? COLOR_MUTED : color;
    s += text(cx, digitsCy + idiv(digitH, 2) + idiv(r * 18, 100), fontSize, statusColor, content.status, 500, opa);
    return s;
  }

  function birthdayFlags(now, opa, mirrorLeft) {
    const waveMs = 2400, frameMs = 50;
    return [96, 302].map((left, i) => {
      const phaseMs = Math.floor((now % waveMs) / frameMs) * frameMs + i * waveMs / 3;
      const phase = 2 * Math.PI * phaseMs / waveMs;
      const rect = (x, y, w, h) => `M${x} ${y}h${w}v${h}h${-w}z`;
      let red = '', white = '';
      let redStops = '', whiteStops = '';
      for (let x = 0; x < 74; x += 2) {
        const u = x / 74, fold = phase - 7 * u;
        const y = 10 + Math.round(8 * u * Math.sin(fold));
        const light = 0.5 + (0.25 + 0.22 * u) * Math.cos(fold);
        const offset = ((x + 1) / 74).toFixed(4);
        redStops += `<stop offset="${offset}" stop-color="${mix('#DE2942', '#A81027', light)}"/>`;
        whiteStops += `<stop offset="${offset}" stop-color="${mix('#FFF9F8', '#E5DCE1', light)}"/>`;
        if (x >= 24 && x < 32) {
          white += rect(4 + x, y, 2, 56);
        } else {
          red += rect(4 + x, y, 2, 24) + rect(4 + x, y + 32, 2, 24);
          white += rect(4 + x, y + 24, 2, 8);
        }
      }
      const transform = mirrorLeft && i === 0
        ? `translate(${left + 82} 70) scale(-1 1)` : `translate(${left} 70)`;
      const gradient = (color, stops) =>
        `<linearGradient id="flag-${i}-${color}" gradientUnits="userSpaceOnUse" x1="4" y1="0" x2="78" y2="0">${stops}</linearGradient>`;
      return `<g class="birthday-flag" transform="${transform}" opacity="${opa.toFixed(3)}">` +
        `<defs>${gradient('red', redStops)}${gradient('white', whiteStops)}</defs>` +
        `<rect x="2" y="4" width="2" height="76" fill="#88818A"/>` +
        `<rect x="1" y="2" width="4" height="4" fill="#EAC77B"/>` +
        `<path d="${red}" fill="url(#flag-${i}-red)"/><path d="${white}" fill="url(#flag-${i}-white)"/></g>`;
    }).join('');
  }

  function paletteColor(colors, progress) {
    const phase = progress * colors.length, i = Math.floor(phase);
    return mix(colors[(i + 1) % colors.length], colors[i], phase - i);
  }

  function synthRing(secFrac, opa) {
    let s = '<g class="synth-ring">';
    const sweep = Math.floor(secFrac * 360), step = 5;
    for (let start = 0; start < sweep; start += step) {
      const end = Math.min(start + step, sweep);
      const color = paletteColor(SYNTH_RING_COLORS, (start + step / 2) / 360);
      s += arcPath(240, 240, 234, 4, 270 + start, 270 + end, color, opa, 'butt');
    }
    return s + '</g>';
  }

  function synthBackdrop(opa, now) {
    let s = `<g class="synth-backdrop" opacity="${opa.toFixed(3)}">`;
    const stars = [[137, 65, 2], [172, 115, 3], [307, 56, 2], [343, 92, 3],
      [118, 130, 2], [383, 145, 2], [94, 210, 2], [387, 244, 2]];
    for (const [x, y, d] of stars) s += rrect(x, y, x + d, y + d, '#5D467E', 1);
    for (let dy = -50; dy <= 50; dy += 2) {
      if ((dy >= 14 && dy < 18) || (dy >= 26 && dy < 32) || (dy >= 38 && dy < 46)) continue;
      const half = Math.floor(Math.sqrt(51 ** 2 - dy ** 2));
      s += `<rect x="${240 - half}" y="${97 + dy}" width="${2 * half + 1}" height="2" fill="${mix('#FFB660', '#F02D91', (50 - dy) / 100)}"/>`;
    }

    const horizon = 366, clipR = 228;
    const grid = paletteColor(SYNTH_GRID_COLORS, (now % 24000) / 24000);
    const horizonColor = mix('#EFD4FF', grid, 0.3);
    const line = (x, y, endX, endY, color, width) =>
      `<path d="M${x} ${y} L${endX} ${endY}" stroke="${color}" stroke-width="${width}"/>`;
    for (let ray = -8; ray <= 8; ++ray) {
      const x = ray * 11, y = horizon - 240, dx = ray * 43, dy = 148;
      const a = dx * dx + dy * dy, b = x * dx + y * dy, c = x * x + y * y - clipR * clipR;
      const end = (Math.sqrt(b * b - a * c) - b) / a;
      s += line(240 + x, horizon, 240 + x + Math.round(dx * end), horizon + Math.round(dy * end), grid, 1);
    }
    for (const y of [horizon, 371, 379, 391, 408, 431, 460]) {
      const half = Math.floor(Math.sqrt(clipR ** 2 - (y - 240) ** 2));
      s += line(240 - half, y, 240 + half, y, y === horizon ? horizonColor : grid, y === horizon ? 2 : 1);
    }
    return s + '</g>';
  }

  function flowerBackdrop(opa, ringOpa, second) {
    const { ink, paper, coral, pink, gold, green, ring } = FLOWER;
    let s = '<g class="flower-backdrop">';
    let alpha = opa.toFixed(3);
    const circle = (x, y, r, color) => {
      s += `<circle cx="${x}" cy="${y}" r="${r}" fill="${color}" opacity="${alpha}"/>`;
    };
    const daisy = (x, y, reach, petal, color, middle) => {
      const d = Math.round(reach * 0.71);
      for (const [dx, dy] of [[0, -reach], [d, -d], [reach, 0], [d, d],
        [0, reach], [-d, d], [-reach, 0], [-d, -d]]) {
        circle(x + dx, y + dy, petal, color);
      }
      circle(x, y, idiv(reach, 2) + 1, middle);
    };
    circle(240, 240, 238, ink);
    circle(240, 240, 211, gold);
    circle(240, 240, 207, paper);
    for (const [radius, color] of [[191.5, pink], [177.5, coral], [163.5, gold]]) {
      s += arcPath(240, 240, radius, 11, 50, 130, color, opa);
    }

    daisy(236, 99, 24, 18, coral, ink);
    circle(236, 99, 8, gold);
    daisy(278, 124, 11, 8, pink, gold);
    daisy(193, 124, 8, 6, gold, ink);
    daisy(115, 358, 14, 10, pink, gold);
    daisy(143, 379, 9, 6, coral, ink);
    daisy(364, 358, 14, 10, gold, ink);
    daisy(341, 379, 9, 6, green, gold);
    circle(185, 98, 3, green);
    circle(282, 89, 4, green);
    circle(297, 111, 2, coral);
    circle(91, 342, 3, coral);
    circle(389, 342, 3, pink);
    s += '</g>';

    if (ringOpa > 0.01) {
      s += '<g class="flower-ring">';
      alpha = ringOpa.toFixed(3);
      FLOWER_MARKS.forEach(([x, y], i) => {
        const current = i === second, past = i < second;
        const color = current || past ? ring[idiv(i, 15)] : '#916478';
        s += `<g class="flower-mark${current ? ' current' : past ? ' elapsed' : ''}">`;
        if (i % 5 === 0) {
          daisy(x, y, current ? 5 : 4, current ? 3 : 2, color, current ? paper : ink);
        } else {
          circle(x, y, current ? 4 : past ? 2 : 1, color);
          if (current) circle(x, y, 1, paper);
        }
        s += '</g>';
      });
      s += '</g>';
    }
    return s;
  }

  function christmasBackdrop(opa) {
    const { gold, snow } = CHRISTMAS;
    const alpha = opa.toFixed(3);
    let s = '<g class="christmas-backdrop">';
    const circle = (x, y, r, color) => {
      s += `<circle cx="${x}" cy="${y}" r="${r}" fill="${color}" opacity="${alpha}"/>`;
    };
    const line = (x, y, endX, endY, width, color) => {
      s += `<path d="M${x} ${y} L${endX} ${endY}" stroke="${color}" stroke-width="${width}" opacity="${alpha}"/>`;
    };
    circle(240, 240, 221, '#36554A');
    circle(240, 240, 219, '#112A25');
    for (const [x, y, r] of [[137, 64, 2], [166, 115, 2], [125, 141, 2], [87, 181, 2],
      [69, 251, 2], [129, 342, 2], [107, 360, 1], [156, 385, 2], [323, 63, 2],
      [312, 109, 2], [359, 135, 2], [388, 181, 2], [414, 242, 2], [360, 348, 2],
      [377, 360, 1], [323, 376, 2]]) {
      circle(x, y, r, '#789F90');
    }

    for (const x of [203, 277]) {
      line(x, 54, x, 101, 1, '#A08354');
      circle(x, 111, 12, '#AD3C46');
      circle(x - 3, 107, 3, '#E08C85');
      line(x - 6, 99, x + 6, 99, 3, gold);
    }
    line(240, 41, 240, 68, 1, '#A08354');
    line(226, 94, 254, 94, 2, gold);
    line(240, 68, 240, 120, 3, gold);
    line(227, 81, 253, 107, 2, gold);
    line(227, 107, 253, 81, 2, gold);
    circle(240, 94, 6, snow);

    for (const [x, top, span, step, color] of [[169, 385, 15, 7, '#648C7A'],
      [204, 377, 18, 9, '#9CC2AF'], [240, 369, 26, 13, '#C5E0C8'],
      [276, 377, 18, 9, '#9CC2AF'], [311, 385, 15, 7, '#648C7A']]) {
      line(x, top, x, top + 5 * step, 2, color);
      for (let i = 0; i < 3; i++) {
        const y = top + i * step;
        const w = idiv(span * (i + 2), 4);
        line(x, y, x - w, y + 2 * step, 3, color);
        line(x, y, x + w, y + 2 * step, 3, color);
      }
    }
    for (const [x, y] of [[233, 399], [245, 396], [249, 412], [228, 407]]) circle(x, y, 2, gold);
    s += arcPath(240, 240, 196, 2, 63, 117, '#648C7A', opa);
    s += arcPath(240, 240, 203, 2, 71, 109, '#36554A', opa);
    return s + '</g>';
  }

  function renderMain(state) {
    const { core, scene, now, date, countUp } = state;
    const txt = strings(state.lang);
    const t = state.animNow !== undefined ? state.animNow : now;
    scene.sync(core, t);
    let s = '';

    const saverLevel = clamp01(state.saverLevel || 0);
    const saverElapsed = state.saverElapsedMs || 0;
    const saverAccent = screensaverAccent(saverElapsed);
    const ringFade = Math.min(saverLevel * 2, 1);
    const driftMs = Math.max(0, saverElapsed - SCREENSAVER_DRIFT_DELAY_MS);
    const shiftAngle = 2 * Math.PI * (driftMs % 90000) / 90000;
    const radius = 8 * EASE.inOutCubic(clamp01(driftMs / 1400)) * Math.max((saverLevel - 0.5) * 2, 0);
    const shiftX = Math.round(radius * Math.sin(shiftAngle));
    const shiftY = Math.round(radius * Math.cos(shiftAngle));
    const idleOpa = clamp01(scene.idle.value(t).opa);
    const themeOpa = idleOpa * (1 - saverLevel);
    const synthFace = state.clockFace === 'synthwave', flowerFace = state.clockFace === 'flower-power';
    const christmasFace = state.clockFace === 'christmas';
    const synth = synthFace ? themeOpa : 0;
    const ringOpa = idleOpa * (1 - ringFade);
    const themed = (classic, neon, floral, winter) => christmasFace ? mix(winter, classic, themeOpa) :
      flowerFace ? mix(floral, classic, themeOpa) :
      synth > 0 ? mix(neon, classic, synth) : classic;
    if (synth > 0.01) s += synthBackdrop(synth, t);
    if (flowerFace && themeOpa > 0.01) s += flowerBackdrop(themeOpa, ringOpa, date.getSeconds());
    if (christmasFace && themeOpa > 0.01) s += christmasBackdrop(themeOpa);
    const clk = scene.clock.value(t);
    const h = Math.round(clk.r), top = Math.round(clk.cy - clk.r / 2);
    if (idleOpa > 0.01) {
      if (!flowerFace && ringOpa > 0.01) {
        s += arcPath(240, 240, 234, 4, 0, 360,
          themed('#1A1A1C', '#321841', FLOWER.ink, CHRISTMAS.track), ringOpa * (1 - saverLevel / 3));
        const secFrac = (date.getSeconds() * 10 + idiv(date.getMilliseconds(), 100)) / 600;
        const secondsColor = christmasFace ? mix(CHRISTMAS.gold, TIMER_COLORS[0], themeOpa) : TIMER_COLORS[0];
        s += synth > 0.01 ? synthRing(secFrac, ringOpa) :
          arcPath(240, 240, 234, 4, 270, 270 + 360 * secFrac, mix(saverAccent, secondsColor, saverLevel), ringOpa);
      }
      const dateColor = mix(themed(COLOR_MUTED, '#A4F5FF', '#6C304A', CHRISTMAS.gold), saverAccent, 1 - saverLevel * 3 / 8);
      s += text(240 + shiftX, top + shiftY + h + 34 + LINE_H[28] / 2, 28, dateColor,
        txt.date(txt, date), 500, idleOpa);
      const hintOpa = themeOpa;
      if (hintOpa > 0.01) {
        const size = synthFace || flowerFace || christmasFace ? 14 : 20;
        const hintTop = flowerFace ? 348 : christmasFace ? 344 : synthFace ? 342 : 372;
        s += text(240 + shiftX, hintTop + shiftY + LINE_H[size] / 2,
          size, themed(COLOR_FAINT, '#D6B2F2', '#3D5C4E', '#A6C3B4'), txt.hint, 500, hintOpa);
      }
    }
    const clockColor = mix(mix(themed(COLOR_TEXT, '#FFD4F7', FLOWER.ink, CHRISTMAS.snow), COLOR_MUTED, Math.round(idleOpa * 255) / 255),
      saverAccent, 1 - saverLevel);
    s += segCentered(240 + shiftX, top + shiftY, h, clockText(date), clockColor);

    const flagOpa = idleOpa * (1 - saverLevel);
    if (core.count === 0 && flagOpa > 0.01 &&
        state.birthdayDates?.some(([month, day]) => month === date.getMonth() + 1 && day === date.getDate())) {
      s += birthdayFlags(t, flagOpa, state.mirrorBirthdayFlags);
    }

    for (let c = 0; c < MAX_TIMERS; c++) {
      const g = scene.card(c, t);
      const i = core.timers.findIndex(tm => tm.color === c);
      if (i >= 0) {
        const prev = scene.content[c];
        scene.content[c] = cardContent(core, i, now, countUp, txt);
        if (scene.content[c].tooHard && prev && !prev.tooHard) scene.pulse(c, PULSE.pressure, t);
      }
      if (g.opa <= 0.01 || g.r < 10 || !scene.content[c]) continue;
      s += renderCard(scene.content[c], g);
    }

    const a = scene.add.value(t);
    const aOpa = clamp01(a.opa);
    if (aOpa > 0.01 && a.r > 0) {
      const { cx, cy, r } = a;
      s += layout(core.count).addIsGhost
        ? `<circle cx="${cx}" cy="${cy}" r="${r - 1.5}" fill="none" stroke="${COLOR_OUTLINE}" stroke-opacity="${aOpa.toFixed(3)}" stroke-width="3"/>`
        : `<circle cx="${cx}" cy="${cy}" r="${r}" fill="${COLOR_SURFACE}" fill-opacity="${aOpa.toFixed(3)}"/>`;
      s += plusGlyph(cx, cy, 20, COLOR_MUTED, aOpa);
    }
    return s;
  }

  // Settings screen geometry (src/ui/ui.cpp build_settings)
  const SETTINGS_BUTTONS = [
    { id: 'hourUp', x: 182 - 36, y: 108 - 22, w: 72, h: 44 },
    { id: 'hourDown', x: 182 - 36, y: 252 - 22, w: 72, h: 44 },
    { id: 'minUp', x: 298 - 36, y: 108 - 22, w: 72, h: 44 },
    { id: 'minDown', x: 298 - 36, y: 252 - 22, w: 72, h: 44 },
    { id: 'mode', x: 64, y: 286, w: 168, h: 44 },
    { id: 'sound', x: 248, y: 286, w: 168, h: 44 },
    { id: 'face', x: 114, y: 338, w: 252, h: 44 },
    { id: 'lang', x: 114, y: 390, w: 118, h: 44 },
    { id: 'done', x: 248, y: 390, w: 118, h: 44 },
  ];

  function renderSettings(st) {
    const txt = strings(st.lang);
    let s = text(240, 48 + LINE_H[20] / 2, 20, COLOR_MUTED, txt.settings);
    s += segCentered(240, 144, 72, `${pad2(st.hour)}:${pad2(st.minute)}`, COLOR_TEXT);
    for (const b of SETTINGS_BUTTONS) {
      const fill = b.id === 'done' ? '#1F8A4C' : COLOR_SURFACE;
      s += rrect(b.x, b.y, b.x + b.w, b.y + b.h, fill, 1);
      const cx = b.x + b.w / 2, cy = b.y + b.h / 2;
      if (b.id.endsWith('Up')) s += chevron(cx, cy, true, COLOR_TEXT);
      else if (b.id.endsWith('Down')) s += chevron(cx, cy, false, COLOR_TEXT);
      else if (b.id === 'mode') s += text(cx, cy, 20, COLOR_TEXT, st.countUp ? `\u2191  ${txt.countUp}` : `\u2193  ${txt.countdown}`);
      else if (b.id === 'sound') s += text(cx, cy, 20, COLOR_TEXT, st.sound ? `\u266A  ${txt.sound}` : `\u00D7  ${txt.muted}`);
      else if (b.id === 'face') s += text(cx, cy, 20, COLOR_TEXT, `${txt.face}: ${faceName(st.clockFace, txt)}`);
      else if (b.id === 'lang') s += text(cx, cy, 20, COLOR_TEXT, txt.language);
      else s += text(cx, cy, 20, '#FFFFFF', txt.ok);
    }
    return s;
  }

  /**
   * Renders a full device frame.
   * state: { screen: 'main'|'settings', core, scene?, now, animNow?, date, countUp, lang: 'en'|'da',
   *          clockFace?, saverLevel?, saverElapsedMs?, birthdayDates?: [month, day][], mirrorBirthdayFlags?,
   *          settings: {hour, minute, countUp, sound, lang, clockFace?} }
   * Without a scene the main screen is drawn fully settled (no transition in progress).
   */
  function renderSVG(state, opts) {
    const bezel = (opts && opts.bezel) || 20;
    const size = SCREEN + bezel * 2;
    let content;
    if (state.screen === 'settings') content = renderSettings(state.settings);
    else content = renderMain(Object.assign({}, state, { scene: state.scene || new BrushScene(state.core, state.animNow !== undefined ? state.animNow : state.now) }));
    return `<svg xmlns="http://www.w3.org/2000/svg" width="${size}" height="${size}" viewBox="0 0 ${size} ${size}">` +
      `<defs><clipPath id="screen"><circle cx="240" cy="240" r="240"/></clipPath>` +
      `<radialGradient id="bezel" cx="50%" cy="40%" r="60%"><stop offset="80%" stop-color="#2A2A2D"/><stop offset="100%" stop-color="#111113"/></radialGradient></defs>` +
      `<circle cx="${size / 2}" cy="${size / 2}" r="${size / 2}" fill="url(#bezel)"/>` +
      `<g transform="translate(${bezel} ${bezel})" clip-path="url(#screen)"><rect width="480" height="480" fill="#000"/>${content}</g></svg>`;
  }

  const api = { BrushCore, BrushScene, OralbTracker, parseOralb, oralbAdvert, ORALB, layout, formatMMSS, renderSVG, segWidth, SETTINGS_BUTTONS, MAX_TIMERS, PULSE, STRINGS, LANGS, CLOCK_FACES };
  if (typeof module !== 'undefined' && module.exports) module.exports = api;
  else root.BrushSim = api;
})(typeof window !== 'undefined' ? window : globalThis);
