#include "gameboy.h"

#include "handmade.h"

#include <stdio.h>
#include <stdlib.h>
#include <stdarg.h>

#define INSTRUCTION_EXECUTE_FN_NOSTATIC(name) \
    void name(GameBoy* gb, uint8* instr)
#define INSTRUCTION_EXECUTE_FN(name) static INSTRUCTION_EXECUTE_FN_NOSTATIC(name)

#define NOT_IMPLEMENTED() { gbError(gb, "Function '%s' not implemented.", __FUNCTION__); }

typedef INSTRUCTION_EXECUTE_FN_NOSTATIC(InstructionExecuteFn);

static enum Register16 BC_DE_HL_SP[4] = {REG_BC, REG_DE, REG_HL, REG_SP};
static enum Register16 BC_DE_HL_AF[4] = {REG_BC, REG_DE, REG_HL, REG_AF};

static void vgbprintf(GameBoy* gb, const char* message, va_list args) {
    for (uint16 i = 0; i < gb->callStackHeight; i++) {
        printf(" ");
    }
    
    vprintf(message, args);
}

static void gbprintf(GameBoy* gb, const char* message, ...) {
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

static uint16 get16BitArgument(uint8* instr) {
    return read16Bits(instr + 1);
}

static int8 getSigned8BitArgument(uint8* instr) {
    // TODO(octave) : bulletproof this.  For now, implementation
    // defined but works on x86 (and probably any 2's complement
    // machine?)
    return (int8) instr[1];
}

static void updateZeroFlag(GameBoy* gb, uint8 result) {
    setFlag(gb, FLAG_Z, result == 0);
}

typedef struct InstructionHandler {
    uint16 length;
    uint16 cycles;
    InstructionExecuteFn* execute;
    const char* name;
} InstructionHandler;

INSTRUCTION_EXECUTE_FN(loadRegToReg) {
    enum Register8 src = instr[0] & 0x7;
    enum Register8 dst = (instr[0] >> 3) & 0x7;

    setReg8(gb, dst, getReg8(gb, src));
}

INSTRUCTION_EXECUTE_FN(loadImm8ToReg) {
    enum Register8 reg = (instr[0] - 6) >> 3;

    setReg8(gb, reg, instr[1]);
}

INSTRUCTION_EXECUTE_FN(loadAddrHLToReg) {
    enum Register8 dst = (instr[0] >> 3) & 0x7;
    
    setReg8(gb, dst, MEM(REG(HL)));
}

INSTRUCTION_EXECUTE_FN(loadRegToAddrHL) {
    enum Register8 src = instr[0] & 0x7;
    
    MEM(REG(HL)) = getReg8(gb, src);
}

INSTRUCTION_EXECUTE_FN(loadImm8ToAddrHL) {
    MEM(REG(HL)) = instr[1];
}

INSTRUCTION_EXECUTE_FN(loadAddrBCToA) {
    setReg8(gb, REG_A, MEM(REG(BC)));
}

INSTRUCTION_EXECUTE_FN(loadAddrDEToA) {
    setReg8(gb, REG_A, MEM(REG(DE)));
}

INSTRUCTION_EXECUTE_FN(loadAToAddrBC) {
    MEM(REG(BC)) = getReg8(gb, REG_A);
}

INSTRUCTION_EXECUTE_FN(loadAToAddrDE) {
    MEM(REG(DE)) = getReg8(gb, REG_A);
}

INSTRUCTION_EXECUTE_FN(loadAToAddr16) {
    MEM(get16BitArgument(instr)) = getReg8(gb, REG_A);
}

INSTRUCTION_EXECUTE_FN(loadIOPortImm8ToA) {
    setReg8(gb, REG_A, MEM(0xFF00 + instr[1]));
}

INSTRUCTION_EXECUTE_FN(loadAToIOPortImm8) {
    MEM(0xFF00 + instr[1]) = getReg8(gb, REG_A);
}

INSTRUCTION_EXECUTE_FN(loadIOPortCToA) {
    setReg8(gb, REG_A, MEM(0xFF00 + getReg8(gb, REG_C)));
}

INSTRUCTION_EXECUTE_FN(loadAToIOPortC) {
    MEM(0xFF00 + getReg8(gb, REG_C)) = getReg8(gb, REG_A);
}

INSTRUCTION_EXECUTE_FN(loadAndIncrementAToAddrHL) {
    MEM(REG(HL)) = getReg8(gb, REG_A);
    REG(HL)++;
}

INSTRUCTION_EXECUTE_FN(loadAndIncrementAddrHLToA) {
    setReg8(gb, REG_A, MEM(REG(HL)));
    REG(HL)++;
}

INSTRUCTION_EXECUTE_FN(loadAndDecrementAToAddrHL) {
    MEM(REG(HL)) = getReg8(gb, REG_A);
    REG(HL)--;
}

INSTRUCTION_EXECUTE_FN(loadAndDecrementAddrHLToA) {
    setReg8(gb, REG_A, MEM(REG(HL)));
    REG(HL)--;
}

INSTRUCTION_EXECUTE_FN(loadImm16ToReg) {
    enum Register16 dst = BC_DE_HL_SP[(instr[0] - 1) >> 4];

    gb->registers[dst] = get16BitArgument(instr);
}

INSTRUCTION_EXECUTE_FN(loadSPToAddr16) {
    write16Bits(&MEM(get16BitArgument(instr)), REG(SP));
}

INSTRUCTION_EXECUTE_FN(loadSPToHL) {
    REG(HL) = REG(SP);
}

INSTRUCTION_EXECUTE_FN(push) {
    enum Register16 src = BC_DE_HL_AF[(instr[0] - 0xC5) >> 4];
    
    REG(SP) = REG(SP) - 2;
    write16Bits(&MEM(REG(SP)), gb->registers[src]);
}

INSTRUCTION_EXECUTE_FN(pop) {
    enum Register16 dst = BC_DE_HL_AF[(instr[0] - 0xC1) >> 4];

    gb->registers[dst] = read16Bits(&MEM(REG(SP)));
    REG(SP) = REG(SP) + 2;
}

INSTRUCTION_EXECUTE_FN(addReg) {
    enum Register8 rhs = instr[0] - 0x80;
    uint8 result = getReg8(gb, REG_A) + getReg8(gb, rhs);

    setReg8(gb, REG_A, result);
    updateZeroFlag(gb, result);
}

INSTRUCTION_EXECUTE_FN(addImm8) {
    uint8 result = getReg8(gb, REG_A) + instr[1];
    
    setReg8(gb, REG_A, result);
    
    updateZeroFlag(gb, result);
}

INSTRUCTION_EXECUTE_FN(addAddrHL) {
    uint8 result = getReg8(gb, REG_A) + MEM(REG(HL));
    setReg8(gb, REG_A, result);
    updateZeroFlag(gb, result);
}


INSTRUCTION_EXECUTE_FN(adcReg) {
    enum Register8 rhs = instr[0] - 0x88;

    uint8 result = getReg8(gb, REG_A) + getReg8(gb, rhs) + getFlag(gb, FLAG_C);
    setReg8(gb, REG_A, result);
    updateZeroFlag(gb, result);
}

INSTRUCTION_EXECUTE_FN(adcImm8) {
    uint8 result = getReg8(gb, REG_A) + instr[1] + getFlag(gb, FLAG_C);
    setReg8(gb, REG_A, result);
    updateZeroFlag(gb, result);
}

INSTRUCTION_EXECUTE_FN(adcAddrHL) {
    uint8 result = getReg8(gb, REG_A) + MEM(REG(HL) + getFlag(gb, FLAG_C));
    setReg8(gb, REG_A, result);
    updateZeroFlag(gb, result);
}


INSTRUCTION_EXECUTE_FN(subReg) {
    enum Register8 rhs = instr[0] - 0x90;

    uint8 result = getReg8(gb, REG_A) - getReg8(gb, rhs);
    setReg8(gb, REG_A, result);
    updateZeroFlag(gb, result);
}

INSTRUCTION_EXECUTE_FN(subImm8) {
    uint8 result = getReg8(gb, REG_A) - instr[1];
    setReg8(gb, REG_A, result);
    updateZeroFlag(gb, result);
}

INSTRUCTION_EXECUTE_FN(subAddrHL) {
    uint8 result = getReg8(gb, REG_A) - MEM(REG(HL));
    setReg8(gb, REG_A, result);
    updateZeroFlag(gb, result);
}


INSTRUCTION_EXECUTE_FN(sbcReg) {
    enum Register8 rhs = instr[0] - 0x98;

    uint8 result = getReg8(gb, REG_A) - getReg8(gb, rhs) - getFlag(gb, FLAG_C);
    setReg8(gb, REG_A, result);
    updateZeroFlag(gb, result);
}

INSTRUCTION_EXECUTE_FN(sbcImm8) {
    uint8 result = getReg8(gb, REG_A) - instr[1] - getFlag(gb, FLAG_C);
    setReg8(gb, REG_A, result);
    updateZeroFlag(gb, result);
}

INSTRUCTION_EXECUTE_FN(sbcAddrHL) {
    uint8 result = getReg8(gb, REG_A) - MEM(REG(HL)) - getFlag(gb, FLAG_C);
    setReg8(gb, REG_A, result);
    updateZeroFlag(gb, result);
}


INSTRUCTION_EXECUTE_FN(andReg) {
    enum Register8 rhs = instr[0] - 0xA0;

    uint8 result = getReg8(gb, REG_A) & getReg8(gb, rhs);
    setReg8(gb, REG_A, result);
    updateZeroFlag(gb, result);
}

INSTRUCTION_EXECUTE_FN(andImm8) {
    uint8 result = getReg8(gb, REG_A) & instr[1];
    setReg8(gb, REG_A, result);
    updateZeroFlag(gb, result);
}

INSTRUCTION_EXECUTE_FN(andAddrHL) {
    uint8 result = getReg8(gb, REG_A) & MEM(REG(HL));
    setReg8(gb, REG_A, result);
    updateZeroFlag(gb, result);
}


INSTRUCTION_EXECUTE_FN(xorReg) {
    enum Register8 rhs = instr[0] - 0xA8;

    uint8 result = getReg8(gb, REG_A) ^ getReg8(gb, rhs);
    setReg8(gb, REG_A, result);
    updateZeroFlag(gb, result);
}

INSTRUCTION_EXECUTE_FN(xorImm8) {
    uint8 result = getReg8(gb, REG_A) ^ instr[1];
    setReg8(gb, REG_A, result);
    updateZeroFlag(gb, result);
}

INSTRUCTION_EXECUTE_FN(xorAddrHL) {
    uint8 result = getReg8(gb, REG_A) ^ MEM(REG(HL));
    setReg8(gb, REG_A, result);
    updateZeroFlag(gb, result);
}


INSTRUCTION_EXECUTE_FN(orReg) {
    enum Register8 rhs = instr[0] - 0xB0;

    uint8 result = getReg8(gb, REG_A) | getReg8(gb, rhs);
    setReg8(gb, REG_A, result);
    updateZeroFlag(gb, result);
}

INSTRUCTION_EXECUTE_FN(orImm8) {
    uint8 result = getReg8(gb, REG_A) | instr[1];
    setReg8(gb, REG_A, result);
    updateZeroFlag(gb, result);
}

INSTRUCTION_EXECUTE_FN(orAddrHL) {
    uint8 result = getReg8(gb, REG_A) | MEM(REG(HL));
    setReg8(gb, REG_A, result);
    updateZeroFlag(gb, result);
}


INSTRUCTION_EXECUTE_FN(compareReg) {
    enum Register8 rhs = instr[0] - 0xB8;

    // TODO(octave)
    NOT_IMPLEMENTED();
}

void compareWithValue(GameBoy* gb, uint8 value) {
    setFlag(gb, FLAG_Z, getReg8(gb, REG_A) == value);
    setFlag(gb, FLAG_N, 1);
    // TODO(octave) : set H flag
    setFlag(gb, FLAG_Z, getReg8(gb, REG_A) < value);
}

INSTRUCTION_EXECUTE_FN(compareImm8) {
    compareWithValue(gb, instr[1]);
}

INSTRUCTION_EXECUTE_FN(compareAddrHL) {
    compareWithValue(gb, MEM(REG(HL)));
}


INSTRUCTION_EXECUTE_FN(incReg) {
    enum Register8 reg = (instr[0] - 0x04) >> 3;

    uint8 result = getReg8(gb, reg) + 1;
    setReg8(gb, reg, result);

    updateZeroFlag(gb, result);
}

INSTRUCTION_EXECUTE_FN(incAddrHL) {
    MEM(REG(HL))++;
    updateZeroFlag(gb, MEM(REG(HL)));
}

INSTRUCTION_EXECUTE_FN(decReg) {
    enum Register8 reg = (instr[0] - 0x05) >> 3;

    uint8 result = getReg8(gb, reg) - 1;
    setReg8(gb, reg, result);
    
    updateZeroFlag(gb, result);
}

INSTRUCTION_EXECUTE_FN(decAddrHL) {
    MEM(REG(HL))--;
    updateZeroFlag(gb, MEM(REG(HL)));
}

INSTRUCTION_EXECUTE_FN(decimalAdjust) {
    // TODO(octave)
    NOT_IMPLEMENTED();
}

INSTRUCTION_EXECUTE_FN(complement) {
    setReg8(gb, REG_A, ~getReg8(gb, REG_A));
}


INSTRUCTION_EXECUTE_FN(addReg16ToHL) {
    enum Register16 reg = BC_DE_HL_SP[(instr[0] - 0x09) >> 4];

    REG(HL) += gb->registers[reg];
}

INSTRUCTION_EXECUTE_FN(incReg16) {
    enum Register16 reg = BC_DE_HL_SP[(instr[0] - 0x03) >> 4];

    gb->registers[reg]++;
}

INSTRUCTION_EXECUTE_FN(decReg16) {
    enum Register16 reg = BC_DE_HL_SP[(instr[0] - 0x08) >> 4];
    
    gb->registers[reg]--;
}

INSTRUCTION_EXECUTE_FN(addSignedToSP) {
    REG(SP) += getSigned8BitArgument(instr);
}

INSTRUCTION_EXECUTE_FN(loadSignedPlusSPToHL) {
    REG(HL) = REG(HL) + getSigned8BitArgument(instr);
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
    setFlag(gb, FLAG_C, a & 1);
}

INSTRUCTION_EXECUTE_FN(rotateALeftThroughCarry) {
    // TODO(octave)
    NOT_IMPLEMENTED();
}

// TODO(octave) : Flags for bit manip functions

INSTRUCTION_EXECUTE_FN(rotateARight) {
    uint8 a = getReg8(gb, REG_A);
    setReg8(gb, REG_A, rotateRight(a));
}

INSTRUCTION_EXECUTE_FN(rotateARightThroughCarry) {
    // TODO(octave)
    NOT_IMPLEMENTED();
}

INSTRUCTION_EXECUTE_FN(rotateRegLeft) {
    enum Register8 reg = instr[0];

    setReg8(gb, reg, rotateLeft(getReg8(gb, reg)));
}

INSTRUCTION_EXECUTE_FN(rotateAddrHLLeft) {
    MEM(REG(HL)) = rotateLeft(MEM(REG(HL)));
}

INSTRUCTION_EXECUTE_FN(rotateRegLeftThroughCarry) {
    // TODO(octave)
    NOT_IMPLEMENTED();
}

INSTRUCTION_EXECUTE_FN(rotateAddrHLLeftThroughCarry) {
    // TODO(octave)
    NOT_IMPLEMENTED();
}

INSTRUCTION_EXECUTE_FN(rotateRegRight) {
    enum Register8 reg = instr[0] - 0x08;

    setReg8(gb, reg, rotateRight(getReg8(gb, reg)));
}

INSTRUCTION_EXECUTE_FN(rotateAddrHLRight) {
    MEM(REG(HL)) = rotateRight(MEM(REG(HL)));
}

INSTRUCTION_EXECUTE_FN(rotateRegRightThroughCarry) {
    // TODO(octave)
    NOT_IMPLEMENTED();
}

INSTRUCTION_EXECUTE_FN(rotateAddrHLRightThroughCarry) {
    // TODO(octave)
    NOT_IMPLEMENTED(); 
}

INSTRUCTION_EXECUTE_FN(shiftRegLeftArithmetic) {
    enum Register8 reg = instr[0] - 0x20;

    setReg8(gb, reg, getReg8(gb, reg) << 1);
}

uint8 shiftRightArithmetic(uint8 value) {
    return (value >> 1) | (value & 0x80);
}

INSTRUCTION_EXECUTE_FN(shiftRegRightArithmetic) {
    enum Register8 reg = instr[0] - 0x20;

    setReg8(gb, reg, shiftRightArithmetic(getReg8(gb, reg)));
}

INSTRUCTION_EXECUTE_FN(shiftAddrHLLeftArithmetic) {
    MEM(REG(HL)) = MEM(REG(HL)) << 1;
}

INSTRUCTION_EXECUTE_FN(shiftAddrHLRightArithmetic) {
    MEM(REG(HL)) = shiftRightArithmetic(MEM(REG(HL)));
}

uint8 swapNibbles(uint8 value) {
    return (value >> 4) | (value << 4);
}

INSTRUCTION_EXECUTE_FN(swapNibblesReg) {
    enum Register8 reg = instr[0] - 0x30;

    setReg8(gb, reg, swapNibbles(getReg8(gb, reg)));
}

INSTRUCTION_EXECUTE_FN(swapNibblesAddrHL) {
    MEM(REG(HL)) = swapNibbles(MEM(REG(HL)));
}

INSTRUCTION_EXECUTE_FN(shiftRegRightLogical) {
    enum Register8 reg = instr[0] - 0x38;

    setReg8(gb, reg, getReg8(gb, reg) >> 1);
}

INSTRUCTION_EXECUTE_FN(shiftAddrHLRightLogical) {
    MEM(REG(HL)) = MEM(REG(HL)) >> 1;
}


INSTRUCTION_EXECUTE_FN(testBitReg) {
    enum Register8 reg = instr[0] % 8;

    uint8 bitIndex = (instr[0] - 0x40) >> 3;
    uint8 bitValue = getBit(getReg8(gb, reg), bitIndex);

    setFlag(gb, FLAG_Z, bitValue);
    setFlag(gb, FLAG_N, 0);
    setFlag(gb, FLAG_H, 1);
}

INSTRUCTION_EXECUTE_FN(testBitAddrHL) {
    uint8 bitIndex = (instr[0] - 0x40) >> 3;
    uint8 bitValue = getBit(MEM(REG(HL)), bitIndex);

    setFlag(gb, FLAG_Z, bitValue);
    setFlag(gb, FLAG_N, 0);
    setFlag(gb, FLAG_H, 1);
}

uint8 setBit(uint8 value, uint8 index) {
    return value | (1 << index);
}

INSTRUCTION_EXECUTE_FN(setBitReg) {
    enum Register8 reg = instr[0] % 8;
    uint8 bitIndex = (instr[0] - 0xC0) >> 3;
    uint8 regVal = getReg8(gb, reg);

    setReg8(gb, reg, setBit(regVal, bitIndex));
}

INSTRUCTION_EXECUTE_FN(setBitAddrHL) {
    uint8 bitIndex = (instr[0] - 0xC0) >> 3;

    MEM(REG(HL)) = setBit(MEM(REG(HL)), bitIndex);
}

uint8 resetBit(uint8 value, uint8 index) {
    return value & ~(1 << index);
}

INSTRUCTION_EXECUTE_FN(resetBitReg) {
    enum Register8 reg = instr[0] % 8;
    uint8 bitIndex = (instr[0] - 0xC0) >> 3;
    uint8 regVal = getReg8(gb, reg);

    setReg8(gb, reg, resetBit(regVal, bitIndex));
}

INSTRUCTION_EXECUTE_FN(resetBitAddrHL) {
    uint8 bitIndex = (instr[0] - 0xC0) >> 3;

    MEM(REG(HL)) = resetBit(REG(HL), bitIndex);
}


INSTRUCTION_EXECUTE_FN(flipCarry) {
    setFlag(gb, FLAG_C, !getFlag(gb, FLAG_C));
}

INSTRUCTION_EXECUTE_FN(setCarry) {
    setFlag(gb, FLAG_C, 1);
}

INSTRUCTION_EXECUTE_FN(nop) {
    return;
}

INSTRUCTION_EXECUTE_FN(halt) {
    // TODO(octave)
    NOT_IMPLEMENTED();
}

INSTRUCTION_EXECUTE_FN(stop) {
    // TODO(octave)
    NOT_IMPLEMENTED();
}

INSTRUCTION_EXECUTE_FN(disableInterrupts) {
    gb->ime = 0;
}

INSTRUCTION_EXECUTE_FN(enableInterrupts) {
    gb->ime = 1;
}


INSTRUCTION_EXECUTE_FN(jumpImm16) {
    uint16 prevPC = REG(PC);
    REG(PC) = get16BitArgument(instr);

    /* gbprintf(gb, "Jump 0x%04X -> 0x%04X\n", prevPC, REG(PC)); */
}

INSTRUCTION_EXECUTE_FN(jumpHL) {
    uint16 prevPC = REG(PC);
    REG(PC) = REG(HL);
    
    /* gbprintf(gb, "Jump HL 0x%04X -> 0x%04X\n", prevPC, REG(PC)); */
}

enum Conditional {
    COND_NZ,
    COND_Z,
    COND_NC,
    COND_C,
};

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
    enum Conditional cond = (instr[0] - 0xC2) >> 3;

    if (checkCondition(gb, cond)) {
        jumpImm16(gb, instr);
    }
}

INSTRUCTION_EXECUTE_FN(relativeJump) {
    // TODO(octave) : check and test this, possible off-by-one error ?
    uint16 prevPC = REG(PC);
    REG(PC) += getSigned8BitArgument(instr);
    /* printf("Relative jump 0x%04X -> 0x%04X\n", prevPC, REG(PC)); */
}

INSTRUCTION_EXECUTE_FN(conditionalRelativeJump) {
    enum Conditional cond = (instr[0] - 0x20) >> 3;

    if (checkCondition(gb, cond)) {
        relativeJump(gb, instr);
    }
}

INSTRUCTION_EXECUTE_FN(callImm16) {
    uint16 prevPC = REG(PC);
    
    REG(SP) -= 2;
    write16Bits(&MEM(REG(SP)), REG(PC));
    REG(PC) = get16BitArgument(instr);

    gbprintf(gb, "Call 0x%04X -> 0x%04X\n", prevPC, REG(PC));
    gb->callStackHeight++;
}

INSTRUCTION_EXECUTE_FN(conditionalCallImm16) {
    enum Conditional cond = (instr[0] - 0xC4) >> 3;

    if (checkCondition(gb, cond)) {
        callImm16(gb, instr);
    }
}

INSTRUCTION_EXECUTE_FN(ret) {
    uint16 prevPC = REG(PC);
    
    REG(PC) = read16Bits(&MEM(REG(SP)));
    REG(SP) += 2;

    gbprintf(gb, "Return 0x%04X -> 0x%04X\n", prevPC, REG(PC));
    gb->callStackHeight--;
}

INSTRUCTION_EXECUTE_FN(conditionalRet) {
    enum Conditional cond = (instr[0] - 0xC0) >> 3;

    if (checkCondition(gb, cond)) {
        ret(gb, instr);
    }
}

INSTRUCTION_EXECUTE_FN(retAndEnableInterrupts) {
    enableInterrupts(gb, instr);
    ret(gb, instr);
}

INSTRUCTION_EXECUTE_FN(reset) {
    uint16 address = instr[0] - 0xC7;
    REG(SP) -= 2;
    write16Bits(&MEM(REG(SP)), REG(PC));

    uint16 prevPC = REG(PC);
    REG(PC) = address;
    printf("Reset 0x%04X -> 0x%04X\n", prevPC, REG(PC));
}

INSTRUCTION_EXECUTE_FN(prefixCB) {
    instr++;

    bool32 isAddrHL = (instr[0] % 8) == 6;

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
    
    if (instr[0] < 0x40) {
        uint8 instrIndex = instr[0] >> 3;
        dispatchTable[instrIndex](gb, instr);
    } else if (instr[0] < 0x80) { // test bit
        if (isAddrHL) {
            testBitAddrHL(gb, instr);
        } else {
            testBitReg(gb, instr);
        }
    } else if (instr[0] < 0xC0) { // reset bit
        if (isAddrHL) {
            resetBitAddrHL(gb, instr);
        } else {
            resetBitReg(gb, instr);
        }
    } else { // set bit
        if (isAddrHL) {
            setBitAddrHL(gb, instr);
        } else {
            setBitReg(gb, instr);
        }
    }
}

#define VARIABLE_CYCLES 0xFFFF

#define INSTR(length, cycles, handler) {length, cycles, handler, #handler}

// reference : https://www.pastraiser.com/cpu/gameboy/gameboy_opcodes.html
static InstructionHandler instructionHandlers[256] = {
    // 00
    INSTR(1, 4, nop),
    INSTR(3, 12, loadImm16ToReg),
    INSTR(1, 8, loadAToAddrBC),
    INSTR(1, 8, incReg16),
    INSTR(1, 4, incReg),
    INSTR(1, 4, decReg),
    INSTR(2, 8, loadImm8ToReg),
    INSTR(1, 4, rotateALeft),
    
    INSTR(3, 20, loadSPToAddr16),
    INSTR(1, 8, addReg16ToHL),
    INSTR(1, 8, loadAddrBCToA),
    INSTR(1, 8, decReg16),
    INSTR(1, 4, incReg),
    INSTR(1, 4, decReg),
    INSTR(2, 8, loadImm8ToReg),
    INSTR(1, 4, rotateARight),

    // 10
    INSTR(2, 4, stop),
    INSTR(3, 12, loadImm16ToReg),
    INSTR(1, 8, loadAToAddrDE),
    INSTR(1, 8, incReg16),
    INSTR(1, 4, incReg),
    INSTR(1, 4, decReg),
    INSTR(2, 8, loadImm8ToReg),
    INSTR(1, 4, rotateALeftThroughCarry),
    
    INSTR(2, 12, relativeJump),
    INSTR(1, 8, addReg16ToHL),
    INSTR(1, 8, loadAddrDEToA),
    INSTR(1, 8, decReg16),
    INSTR(1, 4, incReg),
    INSTR(1, 4, decReg),
    INSTR(2, 8, loadImm8ToReg),
    INSTR(1, 4, rotateARightThroughCarry),

    // 20
    INSTR(2, VARIABLE_CYCLES, conditionalRelativeJump),
    INSTR(3, 12, loadImm16ToReg),
    INSTR(1, 8, loadAndIncrementAToAddrHL),
    INSTR(1, 8, incReg16),
    INSTR(1, 4, incReg),
    INSTR(1, 4, decReg),
    INSTR(2, 8, loadImm8ToReg),
    INSTR(1, 4, decimalAdjust),
    
    INSTR(2, VARIABLE_CYCLES, conditionalRelativeJump),
    INSTR(1, 8, addReg16ToHL),
    INSTR(1, 8, loadAndIncrementAddrHLToA),
    INSTR(1, 8, decReg16),
    INSTR(1, 4, incReg),
    INSTR(1, 4, decReg),
    INSTR(2, 8, loadImm8ToReg),
    INSTR(1, 4, complement),

    // 30
    INSTR(2, VARIABLE_CYCLES, conditionalRelativeJump),
    INSTR(3, 12, loadImm16ToReg),
    INSTR(1, 8, loadAndDecrementAToAddrHL),
    INSTR(1, 8, incReg16),
    INSTR(1, 12, incAddrHL),
    INSTR(1, 12, decAddrHL),
    INSTR(2, 12, loadImm8ToAddrHL),
    INSTR(1, 4, setCarry),
    
    INSTR(2, VARIABLE_CYCLES, conditionalRelativeJump),
    INSTR(1, 8, addReg16ToHL),
    INSTR(1, 8, loadAndDecrementAddrHLToA),
    INSTR(1, 8, decReg16),
    INSTR(1, 4, incReg),
    INSTR(1, 4, decReg),
    INSTR(2, 8, loadImm8ToReg),
    INSTR(1, 4, flipCarry),

    // 40
    INSTR(1, 4, loadRegToReg),
    INSTR(1, 4, loadRegToReg),
    INSTR(1, 4, loadRegToReg),
    INSTR(1, 4, loadRegToReg),
    INSTR(1, 4, loadRegToReg),
    INSTR(1, 4, loadRegToReg),
    INSTR(1, 8, loadAddrHLToReg),
    INSTR(1, 4, loadRegToReg),

    INSTR(1, 4, loadRegToReg),
    INSTR(1, 4, loadRegToReg),
    INSTR(1, 4, loadRegToReg),
    INSTR(1, 4, loadRegToReg),
    INSTR(1, 4, loadRegToReg),
    INSTR(1, 4, loadRegToReg),
    INSTR(1, 8, loadAddrHLToReg),
    INSTR(1, 4, loadRegToReg),

    // 50
    INSTR(1, 4, loadRegToReg),
    INSTR(1, 4, loadRegToReg),
    INSTR(1, 4, loadRegToReg),
    INSTR(1, 4, loadRegToReg),
    INSTR(1, 4, loadRegToReg),
    INSTR(1, 4, loadRegToReg),
    INSTR(1, 8, loadAddrHLToReg),
    INSTR(1, 4, loadRegToReg),

    INSTR(1, 4, loadRegToReg),
    INSTR(1, 4, loadRegToReg),
    INSTR(1, 4, loadRegToReg),
    INSTR(1, 4, loadRegToReg),
    INSTR(1, 4, loadRegToReg),
    INSTR(1, 4, loadRegToReg),
    INSTR(1, 8, loadAddrHLToReg),
    INSTR(1, 4, loadRegToReg),

    // 60
    INSTR(1, 4, loadRegToReg),
    INSTR(1, 4, loadRegToReg),
    INSTR(1, 4, loadRegToReg),
    INSTR(1, 4, loadRegToReg),
    INSTR(1, 4, loadRegToReg),
    INSTR(1, 4, loadRegToReg),
    INSTR(1, 8, loadAddrHLToReg),
    INSTR(1, 4, loadRegToReg),

    INSTR(1, 4, loadRegToReg),
    INSTR(1, 4, loadRegToReg),
    INSTR(1, 4, loadRegToReg),
    INSTR(1, 4, loadRegToReg),
    INSTR(1, 4, loadRegToReg),
    INSTR(1, 4, loadRegToReg),
    INSTR(1, 8, loadAddrHLToReg),
    INSTR(1, 4, loadRegToReg),

    // 70
    INSTR(1, 4, loadRegToAddrHL),
    INSTR(1, 4, loadRegToAddrHL),
    INSTR(1, 4, loadRegToAddrHL),
    INSTR(1, 4, loadRegToAddrHL),
    INSTR(1, 4, loadRegToAddrHL),
    INSTR(1, 4, loadRegToAddrHL),
    INSTR(1, 4, halt),
    INSTR(1, 4, loadRegToAddrHL),

    INSTR(1, 4, loadRegToReg),
    INSTR(1, 4, loadRegToReg),
    INSTR(1, 4, loadRegToReg),
    INSTR(1, 4, loadRegToReg),
    INSTR(1, 4, loadRegToReg),
    INSTR(1, 4, loadRegToReg),
    INSTR(1, 8, loadAddrHLToReg),
    INSTR(1, 4, loadRegToReg),

    // 80
    INSTR(1, 4, addReg),
    INSTR(1, 4, addReg),
    INSTR(1, 4, addReg),
    INSTR(1, 4, addReg),
    INSTR(1, 4, addReg),
    INSTR(1, 4, addReg),
    INSTR(1, 8, addAddrHL),
    INSTR(1, 4, addReg),

    INSTR(1, 4, adcReg),
    INSTR(1, 4, adcReg),
    INSTR(1, 4, adcReg),
    INSTR(1, 4, adcReg),
    INSTR(1, 4, adcReg),
    INSTR(1, 4, adcReg),
    INSTR(1, 8, adcAddrHL),
    INSTR(1, 4, adcReg),

    // 90
    INSTR(1, 4, subReg),
    INSTR(1, 4, subReg),
    INSTR(1, 4, subReg),
    INSTR(1, 4, subReg),
    INSTR(1, 4, subReg),
    INSTR(1, 4, subReg),
    INSTR(1, 8, subAddrHL),
    INSTR(1, 4, subReg),

    INSTR(1, 4, sbcReg),
    INSTR(1, 4, sbcReg),
    INSTR(1, 4, sbcReg),
    INSTR(1, 4, sbcReg),
    INSTR(1, 4, sbcReg),
    INSTR(1, 4, sbcReg),
    INSTR(1, 8, sbcAddrHL),
    INSTR(1, 4, sbcReg),

    // A0
    INSTR(1, 4, andReg),
    INSTR(1, 4, andReg),
    INSTR(1, 4, andReg),
    INSTR(1, 4, andReg),
    INSTR(1, 4, andReg),
    INSTR(1, 4, andReg),
    INSTR(1, 8, andAddrHL),
    INSTR(1, 4, andReg),

    INSTR(1, 4, xorReg),
    INSTR(1, 4, xorReg),
    INSTR(1, 4, xorReg),
    INSTR(1, 4, xorReg),
    INSTR(1, 4, xorReg),
    INSTR(1, 4, xorReg),
    INSTR(1, 8, xorAddrHL),
    INSTR(1, 4, xorReg),

    // B0
    INSTR(1, 4, orReg),
    INSTR(1, 4, orReg),
    INSTR(1, 4, orReg),
    INSTR(1, 4, orReg),
    INSTR(1, 4, orReg),
    INSTR(1, 4, orReg),
    INSTR(1, 8, orAddrHL),
    INSTR(1, 4, orReg),

    INSTR(1, 4, compareReg),
    INSTR(1, 4, compareReg),
    INSTR(1, 4, compareReg),
    INSTR(1, 4, compareReg),
    INSTR(1, 4, compareReg),
    INSTR(1, 4, compareReg),
    INSTR(1, 8, compareAddrHL),
    INSTR(1, 4, compareReg),

    // C0
    INSTR(1, VARIABLE_CYCLES, conditionalRet),
    INSTR(1, 12, pop),
    INSTR(3, VARIABLE_CYCLES, conditionalJumpImm16),
    INSTR(3, 16, jumpImm16),
    INSTR(3, VARIABLE_CYCLES, conditionalCallImm16),
    INSTR(1, 16, push),
    INSTR(2, 8, addImm8),
    INSTR(1, 16, reset),
    
    INSTR(1, VARIABLE_CYCLES, conditionalRet),
    INSTR(1, 16, ret),
    INSTR(3, VARIABLE_CYCLES, conditionalJumpImm16),
    INSTR(2, 4, prefixCB),
    INSTR(3, VARIABLE_CYCLES, conditionalCallImm16),
    INSTR(3, 24, callImm16),
    INSTR(2, 8, adcImm8),
    INSTR(1, 16, reset),

    // D0
    INSTR(1, VARIABLE_CYCLES, conditionalRet),
    INSTR(1, 12, pop),
    INSTR(3, VARIABLE_CYCLES, conditionalJumpImm16),
    {},
    INSTR(3, VARIABLE_CYCLES, conditionalCallImm16),
    INSTR(1, 16, push),
    INSTR(2, 8, subImm8),
    INSTR(1, 16, reset),
    
    INSTR(1, VARIABLE_CYCLES, conditionalRet),
    INSTR(1, 16, retAndEnableInterrupts),
    INSTR(3, VARIABLE_CYCLES, conditionalJumpImm16),
    {},
    INSTR(3, VARIABLE_CYCLES, conditionalCallImm16),
    {},
    INSTR(2, 8, sbcImm8),
    INSTR(1, 16, reset),

    // E0
    INSTR(2, 12, loadAToIOPortImm8),
    INSTR(1, 12, pop),
    INSTR(1, 8, loadAToIOPortC),
    {},
    {},
    INSTR(1, 16, push),
    INSTR(2, 8, andImm8),
    INSTR(1, 16, reset),

    INSTR(2, 16, addSignedToSP),
    INSTR(1, 4, jumpHL),
    INSTR(3, 16, loadAToAddr16),
    {},
    {},
    {},
    INSTR(2, 8, xorImm8),
    INSTR(1, 16, reset),

    // F0
    INSTR(2, 12, loadIOPortImm8ToA),
    INSTR(1, 12, pop),
    INSTR(1, 8, loadIOPortCToA),
    INSTR(1, 4, disableInterrupts),
    {},
    INSTR(1, 16, push),
    INSTR(2, 8, andImm8),
    INSTR(1, 16, reset),

    INSTR(2, 16, addSignedToSP),
    INSTR(1, 4, jumpHL),
    INSTR(3, 16, loadAToAddr16),
    INSTR(1, 4, enableInterrupts),
    {},
    {},
    INSTR(2, 8, compareImm8),
    INSTR(1, 16, reset),
};

void executeInstruction(GameBoy* gb) {
    uint8* instr = &gb->memory[REG(PC)];
    InstructionHandler* handler = &instructionHandlers[instr[0]];

    if (!handler->execute) {
        fprintf(stderr, "Invalid opcode %x at 0x%04X\n", instr[0], REG(PC));
        exit(1);
    }

    /* printf("Executing %s @0x%04X\n", handler->name, REG(PC)); */

    REG(PC) += handler->length;

    handler->execute(gb, instr);
}

void handleInterrupt(GameBoy* gb) {
    uint16 interruptAddresses[] = {
        0x0040, // V-blank
        0x0048, // LCDC Status
        0x0050, // Timer overflow
        0x0058, // Serial transfer
        0x0060, // Hi-Lo of P10-P13
    };

    uint8 enabledPendingInterrupts = MEM(MMR_IF) & MEM(MMR_IE);
    
    // find highest-priority, enabled interrupt that was triggered
    for (uint8 interruptIndex = 0;
         interruptIndex < ARRAY_COUNT(interruptAddresses);
         interruptIndex++) {
        if (getBit(MEM(MMR_IF), interruptIndex)) {
            gbprintf(gb, "Handling interrupt %d\n", interruptIndex);
            gb->callStackHeight++;
            
            // disable interrupts
            gb->ime = 0;

            // reset this interrupt's flag
            MEM(MMR_IF) = resetBit(MEM(MMR_IF), interruptIndex);
        
            // push PC
            REG(SP) = REG(SP) - 2;
            write16Bits(&MEM(REG(SP)), REG(PC));

            // jump to interrupt handler
            REG(PC) = interruptAddresses[interruptIndex];
            break;
        }
    }
}

void executeCycle(GameBoy* gb) {
    executeInstruction(gb);
    if (gb->ime) {
        handleInterrupt(gb);
    }
}
