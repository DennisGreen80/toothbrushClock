# Toothbrush Timer

A sleek bathroom clock and toothbrush timer for the round
[Waveshare ESP32-S3-Touch-LCD-2.8C](https://docs.waveshare.com/ESP32-S3-Touch-LCD-2.8C)
(480×480 ST7701 display, GT911 touch, PCF85063 RTC, buzzer).

| Idle clock | One timer | Two timers | Four timers |
| :---: | :---: | :---: | :---: |
| ![Idle clock](docs/screens/1-idle-clock.svg) | ![One timer](docs/screens/2-one-timer.svg) | ![Two timers](docs/screens/3-two-timers.svg) | ![Four timers](docs/screens/5-four-timers.svg) |
| **Three timers + add slot** | **Done (2:00 reached)** | **Count-up mode** | **Settings** |
| ![Three timers](docs/screens/4-three-timers.svg) | ![Done](docs/screens/6-done.svg) | ![Count up](docs/screens/7-count-up-mode.svg) | ![Settings](docs/screens/8-settings.svg) |

### Clock faces

**Classic** keeps the original look. **Synthwave** adds a striped sunset, a perspective grid that
slowly cycles through neon colors, and a seconds ring that reveals a fixed gradient as the minute
progresses. **Flower Power** takes its cue from late-'60s screen prints: a cream dial, bold
daisies, curved stripes, and a rim that lights one dot or daisy per second, then resets each
minute. **Christmas** (Danish: **Jul**) pairs a deep evergreen dial with hanging ornaments,
a little winter forest, and a gold seconds sweep. **90s** uses bright Memphis-inspired shapes,
while **Reindeer** (Danish: **Rensdyr**) has a snowy night and a reindeer. **Woven Heart** (Danish:
**Julehjerte**) is a felt-style Danish braided heart with a red bow, pom-poms and embroidery on a
stitched green dial, and **Candy Cane** (Danish: **Slikstok**) bends two candy canes into a heart above
peppermints and sprinkles, with a red-and-white striped seconds ring. Hold the idle clock (or tap the gear) to open Settings, tap **Face** (Danish:
**Urskive**) to cycle the options, then tap **Done** to save. Alternatively, swipe left or right
on the idle clock to change and save the face. The browser simulator also has a
**Clock face** selector. While timers run, the face is simplified to a plain dial in its colors behind
the rings (light faces like Flower Power and Candy Cane keep their light dial, with timer colors
darkened slightly for contrast); the long-idle screensaver is shared by all faces.

| Synthwave | Flower Power | Christmas |
| :---: | :---: | :---: |
| ![Synthwave clock face](docs/screens/14-synthwave.svg) | ![Flower Power clock face](docs/screens/15-flower-power.svg) | ![Christmas clock face](docs/screens/16-christmas.svg) |

| 90s | Reindeer | Woven Heart | Candy Cane |
| :---: | :---: | :---: | :---: |
| ![90s clock face](docs/screens/17-nineties.svg) | ![Reindeer clock face](docs/screens/18-reindeer.svg) | ![Woven Heart clock face](docs/screens/19-woven-heart.svg) | ![Candy Cane clock face](docs/screens/20-candy-cane.svg) |

### Long-idle screensaver

After 15 minutes without touch and with no timer running, the clock settles into a dim screensaver:
the seconds ring fades away and the accent starts cycling through the timer palette. The clock
and date stay centered for the first 5 minutes of screensaver mode, then gently drift.
Use **Exit screensaver** in the browser preview to wake the clock without starting a timer.

![Long-idle screensaver](docs/screens/12-screensaver.svg)

### Birthday flags

On a listed birthday, two Danish flags wave above the idle clock, with soft highlights that follow
the folds. They stay out of the way while a timer is visible or the screensaver is running.

![Birthday flags](docs/screens/13-birthday.svg)

Add recurring dates to `kBirthdays` in [`src/ui/ui.cpp`](src/ui/ui.cpp) and rebuild the firmware.
Entries are `{month, day}` (for example, `{6, 14},` is 14 June); no dates are enabled by default.
To see the animation without changing the date, turn on **Preview birthday flags** in the simulator.
Use **Birthday flag layout** to compare the default with the mirrored version.

### Transitions

Starting the first timer – the date fades, the big clock glides up into a small header and the ring blooms in:

![Start transition](docs/screens/transition-start.svg)

Adding a timer – the "+" morphs into the new ring while the existing ring slides aside:

![Add transition](docs/screens/transition-add.svg)

Last timer removed – the ring shrinks away and the big clock returns:

![Finish transition](docs/screens/transition-finish.svg)

### Oral-B toothbrush timers

| Brushing | Too hard | Mixed with manual timers |
| :---: | :---: | :---: |
| ![Oral-B brushing](docs/screens/9-oralb-brushing.svg) | ![Too hard](docs/screens/10-oralb-too-hard.svg) | ![Mixed](docs/screens/11-oralb-mixed.svg) |

![Oral-B start transition](docs/screens/transition-oralb-start.svg)

## Features

- **Clock** – large segment-style `HH:MM`, date, and a sweeping seconds ring. Time is kept by the
  on-board PCF85063 RTC (the system clock is re-synced from it every 10 minutes).
- **Brush timers** – tap the clock to start a timer (2:00 by default; 1:40, 1:00 or 0:40 in settings).
  The ring is split into four mouth quadrants (upper right → lower right → lower left → upper left),
  one per quarter of the brushing time (30 s with 2:00).
- **Several timers at once** – up to 4 (one per family member, each in its own color; the first
  timer gets a random color). The layout
  adapts: one big ring, two side by side, or a 2×2 grid with a "+" slot.
- **Countdown or count-up** – selectable in settings. When the time is up the ring morphs into a
  star-eyed smiley; finished timers disappear automatically 6 seconds later.
- **Finish smiley** – when a timer finishes, its digits fade and the ring contracts into a smiley
  in the timer's color that rates the brushing by quarters of the brushing time (with 2:00: under
  30 s a straight mouth, 30–59 s a half smile, 60–90 s a smile, 91–120 s a smile with twinkling gold
  star eyes). A cancelled timer shows its
  smiley for 6 s before disappearing. Preview: [preview/smileys.html](preview/smileys.html).
- **Smooth transitions** – every layout change is animated (eased moves, springy ring bloom,
  fades); rings "breathe" briefly on each quarter mark and with a bigger pulse when done; the
  settings screen cross-fades; the backlight fades in on boot and fades when dimming/waking.
- **Buzzer cues** – the on-board active buzzer is switched on or off; software can't adjust its volume.
  Short cues mark brushing progress (shown for 2:00; the marks are quarters of the brushing time).
  Power-up, settings, pause/resume and cancelling are silent.
  | Event | Pattern |
  | --- | --- |
  | Timer started | One 45 ms pip |
  | 0:30 / 1:00 / 1:30 | One / two / three 30 ms pips |
  | 2:00 reached | Two longer pips (45 ms and 75 ms) |
- **Auto-dim and screensaver** – the backlight dims after 3 minutes without touches when no timer
  is running, then drops to 8% brightness after 15 minutes. The screensaver keeps a soft, color-cycling
  clock and date on screen, with a slow position shift starting at 20 minutes of total inactivity.
  Touching the screen wakes it smoothly; that first touch only wakes it, so tap again to start a timer.
- **Oral-B toothbrush integration** – switching on an Oral-B Bluetooth brush (iO series tested in
  the protocol fixtures; other Bluetooth models use the same advertisement) automatically starts a
  timer with a toothbrush icon, synced to the brush's own session clock. See
  [Oral-B toothbrushes](#oral-b-toothbrushes).
- **English and Danish** – switch in settings (stored in flash; Danish is the default on first
  boot). Danish screenshots: [docs/screens/da](docs/screens/da).

  | Idle | Timers | Settings |
  | :---: | :---: | :---: |
  | ![Idle (da)](docs/screens/da/1-idle-clock.svg) | ![Timers (da)](docs/screens/da/4-three-timers.svg) | ![Settings (da)](docs/screens/da/8-settings.svg) |

## Using it

| Where | Tap | Hold (0.4 s) |
| --- | --- | --- |
| Idle clock | start a timer | open settings |
| ⚙ gear (right of the idle clock) | open settings (set the time, mode, sound, face, language) | – |
| Timer ring | pause / resume (dismiss when done or showing a smiley) | cancel the timer (shows its smiley for 6 s) |
| Toothbrush ring | – while brushing (dismiss when done or showing a smiley) | dismiss the timer (shows its smiley for 6 s) |
| **+** button / empty slot | add another timer | – |

Swipe left or right on the idle clock to cycle clock faces. A press only counts as a tap if the finger
stays within ~24 px, so a swipe never starts a timer by accident.

**Settings** lets you set the time (hold the arrows to repeat), switch countdown/count-up,
mute the buzzer, set the screen brightness (25–100 %, applied immediately; dimming and the
screensaver scale with it), turn the Christmas faces' custom font on/off (**Aa ✓ / ✕**), choose the
brushing time (**🔔 2:00 / 1:40 / 1:00 / 0:40**) and change
the language. Settings are stored in flash (NVS); the time is
written to the RTC.

> **Keeping time without power:** the RTC only survives power loss with a rechargeable cell
> connected to the board's *RTC battery header*. Without one, the firmware seeds the
> clock from the firmware build time on first boot – just correct it in settings.

## Oral-B toothbrushes

Oral-B Bluetooth brushes broadcast their status in BLE advertisements (manufacturer data, company
ID `0x00DC`) several times per second. The timer listens **passively** – it never connects or
pairs, so the Oral-B app keeps working normally and nothing needs to be set up.

| Brush | Timer |
| --- | --- |
| Switched on | a toothbrush timer appears ("bi-bi-beeep"), already at the brush's own time |
| Switched off | the timer pauses |
| On again within 30 s | the same timer resumes silently |
| Off for more than 30 s | the timer disappears |
| Out of range / silent for 4 s | treated as switched off |

- Toothbrush timers show a **toothbrush icon** and an **inner pressure ring**: soft white
  normally, **red with "Too hard"** (and a small pulse) while the brush reports too much pressure.
- The 30 s / 2:00 sounds are the same as for manual timers. Several brushes are tracked
  independently (each gets its own ring, up to 4 timers in total).
- Tapping a toothbrush ring does nothing while brushing (use the brush's button); tap a finished
  one to dismiss it. Holding a ring dismisses it – it won't come back until that brush has been
  off for 30 s.
- **Pressure colors:** the passive advertisement only carries a "too hard" flag. The brush's
  white/green distinction (too soft vs. optimal) is only available over an active GATT connection,
  so the ring stays neutral white instead of turning green.
- BLE scanning uses the NimBLE host built into ESP-IDF. Protocol details are based on the open-source
  [oralb-ble](https://github.com/Bluetooth-Devices/oralb-ble) parser.

## Previews

- `docs/screens/*.svg` – static screenshots (regenerate with `node preview/render.js`)
- `preview/index.html` – interactive simulator: open it in a browser, click/hold on the screen,
  preview the long-idle screensaver, speed up time, hear the buzzer patterns and switch two virtual
  Oral-B brushes on/off.

The preview (`preview/sim.js`) is a port of the timer logic, scene animations, layout and drawing
code, so the screenshots match the device geometry. Keep it in sync when changing the UI.
`node preview/render.js` also writes the transition filmstrips.

## Building and flashing

The firmware is an [ESP-IDF](https://docs.espressif.com/projects/esp-idf/en/v6.1.1/esp32s3/)
project (ESP-IDF **6.1.1**, LVGL **9.6**) built with [pioarduino](https://github.com/pioarduino/platform-espressif32),
the community fork of PlatformIO's ESP32 platform (VS Code extension or CLI). `platformio.ini` pins
pioarduino **61.04.00**, which ships ESP-IDF 6.1.1; the toolchain, ESP-IDF and LVGL (from the
component registry) are downloaded on the first build.

```sh
pio run -e waveshare_28c                 # build (the default environment)
pio run -e waveshare_28c -t upload       # flash via the USB-C "UART" port
pio device monitor -e waveshare_28c      # serial log at 115200 baud
pio test -e native                       # unit tests for the timer logic (needs a host C compiler)
```

If the upload doesn't start automatically, hold **BOOT**, tap **RESET**, release **BOOT** and retry.
Settings saved by the earlier Arduino firmware are kept (same NVS partition, namespace and keys).
It is a regular ESP-IDF project, so `idf.py set-target esp32s3` + `idf.py build flash monitor` works too.

`sdkconfig.defaults` holds the project configuration (flash/PSRAM, NimBLE, LVGL). pioarduino keeps
the resulting configuration in `sdkconfig.waveshare_28c` (idf.py: `sdkconfig`); values already in
that file win over the defaults, so delete it after editing `sdkconfig.defaults`.
`pio run -e waveshare_28c -t menuconfig` (or `idf.py menuconfig`) → **Toothbrush timer** has the display tunables:

| Option | Default | |
| --- | --- | --- |
| LCD pixel clock (MHz) | 18 | ~65 Hz refresh. Below ~12 MHz colored backgrounds pulse in intensity |
| LCD bounce buffer height (lines) | 10 | lines per internal-RAM bounce buffer (4–40, must divide 480) |

The RGB panel scans out a frame buffer in PSRAM. With bounce buffers the LCD DMA reads from two
small internal-RAM buffers that the CPU refills from PSRAM, so heavy PSRAM traffic (LVGL rendering,
flash writes) no longer starves the panel – the cause of the sideways drift seen above 16 MHz in the
Arduino build. If the picture ever shifts or tears, lower the pixel clock or raise the bounce lines.

## Project layout

```
platformio.ini            pioarduino build (ESP-IDF 6.1.1) + native unit-test environment
CMakeLists.txt            ESP-IDF project
sdkconfig.defaults        ESP32-S3, 16 MB flash, 8 MB octal PSRAM, NimBLE, LVGL configuration
partitions.csv            NVS + 6 MB app
main/                     ESP-IDF component that builds src/ (CMakeLists, idf_component.yml
                          with the LVGL dependency, Kconfig "Toothbrush timer" menu)
lib/brush_core/           platform-independent logic (unit tested; also an ESP-IDF component)
  brush_core.*            timers, 30 s marks, screen layout
  brush_scene.*           animated scene: tweens/easing between layouts, ring pulses
  brush_i18n.*            English/Danish strings and date format
  brush_oralb.*           Oral-B advertisement parser + brush-to-timer tracking
src/main.cpp              app_main + UI task, LVGL glue, timekeeping, settings (NVS), app actions
src/board/                hardware drivers
  pins.h                  pin map (from the board schematic)
  hal.h                   millis()/delay_ms()
  i2c_bus.*               shared I2C master bus
  exio.*                  TCA9554 IO expander (LCD/touch reset, LCD CS, buzzer)
  display.*               ST7701 init over 3-wire SPI + RGB565 panel (bounce buffers) + PWM backlight
  touch.*                 GT911 touch controller
  rtc_pcf85063.*          PCF85063 real-time clock
  buzzer.*                buzzer sound patterns (FreeRTOS task)
  oralb_scanner.*         passive BLE scan (ESP-IDF NimBLE) for Oral-B advertisements
src/ui/                   LVGL user interface
  lv_compat.h             small LVGL 9 helpers
  seg_digits.*            scalable segment-style digits (custom drawn)
  brush_icon.*            scalable toothbrush icon (custom drawn)
  ui.*                    idle clock, adaptive timer rings, settings screen
  fonts/                  Montserrat 14/20/28 with ÆØÅæøå + LVGL symbols, Berkshire Swash / Fredoka
                          clock + date fonts for the Christmas faces (generated)
test/test_brush_core/     Unity tests for lib/brush_core
preview/                  browser simulator + SVG screenshot renderer
```
## Hardware notes

| Function | Connection |
| --- | --- |
| I2C (touch, RTC, IMU, expander) | SDA GPIO15, SCL GPIO7 |
| LCD RGB data | B: 5, 45, 48, 47, 21 · G: 14, 13, 12, 11, 10, 9 · R: 46, 3, 8, 18, 17 |
| LCD sync | HSYNC 38, VSYNC 39, DE 40, PCLK 41 |
| LCD init SPI | SDA GPIO1, SCK GPIO2, CS = expander P2 |
| Backlight | GPIO6 (PWM) |
| Touch INT / RST | GPIO16 / expander P1 |
| LCD reset | expander P0 |
| Buzzer | expander P7 (active buzzer, on/off) |

Tunables (durations, brightness, dimming, default language) are at the top of `src/main.cpp`;
colors and layout are in `src/ui/ui.cpp` and `lib/brush_core/src/brush_core.c`.

Montserrat is licensed under the SIL Open Font License 1.1 and FontAwesome Free under the
SIL OFL 1.1 (fonts); both are redistributed by LVGL. Berkshire Swash and Fredoka (SIL OFL 1.1) come
from Google Fonts via [Fontsource](https://fontsource.org/); see `preview/fonts/*-LICENSE.txt`.

## Translations and fonts

All user-visible text lives in `lib/brush_core/src/brush_i18n.c` (mirrored in `preview/sim.js`).
LVGL's built-in fonts are ASCII-only, so `src/ui/fonts/tb_font_*.c` were generated with
[lv_font_conv](https://github.com/lvgl/lv_font_conv) from the Montserrat-Medium and FontAwesome
files in LVGL's `scripts/built_in_font` folder. Text using characters outside the font renders as
boxes – when adding a language or characters, extend the `-r` ranges and regenerate (run in that
folder, once per size 14/20/28):

```sh
npx lv_font_conv@1.5.2 --no-compress --no-prefilter --bpp 4 --size 20 \
  --font Montserrat-Medium.ttf -r 0x20-0x7F,0xB0,0x2022,0xC5,0xC6,0xD8,0xE5,0xE6,0xF8 \
  --font FontAwesome5-Solid+Brands+Regular.woff \
  -r 61441,61448,61451,61452,61453,61457,61459,61461,61465,61468,61473,61478,61479,61480,61502,61507,61512,61515,61516,61517,61521,61522,61523,61524,61543,61544,61550,61552,61553,61556,61559,61560,61561,61563,61587,61589,61636,61637,61639,61641,61664,61671,61674,61683,61724,61732,61787,61931,62016,62017,62018,62019,62020,62087,62099,62212,62189,62810,63426,63650 \
  --format lvgl --force-fast-kern-format --lv-include lvgl.h -o tb_font_20.c
```

The Christmas faces use **Berkshire Swash** (Christmas, Reindeer, Woven Heart) and **Fredoka**
(Candy Cane) for the idle clock and the date; the small hint stays in Montserrat for legibility.
The clock fonts contain only `0-9` and `:`, sized so the widest time clears the settings gear
(`preview/fonts.html` compares other candidates). Regenerate from the Fontsource WOFF files
(`npm pack @fontsource/berkshire-swash @fontsource/fredoka`, files in `package/files/`):

```sh
npx lv_font_conv@1.5.2 --no-compress --no-prefilter --bpp 4 --size 126 \
  --font berkshire-swash-latin-400-normal.woff -r 0x30-0x3A \
  --format lvgl --force-fast-kern-format --lv-include lvgl.h -o tb_font_berkshire_clock.c
npx lv_font_conv@1.5.2 --no-compress --no-prefilter --bpp 4 --size 28 \
  --font berkshire-swash-latin-400-normal.woff -r 0x20-0x7F,0xC5,0xC6,0xD8,0xE5,0xE6,0xF8 \
  --format lvgl --force-fast-kern-format --lv-include lvgl.h -o tb_font_berkshire_28.c
# Fredoka: same with fredoka-latin-600-normal.woff, --size 130 (clock) and 28 (date)
```
