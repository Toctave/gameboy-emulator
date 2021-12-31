#include "gameboy.h"
#include "handmade.h"

#include <stdio.h>
#include <stdlib.h>

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

    return (flags >> flag) & 1;
}

void setFlag(GameBoy* gb, enum CPUFlag flag, bool32 value) {
    uint8 flags = getLowByte(gb, REG_AF);
    uint8 value01 = value & 1;

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
    default:
        fprintf(stderr, "Unknown 8-bit register index %u\n", index);
        exit(1);
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
        fprintf(stderr, "Unknown 8-bit register index %u\n", index);
        exit(1);
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

bool32 loadRom(GameBoy* gb, const char* filename) {
    bool32 success;
    uint64 fsize = platform.getFileSize(filename, &success);
    
    if (!success) {
        fprintf(stderr, "Failed to open file %s\n", filename);
        return false;
    }

    if (fsize != 0x8000) {
        fprintf(stderr, "File %s has wrong file size 0x%lX (expected 0x8000)\n",
                filename, fsize);
    }

    success = platform.readFileIntoMemory(filename, gb->memory, 0x8000);

    return success;
}
