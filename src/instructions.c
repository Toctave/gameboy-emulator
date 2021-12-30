#include "gameboy.h"

#include <stdio.h>
#include <stdlib.h>

#define INSTRUCTION_EXECUTE_FN_NOSTATIC(name) void name(GameBoy* gb, uint8* instr)
#define INSTRUCTION_EXECUTE_FN(name) static INSTRUCTION_EXECUTE_FN_NOSTATIC(name)

#define REG(name) gb->registers[REG_##name]
#define MEM(addr) gb->memory[addr]

typedef INSTRUCTION_EXECUTE_FN_NOSTATIC(InstructionExecuteFn);

static enum Register16 BC_DE_HL_SP[4] = {REG_BC, REG_DE, REG_HL, REG_SP};
static enum Register16 BC_DE_HL_AF[4] = {REG_BC, REG_DE, REG_HL, REG_AF};

static uint8 getHighByte(GameBoy* gb, enum Register16 reg) {
    return gb->registers[reg] >> 8;
}

static uint8 getLowByte(GameBoy* gb, enum Register16 reg) {
    return gb->registers[reg] & 0xFF;
}

static uint8 getCarryBit(GameBoy* gb) {
    uint8 flags = getLowByte(gb, REG_AF);

    return (flags >> 4) & 0x01;
}

static void setHighByte(GameBoy* gb, enum Register16 reg, uint8 value) {
    gb->registers[reg] =
        (value << 8) | (gb->registers[reg] & 0xFF);
}

static void setLowByte(GameBoy* gb, enum Register16 reg, uint8 value) {
    gb->registers[reg] =
        (gb->registers[reg] & 0xFF00) | value;
}

static uint16 read16Bits(uint8* address) {
    return (address[0] << 8) | address[1];
}

static void write16Bits(uint8* address, uint16 value) {
    address[0] = value >> 8;
    address[1] = value & 0xFF;
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

static uint8 getReg8(GameBoy* gb, uint8 index) {
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

static void setReg8(GameBoy* gb, uint8 index, uint8 value) {
    switch (index) {
    case REG_B:
        setHighByte(gb, REG_BC, value);
    case REG_C:
        setLowByte(gb, REG_BC, value);
    case REG_D:
        setHighByte(gb, REG_DE, value);
    case REG_E:
        setLowByte(gb, REG_DE, value);
    case REG_H:
        setHighByte(gb, REG_HL, value);
    case REG_L:
        setLowByte(gb, REG_HL, value);
    case REG_A:
        setHighByte(gb, REG_AF, value);
    default:
        fprintf(stderr, "Unknown 8-bit register index %u\n", index);
        exit(1);
        break;
    }
}

typedef struct InstructionHandler {
    uint16 length;
    uint16 cycles;
    InstructionExecuteFn* execute;
} InstructionHandler;

INSTRUCTION_EXECUTE_FN(loadRegToReg) {
    uint8 srcIndex = instr[0] & 0x7;
    uint8 dstIndex = (instr[0] >> 3) & 0x7;

    setReg8(gb, dstIndex, getReg8(gb, srcIndex));
}

INSTRUCTION_EXECUTE_FN(loadImm8ToReg) {
    uint8 regIndex = (instr[0] - 6) >> 3;

    setReg8(gb, regIndex, instr[1]);
}

INSTRUCTION_EXECUTE_FN(loadAddrHLToReg) {
    uint8 dstIndex = (instr[0] >> 3) & 0x7;
    
    setReg8(gb, dstIndex, MEM(REG(HL)));
}

INSTRUCTION_EXECUTE_FN(loadRegToAddrHL) {
    uint8 srcIndex = instr[0] & 0x7;
    
    MEM(REG(HL)) = getReg8(gb, srcIndex);
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
    MEM(get16BitArgument(instr)) = REG(SP);
}

INSTRUCTION_EXECUTE_FN(loadSPToHL) {
    REG(HL) = REG(SP);
}

INSTRUCTION_EXECUTE_FN(push) {
    enum Register16 src = BC_DE_HL_AF[(instr[0] - 0xC5) >> 4];
    
    REG(SP) = REG(SP) - 2;
    MEM(REG(SP)) = gb->registers[src];
}

INSTRUCTION_EXECUTE_FN(pop) {
    enum Register16 src = BC_DE_HL_AF[(instr[0] - 0xC2) >> 4];

    gb->registers[src] = MEM(REG(SP));
    REG(SP) = REG(SP) + 2;
}

INSTRUCTION_EXECUTE_FN(addReg) {
    enum Register8 rhs = instr[0] - 0x80;

    setReg8(gb, REG_A, getReg8(gb, REG_A) + getReg8(gb, rhs));
}

INSTRUCTION_EXECUTE_FN(addImm8) {
    setReg8(gb, REG_A, getReg8(gb, REG_A) + instr[1]);
}

INSTRUCTION_EXECUTE_FN(addAddrHL) {
    setReg8(gb, REG_A, getReg8(gb, REG_A) + MEM(REG(HL)));
}


INSTRUCTION_EXECUTE_FN(adcReg) {
    enum Register8 rhs = instr[0] - 0x88;

    setReg8(gb, REG_A, getReg8(gb, REG_A) + getReg8(gb, rhs) + getCarryBit(gb));
}

INSTRUCTION_EXECUTE_FN(adcImm8) {
    setReg8(gb, REG_A, getReg8(gb, REG_A) + instr[1] + getCarryBit(gb));
}

INSTRUCTION_EXECUTE_FN(adcAddrHL) {
    setReg8(gb, REG_A, getReg8(gb, REG_A) + MEM(REG(HL) + getCarryBit(gb)));
}


INSTRUCTION_EXECUTE_FN(subReg) {
    enum Register8 rhs = instr[0] - 0x90;

    setReg8(gb, REG_A, getReg8(gb, REG_A) - getReg8(gb, rhs));
}

INSTRUCTION_EXECUTE_FN(subImm8) {
    setReg8(gb, REG_A, getReg8(gb, REG_A) - instr[1]);
}

INSTRUCTION_EXECUTE_FN(subAddrHL) {
    setReg8(gb, REG_A, getReg8(gb, REG_A) - MEM(REG(HL)));
}


INSTRUCTION_EXECUTE_FN(sbcReg) {
    enum Register8 rhs = instr[0] - 0x98;

    setReg8(gb, REG_A, getReg8(gb, REG_A) - getReg8(gb, rhs) - getCarryBit(gb));
}

INSTRUCTION_EXECUTE_FN(sbcImm8) {
    setReg8(gb, REG_A, getReg8(gb, REG_A) - instr[1] - getCarryBit(gb));
}

INSTRUCTION_EXECUTE_FN(sbcAddrHL) {
    setReg8(gb, REG_A, getReg8(gb, REG_A) - MEM(REG(HL)) - getCarryBit(gb));
}


INSTRUCTION_EXECUTE_FN(andReg) {
    enum Register8 rhs = instr[0] - 0xA0;

    setReg8(gb, REG_A, getReg8(gb, REG_A) & getReg8(gb, rhs));
}

INSTRUCTION_EXECUTE_FN(andImm8) {
    setReg8(gb, REG_A, getReg8(gb, REG_A) & instr[1]);
}

INSTRUCTION_EXECUTE_FN(andAddrHL) {
    setReg8(gb, REG_A, getReg8(gb, REG_A) & MEM(REG(HL)));
}


INSTRUCTION_EXECUTE_FN(xorReg) {
    enum Register8 rhs = instr[0] - 0xA8;

    setReg8(gb, REG_A, getReg8(gb, REG_A) ^ getReg8(gb, rhs));
}

INSTRUCTION_EXECUTE_FN(xorImm8) {
    setReg8(gb, REG_A, getReg8(gb, REG_A) ^ instr[1]);
}

INSTRUCTION_EXECUTE_FN(xorAddrHL) {
    setReg8(gb, REG_A, getReg8(gb, REG_A) ^ MEM(REG(HL)));
}


INSTRUCTION_EXECUTE_FN(orReg) {
    enum Register8 rhs = instr[0] - 0xB0;

    setReg8(gb, REG_A, getReg8(gb, REG_A) | getReg8(gb, rhs));
}

INSTRUCTION_EXECUTE_FN(orImm8) {
    setReg8(gb, REG_A, getReg8(gb, REG_A) | instr[1]);
}

INSTRUCTION_EXECUTE_FN(orAddrHL) {
    setReg8(gb, REG_A, getReg8(gb, REG_A) | MEM(REG(HL)));
}


INSTRUCTION_EXECUTE_FN(compareReg) {
    enum Register8 rhs = instr[0] - 0xB8;

    // TODO(octave)
}

INSTRUCTION_EXECUTE_FN(compareImm8) {
    // TODO(octave)
}

INSTRUCTION_EXECUTE_FN(compareAddrHL) {
    // TODO(octave)
}


INSTRUCTION_EXECUTE_FN(incReg) {
    enum Register8 reg = (instr[0] - 0x04) >> 3;

    setReg8(gb, reg, getReg8(gb, reg) + 1);
}

INSTRUCTION_EXECUTE_FN(incAddrHL) {
    MEM(REG(HL))++;
}

INSTRUCTION_EXECUTE_FN(decReg) {
    enum Register8 reg = (instr[0] - 0x05) >> 3;

    setReg8(gb, reg, getReg8(gb, reg) - 1);
}

INSTRUCTION_EXECUTE_FN(decAddrHL) {
    MEM(REG(HL))--;
}

INSTRUCTION_EXECUTE_FN(decimalAdjust) {
    // TODO(octave)
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
    setReg8(gb, REG_A, rotateLeft(a));
}

INSTRUCTION_EXECUTE_FN(rotateALeftThroughCarry) {
    // TODO(octave)    
}

INSTRUCTION_EXECUTE_FN(rotateARight) {
    uint8 a = getReg8(gb, REG_A);
    setReg8(gb, REG_A, rotateRight(a));
}

INSTRUCTION_EXECUTE_FN(rotateARightThroughCarry) {
    // TODO(octave)    
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
}

INSTRUCTION_EXECUTE_FN(rotateAddrHLLeftThroughCarry) {
    // TODO(octave)
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
}

INSTRUCTION_EXECUTE_FN(rotateAddrHLRightThroughCarry) {
    // TODO(octave) : 
}

INSTRUCTION_EXECUTE_FN(shiftRegLeftArithmetic) {
    enum Register8 reg = instr[0] - 0x20;

    setReg8(gb, reg, getReg8(gb, reg) << 1);
}

INSTRUCTION_EXECUTE_FN(shiftAddrHLLeftArithmetic) {
    MEM(REG(HL)) = MEM(REG(HL)) << 1;
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
    // TODO(octave)
}

INSTRUCTION_EXECUTE_FN(testBitAddrHL) {
    // TODO(octave)
}

INSTRUCTION_EXECUTE_FN(setBitReg) {
    // TODO(octave)
}

INSTRUCTION_EXECUTE_FN(setBitAddrHL) {
    // TODO(octave)
}

INSTRUCTION_EXECUTE_FN(resetBitReg) {
    // TODO(octave)
}

INSTRUCTION_EXECUTE_FN(resetBitAddrHL) {
    // TODO(octave)
}


INSTRUCTION_EXECUTE_FN(flipCarry) {
    // TODO(octave)
}

INSTRUCTION_EXECUTE_FN(setCarry) {
    // TODO(octave)
}

INSTRUCTION_EXECUTE_FN(nop) {
    return;
}

INSTRUCTION_EXECUTE_FN(halt) {
    // TODO(octave)
}

INSTRUCTION_EXECUTE_FN(stop) {
    // TODO(octave)
}

INSTRUCTION_EXECUTE_FN(disableInterrupts) {
    MEM(0xFFFF) = 0;
}

INSTRUCTION_EXECUTE_FN(enableInterrupts) {
    MEM(0xFFFF) = 1;
}


INSTRUCTION_EXECUTE_FN(jumpImm16) {
    REG(PC) = get16BitArgument(instr);
}

INSTRUCTION_EXECUTE_FN(jumpHL) {
    REG(PC) = REG(HL);
}

INSTRUCTION_EXECUTE_FN(conditionalJumpImm16) {
    // TODO(octave)
}

INSTRUCTION_EXECUTE_FN(relativeJump) {
    // TODO(octave) : check and test this, possible off-by-one error ?
    REG(PC) += getSigned8BitArgument(instr) - 1;
}

INSTRUCTION_EXECUTE_FN(conditionalRelativeJump) {
    // TODO(octave)
}

INSTRUCTION_EXECUTE_FN(callImm16) {
    REG(SP) -= 2;
    MEM(REG(SP)) = REG(PC);
    REG(PC) = get16BitArgument(instr);
}

INSTRUCTION_EXECUTE_FN(conditionalCallImm16) {
    // TODO(octave)
}

INSTRUCTION_EXECUTE_FN(ret) {
    REG(PC) = MEM(REG(SP));
    REG(SP) += 2;
}

INSTRUCTION_EXECUTE_FN(conditionalRet) {
    // TODO(octave)
}

INSTRUCTION_EXECUTE_FN(retAndEnableInterrupts) {
    enableInterrupts(gb, instr);
    ret(gb, instr);
}

INSTRUCTION_EXECUTE_FN(reset) {
    uint16 address = instr[0] - 0xC7;
    REG(SP) -= 2;
    MEM(REG(SP)) = REG(PC);
    REG(PC) = address;
}

INSTRUCTION_EXECUTE_FN(prefixCB) {
    // TODO(octave)
}

#define VARIABLE_CYCLES 0xFFFF

// reference : https://www.pastraiser.com/cpu/gameboy/gameboy_opcodes.html
static InstructionHandler instructionHandlers[256] = {
    // 00
    {1, 4, nop},
    {3, 12, loadImm16ToReg},
    {1, 8, loadAToAddrBC},
    {1, 8, incReg16},
    {1, 4, incReg},
    {1, 4, decReg},
    {2, 8, loadImm8ToReg},
    {1, 4, rotateALeft},
    
    {3, 20, loadSPToAddr16},
    {1, 8, addReg16ToHL},
    {1, 8, loadAddrBCToA},
    {1, 8, decReg16},
    {1, 4, incReg},
    {1, 4, decReg},
    {2, 8, loadImm8ToReg},
    {1, 4, rotateARight},

    // 10
    {2, 4, stop},
    {3, 12, loadImm16ToReg},
    {1, 8, loadAToAddrDE},
    {1, 8, incReg16},
    {1, 4, incReg},
    {1, 4, decReg},
    {2, 8, loadImm8ToReg},
    {1, 4, rotateALeftThroughCarry},
    
    {2, 12, relativeJump},
    {1, 8, addReg16ToHL},
    {1, 8, loadAddrDEToA},
    {1, 8, decReg16},
    {1, 4, incReg},
    {1, 4, decReg},
    {2, 8, loadImm8ToReg},
    {1, 4, rotateARightThroughCarry},

    // 20
    {2, VARIABLE_CYCLES, conditionalRelativeJump},
    {3, 12, loadImm16ToReg},
    {1, 8, loadAndIncrementAToAddrHL},
    {1, 8, incReg16},
    {1, 4, incReg},
    {1, 4, decReg},
    {2, 8, loadImm8ToReg},
    {1, 4, decimalAdjust},
    
    {2, VARIABLE_CYCLES, conditionalRelativeJump},
    {1, 8, addReg16ToHL},
    {1, 8, loadAndIncrementAddrHLToA},
    {1, 8, decReg16},
    {1, 4, incReg},
    {1, 4, decReg},
    {2, 8, loadImm8ToReg},
    {1, 4, complement},

    // 30
    {2, VARIABLE_CYCLES, conditionalRelativeJump},
    {3, 12, loadImm16ToReg},
    {1, 8, loadAndDecrementAToAddrHL},
    {1, 8, incReg16},
    {1, 12, incAddrHL},
    {1, 12, decAddrHL},
    {2, 12, loadImm8ToAddrHL},
    {1, 4, setCarry},
    
    {2, VARIABLE_CYCLES, conditionalRelativeJump},
    {1, 8, addReg16ToHL},
    {1, 8, loadAndDecrementAddrHLToA},
    {1, 8, decReg16},
    {1, 4, incReg},
    {1, 4, decReg},
    {2, 8, loadImm8ToReg},
    {1, 4, flipCarry},

    // 40
    {1, 4, loadRegToReg},
    {1, 4, loadRegToReg},
    {1, 4, loadRegToReg},
    {1, 4, loadRegToReg},
    {1, 4, loadRegToReg},
    {1, 4, loadRegToReg},
    {1, 8, loadAddrHLToReg},
    {1, 4, loadRegToReg},

    {1, 4, loadRegToReg},
    {1, 4, loadRegToReg},
    {1, 4, loadRegToReg},
    {1, 4, loadRegToReg},
    {1, 4, loadRegToReg},
    {1, 4, loadRegToReg},
    {1, 8, loadAddrHLToReg},
    {1, 4, loadRegToReg},

    // 50
    {1, 4, loadRegToReg},
    {1, 4, loadRegToReg},
    {1, 4, loadRegToReg},
    {1, 4, loadRegToReg},
    {1, 4, loadRegToReg},
    {1, 4, loadRegToReg},
    {1, 8, loadAddrHLToReg},
    {1, 4, loadRegToReg},

    {1, 4, loadRegToReg},
    {1, 4, loadRegToReg},
    {1, 4, loadRegToReg},
    {1, 4, loadRegToReg},
    {1, 4, loadRegToReg},
    {1, 4, loadRegToReg},
    {1, 8, loadAddrHLToReg},
    {1, 4, loadRegToReg},

    // 60
    {1, 4, loadRegToReg},
    {1, 4, loadRegToReg},
    {1, 4, loadRegToReg},
    {1, 4, loadRegToReg},
    {1, 4, loadRegToReg},
    {1, 4, loadRegToReg},
    {1, 8, loadAddrHLToReg},
    {1, 4, loadRegToReg},

    {1, 4, loadRegToReg},
    {1, 4, loadRegToReg},
    {1, 4, loadRegToReg},
    {1, 4, loadRegToReg},
    {1, 4, loadRegToReg},
    {1, 4, loadRegToReg},
    {1, 8, loadAddrHLToReg},
    {1, 4, loadRegToReg},

    // 70
    {1, 4, loadRegToReg},
    {1, 4, loadRegToReg},
    {1, 4, loadRegToReg},
    {1, 4, loadRegToReg},
    {1, 4, loadRegToReg},
    {1, 4, loadRegToReg},
    {1, 4, halt},
    {1, 4, loadRegToReg},

    {1, 4, loadRegToReg},
    {1, 4, loadRegToReg},
    {1, 4, loadRegToReg},
    {1, 4, loadRegToReg},
    {1, 4, loadRegToReg},
    {1, 4, loadRegToReg},
    {1, 8, loadAddrHLToReg},
    {1, 4, loadRegToReg},

    // 80
    {1, 4, addReg},
    {1, 4, addReg},
    {1, 4, addReg},
    {1, 4, addReg},
    {1, 4, addReg},
    {1, 4, addReg},
    {1, 8, addAddrHL},
    {1, 4, addReg},

    {1, 4, adcReg},
    {1, 4, adcReg},
    {1, 4, adcReg},
    {1, 4, adcReg},
    {1, 4, adcReg},
    {1, 4, adcReg},
    {1, 8, adcAddrHL},
    {1, 4, adcReg},

    // 90
    {1, 4, subReg},
    {1, 4, subReg},
    {1, 4, subReg},
    {1, 4, subReg},
    {1, 4, subReg},
    {1, 4, subReg},
    {1, 8, subAddrHL},
    {1, 4, subReg},

    {1, 4, sbcReg},
    {1, 4, sbcReg},
    {1, 4, sbcReg},
    {1, 4, sbcReg},
    {1, 4, sbcReg},
    {1, 4, sbcReg},
    {1, 8, sbcAddrHL},
    {1, 4, sbcReg},

    // A0
    {1, 4, andReg},
    {1, 4, andReg},
    {1, 4, andReg},
    {1, 4, andReg},
    {1, 4, andReg},
    {1, 4, andReg},
    {1, 8, andAddrHL},
    {1, 4, andReg},

    {1, 4, xorReg},
    {1, 4, xorReg},
    {1, 4, xorReg},
    {1, 4, xorReg},
    {1, 4, xorReg},
    {1, 4, xorReg},
    {1, 8, xorAddrHL},
    {1, 4, xorReg},

    // B0
    {1, 4, orReg},
    {1, 4, orReg},
    {1, 4, orReg},
    {1, 4, orReg},
    {1, 4, orReg},
    {1, 4, orReg},
    {1, 8, orAddrHL},
    {1, 4, orReg},

    {1, 4, compareReg},
    {1, 4, compareReg},
    {1, 4, compareReg},
    {1, 4, compareReg},
    {1, 4, compareReg},
    {1, 4, compareReg},
    {1, 8, compareAddrHL},
    {1, 4, compareReg},

    // C0
    {1, VARIABLE_CYCLES, conditionalRet},
    {1, 12, pop},
    {3, VARIABLE_CYCLES, conditionalJumpImm16},
    {3, 16, jumpImm16},
    {3, VARIABLE_CYCLES, conditionalCallImm16},
    {1, 16, push},
    {2, 8, addImm8},
    {1, 16, reset},
    
    {1, VARIABLE_CYCLES, conditionalRet},
    {1, 16, ret},
    {3, VARIABLE_CYCLES, conditionalJumpImm16},
    {1, 4, prefixCB},
    {3, VARIABLE_CYCLES, conditionalCallImm16},
    {3, 24, callImm16},
    {2, 8, adcImm8},
    {1, 16, reset},

    // D0
    {1, VARIABLE_CYCLES, conditionalRet},
    {1, 12, pop},
    {3, VARIABLE_CYCLES, conditionalJumpImm16},
    {},
    {3, VARIABLE_CYCLES, conditionalCallImm16},
    {1, 16, push},
    {2, 8, subImm8},
    {1, 16, reset},
    
    {1, VARIABLE_CYCLES, conditionalRet},
    {1, 16, retAndEnableInterrupts},
    {3, VARIABLE_CYCLES, conditionalJumpImm16},
    {},
    {3, VARIABLE_CYCLES, conditionalCallImm16},
    {},
    {2, 8, sbcImm8},
    {1, 16, reset},

    // E0
    {2, 12, loadAToIOPortImm8},
    {1, 12, pop},
    {2, 8, loadAToIOPortC},
    {},
    {},
    {1, 16, push},
    {2, 8, andImm8},
    {1, 16, reset},

    {2, 16, addSignedToSP},
    {1, 4, jumpHL},
    {3, 16, loadAToAddr16},
    {},
    {},
    {},
    {2, 8, xorImm8},
    {1, 16, reset},

    // F0
    {2, 12, loadIOPortImm8ToA},
    {1, 12, pop},
    {2, 8, loadIOPortCToA},
    {},
    {},
    {1, 16, push},
    {2, 8, andImm8},
    {1, 16, reset},

    {2, 16, addSignedToSP},
    {1, 4, jumpHL},
    {3, 16, loadAToAddr16},
    {},
    {},
    {},
    {2, 8, xorImm8},
    {1, 16, reset},
};

void executeInstruction(GameBoy* gb) {
    uint8* instr = &gb->memory[REG(PC)];
    InstructionHandler* handler = &instructionHandlers[instr[0]];

    if (!handler->execute) {
        fprintf(stderr, "Invalid opcode %x\n", instr[0]);
        exit(1);
    }

    REG(PC) += handler->length;
    handler->execute(gb, instr);
}
