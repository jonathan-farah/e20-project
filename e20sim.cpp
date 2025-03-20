/*
CS-UY 2214
Adapted from Jeff Epstein
Starter code for E20 simulator
sim.cpp
*/
#include <cstddef>
#include <cstdint>
#include <iostream>
#include <string>
#include <vector>
#include <fstream>
#include <iomanip>
#include <regex>
#include <cstdlib>

using namespace std;

// Some helpful constant values
size_t const static NUM_REGS = 8;
size_t const static MEM_SIZE = 1 << 13;
size_t const static REG_SIZE = 1 << 16;

uint16_t memory[MEM_SIZE] = {0};
uint16_t registers[NUM_REGS] = {0};
uint16_t pc = 0;
/*
    Loads an E20 machine code file into memory.
*/
void load_machine_code(ifstream &f, uint16_t mem[]) {
    regex machine_code_re("^ram\\[(\\d+)\\] = 16'b(\\d+);.*$");
    size_t expectedaddr = 0;
    string line;

    while (getline(f, line)) {
        smatch sm;
        if (!regex_match(line, sm, machine_code_re)) {
            cerr << "Can't parse line: " << line << endl;
            exit(1);
        }
        size_t addr = stoi(sm[1], nullptr, 10);
        unsigned instr = stoi(sm[2], nullptr, 2);

        if (addr != expectedaddr) {
            cerr << "Memory addresses encountered out of sequence: " << addr << endl;
            exit(1);
        }
        if (addr >= MEM_SIZE) {
            cerr << "Program too big for memory" << endl;
            exit(1);
        }

        expectedaddr++;
        mem[addr] = instr;
    }
}

/*
    Prints the current state of the simulator.
*/
void print_state(uint16_t pc, uint16_t regs[], uint16_t memory[], size_t memquantity) {
    cout << setfill(' ');
    cout << "Final state:" << endl;
    cout << "\tpc=" << setw(5) << pc << endl;

    for (size_t reg = 0; reg < NUM_REGS; reg++)
        cout << "\t$" << reg << "=" << setw(5) << regs[reg] << endl;

    cout << setfill('0');
    bool cr = false;
    for (size_t count = 0; count < memquantity; count++) {
        cout << hex << setw(4) << memory[count] << " ";
        cr = true;
        if (count % 8 == 7) {
            cout << endl;
            cr = false;
        }
    }
    if (cr)
        cout << endl;
}

// Extract bits from an instruction
uint16_t extract_bits(unsigned instruction, int inner, int outer) {
    unsigned val = (1 << (outer - inner + 1)) - 1;
    return (instruction >> inner) & val;
}

// Sign extend 7-bit immediate
uint16_t sign_extend(unsigned value) {
    if (value & 0b1000000) {
        value |= 0b1111111110000000;  // Sign-extend to 16 bits
    }
    return value & 0xFFFF;
}
//helper function that executes one instruction
unsigned execute_instruction(uint16_t pc, uint16_t memory[], uint16_t registers[]) {
    uint16_t instruction = memory[pc % MEM_SIZE];
    //3 bit opcode
    uint16_t opcode = extract_bits(instruction, 13, 15);
    uint16_t next_pc = pc + 1;

    uint16_t ander = 0xFFFF;

    uint16_t rg1, rg2, regDst, imm, imm4;
    unsigned addr;

    switch (opcode) {
        case 0b000:  // ADD, SUB, OR, AND, SLT, JR
            rg1 = extract_bits(instruction, 10, 12);
            rg2 = extract_bits(instruction, 7, 9);
            regDst = extract_bits(instruction, 4, 6);
            imm4 = extract_bits(instruction, 0, 3);

            if (regDst != 0) {
                switch (imm4) {
                    //add
                    case 0b0000: registers[regDst] = (registers[rg1] + registers[rg2]) & ander; break;
                    //minus
                    case 0b0001: registers[regDst] = (registers[rg1] - registers[rg2]) & ander; break;
                    //or
                    case 0b0010: registers[regDst] = (registers[rg1] | registers[rg2]) & ander; break;
                    //and
                    case 0b0011: registers[regDst] = (registers[rg1] & registers[rg2]) & ander; break;
                    //jr
                    case 0b0100: registers[regDst] = (registers[rg1] < registers[rg2]) ? 1 : 0; break;
                }
            }
            if (imm4 == 0b1000) {  // JR (Jump Register)
                next_pc = registers[rg1] & 0x1FFF;
            }
            break;

        case 0b001:  // ADDI (Add Immediate)
            rg1 = extract_bits(instruction, 10, 12);
            regDst = extract_bits(instruction, 7, 9);
            //important so we can add negatives
            imm = sign_extend(extract_bits(instruction, 0, 6));
            //no writing to reg0
            if (regDst != 0) {
                registers[regDst] = (registers[rg1] + imm) & 0xFFFF;
            }
            break;

        case 0b010:  // J (Jump)
            imm = extract_bits(instruction, 0, 12);
            next_pc = imm;
            break;

        case 0b011:  // JAL (Jump and Link)
            imm = extract_bits(instruction, 0, 12);
            registers[7] = pc + 1;  // Store return address
            next_pc = imm;
            break;

        case 0b100:  // LW (Load Word)
            rg1 = extract_bits(instruction, 10, 12);
            regDst = extract_bits(instruction, 7, 9);
            imm = sign_extend(extract_bits(instruction, 0, 6));
            //memory overload
            addr = (registers[rg1] + imm) & (MEM_SIZE - 1);
            if (regDst != 0) {
                registers[regDst] = memory[addr];
            }
            break;

        case 0b101:  // SW (Store Word)
            rg1 = extract_bits(instruction, 10, 12);
            rg2 = extract_bits(instruction, 7, 9);
            imm = sign_extend(extract_bits(instruction, 0, 6));
            addr = (registers[rg1] + imm) & (MEM_SIZE - 1);
            //write to memory
            memory[addr] = registers[rg2];
            break;

        case 0b110:  // JEQ (Jump if Equal)
            rg1 = extract_bits(instruction, 10, 12);
            rg2 = extract_bits(instruction, 7, 9);
            imm = sign_extend(extract_bits(instruction, 0, 6));
            //geck if reg values are equal
            if (registers[rg1] == registers[rg2]) {
                next_pc = pc + 1 + imm;
            }
            break;

        case 0b111:  // SLTI (Set Less Than Immediate)
            rg1 = extract_bits(instruction, 10, 12);
            regDst = extract_bits(instruction, 7, 9);
            imm = sign_extend(extract_bits(instruction, 0, 6));
            if (regDst != 0) {
                registers[regDst] = (registers[rg1] < imm) ? 1 : 0;
            }
            break;

        default:
            cerr << "Unknown opcode " << opcode << " at pc=" << pc << endl;
            exit(EXIT_FAILURE);
    }

    registers[0] = 0;  // Ensure register 0 is immutable
    return next_pc;
}

/**
 * Simulates the execution of E20 machine code.
 */
void simulate() {
    bool halted = false;

    //run the code while halt isnt seen
    while (!halted) {
        unsigned next_pc = execute_instruction(pc, memory, registers);

        if (next_pc % MEM_SIZE == pc) {
            halted = true;
        }

        pc = next_pc;
    }
}
int main(int argc, char *argv[]) {
    // Parse command-line arguments
    char *filename = nullptr;
    bool do_help = false;
    bool arg_error = false;

    for (int i = 1; i < argc; i++) {
        string arg(argv[i]);
        if (arg.rfind("-", 0) == 0) {
            if (arg == "-h" || arg == "--help")
                do_help = true;
            else
                arg_error = true;
        } else {
            if (filename == nullptr)
                filename = argv[i];
            else
                arg_error = true;
        }
    }

    if (arg_error || do_help || filename == nullptr) {
        cerr << "usage " << argv[0] << " [-h] filename" << endl;
        cerr << "Simulate E20 machine" << endl;
        return 1;
    }

    ifstream f(filename);
    if (!f.is_open()) {
        cerr << "Can't open file " << filename << endl;
        return 1;
    }

    // Load machine code into memory
    load_machine_code(f, memory);
    simulate();
    // Print final state
    print_state(pc, registers, memory, 128);
    return 0;
}
