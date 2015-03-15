
capstone/libcapstone.a: 

	[ -d capstone ] || tar -xf capstone-source.tar.bz2
	cd capstone && ./make.sh

all: capstone/libcapstone.a

clean:

	cd capstone && make clean

install:
