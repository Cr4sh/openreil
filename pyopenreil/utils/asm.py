import sys, os, time, subprocess

from pyopenreil import REIL

class CompilerGas(object):    

    default_cmd_binutils = {

        #
        # Default path to as and objcopy of binutils for each architecture.
        # This constants are Windows/Linux only.
        #
        REIL.ARCH_X86: ( 'as', 'objcopy' ),
        REIL.ARCH_ARM: ( 'arm-linux-gnueabi-as', 'arm-linux-gnueabi-objcopy' )
    }

    # 
    # On OS X we using as + otool for all of the architectures.
    #
    default_cmd_mac = ( 'as', 'otool' )

    arch_names = {

        # architecture name for -arch/-march as option
        REIL.ARCH_X86: 'i686', REIL.ARCH_ARM: 'arm'
    }

    code_section = '.text'

    def __init__(self, arch = None, att_syntax = False, thumb = False, \
                       as_path = None, objcopy_path = None, otool_path = None):

        self.arch, self.att_syntax, self.thumb = arch, att_syntax, thumb
        self.is_mac = sys.platform == 'darwin'

        if att_syntax and arch != REIL.ARCH_X86:

            raise Exception('AT&T syntax is valid for x86 only')        

        if thumb and arch != REIL.ARCH_ARM:

            raise Exception('Thumb mode is valid for ARM only')        

        try:

            self.arch_name = self.arch_names[arch]

            if self.is_mac:

                self.as_path, self.otool_path = self.default_cmd_mac
                self.objcopy_path = None

            else:

                # get architecture specific commands in case of binutils
                self.as_path, self.objcopy_path = self.default_cmd_binutils[arch]              
                self.otool_path = None

        except KeyError:

            raise Exception('Unknown architecture')

        self.as_path = self.as_path if as_path is None else as_path  
        self.objcopy_path = self.objcopy_path if objcopy_path is None else objcopy_path  
        self.otool_path = self.otool_path if otool_path is None else otool_path  

        timestamp = int(time.time())
        temp_name = lambda ext: 'tmp_%d.%s' % (timestamp, ext)

        # generate temporary file names
        self.prog_src = temp_name('asm')
        self.prog_bin = temp_name('bin')  
        self.prog_obj = temp_name('o')

    def prog_read_objcopy(self):

        ret = None

        # dump code section into file using objcopy
        code = os.system('%s -O binary -j %s "%s" "%s"' % \
               (self.objcopy_path, self.code_section, self.prog_obj, self.prog_bin))        

        if code != 0: raise OSError('%s error %d' % (self.objcopy_path, code))

        with open(self.prog_bin, 'rb') as fd: 

            ret = fd.read()

        os.unlink(self.prog_bin)

        return ret

    def prog_read_otool(self):

        '''
            Print hex dump of code section contents into stdout and parse it.
            Example of otool -t output:

            $ otool -t tmp_1428708613.o
            tmp_1428708613.o:
            (__TEXT,__text) section
            00000000 0f 85 01 00 00 00 90 c3

        '''
        p = subprocess.Popen(( self.otool_path, '-t', self.prog_obj ), 
                             stdout = subprocess.PIPE, stderr = subprocess.PIPE)

        stdout, stderr = p.communicate()
        
        p.stdout.close()
        p.stderr.close()

        if p.returncode != 0:

            print stdout
            print stderr
            raise OSError('%s error %d' % (self.otool_path, code))

        ret, addr = '', 0
        parse = lambda line, sep: filter(lambda s: len(s) > 0, 
                                         map(lambda s: s.strip(), line.split(sep)))

        for line in parse(stdout, '\n'):

            try:                            

                # parse hexadecimal values sequence
                line = parse(line, ' ') 
                items = map(lambda s: ( long(s, 16), len(s) / 2 ), line)

                for item in line:

                    # each hexadecimal value must be a byte, word or dword
                    if not len(item) in [ 2, 4, 8 ]:

                        raise Exception('Error while parsing otool -t output')

                # first value of each line in otool -t output must be a valid address
                if len(items) < 2 or items[0][0] != addr:

                    if addr != 0:

                        raise Exception('Error while parsing otool -t output')

                    else:

                        raise ValueError()

                for val, size in items[1:]:

                    bv = lambda byte: (val >> (8 * byte)) & 0xff

                    # split words and dwords to bytes
                    ret += ''.join(map(lambda val: chr(val), {

                        1: lambda: ( bv(0), ),
                        2: lambda: ( bv(0), bv(1) ),
                        4: lambda: ( bv(0), bv(1), bv(2), bv(3) )

                    }[size]()))

                    addr += size

            except ValueError: 

                if addr != 0:

                    raise Exception('Error while parsing otool -t output')

        return ret            

    def prog_write(self, data):

        with open(self.prog_src, 'wb') as fd: 

            if self.arch == REIL.ARCH_ARM:

                bits = 16 if self.thumb else 32

                # set ARM/Thumb mode for ARM target
                fd.write('.code %d\n' % bits)
                fd.write('.syntax unified\n')

            elif self.arch == REIL.ARCH_X86:

                syntax = '.att_syntax' if self.att_syntax else '.intel_syntax noprefix'

                # switch to desired mode and syntax for x86 target
                fd.write('.code32\n')
                fd.write(syntax + '\n')
            
            fd.write(data + '\n')

    def compile_file(self, path):

        options = []        

        if self.arch == REIL.ARCH_X86 and not self.is_mac:

            # to avoid "64bit mode not supported on `i686'" error on 64-bit Linux
            options += [ '--32' ]            

        if self.is_mac:

            # on OS X we need to specify target architecture manually
            options += [ '-arch %s' % self.arch_name ]
        
        # generate object file
        code = os.system('%s "%s" -o "%s" %s' % \
               (self.as_path, path, self.prog_obj, ' '.join(options)))

        if code != 0: raise OSError('%s error %d' % (self.as_path, code))

        if self.is_mac:

            # on OS X we need to use otool to dump code section
            ret = self.prog_read_otool()

        else:

            # on other operating systems we using objcopy
            ret = self.prog_read_objcopy() 

        os.unlink(self.prog_obj)

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

        if code != 0: raise OSError('nasm error %d' % code)

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

    def __init__(self, arch, prog, addr = 0, thumb = False):
        
        data = Compiler(arch, thumb = thumb).compile(prog)

        super(Reader, self).__init__(arch, data, addr = addr)

#
# EoF
#
