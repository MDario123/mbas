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
	cc tmp/mbas.o tmp/tomlc17.o -o bin/mbas $$(pkg-config --libs libpipewire-0.3)
	chmod +x bin/mbas

tmp/mbas.o: src/main.c src/*
	mkdir -p tmp
	cc -c src/main.c -o tmp/mbas.o $$(pkg-config --cflags libpipewire-0.3)

# ==============================
# Dependencies
# ==============================
tmp/tomlc17.o: deps/tomlc17/tomlc17.c
	cc -c deps/tomlc17/tomlc17.c -o tmp/tomlc17.o

# ==============================
# Utils
# ==============================
clean:
	rm -rf bin
	rm -rf tmp
