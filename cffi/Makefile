.PHONY: clean
PROG=wl_analysis

all: build

build:
	python wl_extension_build.py

test:
	python test.py

clean:
	rm -f _$(PROG).* $(PROG).o

