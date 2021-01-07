Basic idea


https://www.imperialviolet.org/2016/12/31/riscv.html

https://content.riscv.org/wp-content/uploads/2016/06/riscv-spec-v2.1.pdf

https://en.wikipedia.org/wiki/RISC-V

https://github.com/riscv/riscv-asm-manual/blob/master/riscv-asm.md

http://earlz.net/view/2017/08/13/0451/the-faults-and-shortcomings-of-the-evm

https://gist.github.com/FrankBuss/c974e59826d33e21d7cad54491ab50e8

https://docs.boom-core.org/en/latest/sections/intro-overview/riscv-isa.html

https://www.reddit.com/r/programming/comments/cixatj/an_exarm_engineer_critiques_riscv/

https://en.wikichip.org/wiki/risc-v/registers

https://github.com/nervosnetwork/ckb-vm


RISC-V has 32 (or 16 in the embedded variant) integer registers, and, when the floating-point extension is implemented, 32 floating-point registers. Except for memory access instructions, instructions address only registers.


The first integer register is a zero register, and the remainder are general-purpose registers. A store to the zero register has no effect, and a read always provides 0. Using the zero register as a placeholder makes for a simpler instruction set.



Like many RISC designs, RISC-V is a load–store architecture: instructions address only registers, with load and store instructions conveying to and from memory.


EVM is a RISC Harvard-architecture stack machine, which is fairly distinct in the world of computer architectures. EVM has around 200 instructions which push and pop values from a stack, occasionally performing specific actions on them (e.g. ADD takes two arguments of the stack, adds them together, and pushes the result back to the stack). If you’re familiar with reverse polish notation (RPN) calculators, then stack machines will appear similar. Stack machines are easy to implement but difficult to reverse-engineer. As a reverse-engineer, I have no registers, local variables, or arguments that I can label and track when looking at a stack machine

RISC-V provides the following features which make it easy to target with high-performance designs:

Relaxed memory model

This greatly simplifies the Load/Store Unit (LSU), which does not need to have loads snoop other loads nor does coherence traffic need to snoop the LSU, as required by sequential consistency.
Accrued Floating Point (FP) exception flags

The FP status register does not need to be renamed, nor can FP instructions throw exceptions themselves.
No integer side-effects

All integer ALU operations exhibit no side-effects, other than the writing of the destination register. This prevents the need to rename additional condition state.
No cmov or predication

Although predication can lower the branch predictor complexity of small designs, it greatly complicates out-of-order pipelines, including the addition of a third read port for integer operations.
No implicit register specifiers

Even JAL requires specifying an explicit register. This simplifies rename logic, which prevents either the need to know the instruction first before accessing the rename tables, or it prevents adding more ports to remove the instruction decode off the critical path.
Registers rs1, rs2, rs3, rd are always in the same place

This allows decode and rename to proceed in parallel.

