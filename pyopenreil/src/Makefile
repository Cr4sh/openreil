
# get Python version and path
PYTHON := python
PYVERSION := $(shell $(PYTHON) -c "import sys; print(sys.version[ : 3])")
PYEXT := $(shell $(PYTHON) -c "import sys; print 'pyd' if sys.platform == 'win32' else 'so'")

include makefile.inc

# get python libraries and headers directories
INCDIR := $(shell $(PYTHON) -c "from distutils import sysconfig; print(sysconfig.get_python_inc())")
PLATINCDIR := $(shell $(PYTHON) -c "from distutils import sysconfig; print(sysconfig.get_python_inc(plat_specific = True))")

../translator.$(PYEXT): translator.o ../../libopenreil/src/libopenreil.a ../../libasmir/src/libasmir.a ../../VEX/libvex.a ../../capstone/capstone/libcapstone.a
	$(CXX) -pthread -shared -o $@ $^ -lpython$(PYVERSION)

translator.o: translator.cpp translator.pyx libopenreil.pxd
	$(CXX) -c translator.cpp -I$(INCDIR) -I$(PLATINCDIR) -I../../libopenreil/include

CYTHON := cython
translator.cpp: translator.pyx
	$(CYTHON) --embed --cplus translator.pyx

all: ../translator.$(PYEXT)

clean:
	@rm *.o *.cpp ../translator.$(PYEXT)

