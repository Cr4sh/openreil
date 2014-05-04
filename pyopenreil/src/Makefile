
# get Python version and path
PYTHON := python
PYVERSION := $(shell $(PYTHON) -c "import sys; print(sys.version[ : 3])")

# get python libraries and headers directories
INCDIR := $(shell $(PYTHON) -c "from distutils import sysconfig; print(sysconfig.get_python_inc())")
PLATINCDIR := $(shell $(PYTHON) -c "from distutils import sysconfig; print(sysconfig.get_python_inc(plat_specific = True))")

../translator.so: translator.o ../../libopenreil/src/libopenreil.a ../../libasmir/src/libasmir.a ../../VEX/libvex.a
	$(CXX) -pthread -shared -o $@ $^ -ldl -lbfd -lopcodes -lz -liberty -lpython$(PYVERSION) $(MINGW_LIBS)

translator.o: translator.cpp translator.pyx libopenreil.pxd
	$(CXX) -c translator.cpp -I$(INCDIR) -I$(PLATINCDIR) -I../../libopenreil/include

CYTHON := cython
translator.cpp: translator.pyx
	$(CYTHON) --embed --cplus translator.pyx

all: ../translator.so

clean:
	@rm *.o *.cpp ../translator.so

