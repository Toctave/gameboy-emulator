#pragma once

#define GAMEBOY_SCREEN_WIDTH 160
#define GAMEBOY_SCREEN_HEIGHT 144

#include "handmade_types.h"

typedef struct GameBoy {
    uint8 screen[GAMEBOY_SCREEN_HEIGHT][GAMEBOY_SCREEN_WIDTH];

    uint16 registers[6];
    uint8 memory[1 << 16];
} GameBoy;

#define REG(name) gb->registers[REG_##name]
#define MEM(addr) gb->memory[addr]

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
    REG_A = 7,
};

enum MemoryMappedRegister {
    MMR_LCDC = 0xFF40,
    MMR_SCY = 0xFF42,
    MMR_SCX = 0xFF43,
    MMR_LY  = 0xFF44,
    MMR_LYC = 0xFF45,
    MMR_BGP = 0xFF47,
    MMR_OBP0 = 0xFF48,
    MMR_OBP1 = 0xFF49,
    MMR_WY  = 0xFF4A,
    MMR_WX  = 0xFF4B,
    MMR_IME = 0xFFFF,
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

uint8 getBit(uint8 byte, uint8 index);

uint8 getHighByte(GameBoy* gb, enum Register16 reg);
uint8 getLowByte(GameBoy* gb, enum Register16 reg);
void setHighByte(GameBoy* gb, enum Register16 reg, uint8 value);
void setLowByte(GameBoy* gb, enum Register16 reg, uint8 value);

bool32 getFlag(GameBoy* gb, enum CPUFlag flag);
void setFlag(GameBoy* gb, enum CPUFlag flag, bool32 value);

uint8 getReg8(GameBoy* gb, uint8 index);
void setReg8(GameBoy* gb, uint8 index, uint8 value);

void executeInstruction(GameBoy* gb);
void printGameboyState(GameBoy* gb);

bool32 loadRom(GameBoy* gb, const char* filename);
void drawBackground(GameBoy* gb);

void gbError(GameBoy* gb, const char* message, ...);
