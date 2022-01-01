#include "gameboy.h"
#include "handmade.h"

#include <stdio.h>
#include <stdlib.h>
#include <stdarg.h>

uint8 readMemory(GameBoy* gb, uint16 address) {
    /* if (address == MMR_LY) { */
        /* TODO(octave) : remove this when properly rendering */
        /* return 0x91; */
    /* } */
    
    return gb->memory[address];
}

void writeMemory(GameBoy* gb, uint16 address, uint8 value) {
    gb->memory[address] = value;

    if (address == MMR_SC && value == 0x81) {
        printf("Serial write : '%c' (0x%02X)\n", gb->memory[MMR_SB], gb->memory[MMR_SB]);
    }
}

void triggerInterrupt(GameBoy* gb, enum Interrupt interrupt) {
    uint8 ifFlag = readMemory(gb, MMR_IF);
    writeMemory(gb, MMR_IF, setBit(ifFlag, interrupt));
}

void vGBError(GameBoy* gb, const char* message, va_list args) {
    fprintf(stderr, "Error : ");
    vfprintf(stderr, message, args);
    fprintf(stderr, "\n");

    fprintf(stderr, "Gameboy state :\n\n");
    printGameboyState(gb);

    exit(1);
}

void gbError(GameBoy* gb, const char* message, ...) {
    va_list args;
    va_start(args, message);

    vGBError(gb, message, args);

    va_end(args);
}

uint8 getBit(uint8 byte, uint8 index) {
    return (byte >> index) & 1;
}

uint8 getHighByte(GameBoy* gb, enum Register16 reg) {
    return gb->registers[reg] >> 8;
}

uint8 getLowByte(GameBoy* gb, enum Register16 reg) {
    return gb->registers[reg] & 0xFF;
}

void setHighByte(GameBoy* gb, enum Register16 reg, uint8 value) {
    gb->registers[reg] =
        (value << 8) | (gb->registers[reg] & 0xFF);
}

void setLowByte(GameBoy* gb, enum Register16 reg, uint8 value) {
    gb->registers[reg] =
        (gb->registers[reg] & 0xFF00) | value;
}

bool32 getFlag(GameBoy* gb, enum CPUFlag flag) {
    uint8 flags = getLowByte(gb, REG_AF);

    return getBit(flags, flag);
}

void setFlag(GameBoy* gb, enum CPUFlag flag, bool32 value) {
    uint8 flags = getLowByte(gb, REG_AF);
    uint8 value01 = (value != 0);

    flags = flags & ~(1 << flag); // turn it off
    flags = flags | (value01 << flag);

    setLowByte(gb, REG_AF, flags);
}

uint8 getReg8(GameBoy* gb, uint8 index) {
    switch (index) {
    case REG_B:
        return getHighByte(gb, REG_BC);
    case REG_C:
        return getLowByte(gb, REG_BC);
    case REG_D:
        return getHighByte(gb, REG_DE);
    case REG_E:
        return getLowByte(gb, REG_DE);
    case REG_H:
        return getHighByte(gb, REG_HL);
    case REG_L:
        return getLowByte(gb, REG_HL);
    case REG_A:
        return getHighByte(gb, REG_AF);
    case REG_F:
        return getLowByte(gb, REG_AF);
    default:
        gbError(gb, "Unknown 8-bit register index %u", index);
        break;
    }
}

void setReg8(GameBoy* gb, uint8 index, uint8 value) {
    switch (index) {
    case REG_B:
        setHighByte(gb, REG_BC, value);
        break;
    case REG_C:
        setLowByte(gb, REG_BC, value);
        break;
    case REG_D:
        setHighByte(gb, REG_DE, value);
        break;
    case REG_E:
        setLowByte(gb, REG_DE, value);
        break;
    case REG_H:
        setHighByte(gb, REG_HL, value);
        break;
    case REG_L:
        setLowByte(gb, REG_HL, value);
        break;
    case REG_A:
        setHighByte(gb, REG_AF, value);
        break;
    default:
        gbError(gb, "Unknown 8-bit register index %u", index);
        break;
    }
}

void printGameboyState(GameBoy* gb) {
    printf("Z N H C\n");
    printf("%d %d %d %d\n",
           getFlag(gb, FLAG_Z),
           getFlag(gb, FLAG_N),
           getFlag(gb, FLAG_H),
           getFlag(gb, FLAG_C));
    
    uint8 a = getReg8(gb, REG_A);
    uint8 b = getReg8(gb, REG_B);
    uint8 c = getReg8(gb, REG_C);
    uint8 d = getReg8(gb, REG_D);
    uint8 e = getReg8(gb, REG_E);
    uint8 h = getReg8(gb, REG_H);
    uint8 l = getReg8(gb, REG_L);

    printf("A  = 0x%02X   (%u)\n", a, a);
    printf("B  = 0x%02X   (%u)\n", b, b);
    printf("C  = 0x%02X   (%u)\n", c, c);
    printf("D  = 0x%02X   (%u)\n", d, d);
    printf("E  = 0x%02X   (%u)\n", e, e);
    printf("H  = 0x%02X   (%u)\n", h, h);
    printf("L  = 0x%02X   (%u)\n", l, l);
    
    uint16 af = gb->registers[REG_AF];
    uint16 bc = gb->registers[REG_BC];
    uint16 de = gb->registers[REG_DE];
    uint16 hl = gb->registers[REG_HL];
    uint16 sp = gb->registers[REG_SP];
    uint16 pc = gb->registers[REG_PC];
    
    printf("AF = 0x%04X (%u)\n", af, af);
    printf("BC = 0x%04X (%u)\n", bc, bc);
    printf("DE = 0x%04X (%u)\n", de, de);
    printf("HL = 0x%04X (%u)\n", hl, hl);
    printf("SP = 0x%04X (%u)\n", sp, sp);
    printf("PC = 0x%04X (%u)\n", pc, pc);

    printf("\n");
}

void printGameboyLogLine(FILE* file, GameBoy* gb) {
    uint8 a = getReg8(gb, REG_A);
    uint8 f = getReg8(gb, REG_F);
    uint8 b = getReg8(gb, REG_B);
    uint8 c = getReg8(gb, REG_C);
    uint8 d = getReg8(gb, REG_D);
    uint8 e = getReg8(gb, REG_E);
    uint8 h = getReg8(gb, REG_H);
    uint8 l = getReg8(gb, REG_L);
    
    uint16 sp = gb->registers[REG_SP];
    uint16 pc = gb->registers[REG_PC];

    fprintf(file, "A: %02X F: %02X B: %02X C: %02X D: %02X E: %02X H: %02X L: %02X SP: %04X PC: 00:%04X (%02X %02X %02X %02X)\n",
            a, f, b, c, d, e, h, l, sp, pc,
            readMemory(gb, pc),
            readMemory(gb, pc + 1),
            readMemory(gb, pc + 2),
            readMemory(gb, pc + 3));
}

bool32 loadRom(GameBoy* gb, const char* filename, uint64 expectedSize) {
    bool32 success;
    uint64 fsize = platform.getFileSize(filename, &success);
    
    if (!success) {
        gbError(gb, "Failed to open file %s", filename);
        return false;
    }

    if (fsize != expectedSize) {
        gbError(gb, "File %s has wrong file size 0x%lX (expected 0x%lX)",
                filename, fsize, expectedSize);
    }

    success = platform.readFileIntoMemory(filename, gb->memory, expectedSize);

    return success;
}

void initializeGameboy(GameBoy* gb) {
    REG(AF) = 0x01B0;
    REG(BC) = 0x0013;
    REG(DE) = 0x00D8;
    REG(HL) = 0x014D;
    REG(PC) = 0x0100;
    REG(SP) = 0xFFFE;

    MMR_REG(P1) = 0xCF;
    MMR_REG(SB) = 0x00;
    MMR_REG(SC) = 0x7E;
    MMR_REG(DIV) = 0x18;
    MMR_REG(TIMA) = 0x00;
    MMR_REG(TMA) = 0x00;
    MMR_REG(TAC) = 0xF8;
    MMR_REG(IF) = 0xE1;
    MMR_REG(NR10) = 0x80;
    MMR_REG(NR11) = 0xBF;
    MMR_REG(NR12) = 0xF3;
    MMR_REG(NR13) = 0xFF;
    MMR_REG(NR14) = 0xBF;
    MMR_REG(NR21) = 0x3F;
    MMR_REG(NR22) = 0x00;
    MMR_REG(NR23) = 0xFF;
    MMR_REG(NR24) = 0xBF;
    MMR_REG(NR30) = 0x7F;
    MMR_REG(NR31) = 0xFF;
    MMR_REG(NR32) = 0x9F;
    MMR_REG(NR33) = 0xFF;
    MMR_REG(NR34) = 0xBF;
    MMR_REG(NR41) = 0xFF;
    MMR_REG(NR42) = 0x00;
    MMR_REG(NR43) = 0x00;
    MMR_REG(NR44) = 0xBF;
    MMR_REG(NR50) = 0x77;
    MMR_REG(NR51) = 0xF3;
    MMR_REG(NR52) = 0xF1;
    MMR_REG(LCDC) = 0x91;
    MMR_REG(STAT) = 0x81;
    MMR_REG(SCY) = 0x00;
    MMR_REG(SCX) = 0x00;
    MMR_REG(LY) = 0x91;
    MMR_REG(LYC) = 0x00;
    MMR_REG(DMA) = 0xFF;
    MMR_REG(BGP) = 0xFC;
    MMR_REG(WY) = 0x00;
    MMR_REG(WX) = 0x00;
    MMR_REG(IE) = 0x00;
}
