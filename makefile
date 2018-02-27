# makefile

all: shell

shell_library.o: shell_library.c
	g++ -c -g shell_library.c

main.o: main.c
	g++ -c -g main.c

shell: shell_library.o main.o
	g++ -o shell shell_library.o main.o