# Chronograph

An activity tracker for Sailfish OS. It records runs, walks, bike rides, rowing and
drives with GPS on an OpenStreetMap map, stores each as a GPX file and replays it. It
counts your steps with the phone's accelerometer, also while the display is off, and
keeps the count of every day. A statistics page totals what you have done, and
achievements in seven levels, from Paper to Diamond, mark the milestones on the way -
and the holidays and days of history you were out on.

Everything stays on the phone. There is no account, no cloud and no upload; the only
thing Chronograph fetches from the internet is the map.

## What it does

- **Record an activity.** Running, walking, cycling, rowing or driving, chosen before
  the start. The activity page shows the duration, the distance, the current and the
  average pace and speed, the elevation and the height climbed, and the land you are
  passing through, with your position, direction and route on the map below. Tap to
  pause and resume; hold the button for three seconds to stop.
- **Smoothed GPS.** Every position goes through a filter tuned to the activity type
  before it is shown or stored. It weighs each fix by its accuracy and leaves out the
  jumps and spikes a phone's receiver produces now and then, so the route stays on the
  road and the distance, the pace and the climb stay steady. A row at the top shows
  how good the signal is.
- **Auto-pause.** Switched on beside the button, and on by default: the recording
  pauses on its own when you stand still for a few seconds, or move faster than the
  activity allows - on a bus, say - and resumes once you move on. Walking and running,
  your steps tell it too. The time you stood is not counted. Auto-paused for half an
  hour, the activity is stopped and stored. A pause by hand is only resumed by hand.
- **Charts.** Swiped to from the values: the pace and the elevation over the
  activity's time, with the average pace as a dotted line.
- **Land cover.** Swiped to after the charts: how far the activity went through
  forest, meadows and parks, farmland, heath and rock, water and built-up land, in
  kilometres and per cent - read off the colours of the map tiles that were on screen.
- **Replay.** The main page lists every activity, newest first. Tapping one replays it
  with a slider: the values, the charts and the route show the activity as it was at
  that moment.
- **Delete.** From an activity's context menu in the list or the pulley menu of its
  page, after a ten-second countdown that a tap undoes.
- **Cover.** While an activity is recorded, the cover shows its type, duration,
  distance and pace, and pauses or resumes it. Otherwise it shows today's steps.
- **Count steps.** From the moment the app starts, also with the display off and the
  phone in a pocket or a bag. A walk counts only after a few regular steps, which are
  then added at once, so picking the phone up, putting it down or shaking it counts
  nothing. The main page shows today's steps and those of the three days before; at
  midnight the count starts over.
- **Statistics.** From the main page's pulley menu: the distance, the time and the
  land cover of all activities together, then of each activity type.
- **Achievements.** From the same pulley menu, each in seven levels - Paper, Wood,
  Bronze, Silver, Gold, Platinum and Diamond - from the first try up to things few will
  ever reach:
  - the longest, the fastest and the distance in all of running, walking, cycling and
    rowing;
  - the biggest climb and the climb in all, the distance over each kind of land, the
    longest activity and the time in all, the number of activities and the most weeks
    in a row with one, the most steps in a day and the steps in all;
  - calendar days collected by being active on them: national days, holy days of the
    major religions, full moons, and the days of wars, peace treaties, revolutions, conquests,
    discoveries, firsts, inventions, natural disasters and total solar eclipses, the
    death days of dictators and the birthdays of composers. Which days count is left
    for you to find out - the app names a day only once you have collected it.

  A tap on one shows what it measures, what you achieved and when, and what the next
  level takes. Levels an activity reaches are announced on its page after the stop.
  A level once reached stays, even when the activities that reached it are deleted.
  Driving counts for none of them.

English and German.

## Installing

Chronograph needs **Sailfish OS 4.6** or newer, and runs on every later release (it is
used on 5.2 on a Jolla Phone 2026). It passes Harbour's validation (`sfdk check`) with
no exception, so it needs nothing that a store app may not have.

It asks for two Sailjail permissions as it starts for the first time:

- **Location** for GPS. It also gives access to the accelerometer, which the step
  counter needs.
- **Internet** for the map's tiles.

## Good to know

- **Battery.** To keep counting steps with the display off, Chronograph keeps the
  phone from going to sleep for as long as it runs, and GPS runs for as long as the
  activity page is open. Close the app when you do not need it; closing it stops the
  step counter.
- **Steps are counted only while the app runs.** There is no background service.
- **Leaving the activity page stops and stores the activity**, as does closing the
  app with the page open. Killing the app or a crash loses the activity in progress:
  nothing is written before the stop. Steps are saved half a minute after a walk, and
  every five minutes during a long one.
- **An activity without a single usable position is not stored**, and the page says
  so. Under a poor sky the filter needs a few consistent fixes before it shows a
  position at all; the map starts where the last activity ended until then.
- **The map uses mobile data**, but only while it is on screen: with the display off
  or the app in the background no tile is downloaded, as OpenStreetMap's [tile usage
  policy](https://operations.osmfoundation.org/policies/tiles/) asks. Tiles already
  seen are kept on the phone (up to 50 MB) and reused.
- **The land cover comes from the tiles that were on screen**, during the activity or
  when replaying it - nothing is downloaded for it. Stretches whose tiles were never
  shown count as unknown until they are. The tiles' colours tell coarse kinds of land
  only; a bridge counts as the road on it, a tunnel as the land above.
- **The GPX files** are in `~/.local/share/de.ygriega/chronograph/gpx/`, one per
  activity, named after its start in UTC. They hold the smoothed positions, the speed
  and the course in Garmin's track point extension, and the pauses as separate
  segments, so sport sites and other apps read them as they are.
- **Moving feasts** - Easter, Ramadan, Diwali and the like - are known up to 2031.
  Their dates follow one reference calendar each (Islamic feasts the Umm al-Qura
  calendar, Hindu, Sikh and Buddhist ones New Delhi's), so a feast may fall a day
  apart where you live.
- **The thresholds have not been tuned on many real activities yet.** The filter, the
  auto-pause limits and the land cover were tuned on generated tracks, the step
  counter on one walker's phone. Reports on how they fare for you are welcome.

## Privacy

Your activities, your steps and your achievements are stored on the phone only, in a
SQLite database and the GPX files in `~/.local/share/de.ygriega/chronograph/`, and
nowhere else. Chronograph sends nothing anywhere, with one exception: the map's tiles
come from `tile.openstreetmap.org`, which, like any tile server, learns which part of
the map is shown. The requests name the app in their User-Agent and carry nothing
else. Achievements and the land cover are worked out on the phone; nothing is fetched
for them.

## Reporting a problem

Open an [issue](https://github.com/Wunderfitz/harbour-chronograph/issues) with the
phone, its Sailfish OS version and what happened. `journalctl -f` as root while
reproducing shows the app's own warnings. Please do not attach GPX files or anything
else that shows where you go unless you are fine with it being public.

## Building

Out of tree, as Qt Creator does it: give `sfdk` the path to the sources and it
shadow-builds into the current directory.

```sh
mkdir -p ../build-harbour-chronograph-4.6.0.13-aarch64
cd ../build-harbour-chronograph-4.6.0.13-aarch64
sfdk -c target=SailfishOS-4.6.0.13-aarch64 build ../harbour-chronograph
sfdk -c target=SailfishOS-4.6.0.13-aarch64 check RPMS/*.rpm
```

The RPMs end up in `RPMS/` inside that build directory. Build against the oldest
supported target, 4.6.0.13 (Qt 5.6), for armv7hl, aarch64 or i486; the package runs on
every later release. Nothing has to be vendored or downloaded first: the app uses only
Qt, Silica and libkeepalive from the SDK.

The step detection, the GPS filter, the activity values, the auto-pause, the charts,
the land cover and the achievements are plain C++ without Qt, with tests that build
and run on the host:

```sh
g++ -std=c++11 -Wall -Wextra -O2 -Isrc tests/tst_track.cpp \
    src/Track.cpp -o /tmp/tst_track && /tmp/tst_track
```

[CLAUDE.md](CLAUDE.md) has the command for each test, how the database and history
tests run in the build engine, how to replay a generated NMEA log in the emulator, and
every design decision with the reason behind it.

## Layout

```
harbour-chronograph.pro   one qmake project: Qt, Silica, QtSensors, QtPositioning, QtSql
src/                      C++: step counting, recording, filter, GPX, database, achievements
qml/                      the pages, the cover, the map and the components
images/                   the icon master, the cover backdrops and the pictograms (SVG)
icons/                    the launcher icon, rendered
translations/             the German catalogue
tests/                    host-side tests and those that run in the build engine
rpm/                      package spec and changelog
CLAUDE.md                 how the app is put together and why
```

## Licence

Chronograph is GPLv3 or later - see [LICENSE](LICENSE) for the licence text, and the
header on each source file for the grant.

## Credits

- Map data © [OpenStreetMap](https://www.openstreetmap.org/copyright) contributors,
  available under the Open Database License. Map tiles by the OpenStreetMap
  Foundation; the land cover is read off the colours of its standard style,
  [OpenStreetMap Carto](https://github.com/gravitystorm/openstreetmap-carto).
- The app icon follows Jolla's apps icon story for Sailfish OS.
- Written by Sebastian J. Wolf [sebastian@ygriega.de](mailto:sebastian@ygriega.de)
  together with Claude, Anthropic's assistant, in
  [Claude Code](https://claude.com/claude-code).
