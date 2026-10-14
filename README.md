# Chronograph

An activity tracker for Sailfish OS, developed iteratively. It counts steps
with the phone's accelerometer from the moment the app starts, also while the
display is off, and keeps the steps of each day in a local database. The main
page and the cover show today's steps, the main page also those of the three
days before.

Requires Sailfish OS 4.6 or newer. Licensed under the GNU GPLv3 or later.

## Building

```sh
mkdir -p ../build-harbour-chronograph-4.6.0.13-aarch64
cd ../build-harbour-chronograph-4.6.0.13-aarch64
sfdk -c target=SailfishOS-4.6.0.13-aarch64 build ../harbour-chronograph
```

See [CLAUDE.md](CLAUDE.md) for how the app is put together and why.
