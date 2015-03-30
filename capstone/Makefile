
capstone/libcapstone.a: 

	[ -d capstone ] || git clone https://github.com/Cr4sh/capstone.git
	cp config.mk capstone
	cd capstone && ./make.sh

all: capstone/libcapstone.a

clean:

	cd capstone && make clean

install:

