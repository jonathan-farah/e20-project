/*
    File: simcache.cpp
    Author: Jonathan Farah
    Purpose: Simulate E20 and different cache configurations
*/
#include <cstddef>
#include <iostream>
#include <string>
#include <vector>
#include <fstream>
#include <limits>
#include <iomanip>
#include <regex>

using namespace std;

// Some helpful constant values that we'll be using.
size_t const static NUM_REGS = 8;
size_t const static MEM_SIZE = 1 << 13;
size_t const static REG_SIZE = 1 << 16;

uint16_t memory[MEM_SIZE] = {0};
uint16_t registers[NUM_REGS] = {0};
uint16_t pc = 0;
// Struct for representing a cache cell
struct cache_cell
{
    bool valid;
    int tag;
    int last_access; //  used to determine LRU, tracks how long a cell has NOT been used, the higher the less recent
};

/*
    Loads an E20 machine code file into the list
    provided by mem. We assume that mem is
    large enough to hold the values in the machine
    code file.

    @param f Open file to read from
    @param mem Array represetnting memory into which to read program
*/
void load_machine_code(ifstream &f, uint16_t mem[])
{
    regex machine_code_re("^ram\\[(\\d+)\\] = 16'b(\\d+);.*$");
    size_t expectedaddr = 0;
    string line;
    while (getline(f, line))
    {
        smatch sm;
        if (!regex_match(line, sm, machine_code_re))
        {
            cerr << "Can't parse line: " << line << endl;
            exit(1);
        }
        size_t addr = stoi(sm[1], nullptr, 10);
        unsigned instr = stoi(sm[2], nullptr, 2);
        if (addr != expectedaddr)
        {
            cerr << "Memory addresses encountered out of sequence: " << addr << endl;
            exit(1);
        }
        if (addr >= MEM_SIZE)
        {
            cerr << "Program too big for memory" << endl;
            exit(1);
        }
        expectedaddr++;
        mem[addr] = instr;
    }
}
/*
    Prints out the correctly-formatted configuration of a cache.

    @param cache_name The name of the cache. "L1" or "L2"

    @param size The total size of the cache, measured in memory cells.
        Excludes metadata

    @param assoc The associativity of the cache. One of [1,2,4,8,16]

    @param blocksize The blocksize of the cache. One of [1,2,4,8,16,32,64])

    @param num_rows The number of rows in the given cache.
*/
void print_cache_config(const string &cache_name, int size, int assoc, int blocksize, int num_rows)
{
    cout << "Cache " << cache_name << " has size " << size << ", associativity " << assoc << ", blocksize " << blocksize << ", rows " << num_rows << endl;
}

/*
    Prints out a correctly-formatted log entry.

    @param cache_name The name of the cache where the event
        occurred. "L1" or "L2"

    @param status The kind of cache event. "SW", "HIT", or
        "MISS"

    @param pc The program counter of the memory
        access instruction

    @param addr The memory address being accessed.

    @param row The cache row or set number where the data
        is stored.
*/
void print_log_entry(const string &cache_name, const string &status, int pc, int addr, int row)
{
    cout << left << setw(8) << cache_name + " " + status << right << " pc:" << setw(5) << pc << "\taddr:" << setw(5) << addr << "\trow:" << setw(4) << row << endl;
}

/*
    Simulates e20 with L1 cache
*/
void simulate_one_cache(uint16_t memory[], uint16_t regs[], uint16_t pc, vector<vector<cache_cell>> cache, int l1_size, int l1_assoc, int l1_blocksize, int l1_rows)
{
    // Do simulation
    bool halt = false; // Flag for when encounter halt instruction
    while (!halt)      // Keep looping through instructions until halt
    {
        // Get e20 instruction fields
        uint16_t opcode = memory[pc % 8192] >> 13;
        uint16_t regA = memory[pc % 8192] >> 10 & 0b111;
        uint16_t regB = memory[pc % 8192] >> 7 & 0b111;
        uint16_t regC = memory[pc % 8192] >> 4 & 0b111;
        uint16_t lsb = memory[pc % 8192] & 0b1111; // Four least significant bits
        uint16_t imm13 = memory[pc % 8192] & 0b1111111111111;

        signed imm7 = memory[pc % 8192] & 0b1111111;
        uint16_t imm_slti = imm7;                // Specifically for slti
        if (memory[pc % 8192] >> 6 & 0b1 == 0b1) // imm is negative
        {
            imm7 = ((imm7 ^ 0b1111111) + 1) * -1; // Convert imm7 to two's complement
            // Convert imm_slti to unsigned 16 bit by sign extending the 7 bit two's complement
            imm_slti = ((imm_slti ^ 0b1111111) + 1) | 0b1111111110000000;
        }

        // Useful cache variables
        int mem_addr;
        int block_id;
        int row;
        int tag;
        bool hit;
        int lru;             // tracks LRU, stores which cell to evict/write
        int lru_last_access; // helps track LRU, determines if there is a new LRU

        switch (opcode)
        {
        case 0:
            // Check last four least significant bits
            switch (lsb)
            {
            case 0: // add
                regs[regC] = (regs[regA] + regs[regB]);
                ++pc;
                break;
            case 1: // sub
                regs[regC] = (regs[regA] - regs[regB]);
                ++pc;
                break;
            case 2: // or
                regs[regC] = (regs[regA] | regs[regB]);
                ++pc;
                break;
            case 3: // and
                regs[regC] = (regs[regA] & regs[regB]);
                ++pc;
                break;
            case 4: // slt
                regs[regC] = (regs[regA] < regs[regB]) ? 1 : 0;
                ++pc;
                break;
            case 8: // jr
                // Keep all 16 bits
                pc = regs[regA];
                break;
            }
            break;
        case 1: // addi
            regs[regB] = (regs[regA] + imm7);
            ++pc;
            break;
        case 2:              // j
            if (pc == imm13) // halt
                halt = true;
            else
                pc = imm13;
            break;
        case 3: // jal
            regs[7] = pc + 1;
            pc = imm13;
            break;
        case 4: // lw (read)
            // initialize useful cache variables
            mem_addr = (regs[regA] + imm7) & 0b1111111111111;
            block_id = mem_addr / l1_blocksize;
            row = block_id % l1_rows;
            tag = block_id / l1_rows;

            // cache hit or miss
            hit = false;
            lru = 0;
            lru_last_access = 0;
            for (size_t i = 0; i < l1_assoc; ++i)
            {
                if (cache[row][i].valid && cache[row][i].tag == tag) // handle hit
                {
                    hit = true;
                    cache[row][i].last_access = 0; // reset last access
                    print_log_entry("L1", "HIT", pc, mem_addr, row);
                    break;
                }

                if (cache[row][i].last_access > lru_last_access) // find LRU
                {
                    lru = i;
                    lru_last_access = cache[row][i].last_access;
                }

                cache[row][i].last_access += 1; // update last access
            }

            if (!hit) // handle miss
            {
                cache[row][lru].valid = true;
                cache[row][lru].tag = tag;
                cache[row][lru].last_access = 0; // reset last access
                print_log_entry("L1", "MISS", pc, mem_addr, row);
            }

            // read memory to registers
            regs[regB] = memory[(regs[regA] + imm7) & 0b1111111111111];
            ++pc;
            break;
        case 5: // sw (write)
            // initialize useful cache variables
            mem_addr = (regs[regA] + imm7) & 0b1111111111111;
            block_id = mem_addr / l1_blocksize;
            row = block_id % l1_rows;
            tag = block_id / l1_rows;

            // find LRU
            lru = 0;
            lru_last_access = 0;
            for (size_t i = 0; i < l1_assoc; ++i)
            {
                if (cache[row][i].last_access > lru_last_access)
                {
                    lru = i;
                    lru_last_access = cache[row][i].last_access;
                }

                cache[row][i].last_access += 1; // update last access
            }

            // write to LRU
            cache[row][lru].valid = true;
            cache[row][lru].tag = tag;
            cache[row][lru].last_access = 0; // reset last access
            print_log_entry("L1", "SW", pc, mem_addr, row);

            // write registers to memory
            memory[(regs[regA] + imm7) & 0b1111111111111] = regs[regB];
            ++pc;
            break;
        case 6: // jeq
            pc = (regs[regA] == regs[regB]) ? (pc + 1 + imm7) : pc + 1;
            break;
        case 7: // slti
            regs[regB] = (regs[regA] < imm_slti) ? 1 : 0;
            ++pc;
            break;
        defualt:
            cerr << opcode << " is not a valid opcode" << endl;
            break;
        }

        regs[0] = 0; // Ensure $0 is still 0
    }
}

/*
    Simulates e20 with L1 and L2 cache
*/
void simulate_two_cache(uint16_t memory[], uint16_t regs[], uint16_t pc,
                        vector<vector<cache_cell>> l1_cache, vector<vector<cache_cell>> l2_cache,
                        int l1_size, int l1_assoc, int l1_blocksize, int l1_rows,
                        int l2_size, int l2_assoc, int l2_blocksize, int l2_rows)
{
    // Do simulation
    bool halt = false; // Flag for when encounter halt instruction
    while (!halt)      // Keep looping through instructions until halt
    {
        // Get e20 instruction fields
        uint16_t opcode = memory[pc % 8192] >> 13;
        uint16_t regA = memory[pc % 8192] >> 10 & 0b111;
        uint16_t regB = memory[pc % 8192] >> 7 & 0b111;
        uint16_t regC = memory[pc % 8192] >> 4 & 0b111;
        uint16_t lsb = memory[pc % 8192] & 0b1111; // Four least significant bits
        uint16_t imm13 = memory[pc % 8192] & 0b1111111111111;

        signed imm7 = memory[pc % 8192] & 0b1111111;
        uint16_t imm_slti = imm7;                // Specifically for slti
        if (memory[pc % 8192] >> 6 & 0b1 == 0b1) // imm is negative
        {
            imm7 = ((imm7 ^ 0b1111111) + 1) * -1; // Convert imm7 to two's complement
            // Convert imm_slti to unsigned 16 bit by sign extending the 7 bit two's complement
            imm_slti = ((imm_slti ^ 0b1111111) + 1) | 0b1111111110000000;
        }

        // Useful cache variables
        int mem_addr;
        int l1_block_id, l2_block_id;
        int l1_row, l2_row;
        int l1_tag, l2_tag;
        bool l1_hit, l2_hit;
        int l1_lru, l2_lru;                         // tracks LRU, stores which cell to evict/write
        int l1_lru_last_access, l2_lru_last_access; // helps track LRU, determines if there is a new LRU

        switch (opcode)
        {
        case 0:
            // Check last four least significant bits
            switch (lsb)
            {
            case 0: // add
                regs[regC] = (regs[regA] + regs[regB]);
                ++pc;
                break;
            case 1: // sub
                regs[regC] = (regs[regA] - regs[regB]);
                ++pc;
                break;
            case 2: // or
                regs[regC] = (regs[regA] | regs[regB]);
                ++pc;
                break;
            case 3: // and
                regs[regC] = (regs[regA] & regs[regB]);
                ++pc;
                break;
            case 4: // slt
                regs[regC] = (regs[regA] < regs[regB]) ? 1 : 0;
                ++pc;
                break;
            case 8: // jr
                // Keep all 16 bits
                pc = regs[regA];
                break;
            }
            break;
        case 1: // addi
            regs[regB] = (regs[regA] + imm7);
            ++pc;
            break;
        case 2:              // j
            if (pc == imm13) // halt
                halt = true;
            else
                pc = imm13;
            break;
        case 3: // jal
            regs[7] = pc + 1;
            pc = imm13;
            break;
        case 4: // lw (read)
            // initialize useful cache variables
            mem_addr = (regs[regA] + imm7) & 0b1111111111111;
            l1_block_id = mem_addr / l1_blocksize;
            l1_row = l1_block_id % l1_rows;
            l1_tag = l1_block_id / l1_rows;
            l2_block_id = mem_addr / l2_blocksize;
            l2_row = l2_block_id % l2_rows;
            l2_tag = l2_block_id / l2_rows;

            // L1 cache hit or miss
            l1_hit = false;
            l1_lru = 0;
            l1_lru_last_access = 0;
            for (size_t i = 0; i < l1_assoc; ++i)
            {
                if (l1_cache[l1_row][i].valid && l1_cache[l1_row][i].tag == l1_tag) // handle hit in L1
                {
                    l1_hit = true;
                    l1_cache[l1_row][i].last_access = 0; // reset last access
                    print_log_entry("L1", "HIT", pc, mem_addr, l1_row);
                    break;
                }

                if (l1_cache[l1_row][i].last_access > l1_lru_last_access) // find LRU
                {
                    l1_lru = i;
                    l1_lru_last_access = l1_cache[l1_row][i].last_access;
                }

                l1_cache[l1_row][i].last_access += 1; // update last access
            }

            if (!l1_hit) // handle miss in L1, move to L2
            {
                // update LRU in L1
                l1_cache[l1_row][l1_lru].valid = true;
                l1_cache[l1_row][l1_lru].tag = l1_tag;
                l1_cache[l1_row][l1_lru].last_access = 0; // reset last access
                print_log_entry("L1", "MISS", pc, mem_addr, l1_row);

                // L2 cache hit or miss
                l2_hit = false;
                l2_lru = 0;
                l2_lru_last_access = 0;
                for (size_t i = 0; i < l2_assoc; ++i)
                {
                    if (l2_cache[l2_row][i].valid && l2_cache[l2_row][i].tag == l2_tag) // handle hit in L2
                    {
                        l2_hit = true;
                        l2_cache[l2_row][i].last_access = 0; // reset last access
                        print_log_entry("L2", "HIT", pc, mem_addr, l2_row);
                        break;
                    }

                    if (l2_cache[l2_row][i].last_access > l2_lru_last_access) // find LRU
                    {
                        l2_lru = i;
                        l2_lru_last_access = l2_cache[l2_row][i].last_access;
                    }

                    l2_cache[l2_row][i].last_access += 1; // update last access
                }

                if (!l2_hit) // handle miss in L2
                {
                    // update LRU in L2
                    l2_cache[l2_row][l2_lru].valid = true;
                    l2_cache[l2_row][l2_lru].tag = l2_tag;
                    l2_cache[l2_row][l2_lru].last_access = 0; // reset last access
                    print_log_entry("L2", "MISS", pc, mem_addr, l2_row);
                }
            }

            // read memory to registers
            regs[regB] = memory[(regs[regA] + imm7) & 0b1111111111111];
            ++pc;
            break;
        case 5: // sw (write)
            // initialize useful cache variables
            mem_addr = (regs[regA] + imm7) & 0b1111111111111;
            l1_block_id = mem_addr / l1_blocksize;
            l1_row = l1_block_id % l1_rows;
            l1_tag = l1_block_id / l1_rows;
            l2_block_id = mem_addr / l2_blocksize;
            l2_row = l2_block_id % l2_rows;
            l2_tag = l2_block_id / l2_rows;

            // find LRU in L1
            l1_lru = 0;
            l1_lru_last_access = 0;
            for (size_t i = 0; i < l1_assoc; ++i)
            {
                if (l1_cache[l1_row][i].last_access > l1_lru_last_access)
                {
                    l1_lru = i;
                    l1_lru_last_access = l1_cache[l1_row][i].last_access;
                }

                l1_cache[l1_row][i].last_access += 1; // update last access
            }

            // find LRU in L2
            l2_lru = 0;
            l2_lru_last_access = 0;
            for (size_t i = 0; i < l2_assoc; ++i)
            {
                if (l2_cache[l2_row][i].last_access > l2_lru_last_access)
                {
                    l2_lru = i;
                    l2_lru_last_access = l2_cache[l2_row][i].last_access;
                }

                l2_cache[l2_row][i].last_access += 1; // update last access
            }

            // write to LRU in L1
            l1_cache[l1_row][l1_lru].valid = true;
            l1_cache[l1_row][l1_lru].tag = l1_tag;
            l1_cache[l1_row][l1_lru].last_access = 0; // reset last access
            print_log_entry("L1", "SW", pc, mem_addr, l1_row);

            // write to LRU in L2
            l2_cache[l2_row][l2_lru].valid = true;
            l2_cache[l2_row][l2_lru].tag = l2_tag;
            l2_cache[l2_row][l2_lru].last_access = 0; // reset last access
            print_log_entry("L2", "SW", pc, mem_addr, l2_row);

            // write registers to memory
            memory[(regs[regA] + imm7) & 0b1111111111111] = regs[regB];
            ++pc;
            break;
        case 6: // jeq
            pc = (regs[regA] == regs[regB]) ? (pc + 1 + imm7) : pc + 1;
            break;
        case 7: // slti
            regs[regB] = (regs[regA] < imm_slti) ? 1 : 0;
            ++pc;
            break;
        defualt:
            cerr << opcode << " is not a valid opcode" << endl;
            break;
        }

        regs[0] = 0; // Ensure $0 is still 0
    }
}

/**
    Main function
    Takes command-line args as documented below
*/
int main(int argc, char *argv[])
{
    /*
        Parse the command-line arguments
    */
    char *filename = nullptr;
    bool do_help = false;
    bool arg_error = false;
    string cache_config;
    for (int i = 1; i < argc; i++)
    {
        string arg(argv[i]);
        if (arg.rfind("-", 0) == 0)
        {
            if (arg == "-h" || arg == "--help")
                do_help = true;
            else if (arg == "--cache")
            {
                i++;
                if (i >= argc)
                    arg_error = true;
                else
                    cache_config = argv[i];
            }
            else
                arg_error = true;
        }
        else
        {
            if (filename == nullptr)
                filename = argv[i];
            else
                arg_error = true;
        }
    }
    /* Display error message if appropriate */
    if (arg_error || do_help || filename == nullptr)
    {
        cerr << "usage " << argv[0] << " [-h] [--cache CACHE] filename" << endl
             << endl;
        cerr << "Simulate E20 cache" << endl
             << endl;
        cerr << "positional arguments:" << endl;
        cerr << "  filename    The file containing machine code, typically with .bin suffix" << endl
             << endl;
        cerr << "optional arguments:" << endl;
        cerr << "  -h, --help  show this help message and exit" << endl;
        cerr << "  --cache CACHE  Cache configuration: size,associativity,blocksize (for one" << endl;
        cerr << "                 cache) or" << endl;
        cerr << "                 size,associativity,blocksize,size,associativity,blocksize" << endl;
        cerr << "                 (for two caches)" << endl;
        return 1;
    }

    /*
        ========== Basic e20 setup ==========
    */

    // Memory, registers, and program counter
    uint16_t memory[8192];
    uint16_t regs[8];
    uint16_t pc = 0;

    // Initialize memory
    for (size_t i = 0; i < 8192; ++i)
        memory[i] = 0;

    // Initialize registers
    for (size_t i = 0; i < 8; ++i)
        regs[i] = 0;

    ifstream f(filename);
    if (!f.is_open())
    {
        cerr << "Can't open file " << filename << endl;
        return 1;
    }
    // Load f and parse using load_machine_code
    load_machine_code(f, memory);

    /*
        ========== Cache simulation ==========
    */

    /* parse cache config */
    if (cache_config.size() > 0)
    {
        vector<int> parts;
        size_t pos;
        size_t lastpos = 0;
        while ((pos = cache_config.find(",", lastpos)) != string::npos)
        {
            parts.push_back(stoi(cache_config.substr(lastpos, pos)));
            lastpos = pos + 1;
        }
        parts.push_back(stoi(cache_config.substr(lastpos)));
        if (parts.size() == 3)
        {
            int L1size = parts[0];
            int L1assoc = parts[1];
            int L1blocksize = parts[2];
            int L1rows = L1size / (L1assoc * L1blocksize);

            vector<vector<cache_cell>> cache;
            for (size_t i = 0; i < L1rows; ++i)
            {
                vector<cache_cell> row;
                for (size_t j = 0; j < L1assoc; ++j)
                {
                    cache_cell c;
                    c.valid = false;
                    c.tag = 0;
                    c.last_access = 0;
                    row.push_back(c);
                }
                cache.push_back(row);
            }

            print_cache_config("L1", L1size, L1assoc, L1blocksize, L1rows);

            // Simulate
            simulate_one_cache(memory, regs, pc, cache, L1size, L1assoc, L1blocksize, L1rows);
        }
        else if (parts.size() == 6)
        {
            int L1size = parts[0];
            int L1assoc = parts[1];
            int L1blocksize = parts[2];
            int L1rows = L1size / (L1assoc * L1blocksize);

            int L2size = parts[3];
            int L2assoc = parts[4];
            int L2blocksize = parts[5];
            int L2rows = L2size / (L2assoc * L2blocksize);

            vector<vector<cache_cell>> l1_cache;
            for (size_t i = 0; i < L1rows; ++i)
            {
                vector<cache_cell> row;
                for (size_t j = 0; j < L1assoc; ++j)
                {
                    cache_cell c;
                    c.valid = false;
                    c.tag = 0;
                    c.last_access = 0;
                    row.push_back(c);
                }
                l1_cache.push_back(row);
            }

            vector<vector<cache_cell>> l2_cache;
            for (size_t i = 0; i < L2rows; ++i)
            {
                vector<cache_cell> row;
                for (size_t j = 0; j < L2assoc; ++j)
                {
                    cache_cell c;
                    c.valid = false;
                    c.tag = 0;
                    c.last_access = 0;
                    row.push_back(c);
                }
                l2_cache.push_back(row);
            }

            print_cache_config("L1", L1size, L1assoc, L1blocksize, L1rows);
            print_cache_config("L2", L2size, L2assoc, L2blocksize, L2rows);

            // Simulate
            simulate_two_cache(memory, regs, pc, l1_cache, l2_cache, L1size, L1assoc, L1blocksize, L1rows, L2size, L2assoc, L2blocksize, L2rows);
        }
        else
        {
            cerr << "Invalid cache config" << endl;
            return 1;
        }
    }

    return 0;
}
// ra0Eequ6ucie6Jei0koh6phishohm9