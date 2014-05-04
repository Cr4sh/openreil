from distutils.core import setup
from distutils.extension import Extension
from Cython.Distutils import build_ext

setup(

    cmdclass = { 'build_ext': build_ext },

    ext_modules = [
        Extension('translator', [ 'translator.pyx' ],
                  language = 'c++',
                  libraries = [ 'bfd', 'dl', 'opcodes' ], 
                  include_dirs = [ '../../libopenreil/include' ],                  
                  extra_objects = [ '../../libopenreil/src/libopenreil.a', # import OpenRAIL
                                    '../../libasmir/src/libasmir.a', # ... which based on libasmir
                                    '../../VEX/libvex.a' ]) ] # ... which based on VEX
)
