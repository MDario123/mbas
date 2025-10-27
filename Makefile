default: build

# ==============================
# Regular build
# ==============================
build: bin/mbas

bin/mbas: tmp/mbas.o tmp/tomlc17.o
	mkdir -p bin
	cc tmp/mbas.o tmp/tomlc17.o -o bin/mbas
	chmod +x bin/mbas

tmp/mbas.o: src/main.c src/*
	mkdir -p tmp
	cc -c src/main.c -o tmp/mbas.o

# ==============================
# Debug build
# ==============================
run_debug: build_debug
	@echo "Running mbas in debug mode..."
	@./bin/mbas_debug

build_debug: bin/mbas_debug

bin/mbas_debug: tmp/mbas_debug.o tmp/tomlc17.o
	mkdir -p bin
	cc -g tmp/mbas_debug.o tmp/tomlc17.o -o bin/mbas_debug 
	chmod +x bin/mbas_debug

tmp/mbas_debug.o: src/main.c src/*
	echo "#define DEBUG \n" | cat - src/main.c > src/main_debug.c
	mkdir -p tmp
	cc -g -c src/main_debug.c -o tmp/mbas_debug.o
	rm src/main_debug.c

# ==============================
# Dependencies
# ==============================
tmp/tomlc17.o: deps/tomlc17/tomlc17.c
	cc -g -c deps/tomlc17/tomlc17.c -o tmp/tomlc17.o

# ==============================
# Utils
# ==============================
clean:
	rm -rf bin
	rm -rf tmp
