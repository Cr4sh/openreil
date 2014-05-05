# Owned and copyright BitBlaze, 2007. All rights reserved.
# Do not copy, disclose, or distribute without explicit written
# permission.


#                                               -*- Autoconf -*-
# Process this file with autoconf to produce a configure script.

AC_PREREQ(2.59)
AC_INIT(Bap, 0.7, bap-dev@lists.andrew.cmu.edu)
AC_CONFIG_SRCDIR([libasmir/src/asm_program.c])
AC_CONFIG_HEADER([libasmir/config.h])

AM_INIT_AUTOMAKE

# Checks for programs.
AC_PROG_CXX
AC_PROG_CC
AC_PROG_MAKE_SET
AC_PROG_RANLIB

AM_PATH_PYTHON(, , AC_MSG_ERROR([Python not found]))

AC_CHECK_PROG(CYTHON_FOUND, cython, yes)
if test x"$CYTHON_FOUND" != x"yes" ; then
    AC_MSG_ERROR([Cython not found])
fi

# Add -DAMD64 when needed
if test "$(uname -m)" == "x86_64";
   then export CFLAGS="$CFLAGS -DAMD64";
fi

# Link with the libintl on MinGW systems
MINGW_LIBS=""
case "$(uname -s)" in
  *MINGW* ) MINGW_LIBS="-lintl" ;;
esac

VEX_DIR=`pwd`/'VEX'
VEX_VERSION=2201

# Checks for libraries.
AC_CHECK_LIB([bfd], [bfd_init], , AC_MSG_ERROR([BFD library not found]))
AC_CHECK_LIB([iberty], [xmalloc], , AC_MSG_ERROR([Iberty library not found]))

# zlib is apparently needed by bfd
AC_CHECK_LIB([z], [inflate], , AC_MSG_ERROR([zlib library not found]))

# Python library
AC_CHECK_LIB([python$PYTHON_VERSION], [Py_Initialize], , AC_MSG_ERROR([Python library not found]))

AC_SUBST(VEX_DIR)
AC_SUBST(MINGW_LIBS)
AC_SUBST(INCDIRS)

rm -f libasmir/makefile.inc
echo "VEX_DIR=${VEX_DIR}" >> libasmir/makefile.inc
echo "MINGW_LIBS=${MINGW_LIBS}" >> libasmir/makefile.inc
echo "LIBASMIR_CXXFLAGS=-I${VEX_DIR}/pub -I`pwd`/src/include $CXXFLAGS" >> libasmir/makefile.inc
echo "LIBASMIR_LDFLAGS=-L${VEX_DIR} -L`pwd`/src $LDFLAGS" >> libasmir/makefile.inc
echo "LIBASMIR_LIBS=-lasmir -lvex -lbfd -ldl -liberty -lz ${MINGW_LIBS}" >> libasmir/makefile.inc
echo "CLIBS=asmir vex opcodes bfd z iberty stdc++" >> libasmir/makefile.inc
AC_SUBST(ASMIR_DIR)
ASMIR_DIR="`pwd`/libasmir"

AC_SUBST(BAP_DIR)
BAP_DIR="$( cd "$( dirname "${BASH_SOURCE[0]}" )" && pwd )"

AC_SUBST(OPENREIL_DIR)
OPENREIL_DIR="`pwd`/libopenreil"

rm -rf pyopenreil/src/makefile.inc
echo "MINGW_LIBS=${MINGW_LIBS}" >> pyopenreil/src/makefile.inc

# Checks for header files.
AC_HEADER_STDC
AC_CHECK_HEADERS([fcntl.h stdint.h stdlib.h string.h unistd.h], ,
   AC_MSG_ERROR([Standard headers missing]))
AC_CHECK_HEADER([bfd.h], ,
   AC_MSG_ERROR([You need BFD headers (e.g., libbfd-dev)]))

# Checks for typedefs, structures, and compiler characteristics.
AC_HEADER_STDBOOL
AC_C_CONST
AC_TYPE_SIZE_T
AC_DEFINE_UNQUOTED([VEX_VERSION], [${VEX_VERSION}], [The VEX SVN Version])

AC_CHECK_SIZEOF(bfd_vma, [], [#include <bfd.h>])

# Checks for library functions.
AC_FUNC_MALLOC
AC_FUNC_MEMCMP
AC_CHECK_FUNCS([bzero memset strstr strtoul])


# Check for library function versions

AC_CONFIG_FILES([Makefile
                 libasmir/Makefile
                 libasmir/apps/Makefile
                 libasmir/src/Makefile
                 libopenreil/Makefile
                 libopenreil/src/Makefile
                 libopenreil/apps/Makefile
                 pyopenreil/Makefile])
AC_OUTPUT
