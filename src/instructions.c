#include "gameboy.h"

#include "handmade.h"

#include <stdio.h>
#include <stdlib.h>
#include <stdarg.h>

#define WR(address, value) writeMemory(gb, address, value)
#define RD(address) readMemory(gb, address)

#define INSTRUCTION_EXECUTE_FN_NOSTATIC(name) \
    void name(GameBoy* gb, uint8 opcode)
#define INSTRUCTION_EXECUTE_FN(name) static INSTRUCTION_EXECUTE_FN_NOSTATIC(name)

#define NOT_IMPLEMENTED() { gbError(gb, "Function '%s' not implemented.", __FUNCTION__); }

typedef INSTRUCTION_EXECUTE_FN_NOSTATIC(InstructionExecuteFn);

static enum Register16 BC_DE_HL_SP[4] = {REG_BC, REG_DE, REG_HL, REG_SP};
static enum Register16 BC_DE_HL_AF[4] = {REG_BC, REG_DE, REG_HL, REG_AF};

#if 0

static void vgbprintf(GameBoy* gb, const char* message, va_list args) {
    for (uint16 i = 0; i < gb->callStackHeight; i++) {
        if (i > 10) {
            printf("+%d ", gb->callStackHeight - i);
            break;
        }
        printf(" ");
    }
    
    vprintf(message, args);
}

#else

static void vgbprintf(GameBoy* gb, const char* message, va_list args) {
}

#endif

void gbprintf(GameBoy* gb, const char* message, ...) {
    va_list args;
    
    va_start(args, message);

    vgbprintf(gb, message, args);

    va_end(args);
}

static uint16 read16Bits(uint8* address) {
    return (address[1] << 8) | address[0];
}

static void write16Bits(uint8* address, uint16 value) {
    address[0] = value & 0xFF;
    address[1] = value >> 8;
}

static uint16 readMemory16(GameBoy* gb, uint16 address) {
    uint8 lsb = readMemory(gb, address);
    uint8 msb = readMemory(gb, address + 1);
    return (msb << 8) | lsb;
}

static void writeMemory16(GameBoy* gb, uint16 address, uint16 value) {
    writeMemory(gb, address, value & 0xFF);
    writeMemory(gb, address + 1, value >> 8);
}

static uint8 readImm8(GameBoy* gb) {
    uint8 result = RD(REG(PC));
    REG(PC)++;

    return result;
}

static uint16 readImm16(GameBoy* gb) {
    uint8 lsb = readImm8(gb);
    uint8 msb = readImm8(gb);
    return (msb << 8) | lsb;
}

static int8 readImmSigned(GameBoy* gb) {
    // TODO(octave) : bulletproof this.  For now, implementation
    // defined but works on x86 (and probably any 2's complement
    // machine?)
    return (int8) readImm8(gb);
}

static void updateZeroFlag(GameBoy* gb, uint8 result) {
    setFlag(gb, FLAG_Z, result == 0);
}

INSTRUCTION_EXECUTE_FN(loadRegToReg) {
    enum Register8 src = opcode & 0x7;
    enum Register8 dst = (opcode >> 3) & 0x7;

    setReg8(gb, dst, getReg8(gb, src));
}

INSTRUCTION_EXECUTE_FN(loadImm8ToReg) {
    enum Register8 reg = (opcode - 6) >> 3;

    setReg8(gb, reg, readImm8(gb));
}

INSTRUCTION_EXECUTE_FN(loadAddrHLToReg) {
    enum Register8 dst = (opcode >> 3) & 0x7;
    
    setReg8(gb, dst, RD(REG(HL)));
}

INSTRUCTION_EXECUTE_FN(loadRegToAddrHL) {
    enum Register8 src = opcode & 0x7;
    
    WR(REG(HL), getReg8(gb, src));
}

INSTRUCTION_EXECUTE_FN(loadImm8ToAddrHL) {
    WR(REG(HL), readImm8(gb));
}

INSTRUCTION_EXECUTE_FN(loadAddrBCToA) {
    setReg8(gb, REG_A, RD(REG(BC)));
}

INSTRUCTION_EXECUTE_FN(loadAddrDEToA) {
    setReg8(gb, REG_A, RD(REG(DE)));
}

INSTRUCTION_EXECUTE_FN(loadAToAddrBC) {
    WR(REG(BC), getReg8(gb, REG_A));
}

INSTRUCTION_EXECUTE_FN(loadAToAddrDE) {
    WR(REG(DE), getReg8(gb, REG_A));
}

INSTRUCTION_EXECUTE_FN(loadAToAddr16) {
    WR(readImm16(gb), getReg8(gb, REG_A));
}

INSTRUCTION_EXECUTE_FN(loadAddr16ToA) {
    setReg8(gb, REG_A, RD(readImm16(gb)));
}

INSTRUCTION_EXECUTE_FN(loadIOPortImm8ToA) {
    setReg8(gb, REG_A, RD(0xFF00 + readImm8(gb)));
}

INSTRUCTION_EXECUTE_FN(loadAToIOPortImm8) {
    WR(0xFF00 + readImm8(gb), getReg8(gb, REG_A));
}

INSTRUCTION_EXECUTE_FN(loadIOPortCToA) {
    setReg8(gb, REG_A, RD(0xFF00 + getReg8(gb, REG_C)));
}

INSTRUCTION_EXECUTE_FN(loadAToIOPortC) {
    WR(0xFF00 + getReg8(gb, REG_C), getReg8(gb, REG_A));
}

INSTRUCTION_EXECUTE_FN(loadAndIncrementAToAddrHL) {
    WR(REG(HL), getReg8(gb, REG_A));
    REG(HL)++;
}

INSTRUCTION_EXECUTE_FN(loadAndIncrementAddrHLToA) {
    setReg8(gb, REG_A, RD(REG(HL)));
    REG(HL)++;
}

INSTRUCTION_EXECUTE_FN(loadAndDecrementAToAddrHL) {
    WR(REG(HL), getReg8(gb, REG_A));
    REG(HL)--;
}

INSTRUCTION_EXECUTE_FN(loadAndDecrementAddrHLToA) {
    setReg8(gb, REG_A, RD(REG(HL)));
    REG(HL)--;
}

INSTRUCTION_EXECUTE_FN(loadImm16ToReg) {
    enum Register16 dst = BC_DE_HL_SP[(opcode - 1) >> 4];

    gb->registers[dst] = readImm16(gb);
}

INSTRUCTION_EXECUTE_FN(loadSPToAddr16) {
    writeMemory16(gb, readImm16(gb), REG(SP));
}

INSTRUCTION_EXECUTE_FN(loadHLToSP) {
    REG(SP) = REG(HL);
}

static void doPush(GameBoy* gb, uint16 value) {
    REG(SP) -= 2;
    writeMemory16(gb, REG(SP), value);
}

static uint16 doPop(GameBoy* gb) {
    uint16 result = readMemory16(gb, REG(SP));
    REG(SP) += 2;

    return result;
}

INSTRUCTION_EXECUTE_FN(push) {
    enum Register16 src = BC_DE_HL_AF[(opcode - 0xC5) >> 4];
    doPush(gb, gb->registers[src]);
}

INSTRUCTION_EXECUTE_FN(pop) {
    enum Register16 dst = BC_DE_HL_AF[(opcode - 0xC1) >> 4];
    gb->registers[dst] = doPop(gb);
    
    if (dst == REG_AF) {
        gb->registers[dst] = gb->registers[dst] & 0xFFF0;
    }
}

static void doAdd(GameBoy* gb, uint8 rhs) {
    uint8 lhs = getReg8(gb, REG_A);
    uint16 result16 = lhs + rhs;
    uint8 result = result16 & 0xFF;
    
    setReg8(gb, REG_A, result);
    
    updateZeroFlag(gb, result);
    setFlag(gb, FLAG_N, 0);
    setFlag(gb, FLAG_H, ((lhs & 0x0F) + (rhs & 0x0F)) & 0x10);
    setFlag(gb, FLAG_C, result16 >> 8);
}

INSTRUCTION_EXECUTE_FN(addReg) {
    enum Register8 rhs = opcode - 0x80;

    doAdd(gb, getReg8(gb, rhs));
}

INSTRUCTION_EXECUTE_FN(addImm8) {
    doAdd(gb, readImm8(gb));
}

INSTRUCTION_EXECUTE_FN(addAddrHL) {
    doAdd(gb, RD(REG(HL)));
}


static void doAdc(GameBoy* gb, uint8 rhs) {
    uint8 lhs = getReg8(gb, REG_A);
    uint8 prevCarry = getFlag(gb, FLAG_C);
    uint16 result16 = lhs + rhs + prevCarry;
    uint8 result = result16 & 0xFF;
    
    setReg8(gb, REG_A, result);
    
    updateZeroFlag(gb, result);
    setFlag(gb, FLAG_N, 0);
    setFlag(gb, FLAG_H, ((lhs & 0x0F) + (rhs & 0x0F) + prevCarry) & 0x10);
    setFlag(gb, FLAG_C, result16 >> 8);
}

INSTRUCTION_EXECUTE_FN(adcReg) {
    enum Register8 rhs = opcode - 0x88;
    doAdc(gb, getReg8(gb, rhs));
}

INSTRUCTION_EXECUTE_FN(adcImm8) {
    doAdc(gb, readImm8(gb));
}

INSTRUCTION_EXECUTE_FN(adcAddrHL) {
    doAdc(gb, RD(REG(HL)));
}

static void doSub(GameBoy* gb, uint8 rhs) {
    uint8 lhs = getReg8(gb, REG_A);
    uint16 result16 = lhs - rhs;
    uint8 result = result16 & 0xFF;
    
    setReg8(gb, REG_A, result);
    
    updateZeroFlag(gb, result);
    setFlag(gb, FLAG_N, 1);
    setFlag(gb, FLAG_H, ((lhs & 0x0F) - (rhs & 0x0F)) & 0x10);
    setFlag(gb, FLAG_C, result16 >> 8);
}

INSTRUCTION_EXECUTE_FN(subReg) {
    enum Register8 rhs = opcode - 0x90;
    doSub(gb, getReg8(gb, rhs));
}

INSTRUCTION_EXECUTE_FN(subImm8) {
    doSub(gb, readImm8(gb));
}

INSTRUCTION_EXECUTE_FN(subAddrHL) {
    doSub(gb, RD(REG(HL)));
}

static void doSbc(GameBoy* gb, uint8 rhs) {
    uint8 lhs = getReg8(gb, REG_A);
    uint8 prevCarry = getFlag(gb, FLAG_C);
    uint16 result16 = lhs - rhs - prevCarry;
    uint8 result = result16 & 0xFF;
    
    setReg8(gb, REG_A, result);
    
    updateZeroFlag(gb, result);
    setFlag(gb, FLAG_N, 1);
    setFlag(gb, FLAG_H, ((lhs & 0x0F) - (rhs & 0x0F) - prevCarry) & 0x10);
    setFlag(gb, FLAG_C, result16 >> 8);
}

INSTRUCTION_EXECUTE_FN(sbcReg) {
    enum Register8 rhs = opcode - 0x98;
    doSbc(gb, getReg8(gb, rhs));
}

INSTRUCTION_EXECUTE_FN(sbcImm8) {
    doSbc(gb, readImm8(gb));
}

INSTRUCTION_EXECUTE_FN(sbcAddrHL) {
    doSbc(gb, RD(REG(HL)));
}


static void doAnd(GameBoy* gb, uint8 rhs) {
    uint8 result = getReg8(gb, REG_A) & rhs;
    setReg8(gb, REG_A, result);
    
    updateZeroFlag(gb, result);
    setFlag(gb, FLAG_N, 0);
    setFlag(gb, FLAG_H, 1);
    setFlag(gb, FLAG_C, 0);
}
    

INSTRUCTION_EXECUTE_FN(andReg) {
    enum Register8 rhs = opcode - 0xA0;
    doAnd(gb, getReg8(gb, rhs));
}

INSTRUCTION_EXECUTE_FN(andImm8) {
    doAnd(gb, readImm8(gb));
}

INSTRUCTION_EXECUTE_FN(andAddrHL) {
    doAnd(gb, RD(REG(HL)));
}


static void doXor(GameBoy* gb, uint8 rhs) {
    uint8 result = getReg8(gb, REG_A) ^ rhs;
    setReg8(gb, REG_A, result);
    
    updateZeroFlag(gb, result);
    setFlag(gb, FLAG_N, 0);
    setFlag(gb, FLAG_H, 0);
    setFlag(gb, FLAG_C, 0);
}

INSTRUCTION_EXECUTE_FN(xorReg) {
    enum Register8 rhs = opcode - 0xA8;
    doXor(gb, getReg8(gb, rhs));
}

INSTRUCTION_EXECUTE_FN(xorImm8) {
    doXor(gb, readImm8(gb));
}

INSTRUCTION_EXECUTE_FN(xorAddrHL) {
    doXor(gb, RD(REG(HL)));
}


static void doOr(GameBoy* gb, uint8 rhs) {
    uint8 result = getReg8(gb, REG_A) | rhs;
    setReg8(gb, REG_A, result);
    
    updateZeroFlag(gb, result);
    setFlag(gb, FLAG_N, 0);
    setFlag(gb, FLAG_H, 0);
    setFlag(gb, FLAG_C, 0);
}

INSTRUCTION_EXECUTE_FN(orReg) {
    enum Register8 rhs = opcode - 0xB0;
    doOr(gb, getReg8(gb, rhs));
}

INSTRUCTION_EXECUTE_FN(orImm8) {
    doOr(gb, readImm8(gb));
}

INSTRUCTION_EXECUTE_FN(orAddrHL) {
    doOr(gb, RD(REG(HL)));
}


void doCompare(GameBoy* gb, uint8 rhs) {
    uint8 lhs = getReg8(gb, REG_A);
    
    setFlag(gb, FLAG_Z, lhs == rhs);
    setFlag(gb, FLAG_N, 1);
    setFlag(gb, FLAG_H, ((lhs & 0x0F) - (rhs & 0x0F)) & 0x10);
    setFlag(gb, FLAG_C, lhs < rhs);
}

INSTRUCTION_EXECUTE_FN(compareReg) {
    enum Register8 rhs = opcode - 0xB8;

    doCompare(gb, getReg8(gb, rhs));
}

INSTRUCTION_EXECUTE_FN(compareImm8) {
    doCompare(gb, readImm8(gb));
}

INSTRUCTION_EXECUTE_FN(compareAddrHL) {
    doCompare(gb, RD(REG(HL)));
}


INSTRUCTION_EXECUTE_FN(incReg) {
    enum Register8 reg = (opcode - 0x04) >> 3;

    uint8 lhs = getReg8(gb, reg);
    uint8 result = lhs + 1;
    setReg8(gb, reg, result);

    updateZeroFlag(gb, result);
    setFlag(gb, FLAG_N, 0);
    setFlag(gb, FLAG_H, ((lhs & 0x0F) + 1) & 0x10);
}

INSTRUCTION_EXECUTE_FN(incAddrHL) {
    uint8 lhs = RD(REG(HL));
    WR(REG(HL), lhs + 1);
    
    updateZeroFlag(gb, RD(REG(HL)));
    setFlag(gb, FLAG_N, 0);
    setFlag(gb, FLAG_H, ((lhs & 0x0F) + 1) & 0x10);
}

INSTRUCTION_EXECUTE_FN(decReg) {
    enum Register8 reg = (opcode - 0x05) >> 3;

    uint8 lhs = getReg8(gb, reg);
    uint8 result = lhs - 1;
    setReg8(gb, reg, result);
    
    updateZeroFlag(gb, result);
    setFlag(gb, FLAG_N, 1);
    setFlag(gb, FLAG_H, ((lhs & 0x0F) - 1) & 0x10);
}

INSTRUCTION_EXECUTE_FN(decAddrHL) {
    uint8 lhs = RD(REG(HL));
    WR(REG(HL), lhs - 1);

    updateZeroFlag(gb, RD(REG(HL)));
    setFlag(gb, FLAG_N, 1);
    setFlag(gb, FLAG_H, ((lhs & 0x0F) - 1) & 0x10);
}

// Source : https://forums.nesdev.org/viewtopic.php?t=15944
INSTRUCTION_EXECUTE_FN(decimalAdjust) {
    bool32 h = getFlag(gb, FLAG_H);
    bool32 c = getFlag(gb, FLAG_C);
    bool32 n = getFlag(gb, FLAG_N);
    
    uint8 a = getReg8(gb, REG_A);
    
    // note: assumes a is a uint8_t and wraps from 0xff to 0
    if (!n) {
        // after an addition, adjust if (half-)carry occurred or if result is out of bounds
        if (c || a > 0x99) {
            a += 0x60;
            setFlag(gb, FLAG_C, 1);
        }
        if (h || (a & 0x0f) > 0x09) {
            a += 0x6;
        }
    } else {
        // after a subtraction, only adjust if (half-)carry occurred
        if (c) {
            a -= 0x60;
        }
        if (h) {
            a -= 0x6;
        }
    }

    setReg8(gb, REG_A, a);
    
    // these flags are always updated
    setFlag(gb, FLAG_Z, (a == 0));
    setFlag(gb, FLAG_H, 0);
}

INSTRUCTION_EXECUTE_FN(complement) {
    setReg8(gb, REG_A, ~getReg8(gb, REG_A));
    setFlag(gb, FLAG_N, 1);
    setFlag(gb, FLAG_H, 1);
}


INSTRUCTION_EXECUTE_FN(addReg16ToHL) {
    enum Register16 reg = BC_DE_HL_SP[(opcode - 0x09) >> 4];

    uint16 lhs = REG(HL);
    uint16 rhs = gb->registers[reg];
    uint32 result32 = lhs + rhs;
    
    REG(HL) = result32 & 0xFFFF;

    setFlag(gb, FLAG_N, 0);
    setFlag(gb, FLAG_H, ((lhs & 0xFFF) + (rhs & 0xFFF)) & 0x1000);
    setFlag(gb, FLAG_C, result32 >> 16);
}

INSTRUCTION_EXECUTE_FN(incReg16) {
    enum Register16 reg = BC_DE_HL_SP[(opcode - 0x03) >> 4];

    gb->registers[reg]++;
}

INSTRUCTION_EXECUTE_FN(decReg16) {
    enum Register16 reg = BC_DE_HL_SP[(opcode - 0x08) >> 4];
    
    gb->registers[reg]--;
}

INSTRUCTION_EXECUTE_FN(addSignedToSP) {
    uint16 lhs = REG(SP);
    int16 rhs = readImmSigned(gb);
    uint32 result32 = lhs + rhs;

    REG(SP) = result32 & 0xFFFF;

    setFlag(gb, FLAG_Z, 0);
    setFlag(gb, FLAG_N, 0);
    setFlag(gb, FLAG_H, ((lhs & 0xF) + (rhs & 0xF)) & 0x10);
    setFlag(gb, FLAG_C, ((lhs & 0xFF) + (rhs & 0xFF)) & 0x100);
}

INSTRUCTION_EXECUTE_FN(loadSignedPlusSPToHL) {
    uint16 lhs = REG(SP);
    int16 rhs = readImmSigned(gb);
    uint32 result32 = lhs + rhs;

    REG(HL) = result32 & 0xFFFF;

    setFlag(gb, FLAG_Z, 0);
    setFlag(gb, FLAG_N, 0);
    setFlag(gb, FLAG_H, ((lhs & 0xF) + (rhs & 0xF)) & 0x10);
    setFlag(gb, FLAG_C, ((lhs & 0xFF) + (rhs & 0xFF)) & 0x100);
}


static uint8 rotateLeft(uint8 value) {
    return (value << 1) | (value >> 7);
}

static uint8 rotateRight(uint8 value) {
    return (value >> 1) | (value << 7);
}

INSTRUCTION_EXECUTE_FN(rotateALeft) {
    uint8 a = getReg8(gb, REG_A);
    a = rotateLeft(a);
    setReg8(gb, REG_A, a);
    
    setFlag(gb, FLAG_Z, 0);
    setFlag(gb, FLAG_N, 0);
    setFlag(gb, FLAG_H, 0);
    setFlag(gb, FLAG_C, a & 1);
}

static uint8 rotateLeftThroughCarry(GameBoy* gb, uint8 value) {
    uint8 oldCarry = getFlag(gb, FLAG_C);
    setFlag(gb, FLAG_C, value >> 7);

    return (value << 1) | oldCarry;
}

static uint8 rotateRightThroughCarry(GameBoy* gb, uint8 value) {
    uint8 oldCarry = getFlag(gb, FLAG_C);
    setFlag(gb, FLAG_C, value & 1);

    return (value >> 1) | (oldCarry << 7);
}

INSTRUCTION_EXECUTE_FN(rotateALeftThroughCarry) {
    uint8 a = getReg8(gb, REG_A);
    a = rotateLeftThroughCarry(gb, a);
    
    setReg8(gb, REG_A, a);

    setFlag(gb, FLAG_Z, 0);
    setFlag(gb, FLAG_N, 0);
    setFlag(gb, FLAG_H, 0);
}

INSTRUCTION_EXECUTE_FN(rotateARight) {
    uint8 a = getReg8(gb, REG_A);
    a = rotateRight(a);
    setReg8(gb, REG_A, a);

    setFlag(gb, FLAG_Z, 0);
    setFlag(gb, FLAG_N, 0);
    setFlag(gb, FLAG_H, 0);
    setFlag(gb, FLAG_C, a >> 7);
}

INSTRUCTION_EXECUTE_FN(rotateARightThroughCarry) {
    uint8 a = getReg8(gb, REG_A);
    a = rotateRightThroughCarry(gb, a);
    
    setReg8(gb, REG_A, a);

    setFlag(gb, FLAG_Z, 0);
    setFlag(gb, FLAG_N, 0);
    setFlag(gb, FLAG_H, 0);
}

static uint8 doRotateLeft(GameBoy* gb, uint8 value) {
    uint8 rotated = rotateLeft(value);
    
    setFlag(gb, FLAG_Z, rotated == 0);
    setFlag(gb, FLAG_N, 0);
    setFlag(gb, FLAG_H, 0);
    setFlag(gb, FLAG_C, value >> 7);

    return rotated;
}

INSTRUCTION_EXECUTE_FN(rotateRegLeft) {
    enum Register8 reg = opcode;

    setReg8(gb, reg, doRotateLeft(gb, getReg8(gb, reg)));
}

INSTRUCTION_EXECUTE_FN(rotateAddrHLLeft) {
    WR(REG(HL), doRotateLeft(gb, RD(REG(HL))));
}

static uint8 doRotateLeftThroughCarry(GameBoy* gb, uint8 value) {
    uint8 rotated = rotateLeftThroughCarry(gb, value);
    
    setFlag(gb, FLAG_Z, rotated == 0);
    setFlag(gb, FLAG_N, 0);
    setFlag(gb, FLAG_H, 0);

    return rotated;
}

INSTRUCTION_EXECUTE_FN(rotateRegLeftThroughCarry) {
    enum Register8 reg = opcode - 0x10;

    setReg8(gb, reg, doRotateLeftThroughCarry(gb, getReg8(gb, reg)));
}

INSTRUCTION_EXECUTE_FN(rotateAddrHLLeftThroughCarry) {
    WR(REG(HL), doRotateLeftThroughCarry(gb, RD(REG(HL))));
}

static uint8 doRotateRight(GameBoy* gb, uint8 value) {
    uint8 rotated = rotateRight(value);
    
    setFlag(gb, FLAG_Z, rotated == 0);
    setFlag(gb, FLAG_N, 0);
    setFlag(gb, FLAG_H, 0);
    setFlag(gb, FLAG_C, value & 1);

    return rotated;
}

INSTRUCTION_EXECUTE_FN(rotateRegRight) {
    enum Register8 reg = opcode - 0x08;

    setReg8(gb, reg, doRotateRight(gb, getReg8(gb, reg)));
}

INSTRUCTION_EXECUTE_FN(rotateAddrHLRight) {
    WR(REG(HL), doRotateRight(gb, RD(REG(HL))));
}

static uint8 doRotateRightThroughCarry(GameBoy* gb, uint8 value) {
    uint8 rotated = rotateRightThroughCarry(gb, value);
    
    setFlag(gb, FLAG_Z, rotated == 0);
    setFlag(gb, FLAG_N, 0);
    setFlag(gb, FLAG_H, 0);

    return rotated;
}

INSTRUCTION_EXECUTE_FN(rotateRegRightThroughCarry) {
    enum Register8 reg = opcode - 0x18;

    setReg8(gb, reg, doRotateRightThroughCarry(gb, getReg8(gb, reg)));
}

INSTRUCTION_EXECUTE_FN(rotateAddrHLRightThroughCarry) {
    WR(REG(HL), doRotateRightThroughCarry(gb, RD(REG(HL))));
}

static uint8 doShiftLeftArithmetic(GameBoy* gb, uint8 value) {
    uint8 result = value << 1;

    setFlag(gb, FLAG_Z, result == 0);
    setFlag(gb, FLAG_N, 0);
    setFlag(gb, FLAG_H, 0);
    setFlag(gb, FLAG_C, value >> 7);

    return result;
}

INSTRUCTION_EXECUTE_FN(shiftRegLeftArithmetic) {
    enum Register8 reg = opcode - 0x20;

    setReg8(gb, reg, doShiftLeftArithmetic(gb, getReg8(gb, reg)));
}

INSTRUCTION_EXECUTE_FN(shiftAddrHLLeftArithmetic) {
    WR(REG(HL), doShiftLeftArithmetic(gb, RD(REG(HL))));
}

static uint8 doShiftRightArithmetic(GameBoy* gb, uint8 value) {
    uint8 result = (value >> 1) | (value & 0x80);

    setFlag(gb, FLAG_Z, result == 0);
    setFlag(gb, FLAG_N, 0);
    setFlag(gb, FLAG_H, 0);
    setFlag(gb, FLAG_C, value & 1);

    return result;
}

INSTRUCTION_EXECUTE_FN(shiftRegRightArithmetic) {
    enum Register8 reg = opcode - 0x28;

    setReg8(gb, reg, doShiftRightArithmetic(gb, getReg8(gb, reg)));
}

INSTRUCTION_EXECUTE_FN(shiftAddrHLRightArithmetic) {
    WR(REG(HL), doShiftRightArithmetic(gb, RD(REG(HL))));
}

uint8 doSwapNibbles(GameBoy* gb, uint8 value) {
    uint8 result = (value >> 4) | (value << 4);
    
    setFlag(gb, FLAG_Z, result == 0);
    setFlag(gb, FLAG_N, 0);
    setFlag(gb, FLAG_H, 0);
    setFlag(gb, FLAG_C, 0);

    return result;
}

INSTRUCTION_EXECUTE_FN(swapNibblesReg) {
    enum Register8 reg = opcode - 0x30;

    setReg8(gb, reg, doSwapNibbles(gb, getReg8(gb, reg)));
}

INSTRUCTION_EXECUTE_FN(swapNibblesAddrHL) {
    WR(REG(HL), doSwapNibbles(gb, RD(REG(HL))));
}


uint8 doShiftRightLogical(GameBoy* gb, uint8 value) {
    uint8 result = value >> 1;
    
    setFlag(gb, FLAG_Z, result == 0);
    setFlag(gb, FLAG_N, 0);
    setFlag(gb, FLAG_H, 0);
    setFlag(gb, FLAG_C, value & 1);

    return result;
}

INSTRUCTION_EXECUTE_FN(shiftRegRightLogical) {
    enum Register8 reg = opcode - 0x38;

    setReg8(gb, reg, doShiftRightLogical(gb, getReg8(gb, reg)));
}

INSTRUCTION_EXECUTE_FN(shiftAddrHLRightLogical) {
    WR(REG(HL), doShiftRightLogical(gb, RD(REG(HL))));
}


static void doTestBit(GameBoy* gb, uint8 byte, uint8 bitIndex) {
    uint8 bitValue = getBit(byte, bitIndex);

    setFlag(gb, FLAG_Z, !bitValue);
    setFlag(gb, FLAG_N, 0);
    setFlag(gb, FLAG_H, 1);
}

INSTRUCTION_EXECUTE_FN(testBitReg) {
    enum Register8 reg = opcode % 8;

    uint8 bitIndex = (opcode - 0x40) >> 3;
    doTestBit(gb, getReg8(gb, reg), bitIndex);
}

INSTRUCTION_EXECUTE_FN(testBitAddrHL) {
    uint8 bitIndex = (opcode - 0x40) >> 3;
    doTestBit(gb, RD(REG(HL)), bitIndex);
}

INSTRUCTION_EXECUTE_FN(setBitReg) {
    enum Register8 reg = opcode % 8;
    uint8 bitIndex = (opcode - 0xC0) >> 3;
    uint8 regVal = getReg8(gb, reg);

    setReg8(gb, reg, setBit(regVal, bitIndex));
}

INSTRUCTION_EXECUTE_FN(setBitAddrHL) {
    uint8 bitIndex = (opcode - 0xC0) >> 3;

    WR(REG(HL), setBit(RD(REG(HL)), bitIndex));
}

INSTRUCTION_EXECUTE_FN(resetBitReg) {
    enum Register8 reg = opcode % 8;
    uint8 bitIndex = (opcode - 0x80) >> 3;
    uint8 regVal = getReg8(gb, reg);

    setReg8(gb, reg, resetBit(regVal, bitIndex));
}

INSTRUCTION_EXECUTE_FN(resetBitAddrHL) {
    uint8 bitIndex = (opcode - 0x80) >> 3;

    WR(REG(HL), resetBit(RD(REG(HL)), bitIndex));
}


INSTRUCTION_EXECUTE_FN(flipCarry) {
    setFlag(gb, FLAG_N, 0);
    setFlag(gb, FLAG_H, 0);
    setFlag(gb, FLAG_C, !getFlag(gb, FLAG_C));
}

INSTRUCTION_EXECUTE_FN(setCarry) {
    setFlag(gb, FLAG_N, 0);
    setFlag(gb, FLAG_H, 0);
    setFlag(gb, FLAG_C, 1);
}

INSTRUCTION_EXECUTE_FN(nop) {
    return;
}

INSTRUCTION_EXECUTE_FN(halt) {
    gb->halted = true;
}

INSTRUCTION_EXECUTE_FN(stop) {
    // TODO(octave)
    IO(DIV) = 0;
}

INSTRUCTION_EXECUTE_FN(disableInterrupts) {
    gb->ime = 0;
}

INSTRUCTION_EXECUTE_FN(enableInterrupts) {
    gb->ime = 1;
}


INSTRUCTION_EXECUTE_FN(jumpImm16) {
    uint16 prevPC = REG(PC);
    REG(PC) = readImm16(gb);
    gbprintf(gb, "jump %04X -> %04X\n", prevPC, REG(PC));
}

INSTRUCTION_EXECUTE_FN(jumpHL) {
    uint16 prevPC = REG(PC);
    REG(PC) = REG(HL);
    gbprintf(gb, "jump HL %04X -> %04X\n", prevPC, REG(PC));
}

bool32 checkCondition(GameBoy* gb, enum Conditional cond) {
    switch (cond) {
    case COND_NZ:
        return !getFlag(gb, FLAG_Z);
    case COND_Z:
        return getFlag(gb, FLAG_Z);
    case COND_NC:
        return !getFlag(gb, FLAG_C);
    case COND_C:
        return getFlag(gb, FLAG_C);
    default:
        fprintf(stderr, "Unknown conditional value");
        exit(1);
    }
}

INSTRUCTION_EXECUTE_FN(conditionalJumpImm16) {
    enum Conditional cond = (opcode - 0xC2) >> 3;

    if (checkCondition(gb, cond)) {
        jumpImm16(gb, opcode);
        gb->variableCycles = 16;
    } else {
        REG(PC) += 2; // discard jump address
        gb->variableCycles = 12;
    }
}

INSTRUCTION_EXECUTE_FN(relativeJump) {
    uint16 prevPC = REG(PC);
    REG(PC) += readImmSigned(gb);
    /* gbprintf(gb, "relative jump %04X -> %04X\n", prevPC, REG(PC)); */
}

INSTRUCTION_EXECUTE_FN(conditionalRelativeJump) {
    enum Conditional cond = (opcode - 0x20) >> 3;

    if (checkCondition(gb, cond)) {
        relativeJump(gb, opcode);

        gb->variableCycles = 12;
    } else {
        REG(PC) += 1; // discard jump address
        gb->variableCycles = 8;
    }
}

INSTRUCTION_EXECUTE_FN(callImm16) {
    uint16 prevPC = REG(PC);

    uint16 callAddress = readImm16(gb);
    doPush(gb, REG(PC));
    REG(PC) = callAddress;

    gb->callStackHeight++;

    gbprintf(gb, "call %04X -> %04X\n", prevPC, REG(PC));
}

INSTRUCTION_EXECUTE_FN(conditionalCallImm16) {
    enum Conditional cond = (opcode - 0xC4) >> 3;

    if (checkCondition(gb, cond)) {
        callImm16(gb, opcode);
        gb->variableCycles = 24;
    } else {
        REG(PC) += 2; // discard jump address
        gb->variableCycles = 12;
    }
}

INSTRUCTION_EXECUTE_FN(ret) {
    uint16 prevPC = REG(PC);
    
    REG(PC) = doPop(gb);

    gbprintf(gb, "ret %04X -> %04X\n", prevPC, REG(PC));
    gb->callStackHeight--;
}

INSTRUCTION_EXECUTE_FN(conditionalRet) {
    enum Conditional cond = (opcode - 0xC0) >> 3;

    if (checkCondition(gb, cond)) {
        ret(gb, opcode);
        gb->variableCycles = 20;
    } else {
        gb->variableCycles = 8;
    }
}

INSTRUCTION_EXECUTE_FN(retAndEnableInterrupts) {
    enableInterrupts(gb, opcode);
    ret(gb, opcode);
}

INSTRUCTION_EXECUTE_FN(reset) {
    uint16 address = opcode - 0xC7;
    doPush(gb, REG(PC));

    uint16 prevPC = REG(PC);
    REG(PC) = address;
    
    gbprintf(gb, "reset %04X -> %04X\n", prevPC, REG(PC));
}

INSTRUCTION_EXECUTE_FN(prefixCB) {
    opcode = readImm8(gb);
    bool32 isAddrHL = (opcode % 8) == 6;

    if (isAddrHL) {
        if (opcode >= 0x40 && opcode < 0x80) {
            gb->variableCycles = 12;
        } else {
            gb->variableCycles = 16;
        }
    } else {
        gb->variableCycles = 8;
    }

    InstructionExecuteFn* dispatchTable[8];

    if (isAddrHL) {
        dispatchTable[0] = rotateAddrHLLeft;
        dispatchTable[1] = rotateAddrHLRight;
        dispatchTable[2] = rotateAddrHLLeftThroughCarry;
        dispatchTable[3] = rotateAddrHLRightThroughCarry;
        dispatchTable[4] = shiftAddrHLLeftArithmetic;
        dispatchTable[5] = shiftAddrHLRightArithmetic;
        dispatchTable[6] = swapNibblesAddrHL;
        dispatchTable[7] = shiftAddrHLRightLogical;
    } else {
        dispatchTable[0] = rotateRegLeft;
        dispatchTable[1] = rotateRegRight;
        dispatchTable[2] = rotateRegLeftThroughCarry;
        dispatchTable[3] = rotateRegRightThroughCarry;
        dispatchTable[4] = shiftRegLeftArithmetic;
        dispatchTable[5] = shiftRegRightArithmetic;
        dispatchTable[6] = swapNibblesReg;
        dispatchTable[7] = shiftRegRightLogical;
    }
    
    if (opcode < 0x40) {
        uint8 instrIndex = opcode >> 3;
        dispatchTable[instrIndex](gb, opcode);
    } else if (opcode < 0x80) { // test bit
        if (isAddrHL) {
            testBitAddrHL(gb, opcode);
        } else {
            testBitReg(gb, opcode);
        }
    } else if (opcode < 0xC0) { // reset bit
        if (isAddrHL) {
            resetBitAddrHL(gb, opcode);
        } else {
            resetBitReg(gb, opcode);
        }
    } else { // set bit
        if (isAddrHL) {
            setBitAddrHL(gb, opcode);
        } else {
            setBitReg(gb, opcode);
        }
    }
}

#include "disassembly.c"

#define VARIABLE_CYCLES 0xFFFF
#define INSTR(cycles, handler) {cycles, handler, handler##_disassemble, #handler}

typedef struct InstructionHandler {
    uint16 cycles;
    InstructionExecuteFn* execute;
    InstructionDisassembleFn* disassemble;
    const char* name;
} InstructionHandler;

// reference : https://www.pastraiser.com/cpu/gameboy/gameboy_opcodes.html
static InstructionHandler instructionHandlers[256] = {
    // 00
    INSTR(4, nop),
    INSTR(12, loadImm16ToReg),
    INSTR(8, loadAToAddrBC),
    INSTR(8, incReg16),
    INSTR(4, incReg),
    INSTR(4, decReg),
    INSTR(8, loadImm8ToReg),
    INSTR(4, rotateALeft),
    
    INSTR(20, loadSPToAddr16),
    INSTR(8, addReg16ToHL),
    INSTR(8, loadAddrBCToA),
    INSTR(8, decReg16),
    INSTR(4, incReg),
    INSTR(4, decReg),
    INSTR(8, loadImm8ToReg),
    INSTR(4, rotateARight),

    // 10
    INSTR(4, stop),
    INSTR(12, loadImm16ToReg),
    INSTR(8, loadAToAddrDE),
    INSTR(8, incReg16),
    INSTR(4, incReg),
    INSTR(4, decReg),
    INSTR(8, loadImm8ToReg),
    INSTR(4, rotateALeftThroughCarry),
    
    INSTR(12, relativeJump),
    INSTR(8, addReg16ToHL),
    INSTR(8, loadAddrDEToA),
    INSTR(8, decReg16),
    INSTR(4, incReg),
    INSTR(4, decReg),
    INSTR(8, loadImm8ToReg),
    INSTR(4, rotateARightThroughCarry),

    // 20
    INSTR(VARIABLE_CYCLES, conditionalRelativeJump),
    INSTR(12, loadImm16ToReg),
    INSTR(8, loadAndIncrementAToAddrHL),
    INSTR(8, incReg16),
    INSTR(4, incReg),
    INSTR(4, decReg),
    INSTR(8, loadImm8ToReg),
    INSTR(4, decimalAdjust),
    
    INSTR(VARIABLE_CYCLES, conditionalRelativeJump),
    INSTR(8, addReg16ToHL),
    INSTR(8, loadAndIncrementAddrHLToA),
    INSTR(8, decReg16),
    INSTR(4, incReg),
    INSTR(4, decReg),
    INSTR(8, loadImm8ToReg),
    INSTR(4, complement),

    // 30
    INSTR(VARIABLE_CYCLES, conditionalRelativeJump),
    INSTR(12, loadImm16ToReg),
    INSTR(8, loadAndDecrementAToAddrHL),
    INSTR(8, incReg16),
    INSTR(12, incAddrHL),
    INSTR(12, decAddrHL),
    INSTR(12, loadImm8ToAddrHL),
    INSTR(4, setCarry),
    
    INSTR(VARIABLE_CYCLES, conditionalRelativeJump),
    INSTR(8, addReg16ToHL),
    INSTR(8, loadAndDecrementAddrHLToA),
    INSTR(8, decReg16),
    INSTR(4, incReg),
    INSTR(4, decReg),
    INSTR(8, loadImm8ToReg),
    INSTR(4, flipCarry),

    // 40
    INSTR(4, loadRegToReg),
    INSTR(4, loadRegToReg),
    INSTR(4, loadRegToReg),
    INSTR(4, loadRegToReg),
    INSTR(4, loadRegToReg),
    INSTR(4, loadRegToReg),
    INSTR(8, loadAddrHLToReg),
    INSTR(4, loadRegToReg),

    INSTR(4, loadRegToReg),
    INSTR(4, loadRegToReg),
    INSTR(4, loadRegToReg),
    INSTR(4, loadRegToReg),
    INSTR(4, loadRegToReg),
    INSTR(4, loadRegToReg),
    INSTR(8, loadAddrHLToReg),
    INSTR(4, loadRegToReg),

    // 50
    INSTR(4, loadRegToReg),
    INSTR(4, loadRegToReg),
    INSTR(4, loadRegToReg),
    INSTR(4, loadRegToReg),
    INSTR(4, loadRegToReg),
    INSTR(4, loadRegToReg),
    INSTR(8, loadAddrHLToReg),
    INSTR(4, loadRegToReg),

    INSTR(4, loadRegToReg),
    INSTR(4, loadRegToReg),
    INSTR(4, loadRegToReg),
    INSTR(4, loadRegToReg),
    INSTR(4, loadRegToReg),
    INSTR(4, loadRegToReg),
    INSTR(8, loadAddrHLToReg),
    INSTR(4, loadRegToReg),

    // 60
    INSTR(4, loadRegToReg),
    INSTR(4, loadRegToReg),
    INSTR(4, loadRegToReg),
    INSTR(4, loadRegToReg),
    INSTR(4, loadRegToReg),
    INSTR(4, loadRegToReg),
    INSTR(8, loadAddrHLToReg),
    INSTR(4, loadRegToReg),

    INSTR(4, loadRegToReg),
    INSTR(4, loadRegToReg),
    INSTR(4, loadRegToReg),
    INSTR(4, loadRegToReg),
    INSTR(4, loadRegToReg),
    INSTR(4, loadRegToReg),
    INSTR(8, loadAddrHLToReg),
    INSTR(4, loadRegToReg),

    // 70
    INSTR(8, loadRegToAddrHL),
    INSTR(8, loadRegToAddrHL),
    INSTR(8, loadRegToAddrHL),
    INSTR(8, loadRegToAddrHL),
    INSTR(8, loadRegToAddrHL),
    INSTR(8, loadRegToAddrHL),
    INSTR(4, halt),
    INSTR(8, loadRegToAddrHL),

    INSTR(4, loadRegToReg),
    INSTR(4, loadRegToReg),
    INSTR(4, loadRegToReg),
    INSTR(4, loadRegToReg),
    INSTR(4, loadRegToReg),
    INSTR(4, loadRegToReg),
    INSTR(8, loadAddrHLToReg),
    INSTR(4, loadRegToReg),

    // 80
    INSTR(4, addReg),
    INSTR(4, addReg),
    INSTR(4, addReg),
    INSTR(4, addReg),
    INSTR(4, addReg),
    INSTR(4, addReg),
    INSTR(8, addAddrHL),
    INSTR(4, addReg),

    INSTR(4, adcReg),
    INSTR(4, adcReg),
    INSTR(4, adcReg),
    INSTR(4, adcReg),
    INSTR(4, adcReg),
    INSTR(4, adcReg),
    INSTR(8, adcAddrHL),
    INSTR(4, adcReg),

    // 90
    INSTR(4, subReg),
    INSTR(4, subReg),
    INSTR(4, subReg),
    INSTR(4, subReg),
    INSTR(4, subReg),
    INSTR(4, subReg),
    INSTR(8, subAddrHL),
    INSTR(4, subReg),

    INSTR(4, sbcReg),
    INSTR(4, sbcReg),
    INSTR(4, sbcReg),
    INSTR(4, sbcReg),
    INSTR(4, sbcReg),
    INSTR(4, sbcReg),
    INSTR(8, sbcAddrHL),
    INSTR(4, sbcReg),

    // A0
    INSTR(4, andReg),
    INSTR(4, andReg),
    INSTR(4, andReg),
    INSTR(4, andReg),
    INSTR(4, andReg),
    INSTR(4, andReg),
    INSTR(8, andAddrHL),
    INSTR(4, andReg),

    INSTR(4, xorReg),
    INSTR(4, xorReg),
    INSTR(4, xorReg),
    INSTR(4, xorReg),
    INSTR(4, xorReg),
    INSTR(4, xorReg),
    INSTR(8, xorAddrHL),
    INSTR(4, xorReg),

    // B0
    INSTR(4, orReg),
    INSTR(4, orReg),
    INSTR(4, orReg),
    INSTR(4, orReg),
    INSTR(4, orReg),
    INSTR(4, orReg),
    INSTR(8, orAddrHL),
    INSTR(4, orReg),

    INSTR(4, compareReg),
    INSTR(4, compareReg),
    INSTR(4, compareReg),
    INSTR(4, compareReg),
    INSTR(4, compareReg),
    INSTR(4, compareReg),
    INSTR(8, compareAddrHL),
    INSTR(4, compareReg),

    // C0
    INSTR(VARIABLE_CYCLES, conditionalRet),
    INSTR(12, pop),
    INSTR(VARIABLE_CYCLES, conditionalJumpImm16),
    INSTR(16, jumpImm16),
    INSTR(VARIABLE_CYCLES, conditionalCallImm16),
    INSTR(16, push),
    INSTR(8, addImm8),
    INSTR(16, reset),
    
    INSTR(VARIABLE_CYCLES, conditionalRet),
    INSTR(16, ret),
    INSTR(VARIABLE_CYCLES, conditionalJumpImm16),
    INSTR(VARIABLE_CYCLES, prefixCB),
    INSTR(VARIABLE_CYCLES, conditionalCallImm16),
    INSTR(24, callImm16),
    INSTR(8, adcImm8),
    INSTR(16, reset),

    // D0
    INSTR(VARIABLE_CYCLES, conditionalRet),
    INSTR(12, pop),
    INSTR(VARIABLE_CYCLES, conditionalJumpImm16),
    {},
    INSTR(VARIABLE_CYCLES, conditionalCallImm16),
    INSTR(16, push),
    INSTR(8, subImm8),
    INSTR(16, reset),
    
    INSTR(VARIABLE_CYCLES, conditionalRet),
    INSTR(16, retAndEnableInterrupts),
    INSTR(VARIABLE_CYCLES, conditionalJumpImm16),
    {},
    INSTR(VARIABLE_CYCLES, conditionalCallImm16),
    {},
    INSTR(8, sbcImm8),
    INSTR(16, reset),

    // E0
    INSTR(12, loadAToIOPortImm8),
    INSTR(12, pop),
    INSTR(8, loadAToIOPortC),
    {},
    {},
    INSTR(16, push),
    INSTR(8, andImm8),
    INSTR(16, reset),

    INSTR(16, addSignedToSP),
    INSTR(4, jumpHL),
    INSTR(16, loadAToAddr16),
    {},
    {},
    {},
    INSTR(8, xorImm8),
    INSTR(16, reset),

    // F0
    INSTR(12, loadIOPortImm8ToA),
    INSTR(12, pop),
    INSTR(8, loadIOPortCToA),
    INSTR(4, disableInterrupts),
    {},
    INSTR(16, push),
    INSTR(8, orImm8),
    INSTR(16, reset),

    INSTR(12, loadSignedPlusSPToHL),
    INSTR(8, loadHLToSP),
    INSTR(16, loadAddr16ToA),
    INSTR(4, enableInterrupts),
    {},
    {},
    INSTR(8, compareImm8),
    INSTR(16, reset),
};

uint8 executeInstruction(GameBoy* gb) {
    uint16 prevPC = REG(PC);
    uint8 instr[4];
    for (uint8 i = 0; i < 4; i++) {
        instr[i] = RD(prevPC + i);
    }
    
    
    uint8 opcode = readImm8(gb);
    InstructionHandler* handler = &instructionHandlers[opcode];

    if (!handler->execute) {
        fprintf(stderr, "Invalid opcode %X at 0x%04X\n", opcode, prevPC);
        exit(1);
    }
    
    if (gb->tracing) {
        printGameboyLogLine(stdout, gb);
        printf("%04x ", prevPC);

        handler->disassemble(stdout, instr);
        printf("\n");

        gb->tracing--;
    }

    gb->variableCycles = 0;
    handler->execute(gb, opcode);

    uint8 duration;
    if (handler->cycles != VARIABLE_CYCLES) {
        ASSERT(!gb->variableCycles);
        duration = handler->cycles;
    } else {
        ASSERT(gb->variableCycles);
        duration = gb->variableCycles;
    }

    return duration;
}

void handleInterrupt(GameBoy* gb) {
    uint16 interruptAddresses[] = {
        [INT_VBLANK] = 0x0040, // V-blank
        [INT_LCDC] = 0x0048, // LCDC Status
        [INT_TIMER] = 0x0050, // Timer overflow
        [INT_SERIAL] = 0x0058, // Serial transfer
        [INT_JOYPAD] = 0x0060, // Hi-Lo of P10-P13
    };

    uint8 ifReg = RD(IO_IF);
    if (ifReg) {
        gb->halted = false;
    }

    if (!gb->ime) {
        return;
    }

    uint8 enabledPendingInterrupts = ifReg & RD(IE_ADDRESS);

    // find highest-priority, enabled interrupt that was triggered
    for (uint8 interruptIndex = 0;
         interruptIndex < INT_COUNT;
         interruptIndex++) {
        if (getBit(enabledPendingInterrupts, interruptIndex)) {
            gb->callStackHeight++;
            
            // disable interrupts
            gb->ime = 0;

            // reset this interrupt's flag
            WR(IO_IF,  resetBit(RD(IO_IF), interruptIndex));
        
            // push PC
            doPush(gb, REG(PC));

            uint16 prevPC = REG(PC);

            // jump to interrupt handler
            uint16 interruptAddress = 0x0040 + 0x08 * interruptIndex;
            REG(PC) = interruptAddress;

            gbprintf(gb, "interrupt %04X -> %04X\n", prevPC, REG(PC));
            break;
        }
    }
}

static void stepClock(GameBoy* gb, uint8 duration) {
    gb->clock += duration;
    IO(DIV) = gb->clock / 16384;
    
    uint8 tac = IO(TAC);
    if (getBit(tac, 2)) {
        uint16 clockPeriods[] = {
            0x400,
            0x10,
            0x40,
            0x100,
        };

        uint16 clockPeriod = clockPeriods[tac & 0x3];
        gb->timerAccumulator += duration;

        while (gb->timerAccumulator >= clockPeriod) {
            gb->timerAccumulator -= clockPeriod;

            IO(TIMA)++;
            if (!IO(TIMA)) {
                triggerInterrupt(gb, INT_TIMER);
                IO(TIMA) = IO(TMA);
            }
        }
    }

    gb->renderingAccumulator += duration;
    while (gb->renderingAccumulator > GAMEBOY_CYCLES_PER_SCANLINE) {
        IO(LY)++;
        
        if (IO(LY) < GAMEBOY_SCREEN_HEIGHT) {
            drawScreenRow(gb, IO(LY));
        }
        
        if (IO(LY) == GAMEBOY_LY_VBLANK) {
            triggerInterrupt(gb, INT_VBLANK);
        } else if (IO(LY) == GAMEBOY_LY_MAX) {
            IO(LY) = 0;
            gb->frameReady = true;
        }

        if (IO(LY) == IO(LYC) && getBit(IO(STAT), 6)) {
            triggerInterrupt(gb, INT_LCDC);
        }

        gb->renderingAccumulator -= GAMEBOY_CYCLES_PER_SCANLINE;
    }

    if (IO(LY) >= GAMEBOY_LY_VBLANK) {
        if (gb->renderingMode != 1 && getBit(IO(STAT), 4)) {
            triggerInterrupt(gb, INT_LCDC);
        }
        
        gb->renderingMode = 1; // vblank
    } else if (gb->renderingAccumulator < 80) {
        if (gb->renderingMode != 2 && getBit(IO(STAT), 5)) {
            triggerInterrupt(gb, INT_LCDC);
        }
        
        gb->renderingMode = 2; // oam read
    } else if (gb->renderingAccumulator < 369) {
        // TODO(octave) : variable mode 3/0 duration
        gb->renderingMode = 3; // drawing pixels
    } else {
        if (gb->renderingMode != 0 && getBit(IO(STAT), 3)) {
            triggerInterrupt(gb, INT_LCDC);
        }
        
        gb->renderingMode = 0; // hblank
    }
}

void executeCycle(GameBoy* gb) {
    uint8 duration;
    if (gb->halted) {
        duration = 4; // if halted, wait one cycle
    } else {
        duration = executeInstruction(gb);
    }

    stepClock(gb, duration);
    
    handleInterrupt(gb);
}
