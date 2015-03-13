<style>m { font-family: Courier New, Courier, monospace; }</style>

## Installation

### Linux and OS X

To build OpenREIL under *nix operating systems you need to install gcc, nasm, make, Python 2.x with header files, [NumPy](https://pypi.python.org/pypi/numpy) and [Cython](http://cython.org/). After that you can run configure and make just as usual. 

Example for Debian:

```
$ sudo apt-get install gcc make nasm python python-dev python-numpy cython
$ ./configure
$ make
$ make test
$ sudo make install
```

If you planning to use [MongoDB](http://www.mongodb.org/) as IR code storage you need to install some additional dependencies:

```
$ sudo apt-get install mongodb python-pymongo python-setuptools
$ sudo easy_install cachetools
```

OpenREIL can be used not only for debugger/diassembler plugins, but also for standalone code analysis tools. For loading of executable files it uses pybfd (ELF, Mach-O) and pefile (Portable Execute) Python libraries:

```
$ sudo easy_install pefile pybfd
```


### Windows

Building OpenREIL on Windows requires [MinGW](http://www.mingw.org/) build environment and all of the dependencies that was mnetioned above.


## IR format

### Available instructions

REIL language traditionally uses three address form of IR instruction where arguments a and b are source, and c is destination. 

Example of IR code for `mov eax, ecx` x86 instruction:

```
;
; asm: mov eax, ecx
; data (2): 89 c8
;
00000000.00     STR         R_ECX:32,                 ,          V_00:32
00000000.01     STR          V_00:32,                 ,         R_EAX:32
```


OpenREIL offers 23 different instructions:

| Mnemonic    | Pseudocode            | Description                 |
|-------------|-----------------------|-----------------------------|
| <m>STR</m>  | <m>c = a</m>          | Store to register           |
| <m>STM</m>  | <m>\*c = a</m>        | Store to memory             |
| <m>LDM</m>  | <m>c = \*a</m>        | Load from memory            |
| <m>ADD</m>  | <m>c = a + b</m>      | Addition                    |
| <m>SUB</m>  | <m>c = a - b</m>      | Substraction                |
| <m>NEG</m>  | <m>c = -a</m>         | Negation                    |
| <m>MUL</m>  | <m>c = a \* b</m>     | Multiplication              |
| <m>DIV</m>  | <m>c = a / b</m>      | Division                    |
| <m>MOD</m>  | <m>c = a % b</m>      | Modulus                     |
| <m>SHL</m>  | <m>c = a << b</m>     | Shift left                  |
| <m>SHR</m>  | <m>c = a >> b</m>     | Shift right                 |
| <m>AND</m>  | <m>c = a & b</m>      | Binary AND                  |
| <m>OR</m>   | <m>c = a &#124; b</m> | Binary OR                   |
| <m>XOR</m>  | <m>c = a ^ b</m>      | Binary XOR                  |
| <m>NOT</m>  | <m>c = ~a</m>         | Binary NOT                  |
| <m>EQ</m>   | <m>c = a == b</m>     | Equation                    |
| <m>LT</m>   | <m>c = a < b</m>      | Less than                   |
| <m>SMUL</m> | <m>c = a \* b</m>     | Signed multiplication       |
| <m>SDIV</m> | <m>c = a / b</m>      | Signed division             |
| <m>SMOD</m> | <m>c = a % b</m>      | Signed modulus              |
| <m>JCC</m>  |                       | Jump to c if a is not zero  |
| <m>NONE</m> |                       | No operation                |
| <m>UNK</m>  |                       | Untranslated instruction    |

          
Each instruction argument can have 1, 8, 16, 32 or 64 bits of length\. Most of arithmetic instructions operates with unsigned arguments\. <m>SMUL</m>, <m>SDIV</m> and <m>SMOD</m> instructions operates with signed arguments in [two’s complement](http://en.wikipedia.org/wiki/Two%27s_complement) signed number representation form\.

Most of instructions supposes the same size of their source and destination arguments, but there’s a several exceptions:

   * Result (argument c) of <m>EQ</m> and <m>LT</m> instructions always has 1 bit size.
   * Argument a of <m>STM</m> and argument c of <m>LDM</m> must have 8 bits or gather size (obviously, there is no memory read/write operations with single bit values).
   * Result of <m>OR</m> instruction can have any size. It may be less, gather or equal than size of it’s input arguments. 

OpenREIL uses <m>OR</m> operation for converting to value of different size. For example, converting value of 8 bit argument <m>V_01</m> to 1 bit argument <m>V_02</m> (1-7 bits of <m>V_01</m> will be ignored):

```
00000000.00      OR           V_01:8,              0:8,           V_02:1
```

Converting 1 bit argument <m>V_02</m> to 32 bit argument <m>V_03</m> (1-31 bits of <m>V_03</m> will be zero extended):

```
00000000.00      OR           V_02:1,              0:1,          V_03:32
```

Instruction argument can have following type:

   * <m>A_REG</m> - CPU register (example: <m>R_EAX:32</m>, <m>R_ZF:1</m>).
   * <m>A_TEMP</m> - temporary register (example: <m>V_01:8</m>, <m>V_02:32</m>).
   * <m>A_CONST</m> - constant value (example: <m>0:1</m>, <m>fffffff4:32</m>).
   * <m>A_NONE</m> - argument is not used by instruction.

Address of each IR instruction consists from two parts: original address of translated machine instruction and IR instruction logical number (inum). First IR instruction of each machine instruction always has inum with value 0. 

Group of IR instructions that represent one machine instruction can set value of some temporary register only once, so, <m>A_TEMP</m> arguments are single state assignment within the confines of machine instruction.


### Optional instruction flags

In addition to address, operation code and arguments IR instruction also has flags which used to store useful metainformation:

   * <m>IOPT_CALL</m> - this JCC instruction represents a function call.
   * <m>IOPT_RET</m> - this JCC instruction represents a function exit.
   * <m>IOPT_ASM_END</m> - last IR instruction of machine instruction.
   * <m>IOPT_BB_END</m> - last IR instruction of basic block.
   * <m>IOPT_ELIMINATED</m> - whole machine instruction was replaced with NONE IR instruction during dead code elimination.


### Representation of x86 registers

IR code for x86 architecture operates only with 32-bit length general purpose CPU registers (<m>R_EAX:32</m>, <m>R_ECX:32</m>, etc.), OpenREIL has no half-sized registers (<m>AX</m>, <m>AL</m>, <m>AH</m>, etc.), translator converts all machine instructions that operates with such registers to full length form.

Example of IR code for `mov ah, al` x86 instruction:

```
;
; asm: mov ah, al
; data (2): 88 c4
;
00000000.00     AND         R_EAX:32,             ff:8,           V_00:8
00000000.01     AND         R_EAX:32,      ffff00ff:32,          V_01:32
00000000.02      OR           V_00:8,             0:32,          V_02:32
00000000.03     SHL          V_02:32,             8:32,          V_03:32
00000000.04      OR          V_01:32,          V_03:32,         R_EAX:32
```


### Representation of x86 EFLAGS

OpenREIL uses separate 1-bit registers to represent <m>ZF</m>, <m>PF</m>, <m>CF</m>, <m>AF</m>, <m>SF</m> and <m>OF</m> bits of <m>EFLAGS</m> register.

Example of IR code for `test eax, eax` x86 instruction which sets some of these bits:

```
;
; asm: test eax, eax
; data (2): 85 c0
;
00000000.00     STR         R_EAX:32,                 ,          V_00:32
00000000.01     STR              0:1,                 ,           R_CF:1
00000000.02     AND          V_00:32,             ff:8,           V_01:8
00000000.03     SHR           V_01:8,              7:8,           V_02:8
00000000.04     SHR           V_01:8,              6:8,           V_03:8
00000000.05     XOR           V_02:8,           V_03:8,           V_04:8
00000000.06     SHR           V_01:8,              5:8,           V_05:8
00000000.07     SHR           V_01:8,              4:8,           V_06:8
00000000.08     XOR           V_05:8,           V_06:8,           V_07:8
00000000.09     XOR           V_04:8,           V_07:8,           V_08:8
00000000.0a     SHR           V_01:8,              3:8,           V_09:8
00000000.0b     SHR           V_01:8,              2:8,           V_10:8
00000000.0c     XOR           V_09:8,           V_10:8,           V_11:8
00000000.0d     SHR           V_01:8,              1:8,           V_12:8
00000000.0e     XOR           V_12:8,           V_01:8,           V_13:8
00000000.0f     XOR           V_11:8,           V_13:8,           V_14:8
00000000.10     XOR           V_08:8,           V_14:8,           V_15:8
00000000.11     AND           V_15:8,              1:1,           V_16:1
00000000.12     NOT           V_16:1,                 ,           R_PF:1
00000000.13     STR              0:1,                 ,           R_AF:1
00000000.14      EQ          V_00:32,             0:32,           R_ZF:1
00000000.15     SHR          V_00:32,            1f:32,          V_17:32
00000000.16     AND             1:32,          V_17:32,          V_18:32
00000000.17      EQ             1:32,          V_18:32,           R_SF:1
00000000.18     STR              0:1,                 ,           R_OF:1
```


For machine instructions which operates with whole <m>EFLAGS</m> value (pushfd, etc.) OpenREIL composes it's value from values of 1-bit registers:

Example of IR code for pushfd x86 instruction:

```
;
; asm: pushfd
; data (1): 9c
;
00000000.00     STR         R_ESP:32,                 ,          V_00:32
00000000.01     SUB          V_00:32,             4:32,          V_01:32
00000000.02     STR          V_01:32,                 ,         R_ESP:32
00000000.03     AND      fffffffe:32,      fffffffb:32,          V_02:32
00000000.04     AND      R_EFLAGS:32,          V_02:32,          V_03:32
00000000.05     AND      ffffffbf:32,      ffffff7f:32,          V_04:32
00000000.06     AND      ffffffef:32,          V_04:32,          V_05:32
00000000.07     AND          V_05:32,      fffff7ff:32,          V_06:32
00000000.08     AND          V_03:32,          V_06:32,      R_EFLAGS:32
00000000.09      OR           R_CF:1,             0:32,          V_07:32
00000000.0a     SHL          V_07:32,             0:32,          V_08:32
00000000.0b      OR           R_PF:1,             0:32,          V_09:32
00000000.0c     SHL          V_09:32,             2:32,          V_10:32
00000000.0d      OR          V_08:32,          V_10:32,          V_11:32
00000000.0e      OR      R_EFLAGS:32,          V_11:32,          V_12:32
00000000.0f      OR           R_AF:1,             0:32,          V_13:32
00000000.10     SHL          V_13:32,             4:32,          V_14:32
00000000.11      OR           R_ZF:1,             0:32,          V_15:32
00000000.12     SHL          V_15:32,             6:32,          V_16:32
00000000.13      OR           R_SF:1,             0:32,          V_17:32
00000000.14     SHL          V_17:32,             7:32,          V_18:32
00000000.15      OR          V_16:32,          V_18:32,          V_19:32
00000000.16      OR          V_14:32,          V_19:32,          V_20:32
00000000.17      OR           R_OF:1,             0:32,          V_21:32
00000000.18     SHL          V_21:32,             b:32,          V_22:32
00000000.19      OR          V_20:32,          V_22:32,          V_23:32
00000000.1a      OR          V_12:32,          V_23:32,      R_EFLAGS:32
00000000.1b     STR      R_EFLAGS:32,                 ,          V_24:32
00000000.1c      OR          V_24:32,           202:32,          V_25:32
00000000.1d     STR       R_DFLAG:32,                 ,          V_26:32
00000000.1e     AND          V_26:32,           400:32,          V_27:32
00000000.1f      OR          V_25:32,          V_27:32,          V_28:32
00000000.20     STR      R_IDFLAG:32,                 ,          V_29:32
00000000.21      OR             15:8,             0:32,          V_30:32
00000000.22     SHL          V_29:32,          V_30:32,          V_31:32
00000000.23     AND          V_31:32,        200000:32,          V_32:32
00000000.24      OR          V_28:32,          V_32:32,          V_33:32
00000000.25     STR      R_ACFLAG:32,                 ,          V_34:32
00000000.26      OR             12:8,             0:32,          V_35:32
00000000.27     SHL          V_34:32,          V_35:32,          V_36:32
00000000.28     AND          V_36:32,         40000:32,          V_37:32
00000000.29      OR          V_33:32,          V_37:32,          V_38:32
00000000.2a     STM          V_38:32,                 ,          V_01:32
```


### Representation of untranslated instructions

## C API

OpenREIL C API is declared in [reil_ir.h]() (IR format) and [libopenreil.h]() (translator API) header files. 

Here is an example of the test program that translates machine code to IR instructions and prints them to stdout:

```cpp
#include <libopenreil.h>

int inst_handler(reil_inst_t *inst, void *context)
{
    // increment IR instruction counter
    *(int *)context += 1;

    // print IR instruction to the stdout
    reil_inst_print(inst);
     
    return 0;
}

int translate_inst(reil_arch_t arch, const unsigned char *data, int len)
{    
    int translated = 0;

    // initialize REIL translator 
    reil_t reil = reil_init(arch, inst_handler, (void *)&translated);
    if (reil)
    {
        // translate single instructions to REIL IR
        reil_translate(reil, 0, data, len);
        reil_close(reil);
        return translated;
    }

    return -1;
}
```


## Python API

### Low level translation API

OpenREIL has low level translation API that returns IR instructions as Python tupple. Here is the description of using this API to decode `push eax` x86 instruction:

```python
from pyopenreil import translator
from pyopenreil.REIL import ARCH_X86

# create REIL translator instance
tr = translator.Translator(ARCH_X86)

# translate machine instruction to REIL IR
insn_lst = tr.to_reil('\x50', addr = 0)

for insn in insn_lst:

    # print single IR instruction
    print insn
    
```


Program output:

```
((0L, 1), 0, 3, ((1, 3, 'R_EAX'), (), (2, 3, 'V_00')), {0: ('push', 'eax'), 1: ‘\x50'})
((0L, 1), 1, 3, ((1, 3, 'R_ESP'), (), (2, 3, 'V_01')), {})
((0L, 1), 2, 7, ((2, 3, 'V_01'), (3, 3, 4L), (2, 3, 'V_02')), {})
((0L, 1), 3, 3, ((2, 3, 'V_02'), (), (1, 3, 'R_ESP')), {})
((0L, 1), 4, 4, ((2, 3, 'V_00'), (), (2, 3, 'V_02')), {2: 8L})
```


pyopenreil.translator module is written in Cython, it’s stands for bridge between C API and high level Python API of OpenREIL. Also, OpenREIL uses JSON representation of these tupples to store translated instruction into the file or Mongo DB collection.

IR constants (operation codes, argument types, etc.) are declared in <m>pyopenreil.IR</m> module.


### High level Python API

High level Python API of OpenREIL has abstractions for IR instructions and arguments, translator, machine instructions reader, IR instructions storage:

<img src="https://dl.dropboxusercontent.com/u/22903093/openreil/python_api.png" alt="OpenREIL Python API diagram" width="465" height="405">

The most important modules:

   * <m>pyopenreil.IR</m> - IR constants.
   * <m>pyopenreil.REIL</m> - translation and analysis API.
   * <m>pyopenreil.symbolic</m> - represents IR as symbolic expressions.
   * <m>pyopenreil.VM</m> - IR emulation.
   * <m>pyopenreil.utils.asm</m> - instruction reader for x86 assembly language.
   * <m>pyopenreil.utils.bin_PE</m> - instruction reader for PE binaries.
   * <m>pyopenreil.utils.bin_BFD</m> - instruction reader for ELF and Mach-O binaries.
   * <m>pyopenreil.utils.kd</m> - instruction reader that uses pykd API.
   * <m>pyopenreil.utils.GDB</m> - instruction reader that uses GDB Python API.
   * <m>pyopenreil.utils.IDA</m> - instruction reader that uses IDA Pro Python API.
   * <m>pyopenreil.utils.mongodb</m> - instruction storage that uses Mongo DB.

Usage example:

```python
from pyopenreil.REIL import *

# create in-memory storage for IR instructions
storage = CodeStorageMem(ARCH_X86)

# initialize raw instruction reader
reader = ReaderRaw('\x50', addr = 0)

# create translator instance
tr = CodeStorageTranslator(ARCH_X86, reader, storage)

# translate single machine instruction
insn_list = tr.get_insn(0)

# enumerate and print machine instruction IR instructions
for insn in insn_list:

    print insn

```

After machine instruction was translated you can fetch it’s IR instructions from the storage:

```python
insn_list = storage.get_insn(0)
```

It’s also possible to get a single IR instruction:

```python
# get 2-nd IR instruction of machine instruction at address 0
insn = storage.get_insn(( 0, 1 ))
```

Copying contents of one storage into another:

```python
# src -> dst
storage_2 = CodeStorageMem(ARCH_X86)
storage.to_storage(storage_2)

# dst <- src
storage_3 = CodeStorageMem(ARCH_X86)
storage_3.from_storage(storage_2)
```


In memory storage contents also can be saved into the JSON file:

```python
storage.to_file('test.json')
```


Loading storage contents from JSON file:

```python
# using new instance
storage = CodeStorageMem(ARCH_X86, from_file = 'test.json')

# using existing instance
storage.from_file('test.json')
```


Instead of raw instruction reader you also can use x86 assembly instruction reader which based on nasm. OpenREIL uses this reader in unit tests, it also might be useful for other purposes:

```python
from pyopenreil.utils import asm

# create reader instance
reader = asm.Reader(ARCH_X86, ( 'push eax' ), addr = 0)
```

### MongoDB as IR code storage

Using of <m>CodeStorageMem()</m> class as IR code storage has one significant disadvantage: real programs are very complex nowadays, so you may need to analyze amount of code that is too big to fit into the RAM. 

To solve scalability problem I implemented other storage class that uses MongoDB collections to store translated code, example of it's usage:

```python
from pyopenreil.utils import mongodb

# create storage instance
storage = mongodb.CodeStorageMongo(ARCH_X86,
          collection = 'test_fib', db = 'openreil',
          host = 'localhost', port = 27017) 

# clear old contents of collection
storage.clear()
```

Here is an example of MongoDB collection record format which is used by OpenREIL:

```
{ 
  "_id" : ObjectId("54e2a2fad87fe0462f000000"), 
  "addr" : NumberLong(1094795585), 
  "size" : 1
  "inum" : 0, 
  "op" : 3,
  "a" : [ 1, 3, "V_00" ], 
  "b" : [ ], 
  "c" : [ 2, 3, "V_01" ],  
  "attr" : { 2 : 0 } 
}
```

This record stores the following IR instruction:

```
41414141.00     STR          V_00:32,                 ,          V_01:32
```

To improove storage performance you also can join a several instances of MongoDB into the single cluster.

### Translating of basic blocks and functions

<m>CodeStorageTranslator</m> class of <m>pyopenreil.REIL</m> is also can do code translation at basic blocks or functions level. Example of basic block translation:

```python
# initialize raw instruction reader
reader = ReaderRaw('\x33\xC0\xC3', addr = 0)

# Create translator instance.
# We are not passing storage instance, translator will create
# instance of CodeStorageMem automatically in this case.
tr = CodeStorageTranslator(ARCH_X86, reader)

# translate single basic block
bb = tr.get_bb(0)

# print basic block information
print 'addr = %s, size = %d bytes' % (bb.ir_addr, bb.size)

# print address of the first and last instruction
print 'first = 0x%x, last = 0x%x' % (bb.first.addr, bb.last.addr)
```


Example of function translation:

```python
# translate whole function
func = tr.get_func(0)

# print function information
print 'function at 0x%x, stack args count: %d' % (func.addr, func.stack_args)

# enumerate function code chunks
nun = 0
for ch in func.chunks:

    # print chunk information
    print 'chunk %d: addr = 0x%x, size = %d' % (ch.addr, ch.size)
    num += 1

# enumerate function basic blocks
for bb in func.bb_list:

    # print basic block information
    print bb

```

### Symbolic expressions

OpenREIL can convert basic blocks and instructions into the symbolic form, it might be useful for such tasks like symbolic execution, intercating with SMT solver, decompilation and analysis of code semantics.

Let's write a program that translates assembly function into IR and prints a set of symbolic expressions for it's first basic block:

```python
from pyopenreil.REIL import *
from pyopenreil.utils import asm

# create assembly instruction reader
reader = asm.Reader(ARCH_X86, ( 'push ebp',
                                'mov ebp, esp',
                                'xor eax, eax',
                                'test ecx, ecx', 
                                'jz _quit',
                                'inc eax',
                                '_quit: leave',
                                'ret'), addr = 0)

# create translator instance
tr = CodeStorageTranslator(ARCH_X86, reader)

# translate first basic block
bb = tr.get_bb(0)

# Get symbolic representation of basic block.
# False value of temp_regs argument indicates that
# we don't need expressions for REIL temp registers.
sym = bb.to_symbolic(temp_regs = False)

# print SymState object
print sym
```

Program output (symbolic represenation of first basic block which contains first 5 machine instructions of the test code):
```
R_ESP = (R_ESP - 0x4)
*(R_ESP - 0x4) = R_EBP
R_EBP = (R_ESP - 0x4)
R_EAX = 0x0
R_CF = 0x0
R_PF = ~((((((R_ECX & 0xff) >> 0x7) ^ ((R_ECX & 0xff) >> 0x6)) ^ (((R_ECX & 0xff) >> 0x5) ^ ((R_ECX & 0xff) >> 0x4))) ^ ((((R_ECX & 0xff) >> 0x3) ^ ((R_ECX & 0xff) >> 0x2)) ^ (((R_ECX & 0xff) >> 0x1) ^ (R_ECX & 0xff)))) & 0x1)
R_AF = 0x0
R_ZF = (R_ECX == 0x0)
R_SF = (0x1 == (0x1 & (R_ECX >> 0x1f)))
R_OF = 0x0
@IP = ((((R_ECX == 0x0) | 0x0) & 0x1)) ? 0xa : 0x9
```

Symbolic expressions are implemented in <m>pyopenreil.symbolic</m> module; <m>symbolic.Sym</m> is a base class for other classes that represents expression members: <m>symbolic.SymVal</m>, <m>symbolic.SymConst</m>, <m>symbolic.SymExp</m>, <m>symbolic.SymCond</m>, <m>symbolic.SymPtr</m> and others.

You can get individual expressions from symbolic representation:

```python
from pyopenreil.symbolic import *

# get expression for ZF register value
exp_zf = sym.query(SymVal('R_ZF'))

# prints (R_ECX == 0x0)
print exp_zf
```

Getting expression that represents instruction pointer value at the end of the basic block:

```python
# get expression for EIP register value
exp_eip = sym.query(SymIP())

# prints ((((R_ECX == 0x0) | 0x0) & 0x1)) ? 0xa : 0x9
print exp_ip
```

You also can compare expressions, OpenREIL will care about commutativity and other important things. To match any valid expression <m>symbolic.SymAny</m> object is used:

```python
# compare expressions
assert exp_zf == SymExp(I_EQ, SymVal('R_ECX'), SymConst(0, U32))
assert exp_zf == SymExp(I_EQ, SymVal('R_ECX'), SymAny())
assert exp_zf == SymAny()
```

### Control flow graphs

OpenREIL can build [control flow graph](http://en.wikipedia.org/wiki/Control_flow_graph) (CFG) of IR code. Let's write some test program in C to demonstrate it:

```cpp
#include <stdio.h>
#include <stdlib.h>

// computes a member of fibbonnaci sequence at given position
int fib(int n)
{
    int result = 1, prev = 0;

    while (n > 0)
    {
        int current = result;

        result += prev;
        prev = current;

        n -= 1;
    }

    return result;
}

int main(int argc, char *argv[])
{
    int n = 0, val = 0;

    if (argc < 2)
    {
        // input argument was not specified
        return -1;
    }

    n = atoi(argv[1]);
    
    // call test function
    return fib(n);
}
```

Now compile it with GCC and get virtual address of <m>fib()</m> function using objdump:

```
$ gcc tests/fib.c -o tests/fib
$ objdump -t tests/fib | grep '.text'
08048360 l    d  .text  00000000              .text
08048390 l     F .text  00000000              __do_global_dtors_aux
080483f0 l     F .text  00000000              frame_dummy
08048540 l     F .text  00000000              __do_global_ctors_aux
08048530 g     F .text  00000002              __libc_csu_fini
08048532 g     F .text  00000000              .hidden __i686.get_pc_thunk.bx
080484c0 g     F .text  00000061              __libc_csu_init
08048360 g     F .text  00000000              _start
0804844b g     F .text  0000006e              main
08048414 g     F .text  00000037              fib
```

OpenREIL allows to work with CFG's using <m>REIL.CFGraph</m>, <m>REIL.CFGraphNode</m> and <m>REIL.CFGraphEdge</m> objects; <m>REIL.CFGraphBuilder</m> object accepts translator instance and performs CFG traversing starting from specified address and untill the end of the current machine code function. 

Let's build control flow graph of <m>fib()</m> function (that located at address <m>0x08048414</m>) of <m>tests/fib</m> executable:

```python
from pyopenreil.REIL import *
from pyopenreil.utils import bin_BFD

# create executable image reader 
reader = bin_BFD.Reader('tests/fib')

# VA of the fib() function inside fib executable
addr = 0x08048414

# create translator instance
tr = CodeStorageTranslator(ARCH_X86, reader)

# build control flow graph
cfg = CFGraphBuilder(tr).traverse(addr)
```

You can save generated graph as file of [Graphviz DOT format](http://www.graphviz.org/content/dot-language):

```python
cfg.to_dot_file('cfg.dot')
```

It's easy to render this file into the PNG image using Graphviz dot utility:

```
$ dot -Tpng cfg.dot > cfg.png
```

Example of the <m>fib()</m> function CFG:

<img src="https://dl.dropboxusercontent.com/u/22903093/openreil/cfg_1.png" alt="OpenREIL Python API diagram" width="181" height="266">

Please note, that generated IR code may have more complicated CFG layout than it's source machine code (because of x86 instructions with REP preffix, for example). Here is IR code for function that consists from `rep movsb` and `ret` instructions:

```
;
; asm: rep movsb byte ptr es:[edi], byte ptr [esi]
; data (2): f3 a4
;
00001337.00     STR         R_ECX:32,                 ,          V_00:32
00001337.01      EQ          V_00:32,             0:32,           V_01:1
00001337.02     JCC           V_01:1,                 ,          1339:32
00001337.03     SUB          V_00:32,             1:32,          V_02:32
00001337.04     STR          V_02:32,                 ,         R_ECX:32
00001337.05     STR       R_DFLAG:32,                 ,          V_03:32
00001337.06     STR         R_EDI:32,                 ,          V_04:32
00001337.07     STR         R_ESI:32,                 ,          V_05:32
00001337.08     LDM          V_05:32,                 ,           V_06:8
00001337.09     STM           V_06:8,                 ,          V_04:32
00001337.0a     ADD          V_04:32,          V_03:32,          V_07:32
00001337.0b     STR          V_07:32,                 ,         R_EDI:32
00001337.0c     ADD          V_05:32,          V_03:32,          V_08:32
00001337.0d     STR          V_08:32,                 ,         R_ESI:32
00001337.0e     JCC              1:1,                 ,          1337:32
;
; asm: ret
; data (1): c3
;
00001339.00     STR         R_ESP:32,                 ,          V_00:32
00001339.01     LDM          V_00:32,                 ,          V_01:32
00001339.02     ADD          V_00:32,             4:32,          V_02:32
00001339.03     STR          V_02:32,                 ,         R_ESP:32
00001339.04     JCC              1:1,                 ,          V_01:32
```

As you can see, this IR code has branch instructions at <m>1337.02</m>  and <m>1337.0e</m>, so, it's DFG has 3 nodes:

<img src="https://dl.dropboxusercontent.com/u/22903093/openreil/cfg_2.png" alt="OpenREIL Python API diagram" width="231" height="96">

### Data flow graphs

As example, let's take a simple function that returns sum of two arguments:

```cpp
long __fastcall sum(long a, long b)
{
    // a comes in ECX, b in EDX
    return a + b;
}
```

Assembly code of the function:

```
_sum:

mov     eax, ecx
add     eax, edx
ret
```

Translated IR code:

```
;
; asm: mov eax, ecx
; data (2): 89 c8
;
00000000.00     STR         R_ECX:32,                 ,          V_00:32
00000000.01     STR          V_00:32,                 ,         R_EAX:32
;
; asm: add eax, edx
; data (2): 01 d0
;
00000002.00     STR         R_EAX:32,                 ,          V_00:32
00000002.01     STR         R_EDX:32,                 ,          V_01:32
00000002.02     ADD          V_00:32,          V_01:32,          V_02:32
00000002.03     STR          V_02:32,                 ,         R_EAX:32
00000002.04     ADD          V_00:32,          V_01:32,          V_03:32
00000002.05      LT          V_03:32,          V_00:32,           R_CF:1
00000002.06     AND          V_03:32,             ff:8,           V_04:8
00000002.07     SHR           V_04:8,              7:8,           V_05:8
00000002.08     SHR           V_04:8,              6:8,           V_06:8
00000002.09     XOR           V_05:8,           V_06:8,           V_07:8
00000002.0a     SHR           V_04:8,              5:8,           V_08:8
00000002.0b     SHR           V_04:8,              4:8,           V_09:8
00000002.0c     XOR           V_08:8,           V_09:8,           V_10:8
00000002.0d     XOR           V_07:8,           V_10:8,           V_11:8
00000002.0e     SHR           V_04:8,              3:8,           V_12:8
00000002.0f     SHR           V_04:8,              2:8,           V_13:8
00000002.10     XOR           V_12:8,           V_13:8,           V_14:8
00000002.11     SHR           V_04:8,              1:8,           V_15:8
00000002.12     XOR           V_15:8,           V_04:8,           V_16:8
00000002.13     XOR           V_14:8,           V_16:8,           V_17:8
00000002.14     XOR           V_11:8,           V_17:8,           V_18:8
00000002.15     AND           V_18:8,              1:1,           V_19:1
00000002.16     NOT           V_19:1,                 ,           R_PF:1
00000002.17     XOR          V_00:32,          V_01:32,          V_20:32
00000002.18     XOR          V_03:32,          V_20:32,          V_21:32
00000002.19     AND            10:32,          V_21:32,          V_22:32
00000002.1a      EQ             1:32,          V_22:32,           R_AF:1
00000002.1b      EQ          V_03:32,             0:32,           R_ZF:1
00000002.1c     SHR          V_03:32,            1f:32,          V_23:32
00000002.1d     AND             1:32,          V_23:32,          V_24:32
00000002.1e      EQ             1:32,          V_24:32,           R_SF:1
00000002.1f     XOR          V_01:32,      ffffffff:32,          V_25:32
00000002.20     XOR          V_00:32,          V_25:32,          V_26:32
00000002.21     XOR          V_00:32,          V_03:32,          V_27:32
00000002.22     AND          V_26:32,          V_27:32,          V_28:32
00000002.23     SHR          V_28:32,            1f:32,          V_29:32
00000002.24     AND             1:32,          V_29:32,          V_30:32
00000002.25      EQ             1:32,          V_30:32,           R_OF:1
;
; asm: ret
; data (1): c3
;
00000004.00     STR         R_ESP:32,                 ,          V_00:32
00000004.01     LDM          V_00:32,                 ,          V_01:32
00000004.02     ADD          V_00:32,             4:32,          V_02:32
00000004.03     STR          V_02:32,                 ,         R_ESP:32
00000004.04     JCC              1:1,                 ,          V_01:32
```

Now let's construct DFG of this function:

```python
from pyopenreil.REIL import *
from pyopenreil.utils import asm

# create assembly instruction reader
reader = asm.Reader(ARCH_X86, ( 'mov eax, ecx',
                                'add eax, edx', 
                                'ret' ), addr = 0)

# create translator instance
tr = CodeStorageTranslator(ARCH_X86, reader)

# build data flow graph
dfg = DFGraphBuilder(tr).traverse(0)
```

<m>REIL.DFGraph</m> allows to apply some basic data flow code optimizations to translated IR code, currently it supports such well known compiler optimizations as [dead code elimination](http://en.wikipedia.org/wiki/Dead_code_elimination) and [constant folding](http://en.wikipedia.org/wiki/Constant_folding). 

Let's apply these optimizations to <m>fib()</m> function code:

```python
# run some of available optimizations
dfg.eliminate_dead_code()
dfg.constant_folding()

# store results
dfg.store(tr.storage)

# print optimized IR code
print tr.storage.get_func(0)
```

Optimized IR code:

```
;
; asm: mov eax, ecx
; data (2): 89 c8
;
00000000.00     STR         R_ECX:32,                 ,          V_00:32
00000000.01     STR          V_00:32,                 ,         R_EAX:32
;
; asm: add eax, edx
; data (2): 01 d0
;
00000002.00     STR         R_EAX:32,                 ,          V_00:32
00000002.01     STR         R_EDX:32,                 ,          V_01:32
00000002.02     ADD          V_00:32,          V_01:32,          V_02:32
00000002.03     STR          V_02:32,                 ,         R_EAX:32
;
; asm: ret
; data (1): c3
;
00000004.00     STR         R_ESP:32,                 ,          V_00:32
00000004.01     LDM          V_00:32,                 ,          V_01:32
00000004.02     ADD          V_00:32,             4:32,          V_02:32
00000004.03     STR          V_02:32,                 ,         R_ESP:32
00000004.04     JCC              1:1,                 ,          V_01:32
```

As you can see, all x86 flags computations was removed during optimization, because dead code elimination assumes that these values will not be used after the function exit.

Let's export and view DFG of achieved code:

```python
dfg.to_dot_file('dfg.dot')
```

<img src="https://dl.dropboxusercontent.com/u/22903093/openreil/dfg_1.png" alt="OpenREIL Python API diagram" width="242" height="536">

It's obvious to figure, that optimized code is still no prefect: it uses additional 7 temp registers (1 in first instruction, 3 in second and third instructions) to represent our machine code, while only one temp register (<m>V_01:32</m> inside IR code of `ret`) is enough.

<m>REIL.DFGraph</m> is also allows to optimize temp registers usage:

```python
dfg.optimize_temp_regs()
dfg.store(tr.storage)

print tr.storage
```

Now translated code looks pretty close to original machine code:

```
;
; asm: mov eax, ecx
; data (2): 89 c8
;
00000000.00     STR         R_ECX:32,                 ,         R_EAX:32
;
; asm: add eax, edx
; data (2): 01 d0
;
00000002.00     ADD         R_EAX:32,         R_EDX:32,         R_EAX:32
;
; asm: ret
; data (1): c3
;
00000004.00     LDM         R_ESP:32,                 ,          V_01:32
00000004.01     ADD         R_ESP:32,             4:32,         R_ESP:32
00000004.02     JCC              1:1,                 ,          V_01:32
```

Data flow graph of final code after all optimizations:

<img src="https://dl.dropboxusercontent.com/u/22903093/openreil/dfg_2.png" alt="OpenREIL Python API diagram" width="269" height="280">

### Handling of unknown instructions

During static code analysis it will be useful to have ability to get metainformation about machine instruction (source and destination registers, etc.) even if translation of this instruction to IR was unsuccessful. With such ability it will be not necessery to interrupt code analysis on unknown/untranslated machine instruction, you'll just loose some analysis accuracy.

VEX library which used in OpenREIL has excellent support of x86 general purpose instructions, but it can't translate to IR some certain system instructions like `rdmsr`, `wrmsr`, `cpuid`, `sidt`, `sgdt`, `sldt` and some others. In this case libopenreil will try to use Capstone engine to get information about registers reads and writes, and pass this information to upper abstraction levels. For representing of unknown/untranslated instructions OpenREIL has <m>I_UNK</m> IR instruction.

Let's try to translate to IR some code that uses `cpuid` instruction to get information about x86 processor info and features:

```python
from pyopenreil.REIL import *
from pyopenreil.utils import asm

# create assembly instruction reader
reader = asm.Reader(ARCH_X86, ( 'mov eax, 00000001h',
                                'mov ecx, 0',
                                'cpuid',
                                'mov eax, ecx',
                                'ret' ), addr = 0)

# create translator instance
tr = CodeStorageTranslator(ARCH_X86, reader)

# translate function and build it's DFG
dfg = DFGraphBuilder(tr).traverse(addr)

# Run all available code optimizations and update 
# storage with new function code.
dfg.optimize_all(tr.storage)

# print IR of translated nad optimized function
print tr.get_func(0)
```

Generated IR code:

```
;
; asm: mov eax, 1
; data (5): b8 01 00 00 00
;
00000000.00     STR             1:32,                 ,         R_EAX:32
;
; asm: mov ecx, 0
; data (5): b9 00 00 00 00
;
00000005.00     STR             0:32,                 ,         R_ECX:32
;
; asm: cpuid -- reads: R_EAX, R_ECX; writes: R_EAX, R_EBX, R_ECX, R_EDX
; data (2): 0f a2
;
0000000a.00     UNK                 ,                 ,
;
; asm: mov eax, ecx
; data (2): 89 c8
;
0000000c.00     STR         R_ECX:32,                 ,         R_EAX:32
;
; asm: ret
; data (1): c3
;
0000000e.00     LDM         R_ESP:32,                 ,          V_01:32
0000000e.01     ADD         R_ESP:32,             4:32,         R_ESP:32
0000000e.02     JCC              1:1,                 ,          V_01:32
```

As you can see, it has <m>I_UNK</m> instruction with <m>a.00</m> IR address.

Data flow graph of IR code with <m>I_UNK</m> node wchich represents `cpuid` instruction with <m>R_EAX</m>, <m>R_ECX</m> as source and <m>R_EDX</m>, <m>R_ECX</m>, <m>R_EBX</m> as destination arguments:

<img src="https://dl.dropboxusercontent.com/u/22903093/openreil/dfg_3.png" alt="OpenREIL Python API diagram" width="426" height="266">

### IR code emulation

OpenREIL has emulator for IR code, currently engine uses it mostly for tests. The main idea of such tests - run machine code natively first, then translate it to IR and run under emulation, and finally - compare execution results. Example of programs that preforms such tests: [tests/test_fib.py]() and [tests/test_rc4.py]().

Let's run some simple code under emulation to demonstrate it's API usage:

```python
from pyopenreil.REIL import *
from pyopenreil.VM import *

from pyopenreil.utils import asm

stack_addr = 0x50000
func_addr = 0x1000

# create assembly instruction reader
reader = asm.Reader(ARCH_X86, ( 'mov eax, ecx',
                                'add eax, edx', 
                                'ret' ), addr = func_addr)

# create translator instance
tr = CodeStorageTranslator(ARCH_X86, reader)

# create virtual CPU instance
cpu = Cpu(ARCH_X86)

# set up stack pointer and input args
cpu.reg('esp').val = stack_addr
cpu.reg('ecx').val = 1 # first arg of test function
cpu.reg('edx').val = 2 # second arg of test function

# run test code untill ret
try: cpu.run(tr, func_addr)
except MemReadError as e: 

    # IR code of ret instruction will generate
    # memory access exception because we passed 
    # an invalid stack pointer to test code.
    # So, here we just stopping emulation when such 
    # exception occurs.
    if e.addr != stack_addr: raise

# check for correct return value (1 + 2 = 3)
assert cpu.reg('eax').val == 3

# print current CPU context info into console
cpu.dump()
```

Console output of the <m>cpu.dump()</m> call:

```
   R_EAX: 0000000000000003
   R_ECX: 0000000000000001
   R_EDX: 0000000000000002
   R_ESP: 0000000000050000
    R_AF: 0000000000000000
    R_PF: 0000000000000001
    R_OF: 0000000000000000
    R_ZF: 0000000000000000
    R_CF: 0000000000000000
    R_SF: 0000000000000000
```

For more user friedndly interacting with emulated code OpenREIL has <m>pyopenreil.VM.Abi</m> class which allows to allocate emulator memory and place arbitrary data there (<m>Abi.string()</m> and <m>Abi.buffer()</m> methods), or call native code functions using <m>Abi.stdcall()</m>, <m>Abi.cdecl()</m> and <m>Abi.fastcall()</m> methods. There's also <m>pyopenreil.VM.Stack</m> class to represent a stack memory.

Here is example of calling test code from above using <m>pyopenreil.VM.Abi</m>:

```python
# Abi constructor accepts translator and virtual CPU instances
abi = Abi(cpu, tr)

# call our test code using fastcall call convention
ret = abi.fastcall(func_addr, 1, 2)

# check for correct return value
assert ret == 3
```

## Using OpenREIL for plugins

### IDA Pro

### GDB

### WinDbg (kd)

## FAQ

## TODO

