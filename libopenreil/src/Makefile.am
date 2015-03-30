
RANLIB = @RANLIB@

lib_LIBRARIES = libopenreil.a

AM_CFLAGS = -I@VEX_DIR@/pub -I@DISASM_INC@ -I@ASMIR_DIR@/include -I../include -fPIC

AM_CXXFLAGS = $(AM_CFLAGS)

libopenreil_a_SOURCES = \
    libopenreil.cpp \
    reil_translator.cpp

libopenreil.a: $(libopenreil_a_OBJECTS) @VEX_DIR@/libvex.a @ASMIR_DIR@/src/libasmir.a
	./makelib.sh

