TRANSLATIONS = translations/en_US.ts \
               translations/de_DE.ts

# Note: include after the TRANSLATIONS definition
include(../../plugins.pri)

TARGET = $$qtLibraryTarget(guh_devicepluginelgato)

SOURCES += \
    devicepluginelgato.cpp \
    aveabulb.cpp \
    commandrequest.cpp

HEADERS += \
    devicepluginelgato.h \
    aveabulb.h \
    commandrequest.h



