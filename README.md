# Chronograph

An activity tracker for Sailfish OS, developed iteratively. The first version
counts steps with the phone's accelerometer from the moment the app starts,
also while the display is off, and shows the count on the main page and on the
cover. Nothing is stored yet.

Requires Sailfish OS 4.6 or newer. Licensed under the GNU GPLv3 or later.

## Building

```sh
mkdir -p ../build-harbour-chronograph-4.6.0.13-aarch64
cd ../build-harbour-chronograph-4.6.0.13-aarch64
sfdk -c target=SailfishOS-4.6.0.13-aarch64 build ../harbour-chronograph
```

See [CLAUDE.md](CLAUDE.md) for how the app is put together and why.
