#define INSTRUCTION_DISASSEMBLE_FN_NOSTATIC(name) \
    void name(FILE* out, uint8* instr)
#define INSTRUCTION_DISASSEMBLE_FN(name) static INSTRUCTION_DISASSEMBLE_FN_NOSTATIC(name##_disassemble)

typedef INSTRUCTION_DISASSEMBLE_FN_NOSTATIC(InstructionDisassembleFn);

static const char* reg8Name(enum Register8 reg) {
    switch (reg) {
    case REG_B:
        return "B";
    case REG_C:
        return "C";
    case REG_D:
        return "D";
    case REG_E:
        return "E";
    case REG_H:
        return "H";
    case REG_L:
        return "L";
    case REG_F:
        return "F";
    case REG_A:
        return "A";
    default:
        return NULL;
    }
}

static const char* reg16Name(enum Register16 reg) {
    switch (reg) {
    case REG_AF:
        return "AF";
    case REG_BC:
        return "BC";
    case REG_DE:
        return "DE";
    case REG_HL:
        return "HL";
    case REG_SP:
        return "SP";
    case REG_PC:
        return "PC";
    default:
        return NULL;
    }
}

static const char* conditionalName(enum Conditional cond) {
    switch (cond) {
    case COND_NZ:
        return "NZ";
    case COND_Z:
        return "Z";
    case COND_NC:
        return "NC";
    case COND_C:
        return "C";
    default:
        return NULL;
    }
}

INSTRUCTION_DISASSEMBLE_FN(loadRegToReg) {
    enum Register8 src = instr[0] & 0x7;
    enum Register8 dst = (instr[0] >> 3) & 0x7;

    fprintf(out, "ld %s, %s", reg8Name(dst), reg8Name(src));
}

INSTRUCTION_DISASSEMBLE_FN(loadImm8ToReg) {
    enum Register8 reg = (instr[0] - 6) >> 3;

    fprintf(out, "ld %s, %02xh", reg8Name(reg), instr[1]);
}

INSTRUCTION_DISASSEMBLE_FN(loadAddrHLToReg) {
    enum Register8 dst = (instr[0] >> 3) & 0x7;
    
    fprintf(out, "ld %s, (HL)", reg8Name(dst));
}

INSTRUCTION_DISASSEMBLE_FN(loadRegToAddrHL) {
    enum Register8 src = instr[0] & 0x7;

    fprintf(out, "ld (HL), %s", reg8Name(src));
}

INSTRUCTION_DISASSEMBLE_FN(loadImm8ToAddrHL) {
    fprintf(out, "ld (HL), %02xh", instr[1]);
}

INSTRUCTION_DISASSEMBLE_FN(loadAddrBCToA) {
    fprintf(out, "ld A, (BC)");
}

INSTRUCTION_DISASSEMBLE_FN(loadAddrDEToA) {
    fprintf(out, "ld A, (DE)");
}

INSTRUCTION_DISASSEMBLE_FN(loadAToAddrBC) {
    fprintf(out, "ld (BC), A");
}

INSTRUCTION_DISASSEMBLE_FN(loadAToAddrDE) {
    fprintf(out, "ld (DE), A");
}

INSTRUCTION_DISASSEMBLE_FN(loadAToAddr16) {
    fprintf(out, "ld (%04xh), A", get16BitArgument(instr));
}

INSTRUCTION_DISASSEMBLE_FN(loadAddr16ToA) {
    fprintf(out, "ld A, (%04xh)", get16BitArgument(instr));
}

INSTRUCTION_DISASSEMBLE_FN(loadIOPortImm8ToA) {
    fprintf(out, "ldh A, (%02xh)", instr[1]);
}

INSTRUCTION_DISASSEMBLE_FN(loadAToIOPortImm8) {
    fprintf(out, "ldh (%02xh), A", instr[1]);
}

INSTRUCTION_DISASSEMBLE_FN(loadIOPortCToA) {
    fprintf(out, "ldh A, (C)");
}

INSTRUCTION_DISASSEMBLE_FN(loadAToIOPortC) {
    fprintf(out, "ldh (C), A");
}

INSTRUCTION_DISASSEMBLE_FN(loadAndIncrementAToAddrHL) {
    fprintf(out, "ldi (HL), A");
}

INSTRUCTION_DISASSEMBLE_FN(loadAndIncrementAddrHLToA) {
    fprintf(out, "ldi A, (HL)");
}

INSTRUCTION_DISASSEMBLE_FN(loadAndDecrementAToAddrHL) {
    fprintf(out, "ldd (HL), A");
}

INSTRUCTION_DISASSEMBLE_FN(loadAndDecrementAddrHLToA) {
    fprintf(out, "ldd A, (HL)");
}

INSTRUCTION_DISASSEMBLE_FN(loadImm16ToReg) {
    enum Register16 dst = BC_DE_HL_SP[(instr[0] - 1) >> 4];

    fprintf(out, "ld %s, %04xh", reg16Name(dst), get16BitArgument(instr));
}

INSTRUCTION_DISASSEMBLE_FN(loadSPToAddr16) {
    fprintf(out, "ld (%04xh), SP", get16BitArgument(instr));
}

INSTRUCTION_DISASSEMBLE_FN(loadHLToSP) {
    fprintf(out, "ld SP, HL");
}

INSTRUCTION_DISASSEMBLE_FN(push) {
    enum Register16 src = BC_DE_HL_AF[(instr[0] - 0xC5) >> 4];
    fprintf(out, "push %s", reg8Name(src));
}

INSTRUCTION_DISASSEMBLE_FN(pop) {
    enum Register16 dst = BC_DE_HL_AF[(instr[0] - 0xC1) >> 4];
    fprintf(out, "pop %s", reg8Name(dst));
}

INSTRUCTION_DISASSEMBLE_FN(addReg) {
    enum Register8 rhs = instr[0] - 0x80;

    fprintf(out, "add A, %s", reg8Name(rhs));
}

INSTRUCTION_DISASSEMBLE_FN(addImm8) {
    fprintf(out, "add A, %02xh", instr[1]);
}

INSTRUCTION_DISASSEMBLE_FN(addAddrHL) {
    fprintf(out, "add A, (HL)");
}

INSTRUCTION_DISASSEMBLE_FN(adcReg) {
    enum Register8 rhs = instr[0] - 0x88;

    fprintf(out, "adc A, %s", reg8Name(rhs));
}

INSTRUCTION_DISASSEMBLE_FN(adcImm8) {
    fprintf(out, "adc A, %02xh", instr[1]);
}

INSTRUCTION_DISASSEMBLE_FN(adcAddrHL) {
    fprintf(out, "adc A, (HL)");
}

INSTRUCTION_DISASSEMBLE_FN(subReg) {
    enum Register8 rhs = instr[0] - 0x90;

    fprintf(out, "sub A, %s", reg8Name(rhs));
}

INSTRUCTION_DISASSEMBLE_FN(subImm8) {
    fprintf(out, "sub A, %02xh", instr[1]);
}

INSTRUCTION_DISASSEMBLE_FN(subAddrHL) {
    fprintf(out, "sub A, (HL)");
}

INSTRUCTION_DISASSEMBLE_FN(sbcReg) {
    enum Register8 rhs = instr[0] - 0x98;

    fprintf(out, "sbc A, %s", reg8Name(rhs));
}

INSTRUCTION_DISASSEMBLE_FN(sbcImm8) {
    fprintf(out, "sbc A, %02xh", instr[1]);
}

INSTRUCTION_DISASSEMBLE_FN(sbcAddrHL) {
    fprintf(out, "sbc A, (HL)");
}    

INSTRUCTION_DISASSEMBLE_FN(andReg) {
    enum Register8 rhs = instr[0] - 0xA0;

    fprintf(out, "and A, %s", reg8Name(rhs));
}

INSTRUCTION_DISASSEMBLE_FN(andImm8) {
    fprintf(out, "and A, %02xh", instr[1]);
}

INSTRUCTION_DISASSEMBLE_FN(andAddrHL) {
    fprintf(out, "and A, (HL)");
}

INSTRUCTION_DISASSEMBLE_FN(xorReg) {
    enum Register8 rhs = instr[0] - 0xA8;

    fprintf(out, "xor A, %s", reg8Name(rhs));
}

INSTRUCTION_DISASSEMBLE_FN(xorImm8) {
    fprintf(out, "xor A, %02xh", instr[1]);
}

INSTRUCTION_DISASSEMBLE_FN(xorAddrHL) {
    fprintf(out, "xor A, (HL)");
}

INSTRUCTION_DISASSEMBLE_FN(orReg) {
    enum Register8 rhs = instr[0] - 0xB0;

    fprintf(out, "or A, %s", reg8Name(rhs));
}

INSTRUCTION_DISASSEMBLE_FN(orImm8) {
    fprintf(out, "or A, %02xh", instr[1]);
}

INSTRUCTION_DISASSEMBLE_FN(orAddrHL) {
    fprintf(out, "or A, (HL)");
}

INSTRUCTION_DISASSEMBLE_FN(compareReg) {
    enum Register8 rhs = instr[0] - 0xB8;

    fprintf(out, "cp A, %s", reg8Name(rhs));
}

INSTRUCTION_DISASSEMBLE_FN(compareImm8) {
    fprintf(out, "cp A, %02xh", instr[1]);
}

INSTRUCTION_DISASSEMBLE_FN(compareAddrHL) {
    fprintf(out, "cp A, (HL)");
}

INSTRUCTION_DISASSEMBLE_FN(incReg) {
    enum Register8 reg = (instr[0] - 0x04) >> 3;
    fprintf(out, "inc %s", reg8Name(reg));
}

INSTRUCTION_DISASSEMBLE_FN(incAddrHL) {
    fprintf(out, "inc (HL)");
}

INSTRUCTION_DISASSEMBLE_FN(decReg) {
    enum Register8 reg = (instr[0] - 0x05) >> 3;
    fprintf(out, "dec %s", reg8Name(reg));
}

INSTRUCTION_DISASSEMBLE_FN(decAddrHL) {
    fprintf(out, "dec (HL)");
}

// Source : https://forums.nesdev.org/viewtopic.php?t=15944
INSTRUCTION_DISASSEMBLE_FN(decimalAdjust) {
    fprintf(out, "daa");
}

INSTRUCTION_DISASSEMBLE_FN(complement) {
    fprintf(out, "cpl");
}

INSTRUCTION_DISASSEMBLE_FN(addReg16ToHL) {
    enum Register16 reg = BC_DE_HL_SP[(instr[0] - 0x09) >> 4];
    fprintf(out, "add HL, %s", reg16Name(reg));
}

INSTRUCTION_DISASSEMBLE_FN(incReg16) {
    enum Register16 reg = BC_DE_HL_SP[(instr[0] - 0x03) >> 4];
    fprintf(out, "inc %s", reg16Name(reg));
}

INSTRUCTION_DISASSEMBLE_FN(decReg16) {
    enum Register16 reg = BC_DE_HL_SP[(instr[0] - 0x08) >> 4];
    fprintf(out, "dec %s", reg16Name(reg));
}

INSTRUCTION_DISASSEMBLE_FN(addSignedToSP) {
    int16 rhs = getSigned8BitArgument(instr);
    fprintf(out, "add SP, %02xh", rhs);
}

INSTRUCTION_DISASSEMBLE_FN(loadSignedPlusSPToHL) {
    int16 rhs = getSigned8BitArgument(instr);
    fprintf(out, "ld HL, SP + %02xh", rhs);
}

INSTRUCTION_DISASSEMBLE_FN(rotateALeft) {
    fprintf(out, "rlca");
}

INSTRUCTION_DISASSEMBLE_FN(rotateALeftThroughCarry) {
    fprintf(out, "rla");
}

INSTRUCTION_DISASSEMBLE_FN(rotateARight) {
    fprintf(out, "rrca");
}

INSTRUCTION_DISASSEMBLE_FN(rotateARightThroughCarry) {
    fprintf(out, "rra");
}

INSTRUCTION_DISASSEMBLE_FN(rotateRegLeft) {
    enum Register8 reg = instr[0];
    fprintf(out, "rlc %s", reg8Name(reg));
}

INSTRUCTION_DISASSEMBLE_FN(rotateAddrHLLeft) {
    fprintf(out, "rlc (HL)");
}

INSTRUCTION_DISASSEMBLE_FN(rotateRegLeftThroughCarry) {
    enum Register8 reg = instr[0] - 0x10;

    fprintf(out, "rl %s", reg8Name(reg));
}

INSTRUCTION_DISASSEMBLE_FN(rotateAddrHLLeftThroughCarry) {
    fprintf(out, "rl (HL)");
}

INSTRUCTION_DISASSEMBLE_FN(rotateRegRight) {
    enum Register8 reg = instr[0] - 0x08;
    fprintf(out, "rrc %s", reg8Name(reg));
}

INSTRUCTION_DISASSEMBLE_FN(rotateAddrHLRight) {
    fprintf(out, "rrc (HL)");
}

INSTRUCTION_DISASSEMBLE_FN(rotateRegRightThroughCarry) {
    enum Register8 reg = instr[0] - 0x18;
    fprintf(out, "rr %s", reg8Name(reg));
}

INSTRUCTION_DISASSEMBLE_FN(rotateAddrHLRightThroughCarry) {
    fprintf(out, "rr (HL)");
}

INSTRUCTION_DISASSEMBLE_FN(shiftRegLeftArithmetic) {
    enum Register8 reg = instr[0] - 0x20;
    fprintf(out, "sla %s", reg8Name(reg));
}

INSTRUCTION_DISASSEMBLE_FN(shiftAddrHLLeftArithmetic) {
    fprintf(out, "sla (HL)");
}

INSTRUCTION_DISASSEMBLE_FN(shiftRegRightArithmetic) {
    enum Register8 reg = instr[0] - 0x28;
    fprintf(out, "sra %s", reg8Name(reg));
}

INSTRUCTION_DISASSEMBLE_FN(shiftAddrHLRightArithmetic) {
    fprintf(out, "sra (HL)");
}

INSTRUCTION_DISASSEMBLE_FN(swapNibblesReg) {
    enum Register8 reg = instr[0] - 0x30;
    fprintf(out, "swap %s", reg8Name(reg));
}

INSTRUCTION_DISASSEMBLE_FN(swapNibblesAddrHL) {
    fprintf(out, "swap (HL)");
}

INSTRUCTION_DISASSEMBLE_FN(shiftRegRightLogical) {
    enum Register8 reg = instr[0] - 0x38;
    fprintf(out, "srl %s", reg8Name(reg));
}

INSTRUCTION_DISASSEMBLE_FN(shiftAddrHLRightLogical) {
    fprintf(out, "srl (HL)");
}

INSTRUCTION_DISASSEMBLE_FN(testBitReg) {
    enum Register8 reg = instr[0] % 8;
    uint8 bitIndex = (instr[0] - 0x40) >> 3;
    fprintf(out, "bit %u, %s", bitIndex, reg8Name(reg));
}

INSTRUCTION_DISASSEMBLE_FN(testBitAddrHL) {
    uint8 bitIndex = (instr[0] - 0x40) >> 3;
    fprintf(out, "bit %u, (HL)", bitIndex);
}

INSTRUCTION_DISASSEMBLE_FN(setBitReg) {
    enum Register8 reg = instr[0] % 8;
    uint8 bitIndex = (instr[0] - 0xC0) >> 3;
    fprintf(out, "set %u, %s", bitIndex, reg8Name(reg));
}

INSTRUCTION_DISASSEMBLE_FN(setBitAddrHL) {
    uint8 bitIndex = (instr[0] - 0xC0) >> 3;
    fprintf(out, "set %u, (HL)", bitIndex);
}

INSTRUCTION_DISASSEMBLE_FN(resetBitReg) {
    enum Register8 reg = instr[0] % 8;
    uint8 bitIndex = (instr[0] - 0x80) >> 3;
    fprintf(out, "reset %u, %s", bitIndex, reg8Name(reg));
}

INSTRUCTION_DISASSEMBLE_FN(resetBitAddrHL) {
    uint8 bitIndex = (instr[0] - 0x80) >> 3;
    fprintf(out, "reset %u, (HL)", bitIndex);
}

INSTRUCTION_DISASSEMBLE_FN(flipCarry) {
    fprintf(out, "ccf");
}

INSTRUCTION_DISASSEMBLE_FN(setCarry) {
    fprintf(out, "scf");
}

INSTRUCTION_DISASSEMBLE_FN(nop) {
    fprintf(out, "nop");
}

INSTRUCTION_DISASSEMBLE_FN(halt) {
    fprintf(out, "halt");
}

INSTRUCTION_DISASSEMBLE_FN(stop) {
    fprintf(out, "stop");
}

INSTRUCTION_DISASSEMBLE_FN(disableInterrupts) {
    fprintf(out, "di");
}

INSTRUCTION_DISASSEMBLE_FN(enableInterrupts) {
    fprintf(out, "ei");
}

INSTRUCTION_DISASSEMBLE_FN(jumpImm16) {
    fprintf(out, "jp %04xh", get16BitArgument(instr));
}

INSTRUCTION_DISASSEMBLE_FN(jumpHL) {
    fprintf(out, "jp HL");
}

INSTRUCTION_DISASSEMBLE_FN(conditionalJumpImm16) {
    enum Conditional cond = (instr[0] - 0xC2) >> 3;
    fprintf(out, "jp %s, %04xh", conditionalName(cond), get16BitArgument(instr));
}

INSTRUCTION_DISASSEMBLE_FN(relativeJump) {
    fprintf(out, "jr %02xh", getSigned8BitArgument(instr));
}

INSTRUCTION_DISASSEMBLE_FN(conditionalRelativeJump) {
    enum Conditional cond = (instr[0] - 0x20) >> 3;

    fprintf(out, "jp %s, %02xh", conditionalName(cond), getSigned8BitArgument(instr));
}

INSTRUCTION_DISASSEMBLE_FN(callImm16) {
    fprintf(out, "call %04xh", get16BitArgument(instr));
}

INSTRUCTION_DISASSEMBLE_FN(conditionalCallImm16) {
    enum Conditional cond = (instr[0] - 0xC4) >> 3;
    fprintf(out, "call %s, %04xh", conditionalName(cond), get16BitArgument(instr));
}

INSTRUCTION_DISASSEMBLE_FN(ret) {
    fprintf(out, "ret");
}

INSTRUCTION_DISASSEMBLE_FN(conditionalRet) {
    enum Conditional cond = (instr[0] - 0xC0) >> 3;
    fprintf(out, "ret %s", conditionalName(cond));
}

INSTRUCTION_DISASSEMBLE_FN(retAndEnableInterrupts) {
    fprintf(out, "reti");
}

INSTRUCTION_DISASSEMBLE_FN(reset) {
    fprintf(out, "reset");
}

INSTRUCTION_DISASSEMBLE_FN(prefixCB) {
    instr++;

    bool32 isAddrHL = (instr[0] % 8) == 6;

    InstructionDisassembleFn* dispatchTable[8];

    if (isAddrHL) {
        dispatchTable[0] = rotateAddrHLLeft_disassemble;
        dispatchTable[1] = rotateAddrHLRight_disassemble;
        dispatchTable[2] = rotateAddrHLLeftThroughCarry_disassemble;
        dispatchTable[3] = rotateAddrHLRightThroughCarry_disassemble;
        dispatchTable[4] = shiftAddrHLLeftArithmetic_disassemble;
        dispatchTable[5] = shiftAddrHLRightArithmetic_disassemble;
        dispatchTable[6] = swapNibblesAddrHL_disassemble;
        dispatchTable[7] = shiftAddrHLRightLogical_disassemble;
    } else {
        dispatchTable[0] = rotateRegLeft_disassemble;
        dispatchTable[1] = rotateRegRight_disassemble;
        dispatchTable[2] = rotateRegLeftThroughCarry_disassemble;
        dispatchTable[3] = rotateRegRightThroughCarry_disassemble;
        dispatchTable[4] = shiftRegLeftArithmetic_disassemble;
        dispatchTable[5] = shiftRegRightArithmetic_disassemble;
        dispatchTable[6] = swapNibblesReg_disassemble;
        dispatchTable[7] = shiftRegRightLogical_disassemble;
    }
    
    if (instr[0] < 0x40) {
        uint8 instrIndex = instr[0] >> 3;
        dispatchTable[instrIndex](out, instr);
    } else if (instr[0] < 0x80) { // test bit
        if (isAddrHL) {
            testBitAddrHL_disassemble(out, instr);
        } else {
            testBitReg_disassemble(out, instr);
        }
    } else if (instr[0] < 0xC0) { // reset bit
        if (isAddrHL) {
            resetBitAddrHL_disassemble(out, instr);
        } else {
            resetBitReg_disassemble(out, instr);
        }
    } else { // set bit
        if (isAddrHL) {
            setBitAddrHL_disassemble(out, instr);
        } else {
            setBitReg_disassemble(out, instr);
        }
    }
}

