#pragma once

#define GAMEBOY_SCREEN_WIDTH 160
#define GAMEBOY_SCREEN_HEIGHT 144
#define GAMEBOY_CPU_FREQUENCY 4194304

#include <stdio.h>
#include "handmade.h"

enum JoypadButton {
    JP_A,
    JP_B,
    JP_SELECT,
    JP_START,
    JP_RIGHT,
    JP_LEFT,
    JP_UP,
    JP_DOWN,
};

typedef struct FIFOPixel {
    uint8 colorIndex;
} FIFOPixel;

typedef struct GameBoy {
    uint16 registers[6];
    uint8 memory[1 << 16];

    uint8 ime;
    uint8 joypad;

    uint16 variableCycles;
    uint16 clock;
    bool32 halted;

    // rendering
    uint8 screen[GAMEBOY_SCREEN_HEIGHT][GAMEBOY_SCREEN_WIDTH];
    FIFOPixel backgroundFifo[16];
    uint8 backgroundFifoStart;
    uint8 backgroundFifoEnd;
    
    FIFOPixel spriteFifo[16];
    uint8 spriteFifoStart;
    uint8 spriteFifoEnd;
    
    // debugging
    uint16 callStackHeight;
    uint32 tracing;
} GameBoy;

#define REG(name) gb->registers[REG_##name]
#define MMR_REG(name) gb->memory[MMR_##name]

enum Register16 {
    REG_AF,
    REG_BC,
    REG_DE,
    REG_HL,
    REG_SP,
    REG_PC,
};

enum Register8 {
    REG_B = 0,
    REG_C = 1,
    REG_D = 2,
    REG_E = 3,
    REG_H = 4,
    REG_L = 5,
    REG_F = 6,
    REG_A = 7,
};

enum MemoryMappedRegister {
    MMR_P1 = 0xFF00,
    MMR_SB = 0xFF01,
    MMR_SC = 0xFF02,
    MMR_DIV = 0xFF04,
    MMR_TIMA = 0xFF05,
    MMR_TMA = 0xFF06,
    MMR_TAC = 0xFF07,
    MMR_IF = 0xFF0F,
    MMR_NR10 = 0xFF10,
    MMR_NR11 = 0xFF11,
    MMR_NR12 = 0xFF12,
    MMR_NR13 = 0xFF13,
    MMR_NR14 = 0xFF14,
    MMR_NR21 = 0xFF16,
    MMR_NR22 = 0xFF17,
    MMR_NR23 = 0xFF18,
    MMR_NR24 = 0xFF19,
    MMR_NR30 = 0xFF1A,
    MMR_NR31 = 0xFF1B,
    MMR_NR32 = 0xFF1C,
    MMR_NR33 = 0xFF1D,
    MMR_NR34 = 0xFF1E,
    MMR_NR41 = 0xFF20,
    MMR_NR42 = 0xFF21,
    MMR_NR43 = 0xFF22,
    MMR_NR44 = 0xFF23,
    MMR_NR50 = 0xFF24,
    MMR_NR51 = 0xFF25,
    MMR_NR52 = 0xFF26,
    MMR_WAV  = 0xFF30,
    MMR_LCDC = 0xFF40,
    MMR_STAT = 0xFF41,
    MMR_SCY = 0xFF42,
    MMR_SCX = 0xFF43,
    MMR_LY  = 0xFF44,
    MMR_LYC = 0xFF45,
    MMR_DMA = 0xFF46,
    MMR_BGP = 0xFF47,
    MMR_OBP0 = 0xFF48,
    MMR_OBP1 = 0xFF49,
    MMR_WY  = 0xFF4A,
    MMR_WX  = 0xFF4B,
    MMR_IE = 0xFFFF,
};

enum CPUFlag {
    FLAG_C = 4,
    FLAG_H = 5,
    FLAG_N = 6,
    FLAG_Z = 7,
};

enum SpecialAddress {
    TILE_DATA_BLOCK0 = 0x8000,
    TILE_DATA_BLOCK1 = 0x8800,
    TILE_DATA_BLOCK2 = 0x9000,

    TILEMAP0 = 0x9800,
    TILEMAP1 = 0x9C00,
};

enum Interrupt {
    INT_VBLANK,
    INT_LCDC,
    INT_TIMER,
    INT_SERIAL,
    INT_JOYPAD,
};

bool32 handleKey(GameBoy* gb, KeyIndex index, PressFlag pressFlag);
uint8 setBit(uint8 value, uint8 index);
uint8 resetBit(uint8 value, uint8 index);
    
void pressButton(GameBoy* gb, enum JoypadButton button);
void releaseButton(GameBoy* gb, enum JoypadButton button);

uint8 readMemory(GameBoy* gb, uint16 address);
void writeMemory(GameBoy* gb, uint16 address, uint8 value);

void triggerInterrupt(GameBoy* gb, enum Interrupt interrupt);

uint8 getBit(uint8 byte, uint8 index);
uint8 resetBit(uint8 value, uint8 index);
uint8 setBit(uint8 value, uint8 index);

uint8 getHighByte(GameBoy* gb, enum Register16 reg);
uint8 getLowByte(GameBoy* gb, enum Register16 reg);
void setHighByte(GameBoy* gb, enum Register16 reg, uint8 value);
void setLowByte(GameBoy* gb, enum Register16 reg, uint8 value);

bool32 getFlag(GameBoy* gb, enum CPUFlag flag);
void setFlag(GameBoy* gb, enum CPUFlag flag, bool32 value);

uint8 getReg8(GameBoy* gb, uint8 index);
void setReg8(GameBoy* gb, uint8 index, uint8 value);

void executeCycle(GameBoy* gb);

void gbprintf(GameBoy* gb, const char* message, ...);
void printGameboyState(GameBoy* gb);
void printGameboyLogLine(FILE* file, GameBoy* gb);

bool32 loadRom(GameBoy* gb, const char* filename, uint64 expectedSize);

void drawBackground(GameBoy* gb);
void drawScreen(GameBoy* gb);

void gbError(GameBoy* gb, const char* message, ...);

void initializeGameboy(GameBoy* gb);
