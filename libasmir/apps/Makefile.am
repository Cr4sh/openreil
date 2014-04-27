bin_PROGRAMS = \
	print-ir \
    translate-insn

print_ir_SOURCES = print-ir.cpp ir_printer.cpp
translate_insn_SOURCES = translate-insn.cpp
LDADD = @ASMIR_DIR@/src/libasmir.a @VEX_DIR@/libvex.a -lbfd -ldl -lopcodes @MINGW_LIBS@
AM_CXXFLAGS = \
              -I../include \
              -I../src/traces \
              -I@VEX_DIR@/pub

