import sys, os, time

from pyopenreil import REIL

NASM_PATH = 'nasm'

class Compiler(object):    

    def __init__(self, arch = None, path = None):

        temp_name = lambda ext: 'tmp_%d.%s' % (int(time.time()), ext)

        self.prog_src = temp_name('asm')
        self.prog_dst = temp_name('bin')

        self.bits = 32 if arch is None else self.get_bits(arch)
        self.nasm_path = NASM_PATH if path is None else path  

    def get_bits(self, arch):

        try: return { 'x86': 32 }[arch]
        except IndexError: raise Exception('Unknown architecture')      

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


class Reader(REIL.ReaderRaw):

    def __init__(self, arch, prog, addr = 0):
        
        data = Compiler(arch).compile(prog)

        super(Reader, self).__init__(data, addr = addr)

#
# EoF
#
