
RANLIB = @RANLIB@
lib_LIBRARIES = libasmir.a
AM_CFLAGS = -I@VEX_DIR@/pub -I@DISASM_INC@ -I../include -fPIC -DGET_OPERANDS
AM_CXXFLAGS = $(AM_CFLAGS)

libasmir_a_SOURCES = \
    stmt.cpp \
    exp.cpp \
    disasm-@DISASM_NAME@.cpp \
    irtoir.cpp \
    irtoir-i386.cpp \
    irtoir-arm.cpp \
    vexir.c \
    vexmem.c
