Name:       harbour-chronograph
Summary:    Activity tracker for Sailfish OS
Version:    0.3.0
Release:    1
License:    GPLv3+
URL:        https://github.com/Wunderfitz/harbour-chronograph
Source0:    %{name}-%{version}.tar.bz2

# Everything the app links against (Qt5Core/Gui/Qml/Quick/Network/Sensors/Sql/
# Positioning, libsailfishapp, libkeepalive, libstdc++) is picked up by
# rpmbuild's ELF dependency generator. What follows is what it cannot see. The
# sensorfw plugin for QtSensors, the SQLite driver for QtSql and the geoclue
# plugin for QtPositioning are not listed: every device ships them, and Harbour
# accepts none of them as a dependency.
Requires:   sailfishsilica-qt5 >= 0.10.9

BuildRequires:  pkgconfig(sailfishapp) >= 1.0.2
BuildRequires:  pkgconfig(Qt5Core)
BuildRequires:  pkgconfig(Qt5Qml)
BuildRequires:  pkgconfig(Qt5Quick)
BuildRequires:  pkgconfig(Qt5Network)
BuildRequires:  pkgconfig(Qt5Positioning)
BuildRequires:  pkgconfig(Qt5Sensors)
BuildRequires:  pkgconfig(Qt5Sql)
BuildRequires:  pkgconfig(keepalive)
# CONFIG += sailfishapp_i18n runs lupdate and lrelease during %%install
BuildRequires:  qt5-qttools-linguist
BuildRequires:  desktop-file-utils

# The last paragraph of the description is the metadata SailfishOS:Chum shows
# the app with (https://github.com/sailfishos-chum/main/blob/main/Metadata.md).
# It must stay the last and contiguous; outside Chum's builds it is left out.
%description
Chronograph is an activity tracker for Sailfish OS. It records your runs,
walks, bike rides, rowing and drives with GPS on an OpenStreetMap map, stores
each as a GPX file and replays it, with charts of the pace and the elevation
and the land cover it went through. It counts your steps with the phone's
accelerometer, also while the display is off, and keeps the count of every
day. A statistics page totals what you have done, and achievements in seven
levels mark the milestones on the way. Everything stays on the phone.

%if 0%{?_chum}
Title: Chronograph
Type: desktop-application
Categories:
 - Utility
 - Maps
Custom:
  Repo: https://github.com/Wunderfitz/harbour-chronograph
PackageIcon: https://github.com/Wunderfitz/harbour-chronograph/raw/master/images/harbour-chronograph.svg
AIRating: V
Screenshots:
 - https://github.com/Wunderfitz/harbour-chronograph/raw/master/screenshots/overview.png
 - https://github.com/Wunderfitz/harbour-chronograph/raw/master/screenshots/activity.png
 - https://github.com/Wunderfitz/harbour-chronograph/raw/master/screenshots/charts.png
 - https://github.com/Wunderfitz/harbour-chronograph/raw/master/screenshots/land-cover.png
 - https://github.com/Wunderfitz/harbour-chronograph/raw/master/screenshots/statistics.png
 - https://github.com/Wunderfitz/harbour-chronograph/raw/master/screenshots/achievements.png
 - https://github.com/Wunderfitz/harbour-chronograph/raw/master/screenshots/achievement.png
%endif

%prep
%setup -q -n %{name}-%{version}

%build
%qmake5
%make_build

%install
%qmake5_install

desktop-file-install --delete-original       \
  --dir %{buildroot}%{_datadir}/applications \
  %{buildroot}%{_datadir}/applications/*.desktop

%files
%defattr(-,root,root,-)
%{_bindir}/%{name}
%{_datadir}/%{name}
%{_datadir}/applications/%{name}.desktop
%{_datadir}/icons/hicolor/*/apps/%{name}.png
