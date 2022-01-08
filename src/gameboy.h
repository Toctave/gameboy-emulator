#pragma once

#define GAMEBOY_SCREEN_WIDTH 160
#define GAMEBOY_SCREEN_HEIGHT 144
#define GAMEBOY_CPU_FREQUENCY 4194304
#define GAMEBOY_CYCLES_PER_SCANLINE 456
#define GAMEBOY_LY_VBLANK 144
#define GAMEBOY_LY_MAX 154


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

typedef struct PixelFIFO {
    FIFOPixel pixels[16];
    uint8 start;
    uint8 end;
} PixelFIFO;

typedef struct GameBoy {
    // memory
    uint16 registers[6];
    uint8 rom[2 * 1024 * 1024]; // Max 16Mb = 2MB total rom
    uint8 ram[8 * 1024]; // 8KB base RAM
    uint8 externalRam[128 * 1024]; // up to 128KB cartridge RAM
    uint8 vram[8 * 1024]; // 8KB video RAM
    uint8 oam[160];
    uint8 io[128];
    uint8 hram[256];
    uint8 ie;
    uint8 ime;

    uint8 joypad;

    // Memory bank controller
    struct {
        uint8 ramEnable;
        uint8 romBankIndex;
        uint8 ramBankIndex;
        uint8 bankingMode;
        
        uint16 romBankCount;
        uint16 ramBankCount;
    } mbc1;

    // timing
    uint16 variableCycles;
    uint32 clock;
    uint16 timerAccumulator;
    uint16 renderingAccumulator;
    
    bool32 halted;

    // rendering
    uint8 screen[GAMEBOY_SCREEN_HEIGHT][GAMEBOY_SCREEN_WIDTH];
    PixelFIFO backgroundFifo;
    PixelFIFO spriteFifo;
    bool32 frameReady;
    
    // debugging
    uint16 callStackHeight;
    uint32 tracing;
} GameBoy;

#define REG(name) gb->registers[REG_##name]

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

#define IO(name) gb->io[IO_##name & 0xFF]

enum IORegister {
    IO_P1 = 0xFF00,
    IO_SB = 0xFF01,
    IO_SC = 0xFF02,
    IO_DIV = 0xFF04,
    IO_TIMA = 0xFF05,
    IO_TMA = 0xFF06,
    IO_TAC = 0xFF07,
    IO_IF = 0xFF0F,
    IO_NR10 = 0xFF10,
    IO_NR11 = 0xFF11,
    IO_NR12 = 0xFF12,
    IO_NR13 = 0xFF13,
    IO_NR14 = 0xFF14,
    IO_NR21 = 0xFF16,
    IO_NR22 = 0xFF17,
    IO_NR23 = 0xFF18,
    IO_NR24 = 0xFF19,
    IO_NR30 = 0xFF1A,
    IO_NR31 = 0xFF1B,
    IO_NR32 = 0xFF1C,
    IO_NR33 = 0xFF1D,
    IO_NR34 = 0xFF1E,
    IO_NR41 = 0xFF20,
    IO_NR42 = 0xFF21,
    IO_NR43 = 0xFF22,
    IO_NR44 = 0xFF23,
    IO_NR50 = 0xFF24,
    IO_NR51 = 0xFF25,
    IO_NR52 = 0xFF26,
    IO_WAV  = 0xFF30,
    IO_LCDC = 0xFF40,
    IO_STAT = 0xFF41,
    IO_SCY = 0xFF42,
    IO_SCX = 0xFF43,
    IO_LY  = 0xFF44,
    IO_LYC = 0xFF45,
    IO_DMA = 0xFF46,
    IO_BGP = 0xFF47,
    IO_OBP0 = 0xFF48,
    IO_OBP1 = 0xFF49,
    IO_WY  = 0xFF4A,
    IO_WX  = 0xFF4B,
};

enum CPUFlag {
    FLAG_C = 4,
    FLAG_H = 5,
    FLAG_N = 6,
    FLAG_Z = 7,
};

enum SpecialAddress {
    ROM_SWITCHABLE_BANK_START = 0x4000,
    VRAM_START = 0x8000,
    EXTERNAL_RAM_START = 0xA000,
    INTERNAL_RAM_START = 0xC000,
    ECHO_RAM_START = 0xE000,
    OAM_START = 0xFE00,
    FORBIDDEN_REGION_START = 0xFEA0,
    IO_PORTS_START = 0xFF00,
    HRAM_START = 0xFF80,
    IE_ADDRESS = 0xFFFF,
    
    TILE_DATA_BLOCK0 = 0x8000,
    TILE_DATA_BLOCK1 = 0x8800,
    TILE_DATA_BLOCK2 = 0x9000,

    TILEMAP0 = 0x9800,
    TILEMAP1 = 0x9C00,

    SPRITE_TILES_TABLE = 0x8000,
    SPRITE_ATTRIBUTE_TABLE = 0xFE00,

    CART_TYPE = 0x0147,
    CART_ROM_SIZE = 0x0148,
    CART_RAM_SIZE = 0x0149,
};

enum Interrupt {
    INT_VBLANK,
    INT_LCDC,
    INT_TIMER,
    INT_SERIAL,
    INT_JOYPAD,
};

enum Conditional {
    COND_NZ,
    COND_Z,
    COND_NC,
    COND_C,
};

enum CartridgeType {
    CART_ROM_ONLY = 0x00,
    CART_MBC1 = 0x01,
};

uint16 get16BitArgument(uint8* instr);
int8 getSigned8BitArgument(uint8* instr);


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

bool32 loadRom(GameBoy* gb, const char* filename);

void drawBackground(GameBoy* gb);
void drawScreen(GameBoy* gb);

void gbError(GameBoy* gb, const char* message, ...);

void initializeGameboy(GameBoy* gb);
