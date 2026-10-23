# harbour-chronograph - activity tracker for Sailfish OS
#
# TARGET names the binary, the main QML file, the .desktop file, the icons and
# the translations alike; renaming it means renaming all of those.

TARGET = harbour-chronograph

CONFIG += sailfishapp
QT += sensors sql positioning network
# BackgroundActivity, which keeps the phone awake while steps are counted
PKGCONFIG += keepalive

SOURCES += \
    src/main.cpp \
    src/Activity.cpp \
    src/AutoPause.cpp \
    src/ActivityHistory.cpp \
    src/ChartLine.cpp \
    src/Database.cpp \
    src/Gpx.cpp \
    src/LandCover.cpp \
    src/LandCoverReader.cpp \
    src/StepCounter.cpp \
    src/StepDetector.cpp \
    src/StepHistory.cpp \
    src/TileCache.cpp \
    src/TileNetworkFactory.cpp \
    src/Track.cpp \
    src/TrackChart.cpp \
    src/TrackFilter.cpp \
    src/TrackLine.cpp \
    src/TrackReader.cpp

HEADERS += \
    src/Activity.h \
    src/AutoPause.h \
    src/ActivityHistory.h \
    src/ChartLine.h \
    src/Database.h \
    src/Gpx.h \
    src/LandCover.h \
    src/LandCoverReader.h \
    src/StepCounter.h \
    src/StepDetector.h \
    src/StepHistory.h \
    src/TileCache.h \
    src/TileNetworkFactory.h \
    src/Track.h \
    src/TrackChart.h \
    src/TrackFilter.h \
    src/TrackLine.h \
    src/TrackReader.h

DISTFILES += \
    qml/harbour-chronograph.qml \
    qml/cover/CoverPage.qml \
    qml/components/AboutLink.qml \
    qml/components/AboutParagraph.qml \
    qml/components/ActivityChart.qml \
    qml/components/ActivityValue.qml \
    qml/components/BackgroundImage.qml \
    qml/components/HoldButton.qml \
    qml/components/LandCoverList.qml \
    qml/components/TileLayer.qml \
    qml/components/TileMap.qml \
    qml/js/Units.js \
    qml/pages/AboutPage.qml \
    qml/pages/ActivityPage.qml \
    qml/pages/OverviewPage.qml \
    harbour-chronograph.desktop \
    $$files(images/*.svg) \
    rpm/harbour-chronograph.spec \
    rpm/harbour-chronograph.changes \
    translations/*.ts \
    tests/tst_stepdetector.cpp \
    tests/tst_stephistory.cpp \
    tests/tst_track.cpp \
    tests/tst_trackfilter.cpp \
    tests/tst_trackchart.cpp \
    tests/tst_autopause.cpp \
    tests/tst_landcover.cpp \
    tests/tst_activityhistory.cpp \
    tests/replay_recording.cpp \
    README.md \
    CLAUDE.md

# sailfishapp.prf deploys qml/ for us but knows nothing about anything else, so
# the cover's artwork and the logo need their own install. QML reaches them as
# ../../images, which holds both here and under /usr/share/harbour-chronograph.
images.files = images
images.path = /usr/share/$${TARGET}
INSTALLS += images

SAILFISHAPP_ICONS = 86x86 108x108 128x128 172x172 256x256

CONFIG += sailfishapp_i18n
TRANSLATIONS += translations/harbour-chronograph-de.ts
