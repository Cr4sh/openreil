#!/bin/sh

#
# This shell program joins all static libraries of OpenREIL
# into the single file using ar.
#
# (Yes, I know about MRI scripts, but this way is more reliable)
#

# delete old library file
[ -f libopenreil.a ] && rm libopenreil.a

# create temp dir
[ -d .ar_temp ] ; mkdir .ar_temp
cd .ar_temp

# remove old object files
rm *.o

# unpack required libs
ar -x ../../../VEX/libvex.a
ar -x ../../../capstone/capstone/libcapstone.a
ar -x ../../../libasmir/src/libasmir.a

# create final library file
ar -q ../libopenreil.a *.o ../*.o

# remove temp dir
cd .. && rm -rf .ar_temp

