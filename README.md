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

This project is designed to simulate the e20 processor with L1 and l2 chaches. It uses labels to simplify algorithm, like a function call. Uses transfer values between registers and memory. It is important to break each 16-but nimber down into a 3 bit op to determine which instruction (or instructions since 6 of them use same opcode for some readon) is being used and determine the format since e20 is unlike e15's consistent 4-2-2-4, as e 20 can use 3-13, 3-3-3-3-4, 3-3-3-7, and other Most of the variables I made use the uint16_t variable type. a switch statement is used to cycle through each opcode to determine instructions. The instructions have different functionalities and formats and they are:


![Screenshot 2025-03-20 220504](https://github.com/user-attachments/assets/0c7c8380-554c-42d3![Screenshot 2025-03-20 221532](https://github.com/user-attachments/assets/5dd43cd7-03a![Screenshot 2025-03-20 221542](https://github.com/user-attachments/assets/769f0090-5d58-4209-afd9-bdd9e5262d67)
c-4862-ab98-c83ede132444)
-91a6-5ad240cf134d)
![Screenshot 2025-03-20 221446](https://github.com/user-attachments/assets/c7ab8a8a-9561-4acd-842f-bea43da15c1c)
![Screenshot 2025-03-20 221501](https://github.com/user-attachments/assets/e9ad21d0-7590-4b26-8b49-8c3bdb72460e)
![Screenshot 2025-03-20 221520](https://github.com/user-attachments/assets/99848dc0-a555-44ee-baf9-5af4856de40e)

However, I needed to add l1 and l1/l2 caches to the project. Caches are small, fast memory storage locations that sit between the CPU and main memory (RAM). Their purpose is to store copies of frequently accessed data or instructions to speed up execution. To implemet caches, I first create a cache_cell class that holds 3 data types:bool valid when when true → The data in the cache line is valid.
else false → The cache line does not hold valid data (it's either empty or outdated).  int tag;
Purpose: Stores the tag portion of the memory address.
Use: Used to check if a memory address matches what's currently stored in the cache line. and Last_access Purpose: Tracks the most recent access time to this cache line.

Use: Helps implement cache replacement policies like:

LRU (Least Recently Used): Replace the cache line with the oldest last_access value. I made 2 cache classes, 1 for l1 only and one for both l1 and l2

