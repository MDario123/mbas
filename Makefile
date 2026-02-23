CFLAGS_DEBUG = -O0 -g -Wall
CFLAGS_RELEASE = -O3
CFLAGS_RELEASE_NATIVE = -O3 -march=native -mtune=native

BUILD ?= RELEASE
CFLAGS = $(CFLAGS_$(BUILD))

default: build

# ==============================
# Regular build
# ==============================
run: build
	@echo "Running mbas..."
	@./bin/mbas

build: bin/mbas

bin/mbas: tmp/mbas.o tmp/tomlc17.o
	mkdir -p bin
	cc $(CFLAGS) tmp/mbas.o tmp/tomlc17.o -o bin/mbas $$(pkg-config --libs libpipewire-0.3)
	chmod +x bin/mbas
ifneq ($(filter RELEASE%,$(BUILD)),)
	strip bin/mbas
endif

tmp/mbas.o: src/main.c src/*
	mkdir -p tmp
	cc $(CFLAGS) -c src/main.c -o tmp/mbas.o $$(pkg-config --cflags libpipewire-0.3)

# ==============================
# Dependencies
# ==============================
tmp/tomlc17.o: deps/tomlc17/tomlc17.c
	cc $(CFLAGS) -c deps/tomlc17/tomlc17.c -o tmp/tomlc17.o

# ==============================
# Utils
# ==============================
clean:
	rm -rf bin
	rm -rf tmp
