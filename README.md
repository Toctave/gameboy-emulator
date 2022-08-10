# GameBoy emulator

![image](https://user-images.githubusercontent.com/21238398/184012126-0807f8fe-5163-43f8-8280-171ce331cb6d.png)

A GameBoy emulator written from scratch with a [handmade](https://handmadehero.org/)-style approach. 

The emulator is unfinished but works. 
For instance, it runs Tetris and other games without issue. 
However, the instruction timings are not entirely in sync with the pixel processing unit's, so games that are very timing-sensitive will not run.
The roadmap for improvements goes something like this : 

- Fix timing issues (fiddly to get 100% right)
- Implement cartridge extensions (easy)
- Implement sound (medium / hard ? Timing should be tricky as well)
- Add other convenience features such as keyboard remapping, screenshots and screen capture, save states, etc. (easy)

However, this project was mostly a go at the handmade development philosophy :
- C99
- Small code base, compiles from scratch in less than half asecond. 
- No dependencies beyond bare input/display (see below)
- No call to malloc/realloc/free. No allocation during 
- Optimization mostly through ["non-pessimization"](https://youtu.be/pgoetgxecw8)
- Hot reloading : recompiling while the program is running allows it to use the new code, as long as the ABI between the (tiny) host executable and the main loop (in a dynamic library) is left unchanged.

## Project structure

```
gameboy.c        | Gameboy logistics : read/write from memory, get input, load ROMs, etc.
instructions.c   | Core of the emulator : implementations of the CPU instructions
disassembly.c    | Z80 disassembly for debugging purposes
rendering.c      | Bare-bones implementation of the Gameboy PPU

handmade_*.c     | entry point and orchestration of the program : manages input, hot-reloading, display, etc.
linux_*.c        | linux-specific code
```

## Dependencies

Only dependencies are X11 for window management and input on Linux, and OpenGL for display. 
OpenGL is clearly overkill for this as I'm only displaying a full-screen texture, but it was an easy way to get something on screen.

## Porting

Tested on Ubuntu 22.04. Porting to other platforms should consist in implementing the functions in `handmade.h` in a new file (say, `windows_handmade.c`).
