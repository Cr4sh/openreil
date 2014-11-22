
RANLIB = @RANLIB@
lib_LIBRARIES = libopenreil.a
AM_CFLAGS = -I@VEX_DIR@/pub -I@CAPSTONE_DIR@/include -I@ASMIR_DIR@/include -I@ASMIR_DIR@/src/traces -I../include -fPIC
AM_CXXFLAGS = $(AM_CFLAGS)

libopenreil_a_SOURCES = \
    libopenreil.cpp \
    reil_translator.cpp
