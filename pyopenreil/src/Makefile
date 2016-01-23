
# get Python version and path
PYTHON = python
PYVERSION = $(shell $(PYTHON) -c "import sys; print(sys.version[ : 3])")
PYEXT = $(shell $(PYTHON) -c "import sys; print 'pyd' if sys.platform == 'win32' else 'so'")

include makefile.inc

# get python libraries and headers directories
INCDIR = $(shell $(PYTHON) -c "from distutils import sysconfig; print(sysconfig.get_python_inc())")
PLATINCDIR = $(shell $(PYTHON) -c "from distutils import sysconfig; print(sysconfig.get_python_inc(plat_specific = True))")

../translator.$(PYEXT): translator.o ../../libopenreil/src/libopenreil.a
	$(CXX) $(CXXFLAGS) -pthread -shared -o $@ $^ -lpython$(PYVERSION)

translator.o: translator.cpp translator.pyx libopenreil.pxd
	$(CXX) $(CXXFLAGS) -c -fPIC translator.cpp -I$(INCDIR) -I$(PLATINCDIR) -I../../libopenreil/include

CYTHON = cython
translator.cpp: translator.pyx
	$(CYTHON) --embed --cplus translator.pyx

all: ../translator.$(PYEXT)

clean:
	@rm *.o *.cpp ../translator.$(PYEXT)

# get Python site-packages directory path
LIBDIR = $(shell $(PYTHON) -c "from distutils import sysconfig; print(sysconfig.get_python_lib().replace(chr(92), chr(47)))")
INSTALLDIR = $(LIBDIR)/pyopenreil

install:
	-mkdir $(INSTALLDIR)
	-mkdir $(INSTALLDIR)/arch	
	-mkdir $(INSTALLDIR)/utils
	-mkdir $(INSTALLDIR)/scripts
	cp ../translator.$(PYEXT) $(INSTALLDIR)
	cp ../*.py $(INSTALLDIR)
	cp ../arch/*.py $(INSTALLDIR)/arch	
	cp ../utils/*.py $(INSTALLDIR)/utils
	cp ../scripts/*.py $(INSTALLDIR)/scripts
