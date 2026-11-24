# Ideas for future features

Collected suggestions, roughly ordered from small to large effort within each group.

## Motivation (especially for kids)

1. **Streaks** – count days in a row with two good brushes (3+ smileys) and show a small flame/star
   counter on the idle clock, with a celebration at milestones (7, 30 days…). The RTC already keeps
   the date; it needs a small history in flash (NVS).
2. **Morning/evening reminder** – at set times, gently pulse the clock (or a soft pip) if nobody has
   brushed yet; stops once a timer has run.
3. **Character instead of a plain ring** – something that fills up while brushing: a tooth getting
   shinier, a snowman being built on the Christmas faces, a flower growing on Flower Power.
   Selectable per face.
4. **Sticker collection** – every good brush earns a small collectible, shown on a swipeable
   "sticker book" screen.

## Family use

5. **Named timers** – assign a person to each of the four colors ("Emma = pink") with a small name or
   initial on the ring. Also makes streaks per person.
6. **Pair a toothbrush with a person** – remember which Oral-B brush belongs to whom, so its timer
   always gets that person's color and name.
7. **Weekly overview** – a screen with the last 7 days as rows of mini smileys per person.

## Oral-B integration

8. **Pressure statistics** – count "too hard" moments per session and show them on the finish smiley
   (e.g. a small red dot).
9. **Brushing mode display** – the brush reports its mode (daily clean, sensitive, whitening…);
   it is already parsed but unused. Could be a small icon.

## Device

10. **Night mode** – at night automatically use a dimmer screen, a warm dim color scheme and silent
    buzzer cues. Builds on the brightness setting.
11. **Time from the internet** – sync the clock over Wi-Fi (incl. daylight saving), so it never needs
    setting by hand and doesn't depend on the RTC battery.
12. **Seasonal faces that switch automatically** – a seasonal face in December, around Easter or on
    birthdays (there is already a birthday list), then back to your normal face.

## Bigger projects

13. **Phone page over Wi-Fi** – a small web page served by the device: brushing history, rename
    people, change settings, upload birthdays.
14. ~~**Bounce buffers / higher refresh rate**~~ – done: the ESP-IDF 6.1 port uses RGB bounce
    buffers and an 18 MHz pixel clock (configurable in menuconfig).

Top picks: **streaks (1)**, **named timers (5)** and **night mode (10)**.
