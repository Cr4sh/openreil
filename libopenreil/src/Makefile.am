
RANLIB = @RANLIB@
lib_LIBRARIES = libopenreil.a
LDADD = @MINGW_LIBS@
AM_CFLAGS = -I@VEX_DIR@/pub -I@ASMIR_DIR@/include -I@ASMIR_DIR@/src/traces -I../include -fPIC
AM_CXXFLAGS = $(AM_CFLAGS)

libopenreil_a_SOURCES = \
	libopenreil.cpp \
	reil_translator.cpp

libopenreil_a_LIBADD = @VEX_DIR@/libvex.a @ASMIR_DIR@/src/libasmir.a

