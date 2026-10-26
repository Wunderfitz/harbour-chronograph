# harbour-chronograph

An activity tracker for Sailfish OS, built **iteratively**. Iteration 1 (version
0.1) counts steps with the accelerometer from the moment the app starts, also
while the display is off, and shows the count on the main page and the cover.
Iteration 2 (0.2) keeps the steps of each calendar day in a local SQLite
database; the main page and the cover show today's, the main page also the
three days before. Iteration 3 (0.3) records runs, walks, bike rides, rowing
and drives with GPS on an activity page with an OpenStreetMap map, stores each
as a GPX file with a row in the database, lists them on the main page and
replays them; swiped to from the values, charts show the pace and the
elevation over the activity's time, and further on the distance over each
land cover, read off the map's tiles. An activity is deleted from the list's
context menu or its page's pulley, after a ten-second remorse timer. While
one is recorded, the cover shows it and pauses or resumes it. Auto-pause
pauses and resumes a recording on its own: standing, too fast for the type,
and after half an hour paused it stops it. A statistics page, from the main
page's pulley, totals the distance, the time and the land cover of all
activities together and of each type. An achievements page, from the same
pulley, shows 26 categories of achievement - the longest, fastest and
farthest per type, the climb, the land cover, the time, the weeks in a row,
the steps - each with seven levels from Paper to Diamond, kept for good once
reached.

## Ground rules

Set by the project owner at the start; they apply to every iteration.

- **C++ and QML/JavaScript**, licensed **GPLv3 or later**. Every file under
  `src/`, `qml/`, `tests/` and `images/` carries the GPL header block; new files
  get the same one. The spec says `License: GPLv3+`.
- **Style follows the author's other apps**: harbour-lauscher first (the most
  recent, and the closest in idiom), then harbour-fernschreiber and
  harbour-piepmatz. All three are checked out under `~/git`. Look there before
  inventing a pattern - pages, cover, about page, comments, naming, `.pro` and
  spec layout all come from them.
- **Sailfish OS 4.6 or newer.** Build against the oldest supported target,
  `SailfishOS-4.6.0.13`, which is Qt 5.6.3: no Qt API newer than that.
- **Harbour-compliant where possible.** Any deviation is asked for first and
  then recorded under [Harbour exceptions](#harbour-exceptions). Do not add a
  new one without asking.
- **Record architectural decisions in this file.** Say what was decided and why,
  and keep it current. A decision that changes gets rewritten, not appended to.
- **Recordings of real walks are personal data** (added 2026-10-14). The raw
  recordings and everything that describes them - timelines, dates and times,
  places, what the walker did, how the phone was carried part by part, the
  analysis of them - live in `tests/raw-data/`, which `.gitignore` excludes,
  and stay on the project owner's machine. Never commit, push, upload or paste
  them anywhere, and do not quote them in committed files: code comments,
  this file, READMEs, commit messages, the changelog. **No recorded data is
  committed at all, not even excerpts**: the samples carry the walker's gait.
  Committed tests are synthetic - a lesson from a recording becomes a generated
  signal with generic parameters, never one fitted to or copied from it. A new
  recording goes straight into `tests/raw-data/`, never into the checkout's
  root or elsewhere.
  Before every commit, check that `git status` shows nothing from
  `tests/raw-data/` and no `recording-*.csv`.

## Harbour exceptions

None. The app is Harbour-clean: `sfdk check` reports no error.

Until 0.3 there was one, `Permissions=Sensors` in `[X-Sailjail]` (approved
2026-10-13): Sailjail lets an app talk to sensorfw (`com.nokia.SensorService`
on the system bus) only with it, and the Harbour validator rejects it (`X-Sailjail
permission not allowed: Sensors`, 4.6 and 5.1 tooling alike). **Dropped
2026-10-17**, at the project owner's decision: activities brought in
`Location`, and Sailjail's `Location.permission` includes `Sensors.permission`
(4.6, 5.1 and the phone's 5.2 alike), so the accelerometer stays reachable.
`[X-Sailjail]` asks for `Location` and `Internet`, both on Harbour's allowed
list, as are QtPositioning, QtNetwork and QtSensors. **Do not remove
`Location`** without bringing `Sensors` back: step counting would silently
stop. Should a later Sailfish OS take `Sensors` out of `Location`, the
accelerometer turns `Unavailable` and the exception has to come back.

The spec still does not `Require` `qt5-qtsensors-plugin-sensorfw`,
`sailjail-permissions` or any other plugin: none is on Harbour's allowed list,
and every device ships them.

Harbour's allowed lists live in the build engine, not on the web. Ask them
rather than guessing:

```sh
sfdk engine exec cat /srv/mer/toolings/SailfishOS-4.6.0.13/usr/share/sdk-harbour-rpmvalidator/allowed_permissions.conf
# allowed_libraries.conf, allowed_qmlimports.conf and allowed_requires.conf sit beside it
```

## Architecture decisions

### Flat single-project layout

One qmake project at the root, in the layout of Fernschreiber and Piepmatz
(`src/`, `qml/`, `images/`, `icons/`, `translations/`, `rpm/`). Lauscher's
`app/` + subdirs split exists only because it vendors a C++20 library; nothing
here needs that. Revisit if a library ever has to be built alongside.

### Steps are found in Qt-free C++, counted by a QObject, kept by another

- `StepDetector` (`src/StepDetector.*`) is the algorithm, in plain C++11 with no
  Qt at all: timestamps and three axes in, steps out. That is what lets
  `tests/tst_stepdetector.cpp` drive it on the host with a bare `g++`.
- `StepCounter` (`src/StepCounter.*`) is the QML-facing `QObject`: it owns the
  `QAccelerometer` and the keepalive, feeds readings to the detector and
  exposes `steps` and `status` (`Starting`, `Counting`, `Unavailable`).
- `StepHistory` (`src/StepHistory.*`) keeps the steps per calendar day (see
  [Persistence](#persistence-sqlite-one-row-per-day)). It gets them through
  `StepCounter::stepsAdded(int)` and exposes `todaySteps` and `recentDays`.
  `StepCounter::steps` is still the count since the app started; only the
  recording uses it now.
- `main.cpp` constructs `Database`, `StepHistory` and `StepCounter` **before**
  the view, so counting starts with the app rather than with whichever page
  first looks at it, and hands the latter two to QML as the `stepHistory` and
  `stepCounter` context properties. They live on `main()`'s stack, so they are
  destroyed after the event loop and the view, in reverse order: that is when
  `StepHistory` makes its last save. `StepCounter` is also registered as
  uncreatable (`de.ygriega.chronograph 1.0`, `StepCounter`) only so QML can name
  the status values - the same split Lauscher uses for `mdr`/`Mdr`.

History, settings and anything else stateful belong in C++ classes next to
`StepHistory`, not in QML.

### Activities: computed in Qt-free C++, recorded by a QObject, kept by a model

The same split as for steps:

- `TrackFilter` (`src/TrackFilter.*`) smooths the receiver's fixes before
  anything else sees them, in plain C++11 without Qt; `tests/tst_trackfilter.cpp`
  drives it on the host. See [Smoothing the fixes](#smoothing-the-fixes-a-kalman-filter-per-activity-type).
- `Track` (`src/Track.*`) is an activity as recorded, in plain C++11 without
  Qt: segments (start or resume to pause or stop) and the points within them,
  and what follows from those at every point - elapsed time with the pauses
  left out, distance, current and average speed, smoothed altitude, climb.
  `tests/tst_track.cpp` drives it on the host. **Live recording and replay go
  through the same calls** (`begin()`, `addPoint()`, `end()`), so a stored
  activity replays exactly as it was shown while recorded - only an
  auto-pause ends a segment with `endAt()`, which may lie behind its latest
  points and drops them, and a replay sees only what is left; `stateAt(elapsed)`
  answers both. Its rules, none tuned on real activities yet:
  - distance: straight lines between points at least 5 m apart, so a standing
    receiver's wander adds nothing; a point whose own speed says standing
    (< 0.5 m/s) does not move it on; nothing counts across a pause. The last
    step of under 5 m of each segment is not counted;
  - current speed: the points' own speed - the filter's, or in a file from
    elsewhere the receiver's - averaged over 5 s, or the distance over 10 s
    for points without one; 0 once the last point is over 5 s old or its
    segment has ended;
  - altitude smoothed with a 5 s time constant, climb counted with 3 m of
    hysteresis.
  The tests model the receiver's error as a slowly wandering offset rather
  than fresh noise per fix: a phone's chipset filters its output, and fresh
  noise would make any 5 m rule overcount by half. `Track` itself does not
  care whether its points are filtered: it treats the filter's estimates
  and the raw fixes of a 0.3 file alike.
- `Activity` (`src/Activity.*`) is the QML-facing `QObject`, **created by
  QML, one per activity page** (`de.ygriega.chronograph 1.0`, `Activity`) -
  unlike `StepCounter`, as the receiver must run only while that page is
  open. It implements `QQmlParserStatus`: at `componentComplete()` an empty
  `fileName` means record, a set one means replay that file. It exposes the
  status (`Ready`, `Recording`, `Paused`, `Finished`), the signal quality,
  the position and all values, and `replayPosition` for the slider.
- `ActivityHistory` (`src/ActivityHistory.*`) is the list on the main page, a
  `QAbstractListModel` over the `activities` table, and owns the GPX
  directory: `Activity` hands it a finished `Track`, it writes the file and
  the row; `remove()` deletes both again. Constructed in `main()` before the view like the others, so that
  an activity page destroyed with the view still finds it. It also names
  the activity being recorded, for the cover - see [The cover](#the-cover).
- `Gpx` (`src/Gpx.*`) reads and writes a `Track` as GPX 1.1.
  `TrackReader` (`src/TrackReader.*`) reads a stored activity's file with
  it in a thread of its own - see opening a stored activity under [The
  activity page](#the-activity-page).
- `TrackLine` (`src/TrackLine.*`) paints the route on the map.
- `AutoPause` (`src/AutoPause.*`) decides when a recording pauses, resumes
  and stops on its own, in plain C++11 without Qt; `tests/tst_autopause.cpp`
  drives it on the host. `Activity` feeds it and does what it says. See
  [Auto-pause](#auto-pause-standing-too-fast-and-the-stop-after-half-an-hour).
- `TrackChart` (`src/TrackChart.*`) is a `Track`'s pace or elevation per
  point and the axes for them, in plain C++11 without Qt;
  `tests/tst_trackchart.cpp` drives it on the host. `ChartLine`
  (`src/ChartLine.*`) paints it. See [The charts](#the-charts-painted-in-c-like-the-route).
- `LandCover` (`src/LandCover.*`) tells the land cover around a place from a
  tile's colours, in plain C++11 without Qt; `tests/tst_landcover.cpp` drives
  it on the host with generated tiles. `TileCache` (`src/TileCache.*`) hands
  it the map's cached tiles, in `LandCoverReader`'s thread
  (`src/LandCoverReader.*`). See [Land cover](#land-cover-read-off-the-maps-tiles).
- `ActivityStatistics` (`src/ActivityStatistics.*`) works out the statistics
  page's totals and keeps them; `TrackSummaryReader`
  (`src/TrackSummaryReader.*`) reads the land cover and the climb of
  activities from before off their files, in a thread. See [The
  statistics](#the-statistics-worked-out-as-activities-change-read-when-shown).
- `AchievementCatalogue` (`src/AchievementCatalogue.*`) is the achievements'
  categories, their levels and how far the activities and the steps have
  come, in plain C++11 without Qt; `tests/tst_achievements.cpp` drives it on
  the host. `Achievements` (`src/Achievements.*`) reads the database for it
  and keeps the levels. See [The
  achievements](#the-achievements-seven-levels-kept-for-good).

### Positioning: QtPositioning in C++

`QGeoPositionInfoSource::createDefaultSource()` (`QT += positioning`; geoclue
with the hybris provider on a phone), satellite methods, a fix a second.
Sailfish OS ships QtPositioning **5.4** beside Qt 5.6: nothing newer than 5.4
in that module.

- Started when the activity page is built, stopped at the stop, never for a
  replay. While it runs `Activity` holds a `BackgroundActivity` of its own, so
  a recording carries on with the display off even if `StepCounter` has given
  its keepalive up.
- **Fixes are timed by the phone's clock** on arrival, not by their own
  timestamp: the segments begin and end by that clock, and a fix must not
  seem to predate its segment. A fix whose own timestamp is over 10 s old is
  a stale last-known position and is dropped.
- **Signal quality** from the horizontal accuracy: good up to 15 m (green),
  fair up to 40 m (yellow), poor beyond (red), as are no fix for 5 s, no
  positioning plugin, and location switched off. Poor fixes go nowhere - not
  into the filter, the map or the track - but are what the GPS row shows. A
  fix without an accuracy counts as fair.
- Location switched off is read from `/etc/location/location.conf`
  (`enabled` and `gps\enabled`), which the `Location` permission makes
  visible in the sandbox; positioning just stays silent then.
- **Only the filter's estimates are shown or recorded**: the map's position
  and direction, the values, the route, the GPX file. Until the filter has
  settled (five fixes that agree) there is no position on the map at all. The
  accuracy circle is the receiver's latest accuracy around the estimate,
  matching the GPS row.
- **The last position is remembered** (decided 2026-10-18, at the project
  owner's request): the filter's latest estimate, written once as the
  receiver stops - at the stop, or as the page goes - not with every fix.
  A new activity's map starts there (`hasLastPosition`, `lastLatitude`,
  `lastLongitude`, read as the `Activity` is made) and moves to the position
  once the filter has one; a replay ignores it. Kept with `QSettings` in
  `settings.conf` in `AppConfigLocation`, i.e. `~/.config/de.ygriega/chronograph/`,
  as Piepmatz keeps its settings: a single value pair is not worth a table.
  A kill loses it, and the map starts where the activity before ended.
- The direction arrow shows only at 0.5 m/s or more: below that a course is
  noise.
- The altitude is the filter's, from the receiver's. Whether the hybris provider gives
  ellipsoid or sea-level height is unverified; in Europe they differ by about
  50 m. The climb does not depend on it.
- `CHRONOGRAPH_NMEA=<file>` replays an NMEA log in place of the receiver
  (`QNmeaPositionInfoSource`, simulation mode) - see [The
  emulator](#the-emulator). Off unless set; the stale-fix check is skipped then.

### The map: OpenStreetMap tiles in plain QML, not QtLocation

Decided 2026-10-17. QtLocation's `Map` was the obvious choice and was ruled
out: on the phone only its C++ library is installed, not the QML module
(`qt5-qtdeclarative-import-location`); its OSM plugin
(`qt5-plugin-geoservices-osm`) is not on Harbour's allowed requires, which
would make a second Harbour exception; and both are Qt 5.4. So `TileMap.qml`
draws `tile.openstreetmap.org` tiles as plain `Image`s and needs nothing but
QtQuick and the `Internet` permission.

- **Web Mercator throughout**: centre and positions in 0-1 world coordinates,
  `worldSize` pixels at the current (fractional) zoom. Tiles are drawn at
  256 × `Theme.pixelRatio`, so labels stay legible on dense screens.
- `TileLayer.qml`: a fixed grid of slots, each showing the tile at its place
  modulo the grid, so a pan reloads one row or column, not all. **Two layers
  take turns**: at a change of level the old one is frozen underneath until
  the new one has loaded over it, so the map never goes blank while zooming.
- `TileMap.tilesLoaded()` comes a second after the last tile of a batch has
  loaded - they are in the disk cache then; the activity page passes it on
  to `Activity.updateLandCover()`.
- Gestures: drag (with `preventStealing`, so the page's back swipe works only
  above the map), pinch, double-tap. Dragging stops following the position; a
  button in the corner resumes it. A finished activity is shown whole.
- `TrackLine` is a `QQuickPaintedItem` painting the route per segment (no line
  across a pause), in a replay full up to the current point and faint after.
  Painting thousands of points is too slow per frame, so the map syncs its
  view to it at most four times a second and moves and scales it in between.
- **OSM's tile usage policy**: `TileNetworkFactory` gives the QML engine a
  network access manager that sends a User-Agent naming the app and caches
  tiles on disk (`~/.cache/de.ygriega/chronograph/tiles`, 50 MB), as the
  servers' headers allow; the attribution sits on the map and in the about
  page. No prefetching, only visible tiles. Heavy use would need a tile
  provider of our own choosing.
- **Tiles are fetched only while the map is viewed** (fixed 2026-10-23): the
  policy forbids fetching any tile a user is not viewing, and a recording
  moves the map along with the display off. `TileMap.viewed`
  (`Qt.application.active` and visible) gates `TileLayer`: while it is false
  the slots keep their tiles and only move, and the level, the first tile
  and the number of slots change only together, in `placement`, so no slot
  asks for a tile from half of a change. Checked in the emulator: no request
  for a minute of driving with the display off.

### The activity page

- Pushed without `fileName` from the pulley menu (record), with one from the
  list (replay). Type combo box (running is the default, as the first
  entry) until the start, then a `PageHeader` with the type and the start's
  date and time in its place, as in a replay; GPS row, then the rest of the upper half a Silica `SlideshowView`
  (Piepmatz's idiom, over a `VisualItemModel`): the eight values with the
  current land cover in a row below them, and swiped
  to, the charts, then the land cover (`LandCoverList.qml`); a dot for each
  below it, the one shown highlighted. Map in
  the lower half, control at the bottom; in landscape the values or charts
  on the left, the map right.
- The slideshow takes horizontal drags, as the map does, so the page's back
  swipe now works only over the type, the GPS row and the control - and the
  page indicator still goes back.
- The control row: the **Auto-pause `TextSwitch`** on the left, the button on
  the right (`buttonWidthSmall`, to leave the switch room), the small hint
  below both, at the very bottom, centred on the map's width: the whole page
  in portrait, the map's right half in landscape. The hint says "Paused automatically" while auto-paused, else "GPS
  signal lost" (see below), else what the button does. The switch is there
  until the stop, on for every new activity (not remembered, by the project
  owner's decision), and works at any time; `automaticCheck: false`, so its
  binding to `activity.autoPause` survives a tap.
- The control: Start; then `HoldButton` - a tap (under 800 ms) pauses or
  resumes, **held for 3 s it stops**, counting the seconds down on itself
  (shown from 250 ms on, so a tap does not flash it); letting go early or
  sliding off gives up. After the stop a `Slider` through the activity, in
  whole seconds, starting at the end.
- **Opening a stored activity off the UI thread** (decided 2026-10-23, at
  the project owner's request, after a delay opening the page). Measured
  on the phone with an activity of 8,300 points, from the tap until the page
  showed, about 400 ms: compiling `ActivityPage.qml` 100 ms (Qt 5.6 has no
  QML disk cache: the first open of each run), reading the GPX file 110 ms,
  creating the page's items 90 ms, the first frame 95 ms (the charts and the
  route painted in 25 ms of it); the land cover, already known, 30 ms in
  its thread. The first two now happen elsewhere:
  - `OverviewPage` compiles the page **as the app starts**, with
    `Qt.createComponent(..., Component.Asynchronous)` in Qt's loader thread,
    and keeps it; pushing the URL then finds it compiled.
  - `Activity` reads the file in a `TrackReader` thread. It is `Finished`
    from the start, with `loading` true until the track is there: the page
    shows a replay at once, with no Start button, no "nothing was stored"
    and no "Not recorded" in the charts meanwhile, and fits the map to the
    route once `loading` turns false. The list passes the activity's
    `type` along, so the header names it rightly before the file says so.
  The items and the first frame stay on their threads: they are the page.
- **Leaving the page while recording or paused stops and stores the
  activity**: `Activity`'s destructor does it, with its signals blocked, so it
  also happens when the app is closed with the page open. A kill or a crash
  loses the activity in progress: nothing is written before the stop.
- **An activity without a single recorded point is not stored** - there is
  nothing to replay - and the page says so.
- **The type can be changed until the start** (decided 2026-10-18, at the
  project owner's request); from then on the header names it, and a stored
  activity's type is fixed. The combo box and the header share one place,
  always as tall as the taller of them, and cross-fade: nothing below moves
  at the start. The header's description is a blank line before it, so its
  height does not change either; the combo box is faded out and disabled
  but never hidden, as a hidden `ComboBox` drops to no height.
- **Deleting** (decided 2026-10-18): from the list's context menu, or from
  the page's pulley once the activity is stored - in a replay, or after the
  stop. The page's content sits in a `SilicaFlickable` for that pulley alone,
  as tall as the page and not interactive before then, so it takes no drag
  from the control while recording; the map and the slideshow keep their own
  drags, so the pulley is pulled over the type, the GPS row or the slider.
  Both go through Silica's remorse, **10 s** rather than its 4 s
  (`app.deleteRemorseTimeout`), as a recording cannot be made again; tapping
  the countdown keeps the activity. Silica deletes at once when the page is
  left during the countdown - `ListItem.remorseAction()` on the list,
  `Remorse.popupAction()` on the activity page, which then goes back to the
  list. Nothing is deleted before the countdown ends, so a kill during it
  keeps the activity.
- Activity type keys are `running`, `walking`, `cycling`, `rowing` and
  `driving` everywhere (C++, database, GPX `<type>`); `app.activityTypes` and
  `app.activityTypeName()` in `harbour-chronograph.qml` are the only place they
  are translated, `isType()` in `Activity.cpp` the only place they are
  checked, and `filterMode()` beside it the only place they are mapped to a
  `TrackFilter::Mode`. A new type goes into all three, and gets a pictogram,
  `images/activity-<key>.svg`, which `app.activityTypeIcon()` finds by the key. Every type shows the
  same values, pace included, and `Track` applies the same rules to all of
  them; only the filter treats them differently.

### Auto-pause: standing, too fast, and the stop after half an hour

Decided 2026-10-20, at the project owner's request, from a list of proposed
events. **Chosen:** A standing, B steps, C too fast for the type, H the stop,
F′ a warning when the signal is lost. `AutoPause` decides, Qt-free; `Activity`
feeds it the filter's speed with every estimate (also while paused), the
pedometer's steps (`pedometer: stepCounter` from QML) and a check once a
second, and does what it returns.

| | Walking | Running | Cycling | Rowing | Driving |
|---|---|---|---|---|---|
| A pause: standing below, for | 0.4 m/s, 10 s | 1.0 m/s, 5 s | 1.0 m/s, 5 s | 0.5 m/s, 20 s | 1.0 m/s, 3 min |
| A resume: moving above, for | 0.7 m/s, 3 s | 1.5 m/s, 2 s | 1.5 m/s, 2 s | 1.0 m/s, 5 s | 3.0 m/s, 3 s |
| B pause also needs no step for | 5 s | 3 s | - | - | - |
| C pause: too fast above, for | 3.5 m/s, 15 s | 8 m/s, 15 s | 25 m/s, 30 s | 7 m/s, 15 s | - |

- **A, standing**: pauses as of the first slow second; walking or running,
  as of the last step if that came later (B). Resumed by moving (A) or,
  walking or running, by a step (B) - even without a signal.
- **C, too fast** (bus, train, car): pauses as of the first second over the
  limit. Resumed walking or running by a step while the speed is fresh and
  within the limit; otherwise, or without steps, by 10 s within the type's
  range - longer than A's resume, so a bus slowing for a stop does not
  resume. **A step counts only after the last too-fast second**: walking
  along a train, the steps must not resume it at the next station.
- **H, the stop**: auto-paused for 30 min, the activity is stopped and
  stored, as if held to stop. Queued, as the decision may come from the
  receiver's own signal, which the stop deletes.
- **F′, the signal lost**: no position good enough to record for 30 s while
  recording (from the start or a resume by hand on), the hint says so. It
  pauses nothing - the track bridges the gap with a straight line once a fix
  comes, which a pause would not count. Shown with the switch off as well.
- **Dated back** (decision 2): a pause ends the segment with `Track::endAt()`
  at the moment the movement ended, dropping the points since, so neither
  the waiting nor the bus counts; the elapsed time visibly steps back. A
  resume begins the segment at the moment it is decided. `TrackLine` and
  `TrackChart` see the track shrink and take it in anew - once per pause.
- **By hand and on its own** (decision 1): a pause by hand is never resumed
  on its own (`AutoPause` is halted). A resume by hand - button or cover -
  restarts it unarmed: it waits for movement (A's resume or a step) before it
  pauses again, so it does not at once pause what was resumed standing. The
  start restarts it armed: standing after Start pauses as of the start.
- **The switch**: off halts it, and resumes a recording it had paused, as
  nothing else would; on while recording restarts it armed.
- **No signal, no decision**: without a speed for 5 s nothing is decided
  from the speed, and the runs of slow or fast seconds start over.
- **Steps** come from `StepCounter::stepsAdded`, which holds a walk's first
  six steps back until confirmed: a step resumes about 3 s into walking on.
  Without a counting accelerometer (`status` not `Counting`) the speed alone
  decides. For cycling, rowing and driving steps are ignored.
- The GPX file is unchanged: an auto-pause is an ordinary segment break.
- **Known limits**: none of the limits is tuned on real activities. The
  walking filter follows a bus's acceleration slowly, so C may come some
  seconds late and part of the ride count. A slow bus with a spurious step
  resumes a walk (D below would catch it).
- **Proposed and not chosen**, kept for reference: D, moving without steps
  (walking or running: over 2.5 m/s with no step for 10 s is a vehicle);
  E, climbing faster than legs can (over 0.8 m/s upwards for 30 s: a lift or
  cable car; needs the phone's altitude checked first); G, the phone put down
  (the accelerometer quiet for 30 s; in `StepCounter`, beside the detector,
  not in it). F, pausing at a lost signal, in favour of F′'s warning.
  **Terrain from OpenStreetMap was ruled out** for auto-pause: the raster
  tiles hold no tags, only colours - enough for the land cover afterwards
  (see [Land cover](#land-cover-read-off-the-maps-tiles)), not for railways
  or ferries, and only where the map was shown; Overpass queries (railway, aerialway, ferry, water - leaving the
  water would have paused a rowing boat being carried) would send the
  position to another server, and vector tiles would need a decoder.

### The charts: painted in C++, like the route

Decided 2026-10-18, at the project owner's request: the pace and the
elevation over the activity's time, one above the other, swiped to from the
values. QtCharts is not on Harbour's allowed lists (none of the three
mentions it), and a QML `Canvas` would walk thousands of points in JavaScript
on every repaint. So the numbers come from Qt-free `TrackChart` and a
`QQuickPaintedItem`, `ChartLine`, draws them; `ActivityChart.qml` adds the
caption and the axis labels in Silica's fonts, placed by `ChartLine`'s
`topValue`, `bottomValue`, `valueTicks`, `length` and `timeTicks`.

- **The same values as the page**: `Track`'s smoothed speed per point as pace
  (none below 0.5556 m/s, i.e. slower than 30:00/km, as `Units.js`), its
  smoothed altitude, against the elapsed time with the pauses left out. The
  line breaks where there is no value and at each pause.
- **Pace upside down** - faster is higher, as sports sites draw it - with a
  dotted line at the whole activity's average pace (distance over duration,
  i.e. what the average shows at the end of a replay; while recording, as of
  the last point).
- **Pace axis from the 5th to the 95th percentile**, plus the average: setting
  off and stopping pass through every pace there is, and min to max would
  squash the rest flat. The extremes run off the edge. At least a minute per
  km tall, never below 0:00.
- **Elevation axis from min to max**, at least 20 m tall, so a receiver's
  wander on the flat does not look like hills.
- **Ticks on whole steps**: at most 3 intervals up (pace in 10 s to 15 min,
  elevation 1-2-5), at most 4 across (15 s to a day); the time axis ends at
  the activity's end, over a minute at least, not on a step. Only the lower
  chart labels the time; both share it.
- **Replay**: full up to the current point, faint after, with a dot at it -
  as the map does.
- **Cost**: each new point is taken over once; the axes are refitted at each
  (a copy and two `nth_element`s - linear), and QML is told only when they
  changed, so the label repeaters are not rebuilt every second. Painting
  draws per column of pixels only the first, highest, lowest and last point,
  so a long activity costs no more than a column's worth each.

### Land cover: read off the map's tiles

Decided 2026-10-23, at the project owner's request, after weighing three
ways: how far an activity went over each kind of land, in km and %, as a
third slide after the charts. Land cover alone was asked for; not the way
type or its surface.

- **From the standard tiles' colours**, which OSM Carto paints each kind of
  land in. Overpass would have had the real tags but sends the route to
  another server; vector tiles would need a decoder and a provider. Raster
  colours need neither, and the map loads the tiles anyway.
- **Only tiles the map has shown**: `TileCache` reads the QML engine's disk
  cache (a `QNetworkDiskCache` of its own on the same directory, read only)
  and never fetches - OSM's tile usage policy forbids fetching a tile nobody
  is viewing, so nothing is fetched for the land cover, ever. Cached tiles
  from earlier activities count too, display off or not.
- **Seven coarse kinds** (`LandCover::Kind`, keys in the GPX file):
  `forest`, `farmland` (also farmyards, orchards, vineyards), `grassland`
  (grass, meadows, parks, pitches, cemeteries, allotments), `builtup`
  (residential, commercial, industrial, buildings, car parks...), `water`
  (lakes, rivers, the sea), `natural` (heath, scrub, bare rock, sand, beach,
  glacier), `unmapped` (Carto's land background); and the land not known.
  Carto's finer classes lie a few shades apart and were merged rather than
  told apart unreliably. `app.landCoverName()` and `app.landCoverColor()` are
  the only places they are named and coloured.
- **The colour table** is Carto's `landcover.mss`, matched per channel within
  2 (the closest two kinds lie 5 apart, so no pixel matches two). Roads,
  labels, symbols, the forest's tree pattern and edges match nothing and are
  left out. A Carto colour that changes makes its kind silent - counted as
  unknown, never as another kind.
- **The vote**: every pixel within 20 m of the point (at least 2 pixels,
  sampled at most 12 on either side), across up to four tiles; the kind with
  the most pixels, unless under 20 % of the circle matched any kind. The
  point's own tile must be at hand; its neighbours count if they are.
- **The finest level that tells**, from 19 down to 10; a finer one replaces a
  coarser later. Levels whose tile was at hand and told nothing are
  remembered per point (`untelling`) and not sampled again.
- **When**: each point as it is recorded, then every time the map says it
  has loaded tiles (`updateLandCover()`), at the stop and when a stored
  activity is opened. After the stop the map shows the whole activity, so
  the stretches recorded with the display off get the level of that view
  (about 12-13 for a few km).
- **In a thread of its own** (decided 2026-10-23, at the project owner's
  request, after opening an activity without its land cover stalled the
  page): `LandCoverReader`, a `QThread` as Piepmatz and Fernschreiber run
  theirs, reads one batch of copied points at a time and owns the
  `TileCache`, which nothing else touches. Requests coalesce while a batch
  runs - the points recorded since, or all of them - and go as the next
  batch once it is done. A result goes into its point only if the track
  still holds that point (same index and time): an auto-pause may have
  dropped it meanwhile. The values show the land cover once a batch has
  found it. A batch running as the page goes is given up, not waited for:
  the next replay reads it again. On the emulator, 221 points with their
  tiles cached took 130 ms - seconds for a long activity on a phone.
- **Per point, in the GPX file**: `<chronograph:landCover level="16">forest</chronograph:landCover>`,
  none where unknown. A stored activity's file is **rewritten** 3 s after the
  last change, or as the page goes, through `ActivityHistory::rewrite()`,
  which refuses an activity no longer in the table, so a deleted one is not
  brought back. Files from before get theirs on their first replay, and are
  rewritten then.
- **The current land cover** (decided 2026-10-23, at the project owner's
  request): one row below the values on their slide - the kind's colour
  and its name in `fontSizeMedium`, with no caption (removed at the
  project owner's request), so it takes a single line.
  `Activity.currentLandCover`, the key at the point the values are from:
  the latest while recording, the slider's in a replay; "Unknown" where the
  point has none, a dash before the first.
  The slide grows the values' half to fit should it not, shrinking the map;
  on the emulator's screen it fits in both orientations as it is.
- **The distance per kind**: each step of `Track`'s distance counts for the
  point that ends it, so the kinds add up to the distance; up to the replay
  position, like the other values.
- **Known limits**: not tried on real activities. Farmland and the land
  background lie 20 apart only; OSM's sea areas with a boundary drawn over
  them have a dotted grey-blue that matches nothing; a bridge counts as the
  road on it, a tunnel as the land above. Stretches whose tiles were never
  shown stay unknown.

### The statistics: worked out as activities change, read when shown

Decided 2026-10-24, at the project owner's request: a page of its own, from
the main page's pulley, with the distance, the time and the land cover (km
and %) of all activities together, and below them the same for each type
that has any, in the order the activity page offers them. **Worked out
whenever an activity is stored and kept in the database**, so that the page
only reads them.

- **Worked out whole, not added to**: `ActivityStatistics::update()` empties
  `statistics` and `statistics_land_cover` and fills them from `activities`
  and `activity_land_cover` with a few `GROUP BY`s, in one transaction. Never
  drifting from what is stored was worth more than the cost: 2,000
  activities took 8 ms in the build engine, so it stays on the UI thread,
  once at each stop. Measure it on a phone should anyone record thousands.
- **When**: `ActivityHistory::activitiesChanged()`, connected in `main()` -
  an activity stored, deleted, or rewritten as its land cover is learnt
  after the stop or at a replay. The last two change the totals as well, so
  they count as "persisted" too. The page reads them as it opens and again
  on `updated()`, should that come while it is open.
- **Each activity's land cover in the database**, `activity_land_cover`,
  which `ActivityHistory` writes with the row and at every rewrite: deleting
  an activity or learning its land cover must change the totals without
  reading a single GPX file. The land not known always has its row, if only
  with 0 - any row says the activity has been summed up.
- **Activities from before** (0.3 files on phones) have no rows. As the
  history is made it reads those files in a `TrackSummaryReader`
  thread, writes the rows of the ones still stored and still without, and
  says the activities changed; a file that cannot be read counts as land not
  known, so it is not read at every start. The same thread reads the climb
  of every activity whose row has none (see [The
  achievements](#the-achievements-seven-levels-kept-for-good)). Until then the statistics count
  their whole distance as not known, so the kinds always add up.
- **The land not known** is the activities' rows with an empty key plus the
  distance of any still waiting; the page lists it last, like an activity's
  land cover slide (`LandCoverList`, shared). Shares are of the land cover's
  sum.
- The time is the duration with the pauses left out, as in the list, through
  `Units.duration()` - hours go past 24.
- **Each section** (`ActivityStatisticsSection.qml`): a `SectionHeader`, the
  distance and the time side by side as `ActivityValue`s, **each centred in
  its half** (decided 2026-10-25, at the project owner's request, after left
  and right alignment both looked off; `ActivityValue.horizontalAlignment`,
  left on the activity page), and the land cover below them.
- Not shown, though stored: the number of activities (it decides which types
  get a section).

### The achievements: seven levels, kept for good

Decided 2026-10-26, at the project owner's request: achievements for
milestones, in categories with stages like medals or wedding anniversaries,
starting very easy and ending very hard; worked out whenever an activity is
stored and kept in the database, **never taken away**, even when activities
are deleted; a page of their own from the main page's pulley, each category
with a pictogram, greyed out until reached, else tinted in its level's hue
with the level named below; a tap shows what it measures, what has been
achieved and what the next level takes. Extensible: more categories will
come. The categories themselves were left to Claude.

- **Seven levels**, by the order of a wedding's anniversaries and a
  podium's medals: Paper, Wood, Bronze, Silver, Gold, Platinum, Diamond.
  Every category has all seven, so the page, the table and the hues are the
  same for all. `app.achievementLevelName()` and `app.achievementLevelColor()`
  name and colour them, a light hue on a dark ambience, a darker one on a
  light ambience.
- **26 categories in 8 groups**, in `AchievementCatalogue.cpp`'s
  `kCategories`, thresholds and all: per type that a body moves - running,
  walking, cycling, rowing - the longest single activity, the distance in
  all, and the fastest average (pace for running and walking, speed for
  cycling and rowing) over an activity of at least 5, 3, 10 and 2 km;
  the biggest climb in one activity and the climb in all; the distance in
  all over forest, meadows and parks, farmland, heath and rock, water and
  built-up land; the longest activity and the time in all; the number of
  activities and the most calendar weeks in a row (Monday to Sunday, ISO)
  with one; the most steps in a day and the steps in all. Paper comes with
  the first try, or nearly; Diamond is a 100 km run, 3:20 per km over 5 km,
  40,000 km cycled, 100 km of climb, two years of weeks in a row, 10
  million steps. The single distances stop at 21 and 42 km rather than a
  marathon's 21.1 and 42.2, as a receiver's distance may fall a little
  short of a course. None of it is tuned on real activities.
- **Driving counts for nothing**: no category of its own, and left out of
  every one that takes all types. Rowing's land is mostly water; a run over
  a bridge counts as the road on it, as for the land cover.
- **Worked out whole, as the statistics are**: `Achievements::update()`
  reads every activity (type, local day of its start, duration, distance,
  climb), their land cover and every day's steps, and `AchievementCatalogue::
  evaluate()` goes through them oldest first - so each level also gets the
  **day it was reached**, that of the activity or the day that took it
  there, even for history from before the achievements.
- **Never taken away**: `achievements` keeps per category the level, the
  day it was reached, the best value ever and the value now.
  `AchievementCatalogue::merge()` keeps the higher of the level stored and
  the level the activities come to, and the higher best value; a level
  that rises is dated anew. So deleting the activity that reached a level
  keeps the level and its value, while the value now drops - and the next
  level needs the value now to reach it. Only rows that changed are
  written, in one transaction.
- **When**: `ActivityHistory::activitiesChanged()` (stored, deleted, or its
  land cover or climb learnt), `StepHistory::saved()` (every save of
  today's steps - 30 s after a walk, every 5 minutes during one), and once,
  queued, as the app starts - so a category added in a later version is
  worked out over everything stored before, and so are the steps of the
  last run's final save. The page reads them as it opens and again on
  `updated()`.
- **Cost**: 2,000 activities, four land cover rows each, and 1,100 days of
  steps took 10 ms in the build engine and **13-14 ms on the Jolla Phone
  (2026)** (measured 2026-10-26 with `tst_activityhistory` built for
  aarch64); the levels themselves take under a millisecond, the rest is
  reading. So it stays on the UI thread, like the statistics. Reading was
  twice that until SQLite worked out the local days (`julianday(start,
  'localtime')`) instead of `QDateTime`, and the queries were made
  forward-only.
- **The climb of each activity** went into `activities` for this (`climb`,
  schema 7), written with the row and at every rewrite; NULL until known.
  Activities from before have it read off their files, in the same thread
  and at the same first start as their land cover; a file that cannot be
  read climbed 0. Until then they count as 0. The receiver's wander adds
  a little climb to every activity (see [Smoothing the
  fixes](#smoothing-the-fixes-a-kalman-filter-per-activity-type)), so the
  climb's lowest levels come a little early.
- **The page** (`AchievementsPage.qml`): a `PageHeader` with how many
  categories have a level, then per group a `SectionHeader` and a grid,
  three across in portrait, as many of that width as fit in landscape.
  Each tile: the medal (`AchievementMedal.qml` - a round medal in the
  level's hue, the pictogram on it, a badge at its lower right for a total
  (sum sign) or the fastest (flash)), the name on two lines, the level or
  "Not yet". Greyed out without a level.
- **The overlay** (`AchievementOverlay.qml`), Fernschreiber's idiom - the
  page dimmed and the content over it, a tap anywhere closes it (with
  `confirmable`, only its OK button does, for the notice below): the medal
  large, the name, the level, what it measures, then the value achieved and
  the day it was reached, the value so far (where there is no level yet, or
  deletions have taken the value now below the best), and the next level
  with what it needs - or that this is the highest.
- **Pictograms**: `images/achievement-<name>.svg`, 64 units, white on
  transparent like the activity types', drawn for this; the per-type
  categories reuse the activity type's. `Icon` tints them with the level's
  hue. The badges, `achievement-badge-total.svg` and `-fastest.svg`, are
  tinted black on a dark ambience and white on a light one, so they stand
  out of the hue.
- **Adding a category**: an entry in `kCategories` (a new kind of measure
  also a `Metric`, worked out in `evaluate()`), its name and description in
  `app.achievementName()` and `app.achievementDescription()`, a pictogram
  if none fits, and a German translation. Keys are stored: never rename
  one, and a category taken out leaves a harmless row. The next start
  works it out over everything stored.
- **The notice after an activity** (decided 2026-10-27, at the project
  owner's request): the levels reached are shown on the activity page
  after the stop, in the same overlay, one at a time, each **confirmed with
  an OK button** - a tap elsewhere does nothing, unlike on the achievements
  page. `Achievements::awarded()` names the categories whose level an
  update raised, as `read()` has them. It comes within
  `ActivityHistory::add()`, so before the activity is `Finished`: the page
  queues what it hears from the moment it records (a replay listens to
  nothing) and shows the queue once it is finished, with "New achievement"
  over it and "1 of 3" where there are more. Also queued: levels reached
  while recording (the steps) and after the stop while the page is open
  (the land cover learnt, then rewritten). A category reached again before
  its turn keeps its place with the higher level; a jump of several levels
  shows only the last. The page behind is faded out, as the map would
  read through.
- **Not shown**: levels reached with no recording page open - steps
  saved, the climb of older activities read at the first start - and those
  of an activity stored by leaving its page (the page is gone). They are
  on the achievements page as ever.
- **Not done**, as not asked for: achievements for driving.

### Smoothing the fixes: a Kalman filter per activity type

Decided 2026-10-17. A phone's fixes jump by tens of metres now and then, its
speeds spike - worst at a walking pace - and its altitude spikes too; recorded
as they came, they made the route jagged, the distance far too long and the
pace and climb jumpy. So `TrackFilter` stands between the receiver and
everything else, and **nothing from the receiver is shown or recorded without
going through it** - except the accuracies, which describe the signal rather
than the movement and stay the receiver's.

- **A Kalman filter with a constant-velocity model**, in metres east and north
  of an origin that follows the estimate (moved once 1 km away), so a flat
  projection is exact enough. Each fix weighs by its own horizontal accuracy
  (taken as the error per axis, never under 3 m; 15 m without one) against
  what the earlier fixes predict. The receiver's speed and course, where it
  gives both, go in as a measured velocity, good to 0.3 m/s or 5 % of the
  accuracy per second. The altitude has a filter of its own, with the
  vertical accuracy (1.5 × the horizontal without one), fed only from fixes
  whose position was taken.
- **The activity type sets how the movement may change**, as the spectral
  density of the acceleration: **along the direction of travel and across it
  apart**. Speed changes slowly, but a corner turns the velocity round within
  a second, so a sudden change of speed is an outlier long before a sudden
  change of direction is. Below 0.5 m/s there is no direction, and only the
  slower rule holds. Walking 0.1 along, 1 across; running 0.3 / 4; cycling
  0.5 / 4 (bikes brake hard); rowing 0.3 / 1, with next to no climb; driving
  4 / 10. Each is about the least that still follows setting off and stopping
  within a second or two of the truth. The type is set before the start
  and fixed from then on.
- **Outliers**: a position or velocity more than 5 sigma off what the filter
  expects is left out (a velocity alone, keeping its fix's position); one
  beyond 2 sigma counts as if its noise were larger by the cube of how much
  further off it is - a linear inflation let spikes through at half strength.
  A fix whose position is left out is not recorded at all; the track bridges
  the gap with a straight line.
- **Recovery**: the filter gives no estimate until five fixes in a row agree,
  starting over at any that does not, so a run of bad fixes at the start
  cannot become the start. Later, the fixes it does not clearly agree with
  feed a second guess; after 10 s with no fix within 2 sigma of the
  estimate, the second guess takes over if five fixes agree with it. The
  altitude starts over from the receiver after 10 s of disagreement.
- **Stored as filtered**: the GPX file holds the estimates, and a replay reads
  them back without filtering again, so it shows what was shown live. The raw
  fixes are not kept: an activity cannot be filtered anew later. Files from
  0.3 hold raw fixes and replay as they are.
- Poor fixes (over 40 m) still go nowhere, as before: under a sky that bad the
  filter would only drift with them. The limits were 10 m and 30 m until the
  filter came (raised 2026-10-17 at the project owner's decision): it weighs
  each fix by its accuracy anyway, and the tighter limits made it likelier
  never to settle - and so to show and record nothing - under a fair sky.
- **What it cannot do**: the receiver's slow wander - metres over a minute -
  looks exactly like real movement or a real hill, and passes. On the
  generated tracks a flat walk of 20 minutes still climbs 15-25 m through
  wander alone, against 75-100 m raw; spikes alone it reduces to 0. Only
  `Track`'s 3 m climb hysteresis keeps it out of the climb (5 m would halve
  it, at the cost of small real hills - not changed, as it applies to every
  file). The distance is not hurt by it: `Track`'s 5 m rule and the filtered
  speed keep standing still at 0 m.
- **Tuned on generated tracks only** (`tests/tst_trackfilter.cpp`), with a
  receiver that wanders by 3 m over 30 s, jumps 20-80 m for 1-3 fixes 1 % of
  the time, spikes its speed by 1-4 m/s and its altitude by 10-40 m 2 % of the
  time. Against that, over 20 seeds, the worst distance error of a walk falls
  from 124 % raw to under 2 %, of a run from 60 % to under 2 %; positions stay
  within about 10 m, against 80 m raw; the largest speed error of a walk falls
  from about 4 m/s to 0.6. None of it has been checked against a phone's real
  fixes yet.

### Reading the accelerometer: QtSensors in C++

`QAccelerometer` from QtSensors (`QT += sensors`), backed by sensorfw.
`libQt5Sensors.so.5` is on Harbour's allowed library list. The QML
`QtSensors` import would have worked too, but the samples would then cross into
JavaScript 30 times a second for no gain.

- **`setAlwaysOn(true)`** is sensorfw's standby override. Without it sensorfw
  stops delivering the moment the display goes off - which, while walking, is
  nearly always.
- **30 Hz** is asked for, or the closest rate the sensor offers
  (`chooseDataRate()`). The detector counts the same at 20, 30 and 100 Hz in its
  tests; 30 leaves headroom over a running cadence while waking the phone a third
  as often as 100 would. **The Jolla Phone (2026) delivers 49.8 Hz regardless**;
  why is open (`StepCounter` logs the rates the sensor offers, but the journal is
  not readable as `defaultuser`, so read it as root).
- Readings are timed by **sensorfw's own timestamp** (microseconds), which keeps
  batched readings apart. Arrival time is the fallback for a backend that leaves
  it at zero.
- `Unavailable` is reached when there is no backend, `start()` fails, or a
  sensor error leaves the sensor inactive. The keepalive is released then too.

### Counting with the display off: libkeepalive

Approved 2026-10-13: counting continues while the display is off. The sensor
staying on is no use if the CPU suspends, so `StepCounter` runs a
`BackgroundActivity` (libkeepalive, `PKGCONFIG += keepalive`) for its whole
lifetime. `libkeepalive.so.1` is Harbour-allowed, and it talks to MCE, which
Sailjail's `Base.permission` already grants. The C++ class rather than the
`Nemo.KeepAlive` QML element, so that the sensor and the keepalive share one
owner and one lifetime.

**This is what costs battery**, for as long as the app runs, and the about page
says so. A later iteration that wants all-day tracking should look at batching
(sensorfw buffering) or a hardware step counter before reaching for a
long-lived keepalive in a background service.

### The step detection algorithm

Documented in `src/StepDetector.h` and, constant by constant with the evidence
behind it, in `src/StepDetector.cpp`. In short: the magnitude of the
acceleration (orientation-independent), gravity removed with a 1 s moving
average, a 40 ms low-pass against jitter, peak detection with hysteresis (rise
above the threshold, fall back below zero), then:

- **The threshold follows the movement, not the peaks**: 0.85 × the RMS of the
  signal over about the last second, never below 0.5 m/s². 0.1 used the walk's
  own typical peak (0.4 × it, at least 1.0), which could not serve a soft walk
  and a pocket at once - see [What the recordings showed](#what-the-recordings-showed).
- Steps closer than 0.2 s are the same stride ringing on.
- A walk ends after 2 s without a step while it is still being confirmed, after
  **2.6 s once confirmed** - checked on every sample, not only at the next peak.
  The longer limit lets a slow walk survive a step too soft to see; applying it
  before confirmation as well let random jolts chain into walks.
- **Run confirmation:** a walk counts only after 6 regular steps, each within
  1.5× of the run's mean interval. They are held back and counted at once.
  Checking against the mean rather than the previous interval stops a chain of
  jolts that slows a little each time from passing as a beat. This is also what
  rejected the rhythmic movements in the recorded experiments; keep it whatever
  else changes.
- **Once confirmed, the walk's rhythm** (median of its last 8 intervals) corrects
  single peaks:
  - an **echo** - a peak within half the usual interval *and* smaller than 0.6×
    the step before - is ignored. Both conditions: a walk that speeds into a run
    also brings early peaks, but strong ones;
  - a **gap** of 1.6-2.4× the usual interval is a missed step, added
    **retroactively** once the next interval is back within 0.7-1.4×. A walk
    that is slowing down gives long intervals too, but keeps giving them.
- **The signal's own beat** (added 2026-10-14) catches a leg too soft to give
  a peak: then the peaks come once per stride, but the signal still repeats once
  per step. The last 4 s are kept on a 20 ms grid and autocorrelated at each
  peak, over lags of 0.24-2 s. The beat is the shortest lag repeating at least
  0.9× as well as the best (two strides often beat one by a hair), halved where
  a peak at about half that lag reaches 0.1 and 0.25× of it - the step inside a
  stride. Below a correlation of 0.3 there is no beat and the rules above apply.
  - **while walking**, an interval of ≥ 1.7 beats is two steps, ≥ 2.7 three;
    this replaces the gap rule wherever the beat is known. 1.7 rather than 1.5
    because the beat lags a walk that is slowing down;
  - **while confirming**, one interval of 1.5-2.5 beats counts as two pending
    steps instead of breaking the run - **only if the run's mean step is within
    30 % of the beat**. Without that condition the recorded rhythmic handling
    passed as a 9-step walk and random jolts tripled (1.2 → 3.6 per 60); with it
    they are 0 and 2.0. The beat must never start a walk on its own: handling
    the phone repeats as strongly as walking does (up to 0.89).

**History.** 0.1 shipped with the adaptive-peak threshold and no rhythm rules.
On the phone it was right in total for normal walks only through double counts
and misses cancelling out, and lost more than half of a slow walk. The rhythm
rules were prototyped in Python against the recordings, scored on all of them
and the synthetic tests at once, and ported line by line - the C++ replay
reproduces the prototype's counts exactly. The beat was tried in a scratch copy
of `StepDetector` with its parameters read from the environment, scored the same
way, then written into `src/`. Tuned on six walks and a second session of
stairs and running, all of one person on one phone, then checked on a third,
fresh session it was not tuned on: 201 for about 224 (-10 %).

**Decided 2026-10-14: the algorithm is final.** The project owner considers it
good enough and wants no further refinements; its known flaws are accepted
(listed under [Accepted flaws](#accepted-flaws)). Do not tune it further unless
the project owner reopens the question. A bug found on a device - a crash, or
counting that breaks outright - is still fixed, with a test case for it.

### Persistence: SQLite, one row per day

Decided 2026-10-14 for iteration 2. **QtSql with its SQLite driver**
(`QT += sql`): `libQt5Sql.so.5` is Harbour-allowed, and Fernschreiber and
Piepmatz use it the same way. The driver plugin (`libqsqlite.so`) ships with
the OS; like the sensorfw plugin it is not `Require`d, as Harbour would refuse it.

- **Where:** `chronograph.db` in `QStandardPaths::AppDataLocation`, i.e.
  `~/.local/share/de.ygriega/chronograph/` - the one place Sailjail lets the
  app write, because the organization and application names (`de.ygriega` /
  `chronograph`) are the same in `main.cpp` and in `[X-Sailjail]`. Keep them in
  step. The recordings share the directory; their clean-up only touches
  `recording-*.csv`.
- **`Database`** (`src/Database.*`) owns the connection and the schema, nothing
  else. **The schema is a list of statements, one per version**, and
  `PRAGMA user_version` says how many a file has had; opening a file applies
  the rest in one transaction. A new feature's table is a new statement
  **appended** to `kSchema` in `Database.cpp` - never edit, reorder or remove
  one, as files on phones are already past it. A file from a newer app version
  is opened as it is. If the file cannot be opened or migrated, it stays closed
  and the app counts in memory only.
- **The classes that use a table own its queries**: `daily_steps` belongs to
  `StepHistory`, `activities` and `activity_land_cover` to `ActivityHistory`,
  `statistics` and `statistics_land_cover` to `ActivityStatistics` - which
  reads `ActivityHistory`'s two to fill them - and `achievements` to
  `Achievements`, which reads those two and `daily_steps`. Nothing else
  reads another's tables. Further features get their own class and their
  own table.
- **`daily_steps`**: `day TEXT PRIMARY KEY` (the local date, `yyyy-MM-dd`, so
  text order is date order) and `steps INTEGER`, the whole day's count. Rows are
  written with `INSERT OR REPLACE`, which is fine while the count is the only
  value; a table that gains columns needs `UPDATE` instead.
- **Steps count for the day they are counted on**, local time. On start
  `StepHistory` reads today's row and carries on from it. The day changes at a
  precise single-shot timer just after midnight (it re-arms itself), and every
  step checks the date as well, which also catches the clock being changed. A
  walk confirmed just after midnight may take a few steps from before it into
  the new day.
- **Writes are batched**, not done per step (twice a second while walking): a
  save comes **30 s after the last step** - the end of a walk or a stop on the
  way - and during a long walk **every 5 minutes at the latest**; also at the
  change of day and when `StepHistory` is destroyed, i.e. when the user closes
  the app. Standing still writes nothing. A crash, a kill or a shutdown loses at
  most the last five minutes: SIGTERM is not handled.
- **The previous days** are the three before today, yesterday first, 0 where
  there is no row. Re-read from the database at start and at each change of day
  only; nothing else changes them.
- **`activities`** (schema 2, its index 3): `id`, `type` (the key), `start`
  (UTC, `yyyy-MM-ddTHH:mm:ssZ`, so text order is time order), `duration`
  (seconds, pauses left out), `distance` (metres) and `file`, the GPX file's
  name. Only what the list shows; everything else is in the file.
- **`activity_land_cover`** (schema 4): `activity` (the id), `kind`
  (`LandCover`'s key, empty for the land not known) and `distance` (metres),
  one row per kind the activity crossed - see [The
  statistics](#the-statistics-worked-out-as-activities-change-read-when-shown).
  Deleted with the activity's row, in one transaction.
- **`statistics`** (schema 5): `type` (the key, empty for all activities
  together), `activities`, `duration` and `distance`; **`statistics_land_cover`**
  (schema 6): `type`, `kind` and `distance`. Derived, emptied and refilled
  at every change.
- **`activities.climb`** (schema 7): metres climbed, as the page's
  elevation gain at the end; NULL until known. The first column added to a
  table rather than a table added: `ALTER TABLE ... ADD COLUMN`, which
  SQLite does in place.
- **`achievements`** (schema 8): `category` (the key in
  `AchievementCatalogue`), `level` (0 to 7), `reached` (the local date,
  `yyyy-MM-dd`, empty at 0), `best` and `current`, in the category's unit.
  Written with `INSERT OR REPLACE`, never deleted.
- **The GPX files** live in `gpx/` beside the database, named after the start
  in UTC in ISO 8601's basic format, `20261016T073000Z.gpx`: sorts by time,
  means the same in every time zone, and has no colons, so it survives being
  copied to any file system. A second activity in the same second gets `-2`.
  Written whole with `QSaveFile` at the stop, with the filter's estimates
  as its points, and again as the land cover is learnt (see [Land
  cover](#land-cover-read-off-the-maps-tiles)). GPX 1.1, one `trkseg` per stretch between pauses; the
  speed and course go into Garmin's `TrackPointExtension` v2, which sport
  sites read, and each point's horizontal and vertical accuracy (the
  receiver's) and each segment's begin and end into Chronograph's own
  namespace - the
  segment times are what lets a replay leave the pauses out exactly. Files
  from other apps read too (GPX 1.0's own `speed`, no extensions).
- **Deleting an activity** removes its row first, then its file, then its
  place in the list, by the file's name: a file left behind is harmless,
  a row whose file is gone would list an activity that cannot be replayed. If
  the row cannot be deleted, nothing is. A name freed this way may be taken
  again by a later activity started in the same second.
- **The list is read 100 at a time**, newest first, by `(start, id)`: the first
  page with the constructor, the next through `fetchMore()`, which the list
  view calls as it nears the end of what it has. Paged by key rather than by
  offset, so an activity added at the top meanwhile shifts nothing. A new
  activity goes into the model at its place, normally the top.

## Layout

```
harbour-chronograph.pro       CONFIG += sailfishapp, QT += sensors, PKGCONFIG += keepalive
harbour-chronograph.desktop   Sailjail: Permissions=Location;Internet (Location brings Sensors)
src/main.cpp                  creates Database, the histories, StepCounter, then the view
src/Database.*                the SQLite connection and the schema's migrations
src/StepHistory.*             steps per day: today's count, saving, the previous days
src/StepCounter.*             QAccelerometer + keepalive, the QML facade
src/StepDetector.*            the algorithm, Qt-free
src/TrackFilter.*             the Kalman filter the fixes go through first, Qt-free
src/Track.*                   an activity's segments and points and their values, Qt-free
src/AutoPause.*               when a recording pauses, resumes and stops on its own, Qt-free
src/Activity.*                positioning, recording and replay: the activity page's facade
src/ActivityHistory.*         the activities table as a paged list model, and the GPX files
src/ActivityStatistics.*      the statistics page's totals, worked out and kept
src/TrackSummaryReader.*      the land cover and climb of older activities off their files,
                              in a thread
src/AchievementCatalogue.*    the achievements' categories, levels and progress, Qt-free
src/Achievements.*            the achievements' levels, worked out and kept for good
src/Gpx.*                     Track to and from GPX 1.1
src/TrackReader.*             reads a stored activity's GPX file, in a thread of its own
src/TrackLine.*               the route, painted over the map
src/TrackChart.*              a Track's pace or elevation per point and the axes, Qt-free
src/ChartLine.*               one chart's grid, line and average, painted
src/LandCover.*               the land cover around a place from a tile's colours, Qt-free
src/TileCache.*               the map's cached tiles, read for LandCover, never fetched
src/LandCoverReader.*         reads the points' land cover off them, in a thread of its own
src/TileNetworkFactory.*      the QML engine's network access: User-Agent and tile cache
tests/tst_stepdetector.cpp    host-side checks for StepDetector (not packaged)
tests/tst_track.cpp           host-side checks for Track
tests/tst_trackfilter.cpp     host-side checks for TrackFilter, raw against filtered
tests/tst_trackchart.cpp      host-side checks for TrackChart
tests/tst_autopause.cpp       host-side checks for AutoPause
tests/tst_landcover.cpp       host-side checks for LandCover, on generated tiles
tests/tst_achievements.cpp    host-side checks for AchievementCatalogue
tests/tst_stephistory.cpp     Database and StepHistory, run in the build engine
tests/tst_activityhistory.cpp the migration, Gpx, ActivityHistory, ActivityStatistics and
                              Achievements, in the build engine
tests/replay_recording.cpp    replays a phone recording through StepDetector
tests/raw-data/               personal data, git-ignored, local only: the recordings,
                              their ground truth (README), parts, FINDINGS.md
qml/harbour-chronograph.qml   ApplicationWindow; the activity types, the land covers and the
                              achievements, named
qml/pages/OverviewPage        the activities above, the steps in the lower third; pulley menu
qml/pages/ActivityPage        record or replay one activity: values, charts or land cover, map,
                              control;
                              pulley to delete a stored one
qml/pages/StatisticsPage      distance, time and land cover of all activities, then per type
qml/pages/AchievementsPage    every achievement by group, its level; a tap shows the details
qml/pages/AboutPage           logo, version, what to know, credits
qml/cover/CoverPage           today's count over the chronograph backdrop, or the activity
                              being recorded, with an action to pause or resume it
qml/components/               BackgroundImage, AboutLink, AboutParagraph (from Lauscher);
                              TileMap, TileLayer, ActivityValue, ActivityChart, HoldButton,
                              LandCoverList, ActivityStatisticsSection, AchievementMedal,
                              AchievementOverlay
qml/js/Units.js               durations, distances, paces and speeds as shown
images/                       icon master, cover backdrops, heading arrow, activity type and
                              achievement pictograms (SVG), installed whole
icons/<size>/                 the rendered launcher icon
translations/                 German catalogue; the template is regenerated, untracked
rpm/                          spec and changelog
```

## Building

Out of tree, as Qt Creator and the SDK do it - one build directory per target,
beside the checkout, never inside it. The build engine only shares the home
directory, so never build from `/tmp`.

```sh
mkdir -p ../build-harbour-chronograph-4.6.0.13-aarch64
cd ../build-harbour-chronograph-4.6.0.13-aarch64
~/SailfishOS/bin/sfdk -c target=SailfishOS-4.6.0.13-aarch64 build ../harbour-chronograph
~/SailfishOS/bin/sfdk -c target=SailfishOS-4.6.0.13-aarch64 check RPMS/*.rpm
```

- Installed targets: 4.6.0.13 and 5.1.0.11, each for aarch64, armv7hl and i486.
  Iterations 1 and 2 were built for 4.6 aarch64 and armv7hl and for 5.1 aarch64
  and i486, with no compiler warnings under `-Wall`; iteration 2 for 4.6 i486
  as well, which the persistence tests need. Iteration 3 for all five of
  those, without warnings.
- `sfdk check` must show no Harbour error (one, the Sensors permission, up to
  0.2; see [Harbour exceptions](#harbour-exceptions)). It also warns that the binary is not stripped, which is how the SDK builds it,
  not something the project controls.
- The spec's `Version` is used as long as this is not a git repository. Once it
  is, sfdk derives the version from the latest tag (see Lauscher's CLAUDE.md):
  tag releases, or pass `-c no-fix-version`.
- `rpm/harbour-chronograph.changes` is the RPM changelog, newest first; sfdk
  appends it to the spec at build time.
- **Bump the version in three places**: the spec, the changelog, and
  `AboutPage.qml`'s label.
- `sailfishapp_i18n` runs lupdate on `translations/` during `%install`. That
  rewrites `harbour-chronograph.ts` (the untracked template) and merges new
  strings into `harbour-chronograph-de.ts` as unfinished - translate them before
  committing.

### Tests

```sh
g++ -std=c++11 -Wall -Wextra -O2 -Isrc tests/tst_stepdetector.cpp \
    src/StepDetector.cpp -o /tmp/tst_stepdetector && /tmp/tst_stepdetector
```

Run from the project root. Two kinds of check:

- **Synthetic signals**, committed, everywhere: standing still, walking, slow
  and very slow walks, running, run into walk and walk into run, running with
  one leg weak, a few steps, random jolts, 20/30/100 Hz, a tilted phone, a
  pause in sensor delivery - and, shaped step by step from bumps and dips with
  natural jitter, what the real walks showed: one leg soft in a slow walk,
  steps peaking at 1 m/s², fidgeting, stairs with one leg's push weak, a walk
  slowing down at once, rhythmic handling in bursts, echoes in the hand, and
  the phone taken from the pocket into the hand while running.
- **The recordings of real walks**, only where `tests/raw-data/` exists: every
  `*.parts` file there is replayed against the steps actually taken. Elsewhere
  the suite prints `SKIP` for them and still passes.

Run it after any change to `StepDetector`, and add a case for any bug found on
a device - synthetic, modelled on what went wrong. Also build once with `-O0`:
it catches odr-uses of static members that `-O2` hides.

**`tests/tst_track.cpp`** checks `Track` on the host the same way, with
generated fixes once a second: a steady run, a walk without receiver speeds,
standing still with and without them, a walk with the receiver's error
wandering slowly and quickly, a pause with movement during it, a fix gone
stale, climbs with and without noise, and fixes outside a segment. Run it after
any change to `Track`, also with `-O0`:

```sh
g++ -std=c++11 -Wall -Wextra -O2 -Isrc tests/tst_track.cpp \
    src/Track.cpp -o /tmp/tst_track && /tmp/tst_track
```

**`tests/tst_trackfilter.cpp`** checks `TrackFilter` on the host: generated
activities of every type - routes whose speed and direction change no faster
than a body or a vehicle can - recorded by a generated receiver with wander,
noise, position jumps, speed spikes and altitude spikes, each replayed into a
`Track` raw and filtered and both held against the truth: distance, position,
speed and climb, the filtered ones better than raw and within limits. Also a
fair signal and one at the edge of fair (settling within 10 s), no receiver speeds, standing still, setting off and stopping per
type, altitude spikes alone, a bad start, half a minute 40 m off, a minute in
a tunnel, a drive filtered as a walk, and 20 seeds of walks and runs. Run it
after any change to `TrackFilter` or `Track`, also with `-O0`:

```sh
g++ -std=c++11 -Wall -Wextra -O2 -Isrc tests/tst_trackfilter.cpp \
    src/TrackFilter.cpp src/Track.cpp -o /tmp/tst_trackfilter && /tmp/tst_trackfilter
```

**`tests/tst_trackchart.cpp`** checks `TrackChart` on the host: an empty
track, a steady run (pace, average, the axis upside down and its ticks), a
run with stops whose slowing down must not stretch the axis, standing still,
a fast drive (the axis not below 0:00), a flat walk and a climb, no
altitude, a pause, points taken one by one against all at once, another
track, and fitting an axis. Run it after any change to `TrackChart`, and to
`Track`'s speed or altitude, also with `-O0`:

```sh
g++ -std=c++11 -Wall -Wextra -O2 -Isrc tests/tst_trackchart.cpp \
    src/TrackChart.cpp src/Track.cpp -o /tmp/tst_trackchart && /tmp/tst_trackchart
```

**`tests/tst_autopause.cpp`** checks `AutoPause` on the host, with a
generated speed a second and steps at a steady cadence: standing while
walking with and without steps, steps without speed, a runner's short and
long stop and a sprint, a bus with and without steps (a stop on the way, the
standing after it), walking along a train, a cycle downhill and in a car, a
traffic light while cycling, a drive's traffic light and parking, a rowing
boat drifting, started standing, resumed by hand while standing and in a
bus, no signal standing and moving, the stop after half an hour, and a noisy
walk and standing. Checked by breaking one rule at a time: each is caught,
except the clamp of a pause to the watch's start, which the rules already
keep. `Track::endAt()` has its case in `tst_track.cpp`. Run it after any
change to `AutoPause`, also with `-O0`:

```sh
g++ -std=c++11 -Wall -Wextra -O2 -Isrc tests/tst_autopause.cpp \
    src/AutoPause.cpp -o /tmp/tst_autopause && /tmp/tst_autopause
```

**`tests/tst_landcover.cpp`** checks `LandCover` on the host with generated
tiles of Carto's fills: the colours and the keys, a road through a forest
with its tree pattern and a label, an edge between two kinds, across a
tile's border and a corner, the place's own tile missing, too little to
tell, levels 19 and 10, the finest level that tells and the levels that told
nothing, the antimeridian and the poles. Checked by breaking one rule at a
time: each is caught except the sampling's density, which only costs time.
Run it after any change to `LandCover`, also with `-O0`:

```sh
g++ -std=c++11 -Wall -Wextra -O2 -Isrc tests/tst_landcover.cpp \
    src/LandCover.cpp -o /tmp/tst_landcover && /tmp/tst_landcover
```

**`tests/tst_stephistory.cpp`** checks `Database` and `StepHistory` with a clock
of its own: the schema and its migration, saving only when asked and on
destruction, a restart the same day, midnight, the clock set back, the previous
days, a newer schema and no database at all. It needs QtSql and its SQLite
driver, which the host does not have, so it runs in the build engine against
the i486 target - from that target's build directory, after one `sfdk build`
there (`build-shell` only works at the top of a build tree, and takes the
command without `--`):

```sh
cd ../build-harbour-chronograph-4.6.0.13-i486
~/SailfishOS/bin/sfdk -c target=SailfishOS-4.6.0.13-i486 build-shell sh -c '
    S=../harbour-chronograph B=../build-tst_stephistory && mkdir -p $B &&
    moc $S/src/StepHistory.h -o $B/moc_StepHistory.cpp &&
    g++ -std=c++11 -Wall -Wextra -fPIC -I$S/src \
        $(pkg-config --cflags Qt5Sql | sed "s/-I/-isystem /g") \
        $S/tests/tst_stephistory.cpp $S/src/Database.cpp $S/src/StepHistory.cpp \
        $B/moc_StepHistory.cpp $(pkg-config --libs Qt5Sql) -o $B/tst_stephistory &&
    $B/tst_stephistory'
```

Qt 5.6's headers go in with `-isystem`: the engine's compiler is newer than
they are and warns about them under `-Wextra`. Run it after any change to
`Database` or `StepHistory`.

**`tests/tst_activityhistory.cpp`** checks, the same way, the migration of a
0.2 database, GPX files written and read back (pauses, speeds, accuracy, times
to the millisecond) and from other apps, and `ActivityHistory`: file names,
rows, newest first, paging across 250 activities with a page boundary among
three of the same second, an activity added between pages, deleting one
(row, file and list; its file already gone; names with a path refused),
rewriting one (with its land cover; not one deleted, unlisted or named with
a path), no database; each activity's land cover rows as it is stored,
rewritten and deleted, those of older activities read off their files (one
whose file is gone, one deleted meanwhile, none read twice); each activity's
climb, with its row, at a rewrite and read off the files of older ones (also
one that has its land cover already); the statistics: all activities and per
type, their land cover with the land not known last, worked out anew at each
change, an activity still waiting counted as not known, read back as stored -
and what working out 2,000 costs; and the achievements: every category before
anything is stored, the first run's level, value and day, nothing written when
nothing changed, the steps, a level kept after its activity is deleted, a
drive counting for nothing, read back as stored, no database - and what
2,000 activities and 1,100 days cost. The build command is at the top of the
file. Run it after any change to `Database`, `Gpx`, `Track`, `LandCover`,
`ActivityHistory`, `ActivityStatistics`, `Achievements` or
`AchievementCatalogue`.

**`tests/tst_achievements.cpp`** checks `AchievementCatalogue` on the host
with generated activities and days: the catalogue (unique keys, rising
thresholds, known units and badges, groups together), the level of a value,
the single best and the totals with the types that count and those that do
not (driving), the fastest with its minimum distance, the land cover, the
weeks in a row (a gap, two in one week, a Sunday and the Monday after, an
activity out of order), the steps, nothing at all, and that `merge()` never
takes a level or the best value away and dates a level only as it rises. Run
it after any change to `AchievementCatalogue`, also with `-O0`:

```sh
g++ -std=c++11 -Wall -Wextra -O2 -Isrc tests/tst_achievements.cpp \
    src/AchievementCatalogue.cpp -o /tmp/tst_achievements && /tmp/tst_achievements
```

**Synthetic tests in place of recorded data** (decided 2026-10-14). Checked by
breaking one rule of `StepDetector` at a time - 17 in all: each correction
rule, the beat and its parts, the threshold's share and floor up and down, the
time limits, the confirmation: the synthetic tests catch 16 of them, the
recordings 16 as well. Only the recordings catch the gap rule (one step, in one
part: it only acts where no beat is found, which generated walks with a
missing step do not reproduce - their beat wanders instead); only the synthetic
jolts catch a 2.6 s limit before confirmation. Each new synthetic test was made
to catch its rule while the real detector counts it right; their seeds are
fixed, and with other seeds some of them vary by a few steps, the echoes most.

### Recording on a phone, replaying on the host

Real walks are recorded by the app itself. Start it with `CHRONOGRAPH_RECORD`
set and `StepCounter` writes every raw sample, with the count after it, to
`~/.local/share/de.ygriega/chronograph/recording-<date>-<time>.csv` - inside the
sandbox's own data directory, so it works under Sailjail. Off unless the
variable is set; the launcher never sets it. Flushed about once a second, so a
killed app keeps its walk.

**Every start deletes the recordings of earlier runs**, recording or not
(decided 2026-10-14): they are for debugging there and then, and a few
megabytes a walk should not pile up on the phone. Copy a recording off
*before* starting the app again - into `tests/raw-data/`, and nowhere else.

Test on the **Jolla Phone (2026)**. It is registered in the local SDK under that
name, which holds its address and key: `sfdk device list` shows them, and
`sfdk device exec` runs a command on it without needing either. Deploy with
`-c device="Jolla Phone (2026)"`. Keep the address and key out of this
repository.

```sh
PHONE="Jolla Phone (2026)"
~/SailfishOS/bin/sfdk device exec "$PHONE" -- sh -c \
    '(CHRONOGRAPH_RECORD=1 nohup sailjail -p harbour-chronograph.desktop -- /usr/bin/harbour-chronograph >/dev/null 2>&1 &)'
# Earlier recordings are deleted at every start, so there is only ever one.
~/SailfishOS/bin/sfdk device exec "$PHONE" -- sh -c \
    'cat .local/share/de.ygriega/chronograph/recording-*.csv' 2>/dev/null \
    > tests/raw-data/recording-<date>-<time>.csv

g++ -std=c++11 -Wall -Wextra -O2 -Isrc tests/replay_recording.cpp \
    src/StepDetector.cpp -o /tmp/replay_recording
/tmp/replay_recording tests/raw-data/<file>.csv [-v]
```

The replay reproduces the phone's count exactly (213 of 213 for the first
recording), so a detector change can be judged on the host. A recording worth
keeping gets what actually happened written down in `tests/raw-data/README.md`
- steps taken, carrying position, the time ranges - and a parts file beside
it, which the suite then replays. A recording without its ground truth is not
a reference. Anything learnt from it that matters for the code goes into this
file in general terms only - what the signal does, how the detector fares -
never when, where or what the walker was doing, nor their cadence or step
strength; and into a synthetic test with generic parameters.

Ask the walker to stand still for ~10 s between parts: the pauses are what lets
the recording be cut into segments afterwards.

QML: `find qml -name '*.qml' -exec qmllint {} +` catches syntax and nothing
else (see Lauscher's notes on what it misses).

### The emulator

`sfdk emulator start` runs Sailfish OS 5.1 (i486); deploy with
`sfdk -c target=SailfishOS-5.1.0.11-i486 -c device="Sailfish OS Emulator 5.1.0.11" deploy --sdk`.

- **It has no accelerometer.** Qt reports "No accelerometer backend available"
  inside the sandbox and outside it alike, and lipstick cannot find its
  orientation sensor either. The app shows its `Unavailable` state there, which
  is all the emulator can verify. Step counting needs a phone.
- Re-deploying a package of the **same version** pops an install confirmation in
  the emulator's UI and `deploy` waits for it indefinitely. Install as root
  instead: `scp` the RPM over and `rpm -Uvh --force` it (`ssh -p 2223 -i
  ~/SailfishOS/vmshare/ssh/private_keys/sdk root@127.0.0.1`).
- Qt's warnings go to the journal, not to stderr: `journalctl | grep chronograph`.
- **New Sailjail permissions are asked for at launch**, in a dialog no one can
  tap in a scripted run. Grant them as root instead: stop `sailjaild`, set
  `Allowed=1`, `Agreed=1` and `Granted=Location;Internet;` in the
  `[harbour-chronograph]` section of
  `/home/.system/var/lib/sailjail/settings/user-100000.settings`, start it.
- **It has no positioning plugin either**: the activity page shows "No
  positioning on this device". Set `CHRONOGRAPH_NMEA` to an NMEA log in the
  app's data directory (`chown` it to `defaultuser`) and launch through
  `sailjail` as for recordings. Qt 5.4's simulation keeps only the **last
  sentence of each second**: put `$GPRMC` last for speed and course, `$GPGGA`
  last for altitude and accuracy - one or the other, not both. A generated
  log (a loop at a few m/s) does; never a recorded one, which is personal
  data like the walks.
- **Its animations run slow** - at about half speed under load - and Silica's
  remorse countdown is an animation: a 10 s countdown took up to 20 s there.
- Without touch input, the activity page was driven by a `Timer` patched into
  the **installed** QML only (push the page, start, pause, resume, stop, move
  the slider, pop the page, `Qt.quit()`), then the clean RPM reinstalled.
- Screenshots need the `privileged` group:
  `su -g privileged defaultuser -c 'DBUS_SESSION_BUS_ADDRESS=unix:path=/run/user/100000/dbus/user_bus_socket dbus-send --session --print-reply --dest=org.nemomobile.lipstick /org/nemomobile/lipstick/screenshot org.nemomobile.lipstick.saveScreenshot string:/home/defaultuser/Pictures/shot.png'`
  as root.

## The cover

- The backdrop goes through `qml/components/BackgroundImage.qml`, Lauscher's
  copy of Fernschreiber's component, and is anchored exactly as both apps anchor
  theirs: a square as wide as the cover is tall, hung off the bottom right, so it
  overflows the cover and reads large. Opacity 0.15.
- **Both ambiences**: `images/background-white.svg` for a dark ambience,
  `background-black.svg` for a light one, picked with
  `Theme.colorScheme ? "black" : "white"` - `Theme.LightOnDark` is 0, so the pale
  artwork is the falsy case. The two files differ only in colour.
- The backdrop is the icon's chronograph redrawn in one colour, in the icon's
  own 86-unit coordinates scaled by 2.6, so the two stay easy to keep in step.
  Its face is an outline rather than a disc, which in one colour would swallow
  the marks and the hand.
- Content: today's count (`fontSizeHuge`, shrinking to fit), "Steps", and a
  status line only while not counting. No action.
- **While an activity is recorded or paused** (decided 2026-10-18, at the
  project owner's request) the cover shows it instead: its type, the elapsed
  time large, the distance and the current pace - the activity page's values,
  through `Units.js` - and "Paused" while paused, "Paused automatically"
  while auto-paused. One action pauses it, and
  once paused resumes it, with the theme's `icon-cover-pause` and
  `icon-cover-play`. Before the start and after the stop it is the steps
  again, without an action.
- **How the cover finds the activity**: the `Activity` is the activity page's
  own, made by QML, and the cover cannot see into the page. So
  `ActivityHistory`, a context property everything can see, has a
  `recording` property, which `Activity::setStatus()` sets at the start and
  clears at the stop - the destructor stops a recording, so it is cleared
  then too. Typed `QObject *` rather than `Activity *`, so that
  `ActivityHistory` and its test do not depend on `Activity`, positioning
  and libkeepalive.
- Actions the way Lauscher has them: **one `CoverActionList` per state**,
  as lipstick takes an enabled list wholesale; none enabled means none shown.

## The activity type pictograms

`images/activity-<key>.svg`, one per type, shown at the left of each
activity in the overview's list, as tall as its two lines. Drawn for this app
on a 64-unit canvas (decided 2026-10-19): the stock theme has nothing for any
of the types.

- **White on transparent, like the theme's own icons**, and shown through
  Silica's `Icon`, which tints them with the palette's primary colour - pale on
  a dark ambience, dark on a light one - and with the highlight colour while
  the item is pressed. One file serves both ambiences; Fernschreiber does its
  own icons the same way. Only the alpha counts, so they are one colour.
- Bold, rounded strokes (5.5 units for figures, 4 for the bicycle), so they
  hold up at the list's size of about 64 px.
- **Qt draws SVG Tiny 1.2**: no masks, filters or CSS. Where two shapes must
  stay apart - the oar across the hull - the gap is cut into the geometry.

## The app icon

`images/harbour-chronograph.svg` is the master; `icons/<size>/` holds what it
renders to (86, 108, 128, 172 and 256 px - Fernschreiber's set). The master sits
in `images/` because that directory is installed whole, which lets `AboutPage`
show it as the logo. It follows Jolla's [Apps icon
story](https://sailfishos.org/content/uploads/2018/11/48_SAILFISH-APPS-ICON-STORY.pdf):

- **86-unit canvas, filled edge to edge** with only a 0.3-unit gap, so the base
  shape is a circle of radius 42.7. The metaphor stays within the 12-unit inset
  outline.
- **The circle and its colours are the guide's page 9, row 2, column 2**: a
  45-degree gradient, dark at the bottom left (`#e16c31`) and light at the top
  right (`#fbb558`). The stops were sampled off the guide's own artwork where the
  diagonal meets the circle.
- **The metaphor borrows the guide's clock on page 12**, the only orange-circle
  example in it: a cream face (`#f7ebc9`) with deep blue hands (`#1472a0`) and
  rounded strokes. A crown and two pushers make it a chronograph rather than a
  clock.

Re-render after editing the SVG:

```sh
for s in 86 108 128 172 256; do
    inkscape images/harbour-chronograph.svg --export-type=png \
        --export-filename=icons/${s}x${s}/harbour-chronograph.png -w $s -h $s
done
```

`rsvg-convert` is not installed on this machine; Inkscape is. ImageMagick alone
drops gradients (see Lauscher's notes).

## QML conventions and gotchas

Inherited from Lauscher, where each one cost debugging time:

- **`import "pages"` in `qml/harbour-chronograph.qml` is mandatory.** Without it
  the initial page does not resolve and the app shows a blank window, silently.
- **Every page sets `allowedOrientations: Orientation.All`.** The window's
  setting alone rotates nothing.
- `Text.WordWrap`, not `Text.Wordwrap` - the misspelling is silently undefined.
- `PageStatus.Deactivating` also fires when a page is pushed on top; use
  `Component.onDestruction` for "the user left for good".
- Silica controls that write their own value (`Slider.value`,
  `ComboBox.currentIndex`, `TextSwitch.checked`) lose a binding on first use.
- Numbers shown to the user go through `toLocaleString(Qt.locale(), "f", 0)`, so
  a German phone reads 12.345.
- "Steps" is a caption under the number, not a plural form, so no count ever
  reads "1 steps" and the catalogue needs no numerus forms.
- **A page without a `PageHeader` keeps clear of a notch itself**: Silica's
  `Screen.hasCutouts` and `Screen.topCutout` (a portrait rect; its height is
  the notch's depth). Push content down by it in `Orientation.Portrait`, and
  in landscape - where the notch sits halfway down whichever side the phone
  was turned to - keep that much margin from the side edge, as Silica's pulley
  menu and the launcher do. `ActivityPage` does. **Silica's `PageHeader`
  keeps clear of the notch itself from 5.x on** (it has a `topMargin`), not
  in 4.6: `ActivityPage` moves its header down only where `topMargin` is
  undefined. The 5.1 emulator reports a 43 px top cutout in portrait.
- **Never `id: layer`** (or any other id that is also a property of `Item`):
  inside an `Image`, `layer.foo` finds the image's own `layer` group, not the
  id - silently, with a TypeError at runtime only. That cost the map its tiles.
- A `Slider` whose `value` and `maximumValue` are both bound may get the value
  first, be clamped to the old maximum, and stay there. Set the value in
  `onMaximumValueChanged` instead.
- Dates cross from C++ as `yyyy-MM-dd` strings, not `QDate`, whose conversion
  to a JavaScript `Date` is known to shift days across time zones in Qt 5. QML
  builds the `Date` from the parts (`OverviewPage.dayName()`):
  `new Date("yyyy-MM-dd")` would be midnight UTC, the day before west of
  Greenwich.

German strings use the formal "Sie", as Lauscher's do.

## Status / next steps

**Iteration 3 (0.3) is done in the emulator**: builds for 4.6 (aarch64,
armv7hl, i486) and 5.1 (aarch64, i486), Harbour-clean since `Sensors` went,
passes all four test suites (five with the filter's), and in the emulator, sandboxed, with a generated
NMEA log: tiles load through the custom User-Agent and cache; the values,
route and heading update every second; pause leaves the time and the distance
out; hold-to-stop shows its countdown; stopping stores the GPX file and the row
and switches to the slider; the list shows the new activity at once and
replays it with the same values; leaving the page and quitting the app while
recording both store it. German, portrait and landscape checked on screen.

The smoothing filter (same day) builds for all five targets without
warnings, stays Harbour-clean, passes five test suites, and in the emulator,
sandboxed, recorded a generated 3 m/s loop with 40-70 m jumps and altitude
spikes and no receiver speeds: the stored points stay within 8 m of the loop
(the fixes strayed 56 m), the distance came to 0.91 km of 0.90, the speeds to
2.5-3.5 m/s, and the map showed a smooth loop live and in the replay.

The charts (2026-10-18) build for all five targets without warnings, stay
Harbour-clean, pass all six test suites, and in the emulator, sandboxed:
the stored generated loop replays with both charts, the slider leaving them
faint after its point; a recording from a generated NMEA log grows them every
second, breaks them at a pause and shows the dotted average; the dots follow
the slideshow. Portrait and landscape, German, checked on screen.

Deleting (2026-10-18) builds for all five targets without warnings, stays
Harbour-clean, passes the activity history's tests, and in the emulator,
sandboxed, driven by a patched-in timer: the context menu, the countdown on
the list item and the popup on the activity page show; each deletes the row
and the file once it runs out, the list loses the item, and the page goes
back; pushing a page during the list's countdown deletes at once.

The activity on the cover (2026-10-18) builds for all five targets without
warnings, stays Harbour-clean, passes the activity history's tests, and in
the emulator, sandboxed, with a generated NMEA log and a patched-in timer:
the steps cover without an action while the page waits for Start; the type,
time, distance and pace with a pause action while recording; "Paused", no
pace and a play action after the cover's pause action fired; recording
again after its resume action; the steps without an action after the stop.

Auto-pause (2026-10-20) builds for all five targets without warnings,
stays Harbour-clean, passes the host suites (`tst_autopause` and
`tst_track` with `endAt()` new) and the activity history's tests, and in the
emulator, sandboxed, walking, with a generated NMEA log and a patched-in
timer: started standing it paused as of the start; 30 s standing paused
after 10 s, the time stepping back by the wait; walking on resumed within
3 s; a bus at 10 m/s paused about 18 s after it set off, distance and time
going back to where it got fast; neither its stop nor the standing after it
resumed; walking on did, after 10 s (the emulator has no steps); the switch
off resumed, on again paused as of that moment; the GPX file held the five
segments; the cover's label followed. A 50 s gap in the log showed "GPS
signal lost" after 30 s and paused nothing. Portrait and landscape, German,
checked on screen.

Land cover (2026-10-23) builds for all five targets without warnings, stays
Harbour-clean, passes all host suites and the activity history's tests, and
in the emulator, sandboxed, driving a generated straight line through
Tampere with the tile cache emptied first and the display off for 2.5 of
almost 4 minutes: no tile was fetched while it was off; the points shown
live got level 16, most of the dark stretch too from the map's view at
either end; after the stop the whole-route view at level 12 classified 15
more and the file was rewritten 4 s later; 5 % stayed unknown. A file from
before replayed, got level 17 and was rewritten. The slide, German, portrait
and landscape, checked on screen.

The statistics (2026-10-24) build for all five targets without warnings,
stay Harbour-clean, pass the activity history's and the step history's
tests, and in the emulator, sandboxed, with a patched-in timer: a database
of 16 generated activities was migrated from schema 3 to 6, their land
cover read off their files at the start and not again at the next; the page
showed all activities, then running, walking and driving, the types adding
up to the whole. German, portrait, checked on screen.

The achievements (2026-10-26) build for all five targets without warnings,
stay Harbour-clean, pass `tst_achievements` (also at `-O0`), the activity
history's tests (also run on the Jolla Phone (2026), for the cost) and the
step history's, and in the emulator, sandboxed, with a patched-in timer: a
generated database of 29 activities and 60 days of steps was migrated from
schema 7 to 8 and its achievements worked out at the start - 22 of 26, each
dated by the activity or day that reached it, the drive left out; the page,
its groups, the greyed-out rowing and heath, and the overlay for a level
reached, one not reached and the steps, German, portrait, checked on
screen. Not checked: landscape, a light ambience, and the "So far" line
after a deletion (the tests cover the value). On the Jolla Phone (2026),
the project owner checked the overlay, landscape and a light ambience.

The notice after an activity (2026-10-27) passes the activity history's
tests (the levels announced within the storing, none when nothing rose)
and in the emulator, which cannot record without a fix, with the wait for
the stop patched out of the installed QML and a timer queueing three: shown
one after another with "1 of 3" to "3 of 3", each going on at OK, the page
back after the last. German, portrait, checked on screen.

Still to confirm on the Jolla Phone (2026):

- [ ] Steps are still counted with only `Location` and `Internet` granted -
      the accelerometer must not report `Unavailable`.

- [ ] A fix inside Sailjail with `Location`, and how fast it comes.
- [ ] Recording with the display off, the page in the background.
- [ ] Whether 15 m / 40 m suit the phone's accuracy figures, and whether its
      altitude is ellipsoid or sea level.
- [ ] Distance against a known course; the 5 m rule and receiver speeds.
- [ ] Pinch-zoom, dragging, the back swipe above the map, holding to stop,
      swiping between the values and the charts, the activity page's pulley,
      tapping a countdown to keep the activity and tapping the cover's pause
      and resume action - all touch, which the emulator cannot do.
- [ ] Tiles on mobile data, and the cache across restarts.
- [ ] No tile fetched with the display off while recording (the map's
      `viewed`), and the land cover on real activities: whether farmland and
      unmapped land are told apart, how much stays unknown, and whether the
      file is rewritten after the stop.
- [ ] The statistics: the pulley's entry, and the land cover of the
      activities recorded before read at the first start.
- [ ] The achievements: the climb of the activities recorded before read
      at the first start, the levels of real activities - whether the first
      levels come as easily as meant and the climb's wander lifts them
      early - and the notice after a real stop, OK by touch.
- [ ] Auto-pause on real activities: whether the filter's speed standing
      stays below the limits (a single second over restarts the count),
      whether steps resume a walk promptly, how late a bus or a car is
      caught, and tapping the switch.
- [ ] The filter against real fixes: whether the route still cuts no corner
      worth speaking of, whether 5 sigma leaves out the phone's real jumps and
      nothing else, how often the second guess takes over (it should be rare),
      and whether the phone's altitude wanders more than the 3 m assumed.
      Real fixes are personal data like the walks: any log of them stays in
      `tests/raw-data/`, and what is learnt goes into the generated receiver.

**Iteration 1 is done**: builds for 4.6 and 5.1, passes the host tests, runs
sandboxed in the emulator (overview, cover, about page and the unavailable state
checked on screen there) and on a phone.

**Iteration 2 (0.2) is done**: builds for 4.6 and 5.1 with only the known
Harbour error, passes both test suites, and in the emulator creates and
migrates the database inside Sailjail. The overview was checked on screen there
with rows put into the database and the unavailable state patched out of the
installed QML for the screenshot; the emulator has no accelerometer to count
with.

Confirmed on the Jolla Phone (2026), 2026-10-15, with the 4.6 aarch64 build:

- [x] Steps survive closing the app from the home screen and starting it again
      the same day.
- [x] The count starts over at midnight while the app runs, and yesterday's
      appears among the previous days.
- [x] What the detector counts, the page shows and the database stores agree.

### Confirmed on hardware, 2026-10-13

Jolla Phone (2026), Sailfish OS 5.2.0.18, with the 4.6 aarch64 build:

- [x] The 4.6 build installs and runs on 5.2.
- [x] The accelerometer delivers inside Sailjail with `Permissions=Sensors`.
- [x] Readings continue with the display off: the longest gap in 405 s, a walk
      in the pocket with the display off included, was 40 ms. Standby override
      and keepalive both work; no batching seen.
- [x] Counted 108 and 105 for 100 steps in the hand and in the pocket; 0 for
      picking up, putting down and shaking.

### What the recordings showed

Three sessions on the Jolla Phone (2026), all of one walker, about 1,000 steps
in all. What was done part by part, the variants tried and how the signal looks
are in `tests/raw-data/FINDINGS.md` - personal data, so it exists only on the
project owner's machine and may be missing from a checkout. In short, with the
final detector:

| Carried | Walking | Running | Stairs |
|---|---|---|---|
| In the hand | 104 of 100 | 31 of ~33 once confirmed | - |
| Trouser pocket | 100 - 102 of 100 | 120 of 120, 54 of ~57 | 29 - 34 of 31 - 33 |
| Front pocket, stairs down | - | - | 20 of 31 |
| Bag | 110 of 105 - 110 | - | - |
| Faster / slower and softer walk | 100 / 106 of 100 | - | - |

Handling the phone - picking it up, putting it down, shaking it, rhythmic
movements - counts 0. Where it falls short is under [Accepted
flaws](#accepted-flaws).

**What was tried on the way**, each scored on all the recordings, the
synthetic transitions and 20 seeds of random jolts:

| Variant | Hand | Pocket | Pocket 2 | Faster | Slower | Notes |
|---|---|---|---|---|---|---|
| 0.1 | 108 | 105 | 106 | 96 | 42 | |
| dead time 0.6× median interval + gap filling | 95 | 98 | - | - | - | walk into run: 84 of 126 - the dead time came from *past* intervals |
| echo rule + retroactive gap filling, 2.6 s limit | 98 | 97 | 99 | 99 | 46 | soft walk still lost |
| + threshold 0.7 / 0.3 × peaks | 103 | 123 | 97 | 100 | 68 | pockets overcount |
| + threshold 0.8 × RMS (2 s) | 102 | 102 | 99 | 99 | 97 | jolts 6 per 60; cliff at 1.0 × RMS |
| + 2.6 s limit only once confirmed | same | | | | | jolts back to 2.5 per 60 |
| + RMS over 1 s, 0.85 × RMS | 101 | 102 | 99 | 99 | 105 | run into walk -5, walk into run 0 |
| **+ the signal's own beat (final)** | **104** | **102** | **100** | **100** | **106** | running in a pocket 108 → 120 of 120 |

0.75-0.85 × RMS all score well; at 0.9 a pocket walk and the synthetic
transitions start to collapse, so the factor sits at the top of its safe range.

### Accepted flaws

Accepted with the algorithm on 2026-10-14 (see [The step detection
algorithm](#the-step-detection-algorithm)). The parts files of the recordings
(local only) pin each one at its current level with a comment, and the
synthetic handover test models the second, so that a change making them worse
still fails.

- Short walks lose their start or all of it: under six steady steps by design,
  and 10-15 steps with an uneven gait or barely moving the phone as well.
- Taking the phone from the pocket into the hand while moving loses the run for
  a few seconds (~12 steps once).
- Front-pocket stairs down: 20 of 31.
- The first pocket walk's weak leg (double humps): right in total (102), but
  step by step still largely cancellation.
- Stairs overcount by up to ~10 %.
- Untried: front-pocket walks, walk/run transitions, a jacket pocket, typing
  while walking, a bus ride, and anyone but the one walker.

### Open

- [ ] Why the phone delivers 50 Hz when 30 is asked for.
- [ ] Battery drain per hour with the app running and the display off.
- [ ] The cover backdrop on a light ambience.

Candidates for later iterations, each to be decided with the project owner:
auto-pause events D, E and G (see [Auto-pause](#auto-pause-standing-too-fast-and-the-stop-after-half-an-hour));
a longer history; a reset; daily goals; distance and calories from step
length; saving on SIGTERM, should a shutdown while walking lose too much; and
whether all-day tracking needs a background service or a hardware step counter
instead of the running app. For activities: changing a stored
one's type; writing the track as it goes, so a crash loses nothing; steps
during an activity; laps and
splits; imperial units; sharing or exporting the GPX file.
