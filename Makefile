.PHONY: all debug release build clean help

all:
	@echo "Run 'make debug' or 'make release'"

debug: CONFIG = Debug
debug: build

release: CONFIG = Release
release: build

build:
	@echo "Config: $(CONFIG)"
	@mkdir -p build
	cmake -S . -B build -DCMAKE_BUILD_TYPE=$(CONFIG) && cmake --build build -- -j 6

clean:
	rm -rf build

help: all
