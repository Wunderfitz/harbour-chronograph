# harbour-chronograph

An activity tracker for Sailfish OS, built **iteratively**. Iteration 1 (version
0.1) counts steps with the accelerometer from the moment the app starts, also
while the display is off, and shows the count on the main page and the cover.
Iteration 2 (0.2) keeps the steps of each calendar day in a local SQLite
database; the main page and the cover show today's, the main page also the
three days before.

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

| What | Why | Approved |
|---|---|---|
| `Permissions=Sensors` in `[X-Sailjail]` | Sailjail only lets an app talk to sensorfw (`com.nokia.SensorService` on the system bus) with this permission; without it there is no accelerometer and no step counting. The Harbour validator rejects it (`X-Sailjail permission not allowed: Sensors`, both 4.6 and 5.1 tooling). | 2026-10-13 |

That is the **only** error `sfdk check` reports, so the app ships through
Chum/OpenRepos until Jolla allows the permission. Everything else stays
Harbour-clean on purpose, so that dropping this one line is all it takes if
they ever do. For the same reason the spec does not `Require`
`qt5-qtsensors-plugin-sensorfw` or `sailjail-permissions`: neither is on
Harbour's allowed list, and every device ships both anyway.

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
  `StepHistory`. Further features get their own class and their own table.
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

## Layout

```
harbour-chronograph.pro       CONFIG += sailfishapp, QT += sensors, PKGCONFIG += keepalive
harbour-chronograph.desktop   Sailjail: Permissions=Sensors (the Harbour exception)
src/main.cpp                  creates Database, StepHistory, StepCounter, then the view
src/Database.*                the SQLite connection and the schema's migrations
src/StepHistory.*             steps per day: today's count, saving, the previous days
src/StepCounter.*             QAccelerometer + keepalive, the QML facade
src/StepDetector.*            the algorithm, Qt-free
tests/tst_stepdetector.cpp    host-side checks for StepDetector (not packaged)
tests/tst_stephistory.cpp     Database and StepHistory, run in the build engine
tests/replay_recording.cpp    replays a phone recording through StepDetector
tests/raw-data/               personal data, git-ignored, local only: the recordings,
                              their ground truth (README), parts, FINDINGS.md
qml/harbour-chronograph.qml   ApplicationWindow
qml/pages/OverviewPage        today's count, the three days before; pulley menu with the about page
qml/pages/AboutPage           logo, version, what to know, credits
qml/cover/CoverPage           today's count over the chronograph backdrop
qml/components/               BackgroundImage, AboutLink, AboutParagraph (from Lauscher)
images/                       icon master and cover backdrops (SVG), installed whole
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
  as well, which the persistence tests need.
- `sfdk check` must show exactly one Harbour error, the Sensors permission. It
  also warns that the binary is not stripped, which is how the SDK builds it,
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
  status line only while not counting. No cover actions yet - there is nothing to act
  on. Add them the way Lauscher does (one `CoverActionList` per combination).

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
- Dates cross from C++ as `yyyy-MM-dd` strings, not `QDate`, whose conversion
  to a JavaScript `Date` is known to shift days across time zones in Qt 5. QML
  builds the `Date` from the parts (`OverviewPage.dayName()`):
  `new Date("yyyy-MM-dd")` would be midnight UTC, the day before west of
  Greenwich.

German strings use the formal "Sie", as Lauscher's do.

## Status / next steps

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
a longer history; a reset; daily goals; distance and calories from step
length; saving on SIGTERM, should a shutdown while walking lose too much; and
whether all-day tracking needs a background service or a hardware step counter
instead of the running app.
