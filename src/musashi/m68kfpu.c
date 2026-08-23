/*
 * DUMMY
*/


void m68040_fpu_op0(m68ki_cpu_core *m68k)
{
	fatalerror("M68kFPU: unimplemented main op %d\n", (m68k->ir >> 6)	& 0x3);
}


void m68040_fpu_op1(m68ki_cpu_core *m68k)
{
	fatalerror("m68040_fpu_op1: unimplemented op %d at %08X\n", (m68k->ir >> 6) & 0x3, REG_PC(m68k)-2);
}



