#include "gameboy.h"
#include "handmade.h"

#include <stdio.h>
#include <stdlib.h>
#include <stdarg.h>

bool32 handleKey(GameBoy* gb, KeyIndex index, PressFlag pressFlag) {
    enum JoypadButton button;
    
    switch (index) {
    case KID_W:
        button = JP_UP;
        break;
    case KID_A:
        button = JP_LEFT;
        break;
    case KID_S:
        button = JP_DOWN;
        break;
    case KID_D:
        button = JP_RIGHT;
        break;
    case KID_J:
        button = JP_B;
        break;
    case KID_I:
        button = JP_A;
        break;
    case KID_5:
        button = JP_START;
        break;
    case KID_6:
        button = JP_SELECT;
        break;
    default:
        return false;
    }

    if (pressFlag == PRESS) {
        pressButton(gb, button);
    } else if (pressFlag == RELEASE) {
        releaseButton(gb, button);
    } else {
        return false;
    }

    return true;
}

uint8 setBit(uint8 value, uint8 index) {
    return value | (1 << index);
}

uint8 resetBit(uint8 value, uint8 index) {
    return value & ~(1 << index);
}

void pressButton(GameBoy* gb, enum JoypadButton button) {
    gb->joypad = resetBit(gb->joypad, button);
}

void releaseButton(GameBoy* gb, enum JoypadButton button) {
    gb->joypad = setBit(gb->joypad, button);
}

uint8 readMemory(GameBoy* gb, uint16 address) {
    if (address == IE_ADDRESS) {
        return gb->ie;
    } else if (address >= HRAM_START) {
        return gb->hram[address - HRAM_START];
    } else if (address >= IO_PORTS_START) {
        uint8 regIndex = address & 0xFF;
        uint8 reg = gb->io[regIndex];
        
        switch (address) {
        case IO_P1:
            if (!(reg & 0x20)) { // Button keys
                return (gb->joypad & 0xF) | (reg & 0xF0);
            } else if (!(reg & 0x10)) { // Direction keys
                return (gb->joypad >> 4) | (reg & 0xF0);
            } else {
                return 0x0F | (reg & 0xF0);
            }
            break;
        case IO_LY:
            /* TODO(octave) : remove this when properly rendering */
            gb->io[regIndex]++;
            return reg;
        default:
            return reg;
        }
    } else if (address >= FORBIDDEN_REGION_START) {
        gbError(gb, "Reading from invalid memory location 0x%04X (forbidden)\n", address);
        return 0;
    } else if (address >= OAM_START) {
        return gb->oam[address - OAM_START];
    } else if (address >= ECHO_RAM_START) {
        gbError(gb, "Reading from invalid memory location 0x%04X (echo RAM)\n", address);
        return 0;
    } else if (address >= INTERNAL_RAM_START) {
        return gb->ram[address - INTERNAL_RAM_START];
    } else if (address >= EXTERNAL_RAM_START) {
        gbError(gb, "Writing to invalid memory location 0x%04X (external RAM)\n", address);
        return 0;
    } else if (address >= VRAM_START) {
        return gb->vram[address - VRAM_START];
    } else if (address >= ROM_SWITCHABLE_BANK_START) {
        // TODO(octave) : bank switching
        return gb->rom[address];
    } else {
        return gb->rom[address];
    }
}

void writeMemory(GameBoy* gb, uint16 address, uint8 value) {
    if (address == IE_ADDRESS) {
        gb->ie = value;
    } else if (address >= HRAM_START) {
        gb->hram[address - HRAM_START] = value;
    } else if (address >= IO_PORTS_START) {
        switch (address) {
        case IO_DMA: {
            uint16 source = value << 8;
            uint16 destination = SPRITE_ATTRIBUTE_TABLE;

            while (destination < SPRITE_ATTRIBUTE_TABLE + 0xA0) {
                uint8 byte = readMemory(gb, source++);
                writeMemory(gb, destination++, byte);
            }
            return;
        }
        case IO_SC:
            if (value == 0x81) {
                printf("%c", IO(SB));
            }
            break;
        case IO_DIV:
            IO(DIV) = 0;
            return;
        default:
            gb->io[address & 0xFF] = value;
        }
    } else if (address >= FORBIDDEN_REGION_START) {
        gbprintf(gb, "Writing to invalid memory location 0x%04X (forbidden)\n", address);
    } else if (address >= OAM_START) {
        gb->oam[address - OAM_START] = value;
    } else if (address >= ECHO_RAM_START) {
        gbError(gb, "Writing to invalid memory location 0x%04X (echo RAM)\n", address);
    } else if (address >= INTERNAL_RAM_START) {
        gb->ram[address - INTERNAL_RAM_START] = value;
    } else if (address >= EXTERNAL_RAM_START) {
        gbError(gb, "Writing to invalid memory location 0x%04X (external RAM)\n", address);
    } else if (address >= VRAM_START) {
        gb->vram[address - VRAM_START] = value;
    } else if (address >= ROM_SWITCHABLE_BANK_START) {
        gbprintf(gb, "Attempt to write to ROM at 0x%04X (bank 1+)\n", address);
    } else {
        gbprintf(gb, "Attempt to write to ROM at 0x%04X (bank 0)\n", address);
    }
}

void triggerInterrupt(GameBoy* gb, enum Interrupt interrupt) {
    uint8 ifFlag = readMemory(gb, IO_IF);
    writeMemory(gb, IO_IF, setBit(ifFlag, interrupt));
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

    success = platform.readFileIntoMemory(filename, gb->rom, expectedSize);

    return success;
}

void initializeGameboy(GameBoy* gb) {
    gb->joypad = 0xFF;
    
    REG(AF) = 0x01B0;
    REG(BC) = 0x0013;
    REG(DE) = 0x00D8;
    REG(HL) = 0x014D;
    REG(PC) = 0x0100;
    REG(SP) = 0xFFFE;

    IO(P1) = 0xCF;
    IO(SB) = 0x00;
    IO(SC) = 0x7E;
    IO(DIV) = 0x18;
    IO(TIMA) = 0x00;
    IO(TMA) = 0x00;
    IO(TAC) = 0xF8;
    IO(IF) = 0xE1;
    IO(NR10) = 0x80;
    IO(NR11) = 0xBF;
    IO(NR12) = 0xF3;
    IO(NR13) = 0xFF;
    IO(NR14) = 0xBF;
    IO(NR21) = 0x3F;
    IO(NR22) = 0x00;
    IO(NR23) = 0xFF;
    IO(NR24) = 0xBF;
    IO(NR30) = 0x7F;
    IO(NR31) = 0xFF;
    IO(NR32) = 0x9F;
    IO(NR33) = 0xFF;
    IO(NR34) = 0xBF;
    IO(NR41) = 0xFF;
    IO(NR42) = 0x00;
    IO(NR43) = 0x00;
    IO(NR44) = 0xBF;
    IO(NR50) = 0x77;
    IO(NR51) = 0xF3;
    IO(NR52) = 0xF1;
    IO(LCDC) = 0x91;
    IO(STAT) = 0x81;
    IO(SCY) = 0x00;
    IO(SCX) = 0x00;
    IO(LY) = 0x91;
    IO(LYC) = 0x00;
    IO(DMA) = 0xFF;
    IO(BGP) = 0xFC;
    IO(WY) = 0x00;
    IO(WX) = 0x00;

    gb->ie = 0x00;
}
