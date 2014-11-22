
RANLIB = @RANLIB@
lib_LIBRARIES = libasmir.a
AM_CFLAGS = -I@VEX_DIR@/pub -I@CAPSTONE_DIR@/include -I../include -fPIC -DGET_OPERANDS
AM_CXXFLAGS = $(AM_CFLAGS)

libasmir_a_SOURCES = \
    ./old_ir/stmt.cpp \
    ./old_ir/exp.cpp \
    disasm-capstone.cpp \
    irtoir.cpp \
    irtoir-i386.cpp \
    irtoir-arm.cpp \
    vexir.c \
    vexmem.c
