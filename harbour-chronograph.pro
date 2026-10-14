# harbour-chronograph - activity tracker for Sailfish OS
#
# TARGET names the binary, the main QML file, the .desktop file, the icons and
# the translations alike; renaming it means renaming all of those.

TARGET = harbour-chronograph

CONFIG += sailfishapp
QT += sensors
# BackgroundActivity, which keeps the phone awake while steps are counted
PKGCONFIG += keepalive

SOURCES += \
    src/main.cpp \
    src/StepCounter.cpp \
    src/StepDetector.cpp

HEADERS += \
    src/StepCounter.h \
    src/StepDetector.h

DISTFILES += \
    qml/harbour-chronograph.qml \
    qml/cover/CoverPage.qml \
    qml/components/AboutLink.qml \
    qml/components/AboutParagraph.qml \
    qml/components/BackgroundImage.qml \
    qml/pages/AboutPage.qml \
    qml/pages/OverviewPage.qml \
    harbour-chronograph.desktop \
    $$files(images/*.svg) \
    rpm/harbour-chronograph.spec \
    rpm/harbour-chronograph.changes \
    translations/*.ts \
    tests/tst_stepdetector.cpp \
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
