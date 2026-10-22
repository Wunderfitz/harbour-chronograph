# Chronograph

An activity tracker for Sailfish OS, developed iteratively. It records runs,
walks, bike rides, rowing and drives with GPS - duration, distance, pace and
speed, elevation and climb, with the route on an OpenStreetMap map,
charts of the pace and the elevation and the distance over each land cover,
read off the map - stores each as a GPX file, replays
it and deletes it again. It also counts steps with the phone's accelerometer from the
moment the app starts, also while the display is off, and keeps the steps of
each day in a local database. The main page lists the activities, newest
first, above today's steps and those of the three days before; the cover shows
today's.

Map data © OpenStreetMap contributors, under the Open Database License.

Requires Sailfish OS 4.6 or newer. Licensed under the GNU GPLv3 or later.

## Building

```sh
mkdir -p ../build-harbour-chronograph-4.6.0.13-aarch64
cd ../build-harbour-chronograph-4.6.0.13-aarch64
sfdk -c target=SailfishOS-4.6.0.13-aarch64 build ../harbour-chronograph
```

See [CLAUDE.md](CLAUDE.md) for how the app is put together and why.
