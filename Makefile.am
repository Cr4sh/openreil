BAPDIRS = libasmir libopenreil pyopenreil
SUBDIRS = VEX capstone $(BAPDIRS)

.PHONY: cscope
cscope:

	cscope -b `find . -name "*.[ch]" -or -name "*.hh" -or -name "*.cc" -or -name "*.cpp"`

.PHONY: bap-clean
bap-clean:

	for d in $(BAPDIRS); do $(MAKE) -C $$d clean; done

.PHONY: test
test:

	python tests/run_unittest.py

.PHONY: doc
doc:

	grip --export && mv README.html docs/README.html

