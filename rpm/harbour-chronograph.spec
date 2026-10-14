Name:       harbour-chronograph
Summary:    Activity tracker for Sailfish OS
Version:    0.1
Release:    1
License:    GPLv3+
URL:        https://github.com/Wunderfitz/harbour-chronograph
Source0:    %{name}-%{version}.tar.bz2

# Everything the app links against (Qt5Core/Gui/Qml/Quick/Sensors, libsailfishapp,
# libkeepalive, libstdc++) is picked up by rpmbuild's ELF dependency generator.
# What follows is what it cannot see. The sensorfw plugin for QtSensors is not
# listed: every device ships it, and Harbour does not accept it as a dependency.
Requires:   sailfishsilica-qt5 >= 0.10.9

BuildRequires:  pkgconfig(sailfishapp) >= 1.0.2
BuildRequires:  pkgconfig(Qt5Core)
BuildRequires:  pkgconfig(Qt5Qml)
BuildRequires:  pkgconfig(Qt5Quick)
BuildRequires:  pkgconfig(Qt5Sensors)
BuildRequires:  pkgconfig(keepalive)
# CONFIG += sailfishapp_i18n runs lupdate and lrelease during %%install
BuildRequires:  qt5-qttools-linguist
BuildRequires:  desktop-file-utils

%description
Chronograph is an activity tracker for Sailfish OS. It counts your steps with
the phone's accelerometer, also while the display is off, and shows them on
its main page and its cover.

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
