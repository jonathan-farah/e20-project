# e20-project
Jonathan Farah, jkf4068@nyu.edu 
an imaginary processor developed by former NYU Professor Jeffery Epstein that acts as simplified version of x86. The E20 processor has eight numbered 16-bit registers. By convention, in E20 assembly language, we write
each register name with a dollar sign, so the registers are $0 through $7. However, register $0 is special,
because its value is always zero and cannot be changed. Attempting to change the value of register $0 is
valid, but will not produce any effect: the register is immutable. Therefore, we have only seven mutable
registers that can actually be used as registers, $1 through $7. The initial value of all registers is zero.
In addition, the E20 processor has a 16-bit program counter register, which cannot be accessed directly
through the usual instructions. The program counter stores the memory address of the currently-executing
instruction. The initial value of the program counter register is zero. After each non-jump instruction, the
program counter is incremented. A jump instruction may adjust the program count

This project is designed to simulate the e20 processor. It uses labels to simplify algorithm, like a function call. Uses transfer values between registers and memory. It is important to break each 16-but nimber down into a 3 bit op to determine which instruction (or instructions since 6 of them use same opcode for some readon) is being used and determine the format since e20 is unlike e15's consistent 4-2-2-4, as e 20 can use 3-13, 3-3-3-3-4, 3-3-3-7, and others. From there, I made a extract bit function to get the specific values required. Another function made was a sign extender since some functions like addi and lw can take negative immediates. Most of the variables I made use the uint16_t variable type. a switch statement is used to cycle through each opcode to determine instructions.

