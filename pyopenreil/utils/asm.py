import sys, os, time

from pyopenreil import REIL

class CompilerGas(object):    

    default_path = {

        #
        # Default path to as and objcopy of binutils for each architecture.
        #
        REIL.ARCH_X86: ( 'as', 'objcopy' ),
        REIL.ARCH_ARM: ( 'arm-linux-gnueabi-as', 'arm-linux-gnueabi-objcopy' )
    }

    code_section = '.text'

    def __init__(self, arch = None, att_syntax = False, thumb = False, \
                       as_path = None, objcopy_path = None):

        self.arch, self.att_syntax, self.thumb = arch, att_syntax, thumb

        if att_syntax and arch != REIL.ARCH_X86:

            raise Exception('AT&T syntax is valid for x86 only')        

        if thumb and arch != REIL.ARCH_ARM:

            raise Exception('Thumb mode is valid for ARM only')        

        try:

            # get architecture specific command names
            self.as_path, self.objcopy_path = self.default_path[arch]    

        except KeyError:

            raise Exception('Unknown architecture')

        self.as_path = self.as_path if as_path is None else as_path  
        self.objcopy_path = self.objcopy_path if objcopy_path is None else objcopy_path  

        timestamp = int(time.time())
        temp_name = lambda ext: 'tmp_%d.%s' % (timestamp, ext)

        # generate temporary file names
        self.prog_src = temp_name('asm')
        self.prog_bin = temp_name('bin')  
        self.prog_obj = temp_name('o')

    def prog_read(self):

        with open(self.prog_bin, 'rb') as fd: return fd.read()

    def prog_write(self, data):

        with open(self.prog_src, 'wb') as fd: 

            if self.arch == REIL.ARCH_ARM:

                bits = 16 if self.thumb else 32

                # set ARM/Thumb mode for ARM target
                fd.write('.code %d\n' % bits)

            elif self.arch == REIL.ARCH_X86:

                syntax = '.att_syntax' if self.att_syntax else '.intel_syntax noprefix'

                # switch to desired mode and syntax for x86 target
                fd.write('.code32\n')
                fd.write(syntax + '\n')
            
            fd.write(data + '\n')

    def compile_file(self, path):
        
        # generate object file
        code = os.system('%s "%s" -o "%s"' % \
               (self.as_path, path, self.prog_obj))        

        if code != 0: raise Exception('"as" error %d' % code)

        # dump contents of the code section
        code = os.system('%s -O binary --only-section=%s "%s" "%s"' % \
               (self.objcopy_path, self.code_section, self.prog_obj, self.prog_bin))        

        if code != 0: raise Exception('"objcopy" error %d' % code)

        # read destination binary contents
        ret = self.prog_read()  
        os.unlink(self.prog_obj)
        os.unlink(self.prog_bin)

        return ret

    def compile(self, prog):

        prog = [ prog ] if isinstance(prog, basestring) else prog

        # write source into the .asm file
        self.prog_write('\n'.join(prog))

        # compile it and dump code section
        ret = self.compile_file(self.prog_src)
        os.unlink(self.prog_src)

        return ret


class CompilerNasm(object):    

    nasm_path = 'nasm'

    def __init__(self, arch = None, nasm_path = None):

        temp_name = lambda ext: 'tmp_%d.%s' % (int(time.time()), ext)

        self.prog_src = temp_name('asm')
        self.prog_dst = temp_name('bin')
        
        self.bits = 32 if arch is None else self.get_bits(arch)
        self.nasm_path = self.nasm_path if nasm_path is None else nasm_path  

    def get_bits(self, arch):

        try: 

            return { REIL.ARCH_X86: 32 }[ arch ]

        except IndexError: 

            raise Exception('Unknown architecture')      

    def prog_read(self):

        with open(self.prog_dst, 'rb') as fd: return fd.read()

    def prog_write(self, data):

        with open(self.prog_src, 'wb') as fd: 

            fd.write('BITS %d\n' % self.bits)
            fd.write(data)

    def compile_file(self, path):
        
        code = os.system('%s %s -o %s' % \
               (self.nasm_path, path, self.prog_dst))        

        if code != 0: raise Exception('nasm error %d' % code)

        # read compiled binary contents
        ret = self.prog_read()
        os.unlink(self.prog_dst)

        return ret

    def compile(self, prog):

        prog = [ prog ] if isinstance(prog, basestring) else prog

        # write source into the .asm file
        self.prog_write('\n'.join(prog))

        # compile it with nasm
        ret = self.compile_file(self.prog_src)
        os.unlink(self.prog_src)

        return ret


class Compiler(CompilerGas):    

    # default asm compiler
    pass


class Reader(REIL.ReaderRaw):

    def __init__(self, arch, prog, addr = 0):
        
        data = Compiler(arch).compile(prog)

        super(Reader, self).__init__(arch, data, addr = addr)

#
# EoF
#
