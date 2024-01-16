# CHIP-8 Interpreter

CHIP-8 is an interpreted programming language, originally created by Joseph Weisbecker for the [COSMAC VIP](https://en.wikipedia.org/wiki/COSMAC_VIP) and [Telmac 1800](https://en.wikipedia.org/wiki/Telmac_1800) microcomputers in the mid-1970s. It was originally created to allow video games to be more easily developed for these systems. Many classic games such as Pong, Space Invaders, Tetris, and Brix were ported to the CHIP-8. Even today, games are still being developed to be played on interpreters through [Octo](https://github.com/JohnEarnest/Octo)! 

## Images and GIFs

Splash Screen

![image](https://github.com/nkasica/chip8-interpreter/assets/156490730/6c6926a3-047e-4add-b4e6-8931ac04ddcd)

Tetris (Ignore my cursor 😅)

![tetris](https://github.com/nkasica/chip8-interpreter/assets/156490730/9b15e6e7-763e-45b6-b8d6-ab7b6325b01c)

Brix -- Play [video](https://clipchamp.com/watch/XajaOpQlwdl) to hear sound!

## Getting Started

### Dependencies

* [SDL2](https://github.com/libsdl-org/SDL/releases/tag/release-2.28.5)
* [MinGW-w64](https://www.mingw-w64.org/downloads/)


### Compilation

Type `make` in the terminal to create an executable

Alternatively, type `make debug` if you want to enable debugging outputs while the program runs

To remove the executable, type `make clean`

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

### Keypad
This image describes how a QWERTY keyboard is translated into the hexadecimal keypad from the original CHIP-8 systems

![image](https://github.com/nkasica/chip8-interpreter/assets/156490730/cf6abc7f-258e-4340-8ca9-845fcde12187)

When using the emulator, your QWERTY inputs will register as the corresponding hexadecimal keypad input.



## Very Helpful Resources!

* [CHIP-8 Wiki](https://en.wikipedia.org/wiki/CHIP-8)
* [SDL Wiki](https://wiki.libsdl.org/SDL2/APIByCategory)
* [Tobias' Guide](https://tobiasvl.github.io/blog/write-a-chip-8-emulator/)
* [Cowgod's Technical Reference](http://devernay.free.fr/hacks/chip8/C8TECH10.HTM)
* [Sound Design](https://blog.tigris.fr/2020/05/13/writing-an-emulator-sound-is-complicated/)
* [Queso Fuego's CHIP-8 Youtube Series](https://www.youtube.com/playlist?list=PLT7NbkyNWaqbyBMzdySdqjnfUFxt8rnU_)
