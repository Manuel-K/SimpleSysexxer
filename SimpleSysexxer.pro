TEMPLATE = app
TARGET = simplesysexxer
DEFINES += __LINUX_ALSASEQ__
RESOURCES += binincludes/binincludes.qrc
LIBS += -lasound
QMAKE_CXXFLAGS = -O0 -g3
# QT += sql

QT += widgets

# Uncomment this to get a non-debug binary
CONFIG += qt release


# Input
FORMS += ui/MainWindow.ui
HEADERS += src/MyMainWindow.h src/MidiIn.h src/MidiOut.h
SOURCES += src/main.cpp src/MyMainWindow.cpp src/MidiIn.cpp src/MidiOut.cpp
OBJECTS_DIR = obj
DESTDIR = bin
MOC_DIR = moc
QRC_DIR = src

TRANSLATIONS += binincludes/translations/de_DE.ts binincludes/translations/en_EN.ts binincludes/translations/fr_FR.ts
# translations.path = :/binincludes/translations
# translation.files = :/binincludes/translations/*.qm

# See http://www.sigvdr.de/mediawiki/index.php?title=Vom_QT4-Programm_zum_Debian_Paket
target.path = /usr/local/bin
target.files += bin/simplesysexxer
desktop.path +=  /usr/share/applications
desktop.files += bin/simplesysexxer.desktop
icon.path +=  /usr/share/pixmaps
icon.files += bin/*.png
icon.files += bin/*.xpm
# icon.files += bin/*.svg
INSTALLS += target
INSTALLS += desktop
INSTALLS += icon

