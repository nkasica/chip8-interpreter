# CHIP-8 Interpreter

CHIP-8 is an interpreted programming language, originally created by Joseph Weisbecker for the [COSMAC VIP](https://en.wikipedia.org/wiki/COSMAC_VIP) and [Telmac 1800](https://en.wikipedia.org/wiki/Telmac_1800) microcomputers in the mid-1970s. It was originally created to allow video games to be more easily developed for these systems. Many classic games such as Pong, Space Invaders, Tetris, and Brix were ported to the CHIP-8. Even today, games are still being developed to be played on interpreters through [Octo](https://github.com/JohnEarnest/Octo)! 

## Description

An in-depth paragraph about your project and overview of use.

## Getting Started

### Dependencies

* [SDL2](https://github.com/libsdl-org/SDL/releases/tag/release-2.28.5)

### Installing

* How/where to download your program
* Any modifications needed to be made to files/folders

### Compilation

Type "make" in the terminal to create an executable
```
make
```
Alternatively, type "make debug" if you want to enable debugging outputs while the program runs
```
make debug
```
To remove the executable, type "make clean"
```
make clean
```

### Usage
To run the executable, use the following command:
```
./main.exe <ROM/PATH.ch8>
```

Here are some other useful features to use while emulating:
* Pause / Unpause emulation (space bar)
* Exit (Esc)
* Lower Volume (-)
* Raise Volume (=)

## Very Helpful Resources!

* [CHIP-8 Wiki](https://en.wikipedia.org/wiki/CHIP-8)
* [SDL Wiki](https://wiki.libsdl.org/SDL2/APIByCategory)
* [Tobias' Guide](https://tobiasvl.github.io/blog/write-a-chip-8-emulator/)
* [Cowgod's Technical Reference](http://devernay.free.fr/hacks/chip8/C8TECH10.HTM)
* [Sound Design](https://blog.tigris.fr/2020/05/13/writing-an-emulator-sound-is-complicated/)
* [Queso Fuego's CHIP-8 Youtube Series](https://www.youtube.com/playlist?list=PLT7NbkyNWaqbyBMzdySdqjnfUFxt8rnU_)
