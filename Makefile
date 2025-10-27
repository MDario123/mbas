default: build

build: bin/mbas

bin/mbas: src/main.c
	mkdir -p bin
	cc -o bin/mbas src/main.c
	chmod +x bin/mbas

run_debug: build_debug
	./bin/mbas_debug

build_debug: bin/mbas_debug

bin/mbas_debug: src/main.c
	echo "#define DEBUG \n" | cat - src/main.c > src/main_debug.c
	mkdir -p bin
	cc -g -o bin/mbas_debug src/main_debug.c
	chmod +x bin/mbas_debug
	rm src/main_debug.c

clean:
	rm -rf bin
