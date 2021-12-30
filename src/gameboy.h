#pragma once

#define GAMEBOY_SCREEN_WIDTH 160
#define GAMEBOY_SCREEN_HEIGHT 144

#include "handmade_types.h"

typedef struct GameBoy {
    uint8 screen[GAMEBOY_SCREEN_HEIGHT][GAMEBOY_SCREEN_WIDTH];

    uint16 registers[6];
    uint8 memory[1 << 16];
} GameBoy;

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

void executeInstruction(GameBoy* gb);
