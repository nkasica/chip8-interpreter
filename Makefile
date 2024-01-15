# Makefile for Chip-8 Interpreter

# "make" to compile and create executable
# "make clean" to remove executable
# "make run" to run executable

all:
	gcc -I SDL2\x86_64-w64-mingw32\src\include\SDL2 -L SDL2\x86_64-w64-mingw32\src\lib -o main main.c -lmingw32 -lSDL2main -lSDL2

debug: 
	gcc -I SDL2\x86_64-w64-mingw32\src\include\SDL2 -L SDL2\x86_64-w64-mingw32\src\lib -o main main.c -lmingw32 -lSDL2main -lSDL2 \
		-D=DEBUG
		
run: main.exe
	./main.exe

clean:
	del *.o main.exe