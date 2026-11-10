.PHONY: all test clean
all:
	./build.sh

test: all
	python3 -m unittest discover -s tests -v

clean:
	rm -f bin/pipi-c bin/pipi-vm bin/pipiwood bin/pipi-c.exe bin/pipi-vm.exe bin/pipiwood.exe
	rm -f dist/*.pbc dist/woodlibs/*.woodlib
