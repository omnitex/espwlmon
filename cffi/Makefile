.PHONY: clean

all: build

build:
	python pi_extension_build.py

test:
	python test.py

# TODO _pi.cpython... also needs deleting but on win rd does not accept wildcards?
clean:
ifeq ($(OS),Windows_NT)
	rd /s /q Release _pi.c pi.o
else
	rm -rfd Release _pi.* pi.o
endif

