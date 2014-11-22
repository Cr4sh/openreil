# Owned and copyright BitBlaze, 2007. All rights reserved.
# Do not copy, disclose, or distribute without explicit written
# permission.


#                                               -*- Autoconf -*-
# Process this file with autoconf to produce a configure script.

AC_PREREQ(2.59)
AC_INIT(OpenREIL, 0.1, cr4sh0@gmail.com)
AC_CONFIG_SRCDIR([libasmir/src/irtoir.cpp])
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

# Python library
AC_CHECK_LIB([python$PYTHON_VERSION], [Py_Initialize], , AC_MSG_ERROR([Python library not found]))

# Add -DAMD64 when needed
if test "$(uname -m)" == "x86_64";
    then export CFLAGS="$CFLAGS -DAMD64";
fi

AC_SUBST(CAPSTONE_DIR)
CAPSTONE_DIR=`pwd`/'capstone/capstone'

AC_SUBST(UDIS86_DIR)
UDIS86_DIR=`pwd`/'libudis86'

# OS-specific stuff
case "$target_os" in  
*linux-gnu)
    ;;
darwin*)
    ;;
mingw*) 
    LIBS="$LIBS -lintl" 
    ;;
esac

# VEX stuff
AC_SUBST(VEX_DIR)
VEX_DIR=`pwd`/'VEX'
VEX_VERSION=2201

AC_SUBST(INCDIRS)

rm -f libasmir/makefile.inc
echo "VEX_DIR=${VEX_DIR}" >> libasmir/makefile.inc
echo "LIBASMIR_CXXFLAGS=-I${VEX_DIR}/pub -I`pwd`/src/include $CXXFLAGS" >> libasmir/makefile.inc
echo "LIBASMIR_LDFLAGS=-L${VEX_DIR} -L`pwd`/src $LDFLAGS" >> libasmir/makefile.inc
echo "LIBASMIR_LIBS=-lasmir -lvex" >> libasmir/makefile.inc
echo "CLIBS=asmir vex stdc++" >> libasmir/makefile.inc
AC_SUBST(ASMIR_DIR)
ASMIR_DIR="`pwd`/libasmir"

AC_SUBST(BAP_DIR)
BAP_DIR="$( cd "$( dirname "${BASH_SOURCE[0]}" )" && pwd )"

AC_SUBST(OPENREIL_DIR)
OPENREIL_DIR="`pwd`/libopenreil"

rm -f pyopenreil/src/makefile.inc
echo "" >> pyopenreil/src/makefile.inc

# Checks for header files.
AC_HEADER_STDC
AC_CHECK_HEADERS([fcntl.h stdint.h stdlib.h stdio.h string.h unistd.h], ,
   AC_MSG_ERROR([Standard headers missing]))

# Checks for typedefs, structures, and compiler characteristics.
AC_HEADER_STDBOOL
AC_C_CONST
AC_TYPE_SIZE_T
AC_DEFINE_UNQUOTED([VEX_VERSION], [${VEX_VERSION}], [The VEX SVN Version])

# Checks for library functions.
AC_FUNC_MALLOC
AC_FUNC_MEMCMP
AC_CHECK_FUNCS([bzero memset strstr strtoul])

# Check for library function versions
AC_CONFIG_FILES([Makefile
                 libasmir/Makefile
                 libasmir/src/Makefile
                 libopenreil/Makefile
                 libopenreil/src/Makefile
                 libopenreil/apps/Makefile
                 pyopenreil/Makefile])
AC_OUTPUT
