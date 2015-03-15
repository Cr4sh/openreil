
capstone/libcapstone.a: 

	[ -d capstone ] || sh -c 'git clone https://github.com/aquynh/capstone.git && cd capstone && git checkout 34271c20ff8c00d588f2484aa4882b5e5267c147'
	cp config.mk capstone
	cd capstone && ./make.sh

all: capstone/libcapstone.a

clean:

	cd capstone && make clean

install:
