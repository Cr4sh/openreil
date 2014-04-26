bin_PROGRAMS = \
	print-ir

# these executables should depend on libasmir, since they link it in
# statically.
#print-ir_DEPENDENCIES = ../src/libasmir.a
#lifttrace_DEPENDENCIES = ../src/libasmir.a

print_ir_SOURCES = print-ir.cpp ir_printer.cpp
LDADD = @ASMIR_DIR@/src/libasmir.a @VEX_DIR@/libvex.a -lbfd -ldl -lopcodes
AM_CXXFLAGS = \
              -I../include \
              -I../src/traces \
              -I@VEX_DIR@/pub

