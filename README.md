# OpenREIL

OpenREIL is open source library that implements translator and tools for REIL (Reverse Engineering Intermediate Language).

+ [About](#_1)
+ [Installation](#_2)
  - [Linux and OS X](#_2_1)
  - [Windows](#_2_2)
+ [IR format](#_3)
  - [Available instructions](#_3_1)
  - [Optional instruction flags](#_3_2)
  - [Representation of x86 registers](#_3_3)
  - [Representation of x86 EFLAGS](#_3_4)
+ [C API](#_4)
+ [Python API](#_5)
  - [Low level translation API](#_5_1)
  - [High level Python API](#_5_2)
  - [MongoDB as IR code storage](#_5_3)
  - [Translating of basic blocks and functions](#_5_4)
  - [Symbolic expressions](#_5_5)
  - [Control flow graphs](#_5_6)
  - [Data flow graphs](#_5_7)
  - [Handling of unknown instructions](#_5_8)
  - [IR code emulation](#_5_9)
+ [Debugging OpenREIL](#_6)
+ [Using with third party tools](#_7)
  - [IDA Pro](#_7_1)
  - [GDB](#_7_2)
  - [WinDbg (kd, cdb)](#_7_3)
+ [Links and examples](#_8)
+ [TODO](#_9)


## About <a id="_1"></a>

REIL was initially developed by [Zynamics](http://www.zynamics.com/) as part of their [BinNavi framework](http://www.zynamics.com/binnavi.html), proprietary code analysis software written in Java. To learn more about REIL read the following documents:

* «REIL &minus; The Reverse Engineering Intermediate Language» ([link](http://www.zynamics.com/binnavi/manual/html/reil_language.htm))

* «The REIL language» ([part 1](http://blog.zynamics.com/2010/03/07/the-reil-language-part-i/), [part 2](http://blog.zynamics.com/2010/06/22/the-reil-language-part-ii/), [part 3](http://blog.zynamics.com/2010/07/19/the-reil-language-part-iii/), [part 4](http://blog.zynamics.com/2010/08/24/the-reil-language-part-iv/))

* «Applications of the Reverse Engineering Language REIL» ([PDF](www.zynamics.com/downloads/slides-h2hc.pdf))

* «REIL: A platform-independent intermediate representation of disassembled code for static code analysis» ([PDF](http://static.googleusercontent.com/media/www.zynamics.com/en//downloads/csw09.pdf))

However, after Zynamics was acquired by Google they abandoned BinNavi, so, I decided to develop my own implementation of REIL. I made it relatively small and portable in comparison with original, the translator itself is just a single library written in C++, it can be statically linked with any program for static or dynamic code analysis. The higher level API of OpenREIL is written in Python, so, it can be easily utilized in plugins and scripts for your favourite reverse engineering tool (almost all modern debuggers and disassemblers has Python bindings).

OpenREIL is not a 100% compatible with Zynamics REIL, it has the same ideology and basics, but there's some changes in IR instruction set and representation of the traget hardware platform features.

OpenREIL is based on my custom fork of libasmir &minus; IR translation library from old versions of [BAP framework](http://bap.ece.cmu.edu/). I removed some 3-rd party dependencies of libasmir (libbfd, libopcodes) and features that irrelevant to translation itself (binary files parsing, traces processing, etc.), than I did some minor refactoring/bugfixes, switched to [Capstone](http://www.capstone-engine.org/) for instruction length disassembling and implemented BAP IR &#8594; REIL translation logic on the top of libasmir. 

Because libasmir uses VEX (production-quality library, part of [Valgrind](http://valgrind.org/)), full code translation sequence inside of OpenREIL is looks as binary &#8594; VEX IR &#8594; BAP IR &#8594; REIL. It's kinda ugly from engineering point of view, but it allows us to have a pretty robust and reliable support of all general instructions of x86. Current version of OpenREIL still has no support of other architectures, but I'm working on x86_64 and ARMv7.

UPDATE: Current version of OpenREIL already has much or less usable ARMv7 support with dynamic switching of translation context between ARM and Thumb execution modes. There is no proper documentation on ARM yet, but you can check some unit tests for this functionality as usage examples: [1](https://github.com/Cr4sh/openreil/blob/08543fec7ac3bdeb2a4c21267b39589c7f650186/pyopenreil/REIL.py#L3261), [2](https://github.com/Cr4sh/openreil/blob/08543fec7ac3bdeb2a4c21267b39589c7f650186/pyopenreil/REIL.py#L974), [3](https://github.com/Cr4sh/openreil/blob/08543fec7ac3bdeb2a4c21267b39589c7f650186/tests/test_md5.py#L91)

Please note, that currently OpenREIL is a far away from stable release, so, I don't recommend you to use it for any serious purposes.


## Installation <a id="_2"></a>

OpenREIL supports Linux, Mac OS X and Windows. Structure of the project source code directory:

* `VEX` &minus; VEX source code with BAP patches applied.
* `capstone` &minus; Capstone engine source code will be downloaded here.
* `docs` &minus; documentation directory.
* `libasmir` &minus; libasmir source code.
* `libopenreil` &minus; OpenREIL translator source code.
* `pyopenreil` &minus; OpenREIL Python API.
* `tests` &minus; unit tests launcher and stand-alone test programs.

### Linux and OS X <a id="_2_1"></a>

To build OpenREIL under *nix operating systems you need to install git, gcc, make, Python 2.x with header files, [NumPy](https://pypi.python.org/pypi/numpy) and [Cython](http://cython.org/). After that you can run configure and make just as usual. 

Example for Debian:

```
$ sudo apt-get install git gcc make python python-dev python-numpy cython
$ git clone https://github.com/Cr4sh/openreil.git
$ cd openreil
$ ./autogen.sh
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

OpenREIL can be used not only for debugger/disassembler plugins, but also for standalone code analysis tools. For loading of executable files it uses pybfd (ELF, Mach-O) and pefile (Portable Execute) Python libraries:

```
$ sudo apt-get install libbfd-dev
$ sudo easy_install pefile pybfd
```


### Windows <a id="_2_2"></a>

Building OpenREIL on Windows requires [MinGW](http://www.mingw.org/) build environment and all of the dependencies that was mentioned above.

You also can download compiled Win32 binaries of OpenREIL:

* [libopenreil-0.1.10-win32.zip](https://github.com/Cr4sh/openreil/releases/download/0.1.10/libopenreil-0.1.10-win32.zip)
* [pyopenreil-0.1.10-win32-python2.7.zip](https://github.com/Cr4sh/openreil/releases/download/0.1.10/pyopenreil-0.1.10-win32-python2.7.zip)


## IR format  <a id="_3"></a>

### Available instructions <a id="_3_1"></a>

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

| Mnemonic  | Pseudocode           | Instruction description     |
|-----------|----------------------|-----------------------------|
| STR       | c = a                | Store to register           |
| STM       | \*c = a              | Store to memory             |
| LDM       | c = \*a              | Load from memory            |
| ADD       | c = a + b            | Addition                    |
| SUB       | c = a &minus; b      | Subtraction                 |
| NEG       | c = &minus;a         | Negation                    |
| MUL       | c = a \* b           | Multiplication              |
| DIV       | c = a / b            | Division                    |
| MOD       | c = a % b            | Modulus                     |
| SHL       | c = a << b           | Shift left                  |
| SHR       | c = a >> b           | Shift right                 |
| AND       | c = a & b            | Binary AND                  |
| OR        | c = a &#124; b       | Binary OR                   |
| XOR       | c = a ^ b            | Binary XOR                  |
| NOT       | c = ~a               | Binary NOT                  |
| EQ        | c = a == b           | Equation                    |
| LT        | c = a < b            | Less than                   |
| SMUL      | c = a \* b           | Signed multiplication       |
| SDIV      | c = a / b            | Signed division             |
| SMOD      | c = a % b            | Signed modulus              |
| JCC       |                      | Jump to c if a is not zero  |
| NONE      |                      | No operation                |
| UNK       |                      | Untranslated instruction    |

          
Each instruction argument can have 1, 8, 16, 32 or 64 bits of length\. Most of arithmetic instructions operates with unsigned arguments\. `SMUL`, `SDIV` and `SMOD` instructions operates with signed arguments in [two’s complement](http://en.wikipedia.org/wiki/Two%27s_complement) signed number representation form\.

Most of instructions supposes the same size of their source and destination arguments, but there’s a several exceptions:

   * Result (argument c) of `EQ` and `LT` instructions always has 1 bit size.
   * Argument a of `STM` and argument c of `LDM` must have 8 bits or gather size (obviously, there is no memory read/write operations with single bit values).
   * Result of `OR` instruction can have any size. It may be less, gather or equal than size of it’s input arguments. 

OpenREIL uses `OR` operation for converting to value of different size. For example, converting value of 8 bit argument `V_01` to 1 bit argument `V_02` (1-7 bits of `V_01` will be ignored):

```
00000000.00      OR           V_01:8,              0:8,           V_02:1
```

Converting 1 bit argument `V_02` to 32 bit argument `V_03` (1-31 bits of `V_03` will be zero extended):

```
00000000.00      OR           V_02:1,              0:1,          V_03:32
```

Instruction argument can have following type:

   * `A_REG` &minus; CPU register (example: `R_EAX:32`, `R_ZF:1`).
   * `A_TEMP` &minus; temporary register (example: `V_01:8`, `V_02:32`).
   * `A_CONST` &minus; constant value (example: `0:1`, `fffffff4:32`).
   * `A_LOC` &minus; jump location that consists from machine instruction address and IR instruction number (example: `8048360.0`, `100000.3`).
   * `A_NONE` &minus; argument is not used by instruction.

Address of each IR instruction consists from two parts: original address of translated machine instruction and IR instruction logical number (inum). First IR instruction of each machine instruction always has inum with value 0. 

Group of IR instructions that represent one machine instruction can set value of some temporary register only once, so, `A_TEMP` arguments are in static single assignment form within the confines of machine instruction.


### Optional instruction flags <a id="_3_2"></a>

In addition to address, operation code and arguments IR instruction also has flags that used to store useful metainformation:

   * `IOPT_CALL` &minus; this JCC instruction represents a function call.
   * `IOPT_RET` &minus; this JCC instruction represents a function exit.
   * `IOPT_ASM_END` &minus; last IR instruction of machine instruction.
   * `IOPT_BB_END` &minus; last IR instruction of basic block.
   * `IOPT_ELIMINATED` &minus; whole machine instruction was replaced with `I_NONE` IR instruction during dead code elimination.


### Representation of x86 registers <a id="_3_3"></a>

IR code for x86 architecture operates only with 32-bit length general purpose CPU registers (`R_EAX:32`, `R_ECX:32`, etc.), OpenREIL has no half-sized registers (`AX`, `AL`, `AH`, etc.), translator converts all machine instructions that operates with such registers to full length form.

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

Many operating systems uses `FS` segment register to access certain system structures. OpenREIL represents this segment register as `R_FS_BASE:32`. Here is an example of function that gets `_PEB` address of the current Windows process:

```cpp
ULONG_PTR get_peb(void)
{
#ifdef _X86_

    return __readfsdword(0x30);

#else _AMD64_

    return __readgsqword(0x60);

#endif
}
```

And IR code of this function:

```
;
; asm: mov eax, dword ptr fs:[0x30]
; data (6): 64 a1 30 00 00 00
;
00401000.00     STR          R_FS:16,                 ,          V_00:16
00401000.01      OR          V_00:16,             0:16,          V_01:32
00401000.02     ADD     R_FS_BASE:32,            30:32,          V_02:32
00401000.03     STR          V_02:64,                 ,          V_03:32
00401000.04     LDM          V_03:32,                 ,          V_04:32
00401000.05     STR          V_04:32,                 ,         R_EAX:32
;
; asm: ret
; data (1): c3
;
00401006.00     LDM         R_ESP:32,                 ,          V_01:32
00401006.01     ADD         R_ESP:32,             4:32,         R_ESP:32
00401006.02     JCC              1:1,                 ,          V_01:32
```


### Representation of x86 EFLAGS <a id="_3_4"></a>

OpenREIL uses separate 1-bit registers to represent `ZF`, `PF`, `CF`, `AF`, `SF` and `OF` bits of `EFLAGS` register.

Example of IR code for `test eax, eax` x86 instruction that sets some of these bits:

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


For machine instructions that operates with whole `EFLAGS` value (`pushfd`, `popfd`, etc.) OpenREIL composes it's value from values of 1-bit registers:

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

Also, as you can see from IR code above, OpenREIL uses 32-bit `R_DFLAG` register to represent [direction flag](http://en.wikipedia.org/wiki/Direction_flag) of x86. This register is used in such instructions like `movsb`, `lodsb`, etc. as increment value for source/destination index, so, `R_DFLAG` must be `1:32` when direction flag is not set, and `ffffffff:32` (minus one) when it's set.

Example of IR code for instruction that clears direction flag:

```
;
; asm: cld
; data (1): fc
;
00000000.00     STR             1:32,                 ,       R_DFLAG:32
```

... and for instruction that sets direction flag:

```
;
; asm: std
; data (1): fd
;
00000001.00     STR      ffffffff:32,                 ,       R_DFLAG:32
```

## C API <a id="_4"></a>

OpenREIL C API is declared in [reil_ir.h](../master/libopenreil/include/reil_ir.h) (IR format) and [libopenreil.h](../master/libopenreil/include/libopenreil.h) (translator API) header files. 

Here is an example of the test program that translates machine code to IR instructions and prints them to stdout:

```cpp
#include <stdio.h>
#include <libopenreil.h>

#define MAX_ARG_STR 50

const char *inst_op[] =
{
    "NONE", "UNK", "JCC",
    "STR", "STM", "LDM",
    "ADD", "SUB", "NEG", "MUL", "DIV", "MOD", "SMUL", "SDIV", "SMOD",
    "SHL", "SHR", "AND", "OR", "XOR", "NOT",
    "EQ", "LT"
};

int arg_size[] = { 1, 8, 16, 32, 64 };

char *arg_print(reil_arg_t *arg, char *arg_str)
{
    memset(arg_str, 0, MAX_ARG_STR);

    switch (arg->type)
    {
    case A_NONE:

        snprintf(arg_str, MAX_ARG_STR - 1, "");
        break;

    case A_REG:
    case A_TEMP:

        snprintf(arg_str, MAX_ARG_STR - 1, "%s:%d", arg->name, arg_size[arg->size]);
        break;

    case A_CONST:

        snprintf(arg_str, MAX_ARG_STR - 1, "%llx:%d", arg->val, arg_size[arg->size]);
        break;
    }

    return arg_str;
}

void inst_print(reil_inst_t *inst)
{
    char arg_str[MAX_ARG_STR];

    // print address and mnemonic
    printf(
        "%.8llx.%.2x %7s ",
        inst->raw_info.addr, inst->inum, inst_op[inst->op]
    );

    // print instruction arguments
    printf("%16s, ", arg_print(&inst->a, arg_str));
    printf("%16s, ", arg_print(&inst->b, arg_str));
    printf("%16s  ", arg_print(&inst->c, arg_str));

    printf("\n");
}

int inst_handler(reil_inst_t *inst, void *context)
{
    // increment IR instruction counter
    *(int *)context += 1;

    // print IR instruction to the stdout
    inst_print(inst);

    return 0;
}

int translate_inst(reil_arch_t arch, unsigned char *data, int len)
{
    int translated = 0;

    // initialize REIL translator
    reil_t reil = reil_init(arch, inst_handler, (void *)&translated);
    if (reil)
    {
        // translate single instruction to REIL
        reil_translate(reil, 0, data, len);
        reil_close(reil);
        return translated;
    }

    return -1;
}

int main(int argc, char *argv[])
{
    unsigned char *test_data = "\x33\xC0"; // xor eax, eax
    int len = 2;

    if (translate_inst(ARCH_X86, test_data, len) >= 0)
    {
        return 0;
    }

    return -1;
}
```

## Python API <a id="_5"></a>

### Low level translation API <a id="_5_1"></a>

OpenREIL has low level translation API that returns IR instructions as Python tuple. Here is an example of decoding `push eax` x86 instruction using this API:

```python
from pyopenreil import translator
from pyopenreil.REIL import ARCH_X86

# create REIL translator instance
tr = translator.Translator(ARCH_X86)

# translate machine instruction to REIL
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


`pyopenreil.translator` module is written in Cython, it’s stands for bridge between C API and high level Python API of OpenREIL. Also, OpenREIL uses JSON representation of these tuples to store translated instruction into the file or MongoDB collection.

IR constants (operation codes, argument types, etc.) are declared in `pyopenreil.IR` module.


### High level Python API <a id="_5_2"></a>

High level Python API of OpenREIL has abstractions for IR instructions format, translator, machine instructions reader and IR instructions storage:

<img src="https://dl.dropboxusercontent.com/u/22903093/openreil/python_api.png" alt="OpenREIL Python API diagram" width="465" height="405">

The most important modules:

   * `pyopenreil.IR` &minus; IR constants.
   * `pyopenreil.REIL` &minus; translation and analysis API.
   * `pyopenreil.symbolic` &minus; represents IR as symbolic expressions.
   * `pyopenreil.VM` &minus; IR emulation.
   * `pyopenreil.utils.asm` &minus; instruction reader for x86 assembly language.
   * `pyopenreil.utils.bin_PE` &minus; instruction reader for PE binaries.
   * `pyopenreil.utils.bin_BFD` &minus; instruction reader for ELF and Mach-O binaries.
   * `pyopenreil.utils.kd` &minus; instruction reader that uses pykd API.
   * `pyopenreil.utils.GDB` &minus; instruction reader that uses GDB Python API.
   * `pyopenreil.utils.IDA` &minus; instruction reader that uses IDA Pro Python API.
   * `pyopenreil.utils.mongodb` &minus; instruction storage that uses MongoDB.

Usage example:

```python
from pyopenreil.REIL import *

# create in-memory storage for IR instructions
storage = CodeStorageMem(ARCH_X86)

# initialise raw instruction reader
reader = ReaderRaw(ARCH_X86, '\x50', addr = 0)

# create translator instance
tr = CodeStorageTranslator(reader, storage)

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


Instead of raw instruction reader you also can use x86 assembly instruction reader that based on GAS. OpenREIL uses this reader in unit tests, it also might be useful for other purposes:

```python
from pyopenreil.utils import asm

# create reader instance
reader = asm.Reader(ARCH_X86, ( 'push eax' ), addr = 0)
```


### MongoDB as IR code storage <a id="_5_3"></a>

Using of `CodeStorageMem()` class as IR code storage has one significant disadvantage: real programs are very complex nowadays, so you may need to analyze amount of code that is too big to fit into the RAM. 

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

Here is an example of MongoDB collection record format that is used by OpenREIL:

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
  "attr" : [ [ 2, 0 ] ]
}
```

This record stores the following IR instruction:

```
41414141.00     STR          V_00:32,                 ,          V_01:32
```

MongoDB module uses `[('addr', ASCENDING), ('inum', ASCENDING)]` as collection index. To improve storage performance you also can join several instances of MongoDB into cluster.


### Translating of basic blocks and functions <a id="_5_4"></a>

`CodeStorageTranslator` class of `pyopenreil.REIL` is also can do code translation at basic blocks or functions level. Example of basic block translation:

```python
# initialise raw instruction reader
reader = ReaderRaw(ARCH_X86, '\x33\xC0\xC3', addr = 0)

# Create translator instance.
# We are not passing storage instance, translator will create
# instance of CodeStorageMem automatically in this case.
tr = CodeStorageTranslator(reader)

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


### Symbolic expressions <a id="_5_5"></a>

OpenREIL can convert basic blocks and instructions into the symbolic form, it might be useful for such tasks like symbolic execution, interacting with SMT solver, decompilation and analysis of code semantics.

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
tr = CodeStorageTranslator(reader)

# translate first basic block
bb = tr.get_bb(0)

# Get symbolic representation of basic block.
# False value of temp_regs argument indicates that
# we don't need expressions for REIL temp registers.
sym = bb.to_symbolic(temp_regs = False)

# print SymState object
print sym
```

Program output (symbolic representation of first basic block that contains first 5 machine instructions of the test code):
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

Symbolic expressions are implemented in `pyopenreil.symbolic` module; `symbolic.Sym` is a base class for other classes that represents expression members: `symbolic.SymVal`, `symbolic.SymConst`, `symbolic.SymExp`, `symbolic.SymCond`, `symbolic.SymPtr` and others.

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

You also can compare expressions, OpenREIL will care about commutativity and other important things. To match any valid expression `symbolic.SymAny` object is used:

```python
# compare expressions
assert exp_zf == SymExp(I_EQ, SymVal('R_ECX'), SymConst(0, U32))
assert exp_zf == SymExp(I_EQ, SymVal('R_ECX'), SymAny())
assert exp_zf == SymAny()
```

For extracting information about input and output arguments of `symbolic.SymState` it has `arg_in()` and `arg_out()` methods:

```python
print 'IN:', ', '.join(map(lambda a: str(a), sym.arg_in()))
print 'OUT:', ', '.join(map(lambda a: str(a), sym.arg_out()))
```

Console output:

```
IN: R_ESP, R_EBP, R_ECX
OUT: R_ESP, *(R_ESP - 0x4), R_EBP, R_EAX, R_CF, R_PF, R_AF, R_ZF, R_SF, R_OF, @IP
```


### Control flow graphs <a id="_5_6"></a>

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

Now compile it with GCC and get virtual address of `fib()` function using objdump:

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

OpenREIL allows to work with CFG's using `REIL.CFGraph`, `REIL.CFGraphNode` and `REIL.CFGraphEdge` objects; `REIL.CFGraphBuilder` object accepts translator instance and performs CFG traversing starting from specified address and until the end of the current machine code function. 

Let's build control flow graph of `fib()` function (that located at address `0x08048414`) of `tests/fib` executable:

```python
from pyopenreil.REIL import *
from pyopenreil.utils import bin_BFD

# create executable image reader 
reader = bin_BFD.Reader('tests/fib')

# VA of the fib() function inside fib executable
addr = 0x08048414

# create translator instance
tr = CodeStorageTranslator(reader)

# build control flow graph
cfg = CFGraphBuilder(tr).traverse(addr)
```

You can save generated graph as file of [Graphviz DOT format](http://www.graphviz.org/content/dot-language):

```python
cfg.to_dot_file('cfg.dot')
```

Then you can render this file into the PNG image using Graphviz dot utility:

```
$ dot -Tpng cfg.dot > cfg.png
```

Rendered control flow graph of the `fib()` function:

<img src="https://dl.dropboxusercontent.com/u/22903093/openreil/cfg_1.png" alt="OpenREIL Python API diagram" width="181" height="266">

Please note, that generated IR code may have more complicated CFG layout than it's source machine code (because of x86 instructions with REP prefix, for example). Here is IR code for function that consists from `rep movsb` and `ret` instructions:

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

As you can see, this IR code has branch instructions at `1337.02`  and `1337.0e`, so, it's DFG has 3 nodes:

<img src="https://dl.dropboxusercontent.com/u/22903093/openreil/cfg_2.png" alt="OpenREIL Python API diagram" width="231" height="96">

### Data flow graphs <a id="_5_7"></a>

As example, let's take a simple function that returns sum of two arguments:

```cpp
long __fastcall sum(long a, long b)
{
    // a comes in ECX, b in EDX
    return a + b;
}
```

It's Assembly code:

```
_sum:

mov     eax, ecx
add     eax, edx
ret
```

Generated IR code:

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
tr = CodeStorageTranslator(reader)

# build data flow graph
dfg = DFGraphBuilder(tr).traverse(0)
```

Rendered data flow graph picture ([link](https://dl.dropboxusercontent.com/u/22903093/openreil/dfg_full.png)) for this simple code looks quite complex because `add` instruction (like any other arithmetic instruction of x86) is doing a lot of `EFLAGS` computations.

`REIL.DFGraph` allows to apply some basic data flow code optimizations to translated IR code, currently it supports such well known compiler optimizations as [dead code elimination](http://en.wikipedia.org/wiki/Dead_code_elimination), [constant folding](http://en.wikipedia.org/wiki/Constant_folding) and very basic [common subexpressions elimination](http://en.wikipedia.org/wiki/Common_subexpression_elimination). 

Let's apply these optimizations to `fib()` function code:

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

It's obvious to figure, that optimized code is still no prefect: it uses additional 7 temp registers (1 in first instruction, 3 in second and third instructions) to represent our machine code, while only one temp register (`V_01:32` inside IR code of `ret`) is enough.

`REIL.DFGraph` is also allows to optimize temp registers usage with common subexpressions elimination:

```python
dfg.eliminate_subexpressions()
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

`REIL.CFGraph` is also has `optimize_all()` method that runs all available optimizations. 

Please note, that you need to specify `True` value for `keep_flags` argument of `CFGraph.eliminate_dead_code()` if you want to keep flag register values at function exit.

It also will be necessary to say, that described optimizations was designed not for defeating code obfuscation or something, but rather for reducing amount of ineffective code produced by libopenreil translator.


### Handling of unknown instructions <a id="_5_8"></a>

During static code analysis it will be useful to have ability to get metainformation about machine instruction (source and destination registers, etc.) even if translation of this instruction to IR was unsuccessful. With such ability it will be not necessary to interrupt code analysis on unknown/untranslated machine instruction, you'll just loose some analysis accuracy.

VEX library used in OpenREIL has excellent support of x86 general purpose instructions, but it can't translate to IR some certain system instructions like `rdmsr`, `wrmsr`, `cpuid`, `sidt`, `sgdt`, `sldt` and some others. In this case libopenreil will try to use Capstone engine to get information about registers reads and writes, and pass this information to upper abstraction levels. For representing of unknown/untranslated instructions OpenREIL has `I_UNK` IR instruction.

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
tr = CodeStorageTranslator(reader)

# translate function and build it's DFG
dfg = DFGraphBuilder(tr).traverse(addr)

# Run all available code optimizations and update 
# storage with new function code.
dfg.optimize_all(tr.storage)

# print IR of translated and optimized function
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

As you can see, it has `I_UNK` instruction with `a.00` IR address.

Data flow graph of IR code with `I_UNK` node that represents `cpuid` instruction with `R_EAX`, `R_ECX` as source and `R_EDX`, `R_ECX`, `R_EBX` as destination arguments:

<img src="https://dl.dropboxusercontent.com/u/22903093/openreil/dfg_3.png" alt="OpenREIL Python API diagram" width="426" height="266">


### IR code emulation <a id="_5_9"></a>

OpenREIL has emulator for IR code, currently engine uses it mostly for tests. The main idea of such tests &minus; run machine code natively first, then translate it to IR and run under emulation, and finally &minus; compare execution results. Example of programs that preforms such tests: [tests/test_fib.py](../blob/master/tests/test_fib.py) and [tests/test_rc4.py](../blob/master/tests/test_rc4.py).

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
tr = CodeStorageTranslator(reader)

# create virtual CPU instance
cpu = Cpu(ARCH_X86)

# set up stack pointer and input args
cpu.reg('esp').val = stack_addr
cpu.reg('ecx').val = 1 # first arg of test function
cpu.reg('edx').val = 2 # second arg of test function

# run test code until ret
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

Console output of the `cpu.dump()` call:

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

For more user friendly interacting with emulated code OpenREIL has `pyopenreil.VM.Abi` class that allows to allocate emulator memory and place arbitrary data there (`Abi.string()` and `Abi.buffer()` methods), or call native code functions using `Abi.stdcall()`, `Abi.cdecl()` and `Abi.fastcall()` methods. There's also `pyopenreil.VM.Stack` class to represent a stack memory.

Here is example of calling test code from above using `pyopenreil.VM.Abi`:

```python
# Abi constructor accepts translator and virtual CPU instances
abi = Abi(cpu, tr)

# call our test code using fastcall call convention
ret = abi.fastcall(func_addr, 1, 2)

# check for correct return value
assert ret == 3
```

Let's try more complex IR code emulation example. Here is the source code of test program that performs RC4 encryption/decryption of some data:

```cpp
#include <stdio.h>
#include <string.h>

#define MIN(a, b) (((a) < (b)) ? (a) : (b))

typedef struct _RC4_CTX 
{
    unsigned char S[256];
    unsigned char x, y;

} RC4_CTX;

void rc4_swap(unsigned char *a, unsigned char *b)
{
    unsigned char c = *a;

    *a = *b;
    *b = c;
}

void rc4_set_key(RC4_CTX *ctx, unsigned char *key, int key_len)
{
    int i = 0;
    unsigned char x = 0, y = 0;    
    unsigned char *S = ctx->S;

    ctx->x = x = 0;
    ctx->y = y = 0;

    for (i = 0; i < 256; i++)
    {
        S[i] = (unsigned char)i;
    }
    
    for (i = 0; i < 256; i++)
    {
        y = (y + S[i] + key[x]) % 256;

        rc4_swap(&S[i], &S[y]);

        x = (x + 1) % key_len;
    }
}

void rc4_crypt(RC4_CTX *ctx, unsigned char *data, int data_len)
{
    int i = 0;
    unsigned char x = 0, y = 0;
    unsigned char *S = ctx->S;

    for (i = 0; i < data_len; i++)
    {
        x = (x + 1) % 256;
        y = (y + S[x]) % 256;

        rc4_swap(&S[x], &S[y]);

        data[i] ^= S[(S[x] + S[y]) % 256];
    }

    ctx->x = x;
    ctx->y = y;
}

#define MAX_DATA_LEN 255

RC4_CTX m_ctx;

int main(int argc, char *argv[])
{
    if (argc < 3)
    {
        return -1;
    }
    
    // get encryption key and data
    char *key = argv[1], *data = argv[2];
    int key_len = strlen(key);
    int data_len = strlen(data);

    // copy data to local in/out buffer
    unsigned char buff[MAX_DATA_LEN];
    memcpy(buff, data, MIN(MAX_DATA_LEN, data_len));
 
    // perform encryption
    rc4_set_key(&m_ctx, (unsigned char *)key, key_len);
    rc4_crypt(&m_ctx, buff, data_len);

    printf("Encrypted data: ");

    for (i = 0; i < data_len; i++)
    {
        printf("%.2x ", buff[i]);
    }

    printf("\n");

    return 0;
}
```

Compile it and get VA's of `rc4_set_key()` and `rc4_crypt()` functions:

```
$ gcc tests/rc4.c -o tests/rc4
$ objdump -t tests/rc4 | grep '.text'
080483e0 l    d  .text  00000000              .text
08048410 l     F .text  00000000              __do_global_dtors_aux
08048470 l     F .text  00000000              frame_dummy
08048980 l     F .text  00000000              __do_global_ctors_aux
08048970 g     F .text  00000002              __libc_csu_fini
080484b9 g     F .text  000000df              rc4_set_key
08048972 g     F .text  00000000              .hidden __i686.get_pc_thunk.bx
08048494 g     F .text  00000025              rc4_swap
08048900 g     F .text  00000061              __libc_csu_init
080483e0 g     F .text  00000000              _start
08048674 g     F .text  00000280              main
08048598 g     F .text  000000dc              rc4_crypt
```

Now let's write a test program that encrypts some data using these functions under emulation and compares encryption results with built-in Python implementation of RC4:

```python
from pyopenreil.REIL import *
from pyopenreil.VM import *

from pyopenreil.utils import bin_BFD

# test input data for RC4 encryption
test_key = 'somekey'
test_val = 'foobar'

# VA's of required functions
rc4_set_key = 0x080484b9
rc4_crypt = 0x08048598

# load test program image
reader = bin_BFD.Reader('tests/rc4')
tr = CodeStorageTranslator(reader)

def code_optimization(addr):

    # construct dataflow graph for given function
    dfg = DFGraphBuilder(tr).traverse(addr)
  
    # run available optimizations
    dfg.optimize_all(tr.storage)

code_optimization(rc4_set_key)
code_optimization(rc4_crypt)

# create CPU and ABI
cpu = Cpu(ARCH_X86)
abi = Abi(cpu, tr)

# allocate buffers for arguments of emulated function
ctx = abi.buff(256 + 4 * 2) # rc4_ctx structure
val = abi.buff(test_val) # data to encrypt

# emulate rc4_set_key() function call
abi.cdecl(rc4_set_key, ctx, test_key, len(test_key))

# emulate rc4_crypt() function call
abi.cdecl(rc4_crypt, ctx, val, len(test_val))

# read results of RC4 encryption
val_1 = abi.read(val, len(test_val))
print 'Encrypted value:', repr(val_1)

# compare results with Python build-in RC4 module
from Crypto.Cipher import ARC4
rc4 = ARC4.new(test_key)
val_2 = rc4.encrypt(test_val)

assert val_1 == val_2
```

It's took around 5 seconds to execute this code, it shows that Python implementation of IR code emulator is a quite slow. I'm not sure if OpenREIL emulation features will be useful for any research purposes (it seems that no), but as was said above, it helps me a lot with translator testing.


## Debugging OpenREIL <a id="_6"></a>

OpenREIL Python and C API providies some settings to control debug information output, error messages, etc.

`libopenreil` has `reil_log_init()` and `reil_log_close()` functions that declared in [libopenreil.h](../master/libopenreil/include/libopenreil.h) header file:

```cpp
/*
    Debug information type constants for mask argument of reil_log_init()
*/
#define REIL_LOG_INFO   0x00000001  // regular message
#define REIL_LOG_WARN   0x00000002  // error
#define REIL_LOG_ERR    0x00000004  // warning
#define REIL_LOG_BIN    0x00000008  // instruction bytes
#define REIL_LOG_ASM    0x00000010  // instruction assembly code
#define REIL_LOG_VEX    0x00000020  // instruction VEX code
#define REIL_LOG_BIL    0x00000040  // instruction BAP IL code

// all log messages
#define REIL_LOG_ALL 0x7FFFFFFF

// disable log messages
#define REIL_LOG_NONE 0

// by default we prints warnings, errors and info to stderr
#define REIL_LOG_DEFAULT (LOG_INFO | LOG_WARN | LOG_ERR)

...

/*
    Initialize debug log. 
    * Individual bits of mask argument tells what kind of information
    we need to include into the log (see REIL_LOG_* constants).
    * Log file path can be specified in path argument, NULL value 
    indicates that we need to print debug information only to stderr.
*/
int reil_log_init(uint32_t mask, const char *path);

/*
    Closes currently opened log file.
*/
void reil_log_close(void);
```

`pyopenreil` has similar functions `REIL.log_init()` and `REIL.log_close()`, here you can see `simple.py` Python program that uses debug output file `debug.log`:

```python
from pyopenreil.REIL import *
from pyopenreil.utils import asm

# set up debug logging
log_init(LOG_ALL, 'debug.log')

# create assembly instruction reader
reader = asm.Reader(ARCH_X86, ( 'xchg dword ptr [esp], edx'), addr = 0)

# create translator instance
tr = CodeStorageTranslator(reader)

# translate first machine instruction
print tr.get_insn(0)

# close currently opened debug log
log_close()
```

After running this code debug output file will contain the following data:

```
------------------------ After instrumentation ------------------------

IRSB {
   t0:I32   t1:I32   t2:I32   t3:I32   t4:I32   t5:I1   t6:I32
   ------ IMark(0x1337, 3, 0) ------
   t2 = GET:I32(24)
   t0 = LDle:I32(t2)
   t1 = GET:I32(16)
   t3 = CASle(t2::t0->t1)
   t5 = CasCmpNE32(t3,t0)
   if (t5) { PUT(68) = 0x1337:I32; exit-Boring }
   PUT(16) = t0
   PUT(68) = 0x133A:I32; exit-Boring
}

VexExpansionRatio 3 65   216 :10

BIN { 00001337: 87 14 24 }
ASM { 00001337: xchg dword ptr [esp], edx ; len = 3 }
BAP {
   label pc_0x1337:
   var T_32t0:reg32_t;
   var T_32t1:reg32_t;
   var T_32t2:reg32_t;
   var T_32t3:reg32_t;
   var T_32t4:reg32_t;
   var T_1t5:reg1_t;
   var T_32t6:reg32_t;
   T_32t2:reg32_t = R_ESP:reg32_t;
   T_32t0:reg32_t = mem[T_32t2:reg32_t];
   T_32t1:reg32_t = R_EDX:reg32_t;
   T_32t3:reg32_t = mem[T_32t2:reg32_t];
   cjmp((T_32t3:reg32_t<>T_32t0:reg32_t),name(L_1),name(L_0));
   label L_0:
   mem[T_32t2:reg32_t] = T_32t1:reg32_t;
   label L_1:
   T_1t5:reg1_t = (T_32t3:reg32_t<>T_32t0:reg32_t);
   cjmp(T_1t5:reg1_t,name(pc_0x1337),name(L_2));
   label L_2:
   R_EDX:reg32_t = T_32t0:reg32_t;
   // BAP label pc_0x1337 at 00001337.00
   // BAP label L_0 at 00001337.07
   // BAP label L_1 at 00001337.08
   // BAP label L_2 at 00001337.0b
}
```

As you can see, it has information about each machine instruction and it's binary, assembly, VEX IR and BIL code that might be very useful for translation errors investigation.

Also, `pyopenreil` allows you to pass debug logging mask and file path to any OpenREIL program using environment variables `REIL_LOG_MASK` and `REIL_LOG_PATH`. Example:

```
$ REIL_LOG_MASK=0x7fffffff REIL_LOG_PATH=debug.log python simple.py
```

Source code of `pyopenreil` has a lot of built-in tests that uses `unittest` [unit testing framework](https://docs.python.org/2/library/unittest.html), you can run all tests using `test` make target:

```
$ make test
```

Or run all unit tests manually using `tests/run_unittest.py` program:

```
$ REIL_LOG_MASK=0x7fffffff python tests/run_unittest.py
```

Or run individual unit test:

```
$ REIL_LOG_MASK=0x7fffffff python tests/run_unittest.py TestArchX86.test_xchg
```

## Using with third party tools <a id="_7"></a>

Most of examples in this document was focused on using OpenREIL to develop stand-alone code analysis tools, but it's also possible to use it with any existing reverse engineering tool that supports Python scripting. OpenREIL has build-in support of IDA Pro, GDB and WinDbg as machine code sources.

### IDA Pro <a id="_7_1"></a>

Sample IDA Pro script that comes with OpenREIL, `ida_translate_func.py`, can translate machine functions to IR code and save it into the JSON files:

```python
import sys, os
import idc
from pyopenreil.REIL import *
from pyopenreil.utils import IDA

DEF_ARCH = ARCH_X86

# get address of the current function
addr = idc.ScreenEA()

arch = DEF_ARCH
path = os.path.join(os.getcwd(), 'sub_%.8X.ir' % addr)

# initialise OpenREIL stuff
reader = IDA.Reader(arch)
storage = CodeStorageMem(arch)
tr = CodeStorageTranslator(reader, storage)

# translate function and enumerate it's basic blocks
func = tr.get_func(addr)

for bb in func.bb_list: print bb

print '[*] Saving instructions into the %s' % path
print '[*] %d IR instructions in %d basic blocks translated' % (len(func), len(func.bb_list))

# save function IR into the JSON file
storage.to_file(path)
```

This script utilises [IDAPython](http://code.google.com/p/idapython/) API and instructions reader from `pyopenreil.utils.IDA` to interact with IDA Pro.

To use this script run «File» &#8594; «Script File» (Alt-F7) IDA command and open `pyopenreil/scripts/ida_translate_func.py`, than script will translate machine code of current subroutine into the IR:

<img src="https://dl.dropboxusercontent.com/u/22903093/openreil/ida_translate_func.png" alt="OpenREIL Python API diagram" width="635" height="203" style="border: 1px solid silver;">

In other script you can use `from_file()` method of `REIL.CodeStorageMem` class to load translated instructions from JSON file and do some code analysis.


### GDB <a id="_7_2"></a>

OpenREIL translation plugin for GDB is located at `pyopenreil/scripts/gdb_reiltrans.py`, it implements `reiltrans` command that allows to generate IR for given instruction, basic block or function and print it to console or save to file.

Usage example:

```
$ gdb tests/fib
GNU gdb (Ubuntu/Linaro 7.4-2012.04-0ubuntu2.1) 7.4-2012.04
Copyright (C) 2012 Free Software Foundation, Inc.
License GPLv3+: GNU GPL version 3 or later <http://gnu.org/licenses/gpl.html>
This is free software: you are free to change and redistribute it.
There is NO WARRANTY, to the extent permitted by law.  Type "show copying"
and "show warranty" for details.
This GDB was configured as "i686-linux-gnu".
For bug reporting instructions, please see:
<http://bugs.launchpad.net/gdb-linaro/>...
Reading symbols from /vagrant_data/openreil/tests/fib...(no debugging symbols found)...done.
(gdb) source pyopenreil/scripts/gdb_reiltrans.py
(gdb) reiltrans
USAGE: reiltrans insn|block|func <address> [options]
(gdb) info address fib
Symbol "fib" is at 0x8048414 in a file compiled without debugging.
(gdb) reiltrans func 0x8048414 --to-file fib.ir
205 instructions saved to fib.ir
```

To fetch machine instructions from GDB in your own script &minus; just use `Reader` class from `pyopenreil.utils.GDB` module:

```python
import gdb
from pyopenreil.utils import GDB

# Inferriors in GDB API are representing programs
# that being run under debugger.
inferrior = gdb.selected_inferior()

# create instruction reader
reader = GDB.Reader(ARCH_X86, inferrior)
```


### WinDbg (kd, cdb) <a id="_7_3"></a>

WinDbg (and also kd, cdb and others) from Microsoft with [pykd](https://pykd.codeplex.com/) Python bindings is also allows to use OpenREIL in similar way. 

Plugin for WinDbg is implemented in `pyopenreil/scripts/kd_reiltrans.py` file, copy it into the `winext` dir of your WinDbg installation (latest version is usually located at` C:\Program Files (x86)\Windows Kits\8.1\Debuggers\x86\winext`).

Usage example:

<img src="https://dl.dropboxusercontent.com/u/22903093/openreil/kd_reiltrans.png" alt="OpenREIL Python API diagram" width="573" height="248" style="border: 1px solid silver;">

Machine code reader for WinDbg is located in `pyopenreil.utils.kd` module:

```python
from pyopenreil.utils import kd

# create instruction reader
reader = kd.Reader(ARCH_X86)
```


## Links and examples <a id="_8"></a>

I made a test program that uses OpenREIL and [Microsoft Z3](http://z3.codeplex.com/) to implement a simple [symbolic execution](http://en.wikipedia.org/wiki/Symbolic_execution) engine for solving [Kao's Toy Project crackme](https://tuts4you.com/download.php?view.3293).

* `tests/test_kao.py` - program source code ([link](../master/tests/test_kao.py)).
* «Automated algebraic cryptanalysis with OpenREIL and Z3» - my article that explains how the program works ([link](http://blog.cr4.sh/2015/03/automated-algebraic-cryptanalysis-with.html)).
* «Kao’s “Toy Project” and Algebraic Cryptanalysis» - [Dcoder's](mailto:dcodr@lavabit.com) article with detailed description of Kao's Toy Project crackme and it's solution ([PDF](https://dl.dropboxusercontent.com/u/22903093/blog/openreil-kao-crackme/solution.pdf)).
* «CONFidence CTF 2015: Reversing 400 "Deobfuscate Me" writeup» - article by [H4x0rPsch0rr team](http://hxp.io/about/) about using OpenREIL to solve CTF task ([link](http://hxp.io/blog/16/CONFidence%20CTF%202015:%20Reversing%20400%20%22Deobfuscate%20Me%22%20writeup/)).


## TODO <a id="_9"></a>

* x86_64 support (VEX already has it).
* Floating point instructions support for x86 and x86_64.
* SSE instructions support for x86 and x86_64.
* AVX instructions support for x86 and x86_64.
* Complete API reference.
* SMT solvers support.
* Symbolic execution.
* REIL &#8594; LLVM IR &#8594; binary code translation.
* Examples of libopenreil usage in PIN and DynamoRIO modules.
* DynamoRIO based tracer that saves IR traces into the MongoDB collection.
* Out of the box support of LLDB and Immunity Debugger.
