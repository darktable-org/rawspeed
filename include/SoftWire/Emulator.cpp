#include "Emulator.hpp"

#include <stdio.h>

namespace SoftWire
{
	float Emulator::sse[8][4];   // Storage for SSE emulation registers

	bool Emulator::emulateSSE = false;

	Emulator::Emulator(bool x64) : Optimizer(x64)
	{
	}

	Emulator::~Emulator()
	{
	}

	OperandREG8 Emulator::t8(unsigned int i)
	{
		static char t[8];

		if(i >= 8) throw;

		return r8(&t[i]);
	}

	OperandREG16 Emulator::t16(unsigned int i)
	{
		static short t[8];

		if(i >= 8) throw;

		return r16(&t[i]);
	}

	OperandREG32 Emulator::t32(unsigned int i)
	{
		static int t[8];

		if(i >= 8) throw;

		return r32(&t[i]);
	}

	Encoding *Emulator::addps(OperandXMMREG xmmi, OperandXMMREG xmmj)
	{
		if(emulateSSE)
		{
			spillMMX();
			const int i = xmmi.reg;
			const int j = xmmj.reg;

			fld(dword_ptr [&sse[i][0]]);
			fadd(dword_ptr [&sse[j][0]]);
			fstp(dword_ptr [&sse[i][0]]);

			fld(dword_ptr [&sse[i][1]]);
			fadd(dword_ptr [&sse[j][1]]);
			fstp(dword_ptr [&sse[i][1]]);

			fld(dword_ptr [&sse[i][2]]);
			fadd(dword_ptr [&sse[j][2]]);
			fstp(dword_ptr [&sse[i][2]]);

			fld(dword_ptr [&sse[i][3]]);
			fadd(dword_ptr [&sse[j][3]]);
			fstp(dword_ptr [&sse[i][3]]);

			return 0;
		}
		
		return Optimizer::addps(xmmi, xmmj);
	}
	
	Encoding *Emulator::addps(OperandXMMREG xmm, OperandMEM128 mem128)
	{
		if(emulateSSE)
		{
			spillMMX();
			const int i = xmm.reg;

			fld(dword_ptr [&sse[i][0]]);
			fadd((OperandMEM32)(mem128+0));
			fstp(dword_ptr [&sse[i][0]]);

			fld(dword_ptr [&sse[i][1]]);
			fadd((OperandMEM32)(mem128+4));
			fstp(dword_ptr [&sse[i][1]]);

			fld(dword_ptr [&sse[i][2]]);
			fadd((OperandMEM32)(mem128+8));
			fstp(dword_ptr [&sse[i][2]]);

			fld(dword_ptr [&sse[i][3]]);
			fadd((OperandMEM32)(mem128+12));
			fstp(dword_ptr [&sse[i][3]]);
			
			return 0;
		}
		
		return Optimizer::addps(xmm, mem128);
	}
	
	Encoding *Emulator::addps(OperandXMMREG xmm, OperandR_M128 r_m128)
	{
		if(r_m128.type == Operand::OPERAND_XMMREG) return addps(xmm, (OperandXMMREG)r_m128);
		else                                       return addps(xmm, (OperandMEM128)r_m128);
	}

	Encoding *Emulator::addss(OperandXMMREG xmmi, OperandXMMREG xmmj)
	{
		if(emulateSSE)
		{
			spillMMX();
			const int i = xmmi.reg;
			const int j = xmmj.reg;
			fld(dword_ptr [&sse[i][0]]);
			fadd(dword_ptr [&sse[j][0]]);
			fstp(dword_ptr [&sse[i][0]]);
			return 0;
		}
		
		return Optimizer::addss(xmmi, xmmj);
	}
	
	Encoding *Emulator::addss(OperandXMMREG xmm, OperandMEM32 mem32)
	{
		if(emulateSSE)
		{
			spillMMX();
			const int i = xmm.reg;
			fld(dword_ptr [&sse[i][0]]);
			fadd((OperandMEM32)mem32);
			fstp(dword_ptr [&sse[i][0]]);
			return 0;
		}
		
		return Optimizer::addss(xmm, mem32);
	}
	
	Encoding *Emulator::addss(OperandXMMREG xmm, OperandXMM32 xmm32)
	{
		if(xmm32.type == Operand::OPERAND_XMMREG) return addss(xmm, (OperandXMMREG)xmm32);
		else                                      return addss(xmm, (OperandMEM32)xmm32);
	}

	Encoding *Emulator::andnps(OperandXMMREG xmmi, OperandXMMREG xmmj)
	{
		if(emulateSSE)
		{
			const int i = xmmi.reg;
			const int j = xmmj.reg;

			mov(t32(0), dword_ptr [&sse[j][0]]);
			not(dword_ptr [&sse[i][0]]);
			and(dword_ptr [&sse[i][0]], t32(0));

			mov(t32(0), dword_ptr [&sse[j][1]]);
			not(dword_ptr [&sse[i][1]]);
			and(dword_ptr [&sse[i][1]], t32(0));

			mov(t32(0), dword_ptr [&sse[j][2]]);
			not(dword_ptr [&sse[i][2]]);
			and(dword_ptr [&sse[i][2]], t32(0));

			mov(t32(0), dword_ptr [&sse[j][3]]);
			not(dword_ptr [&sse[i][3]]);
			and(dword_ptr [&sse[i][3]], t32(0));

			free((OperandREF)0);
			return 0;
		}
		
		return Optimizer::andnps(xmmi, xmmj);
	}
	
	Encoding *Emulator::andnps(OperandXMMREG xmm, OperandMEM128 mem128)
	{
		if(emulateSSE)
		{
			const int i = xmm.reg;

			mov(t32(0), (OperandMEM32)(mem128+0));
			not(dword_ptr [&sse[i][0]]);
			and(dword_ptr [&sse[i][0]], t32(0));

			mov(t32(0), (OperandMEM32)(mem128+4));
			not(dword_ptr [&sse[i][1]]);
			and(dword_ptr [&sse[i][1]], t32(0));

			mov(t32(0), (OperandMEM32)(mem128+8));
			not(dword_ptr [&sse[i][2]]);
			and(dword_ptr [&sse[i][2]], t32(0));

			mov(t32(0), (OperandMEM32)(mem128+12));
			not(dword_ptr [&sse[i][3]]);
			and(dword_ptr [&sse[i][3]], t32(0));

			free((OperandREF)0);
			return 0;
		}
		
		return Optimizer::andnps(xmm, mem128);
	}
	
	Encoding *Emulator::andnps(OperandXMMREG xmm, OperandR_M128 r_m128)
	{
		if(r_m128.type == Operand::OPERAND_XMMREG) return andnps(xmm, (OperandXMMREG)r_m128);
		else                                       return andnps(xmm, (OperandMEM128)r_m128);
	}

	Encoding *Emulator::andps(OperandXMMREG xmmi, OperandXMMREG xmmj)
	{
		if(emulateSSE)
		{
			const int i = xmmi.reg;
			const int j = xmmj.reg;

			mov(t32(0), dword_ptr [&sse[j][0]]);
			and(dword_ptr [&sse[i][0]], t32(0));

			mov(t32(0), dword_ptr [&sse[j][1]]);
			and(dword_ptr [&sse[i][1]], t32(0));

			mov(t32(0), dword_ptr [&sse[j][2]]);
			and(dword_ptr [&sse[i][2]], t32(0));

			mov(t32(0), dword_ptr [&sse[j][3]]);
			and(dword_ptr [&sse[i][3]], t32(0));

			free((OperandREF)0);
			return 0;
		}
		
		return Optimizer::andps(xmmi, xmmj);
	}
	
	Encoding *Emulator::andps(OperandXMMREG xmm, OperandMEM128 mem128)
	{
		if(emulateSSE)
		{
			const int i = xmm.reg;

			mov(t32(0), (OperandMEM32)(mem128+0));
			and(dword_ptr [&sse[i][0]], t32(0));

			mov(t32(0), (OperandMEM32)(mem128+4));
			and(dword_ptr [&sse[i][1]], t32(0));

			mov(t32(0), (OperandMEM32)(mem128+8));
			and(dword_ptr [&sse[i][2]], t32(0));

			mov(t32(0), (OperandMEM32)(mem128+12));
			and(dword_ptr [&sse[i][3]], t32(0));

			free((OperandREF)0);
			return 0;
		}
		
		return Optimizer::andps(xmm, mem128);
	}
	
	Encoding *Emulator::andps(OperandXMMREG xmm, OperandR_M128 r_m128)
	{
		if(r_m128.type == Operand::OPERAND_XMMREG) return andps(xmm, (OperandXMMREG)r_m128);
		else                                       return andps(xmm, (OperandMEM128)r_m128);
	}

	Encoding *Emulator::cmpps(OperandXMMREG xmmi, OperandXMMREG xmmj, char c)
	{
		if(emulateSSE)
		{
			spillMMX();
			const int i = xmmi.reg;
			const int j = xmmj.reg;
			static float zero = 0;
			static float one = 1;
			fld(dword_ptr [&zero]);		// st2
			fld(dword_ptr [&one]);		// st1

			fld(dword_ptr [&sse[j][0]]);
			fld(dword_ptr [&sse[i][0]]);
			fcomip(st0, st1);
			switch(c)
			{
			case 0:   // CMPEQPS
				fcmove(st1);
				fcmovne(st2);
				break;
			case 1:   // CMPLTPS
				fcmovb(st1);
				fcmovnb(st2);
				break;
			case 2:   // CMPLEPS
				fcmovbe(st1);
				fcmovnbe(st2);
				break;
			case 3:   // CMPUNORDPS
				fcmovnu(st1);
				fcmovu(st2);
				break;
			case 4:   // CMPNEQPS
				fcmovne(st1);
				fcmove(st2);
				break;
			case 5:   // CMPNLTPS
				fcmovnb(st1);
				fcmovb(st2);
				break;
			case 6:   // CMPNLEPS
				fcmovnbe(st1);
				fcmovbe(st2);
				break;
			case 7:   // CMPORDPS
				fcmovnu(st1);
				fcmovu(st2);
				break;
			default:
				throw INTERNAL_ERROR;
			}
			fstp(dword_ptr [&sse[i][0]]);

			fld(dword_ptr [&sse[j][1]]);
			fld(dword_ptr [&sse[i][1]]);
			fcomip(st0, st1);
			switch(c)
			{
			case 0:   // CMPEQPS
				fcmove(st1);
				fcmovne(st2);
				break;
			case 1:   // CMPLTPS
				fcmovb(st1);
				fcmovnb(st2);
				break;
			case 2:   // CMPLEPS
				fcmovbe(st1);
				fcmovnbe(st2);
				break;
			case 3:   // CMPUNORDPS
				fcmovnu(st1);
				fcmovu(st2);
				break;
			case 4:   // CMPNEQPS
				fcmovne(st1);
				fcmove(st2);
				break;
			case 5:   // CMPNLTPS
				fcmovnb(st1);
				fcmovb(st2);
				break;
			case 6:   // CMPNLEPS
				fcmovnbe(st1);
				fcmovbe(st2);
				break;
			case 7:   // CMPORDPS
				fcmovnu(st1);
				fcmovu(st2);
				break;
			default:
				throw INTERNAL_ERROR;
			}
			fstp(dword_ptr [&sse[i][1]]);

			fld(dword_ptr [&sse[j][2]]);
			fld(dword_ptr [&sse[i][2]]);
			fcomip(st0, st1);
			switch(c)
			{
			case 0:   // CMPEQPS
				fcmove(st1);
				fcmovne(st2);
				break;
			case 1:   // CMPLTPS
				fcmovb(st1);
				fcmovnb(st2);
				break;
			case 2:   // CMPLEPS
				fcmovbe(st1);
				fcmovnbe(st2);
				break;
			case 3:   // CMPUNORDPS
				fcmovnu(st1);
				fcmovu(st2);
				break;
			case 4:   // CMPNEQPS
				fcmovne(st1);
				fcmove(st2);
				break;
			case 5:   // CMPNLTPS
				fcmovnb(st1);
				fcmovb(st2);
				break;
			case 6:   // CMPNLEPS
				fcmovnbe(st1);
				fcmovbe(st2);
				break;
			case 7:   // CMPORDPS
				fcmovnu(st1);
				fcmovu(st2);
				break;
			default:
				throw INTERNAL_ERROR;
			}
			fstp(dword_ptr [&sse[i][2]]);

			fld(dword_ptr [&sse[j][3]]);
			fld(dword_ptr [&sse[i][3]]);
			fcomip(st0, st1);
			switch(c)
			{
			case 0:   // CMPEQPS
				fcmove(st1);
				fcmovne(st2);
				break;
			case 1:   // CMPLTPS
				fcmovb(st1);
				fcmovnb(st2);
				break;
			case 2:   // CMPLEPS
				fcmovbe(st1);
				fcmovnbe(st2);
				break;
			case 3:   // CMPUNORDPS
				fcmovnu(st1);
				fcmovu(st2);
				break;
			case 4:   // CMPNEQPS
				fcmovne(st1);
				fcmove(st2);
				break;
			case 5:   // CMPNLTPS
				fcmovnb(st1);
				fcmovb(st2);
				break;
			case 6:   // CMPNLEPS
				fcmovnbe(st1);
				fcmovbe(st2);
				break;
			case 7:   // CMPORDPS
				fcmovnu(st1);
				fcmovu(st2);
				break;
			default:
				throw INTERNAL_ERROR;
			}
			fstp(dword_ptr [&sse[i][3]]);

			ffree(st0);
			ffree(st1);

			return 0;
		}
		
		return Optimizer::cmpps(xmmi, xmmj, c);
	}

	Encoding *Emulator::cmpps(OperandXMMREG xmm, OperandMEM128 mem128, char c)
	{
		if(emulateSSE)
		{
			spillMMX();
			const int i = xmm.reg;
			static float zero = 0;
			static float one = 1;
			fld(dword_ptr [&zero]);		// st2
			fld(dword_ptr [&one]);		// st1

			fld((OperandMEM32)(mem128+0));
			fld(dword_ptr [&sse[i][0]]);
			fcomip(st0, st1);
			switch(c)
			{
			case 0:   // CMPEQPS
				fcmove(st1);
				fcmovne(st2);
				break;
			case 1:   // CMPLTPS
				fcmovb(st1);
				fcmovnb(st2);
				break;
			case 2:   // CMPLEPS
				fcmovbe(st1);
				fcmovnbe(st2);
				break;
			case 3:   // CMPUNORDPS
				fcmovnu(st1);
				fcmovu(st2);
				break;
			case 4:   // CMPNEQPS
				fcmovne(st1);
				fcmove(st2);
				break;
			case 5:   // CMPNLTPS
				fcmovnb(st1);
				fcmovb(st2);
				break;
			case 6:   // CMPNLEPS
				fcmovnbe(st1);
				fcmovbe(st2);
				break;
			case 7:   // CMPORDPS
				fcmovnu(st1);
				fcmovu(st2);
				break;
			default:
				throw INTERNAL_ERROR;
			}
			fstp(dword_ptr [&sse[i][0]]);

			fld((OperandMEM32)(mem128+4));
			fld(dword_ptr [&sse[i][1]]);
			fcomip(st0, st1);
			switch(c)
			{
			case 0:   // CMPEQPS
				fcmove(st1);
				fcmovne(st2);
				break;
			case 1:   // CMPLTPS
				fcmovb(st1);
				fcmovnb(st2);
				break;
			case 2:   // CMPLEPS
				fcmovbe(st1);
				fcmovnbe(st2);
				break;
			case 3:   // CMPUNORDPS
				fcmovnu(st1);
				fcmovu(st2);
				break;
			case 4:   // CMPNEQPS
				fcmovne(st1);
				fcmove(st2);
				break;
			case 5:   // CMPNLTPS
				fcmovnb(st1);
				fcmovb(st2);
				break;
			case 6:   // CMPNLEPS
				fcmovnbe(st1);
				fcmovbe(st2);
				break;
			case 7:   // CMPORDPS
				fcmovnu(st1);
				fcmovu(st2);
				break;
			default:
				throw INTERNAL_ERROR;
			}
			fstp(dword_ptr [&sse[i][1]]);

			fld((OperandMEM32)(mem128+8));
			fld(dword_ptr [&sse[i][2]]);
			fcomip(st0, st1);
			switch(c)
			{
			case 0:   // CMPEQPS
				fcmove(st1);
				fcmovne(st2);
				break;
			case 1:   // CMPLTPS
				fcmovb(st1);
				fcmovnb(st2);
				break;
			case 2:   // CMPLEPS
				fcmovbe(st1);
				fcmovnbe(st2);
				break;
			case 3:   // CMPUNORDPS
				fcmovnu(st1);
				fcmovu(st2);
				break;
			case 4:   // CMPNEQPS
				fcmovne(st1);
				fcmove(st2);
				break;
			case 5:   // CMPNLTPS
				fcmovnb(st1);
				fcmovb(st2);
				break;
			case 6:   // CMPNLEPS
				fcmovnbe(st1);
				fcmovbe(st2);
				break;
			case 7:   // CMPORDPS
				fcmovnu(st1);
				fcmovu(st2);
				break;
			default:
				throw INTERNAL_ERROR;
			}
			fstp(dword_ptr [&sse[i][2]]);

			fld((OperandMEM32)(mem128+12));
			fld(dword_ptr [&sse[i][3]]);
			fcomip(st0, st1);
			switch(c)
			{
			case 0:   // CMPEQPS
				fcmove(st1);
				fcmovne(st2);
				break;
			case 1:   // CMPLTPS
				fcmovb(st1);
				fcmovnb(st2);
				break;
			case 2:   // CMPLEPS
				fcmovbe(st1);
				fcmovnbe(st2);
				break;
			case 3:   // CMPUNORDPS
				fcmovnu(st1);
				fcmovu(st2);
				break;
			case 4:   // CMPNEQPS
				fcmovne(st1);
				fcmove(st2);
				break;
			case 5:   // CMPNLTPS
				fcmovnb(st1);
				fcmovb(st2);
				break;
			case 6:   // CMPNLEPS
				fcmovnbe(st1);
				fcmovbe(st2);
				break;
			case 7:   // CMPORDPS
				fcmovnu(st1);
				fcmovu(st2);
				break;
			default:
				throw INTERNAL_ERROR;
			}
			fstp(dword_ptr [&sse[i][3]]);

			ffree(st0);
			ffree(st1);

			return 0;
		}
		
		return Optimizer::cmpps(xmm, mem128, c);
	}

	Encoding *Emulator::cmpps(OperandXMMREG xmm, OperandR_M128 r_m128, char c)
	{
		if(r_m128.type == Operand::OPERAND_XMMREG) return cmpps(xmm, (OperandXMMREG)r_m128, c);
		else                                       return cmpps(xmm, (OperandMEM128)r_m128, c);
	}

	Encoding *Emulator::cmpeqps(OperandXMMREG xmmi, OperandXMMREG xmmj)
	{
		return cmpps(xmmi, xmmj, 0);
	}

	Encoding *Emulator::cmpeqps(OperandXMMREG xmm, OperandMEM128 mem128)
	{
		return cmpps(xmm, mem128, 0);
	}

	Encoding *Emulator::cmpeqps(OperandXMMREG xmm, OperandR_M128 r_m128)
	{
		return cmpps(xmm, r_m128, 0);
	}

	Encoding *Emulator::cmpleps(OperandXMMREG xmmi, OperandXMMREG xmmj)
	{
		return cmpps(xmmi, xmmj, 2);
	}

	Encoding *Emulator::cmpleps(OperandXMMREG xmm, OperandMEM128 mem128)
	{
		return cmpps(xmm, mem128, 2);
	}

	Encoding *Emulator::cmpleps(OperandXMMREG xmm, OperandR_M128 r_m128)
	{
		return cmpps(xmm, r_m128, 2);
	}

	Encoding *Emulator::cmpltps(OperandXMMREG xmmi, OperandXMMREG xmmj)
	{
		return cmpps(xmmi, xmmj, 1);
	}

	Encoding *Emulator::cmpltps(OperandXMMREG xmm, OperandMEM128 mem128)
	{
		return cmpps(xmm, mem128, 1);
	}

	Encoding *Emulator::cmpltps(OperandXMMREG xmm, OperandR_M128 r_m128)
	{
		return cmpps(xmm, r_m128, 1);
	}

	Encoding *Emulator::cmpneqps(OperandXMMREG xmmi, OperandXMMREG xmmj)
	{
		return cmpps(xmmi, xmmj, 4);
	}

	Encoding *Emulator::cmpneqps(OperandXMMREG xmm, OperandMEM128 mem128)
	{
		return cmpps(xmm, mem128, 4);
	}

	Encoding *Emulator::cmpneqps(OperandXMMREG xmm, OperandR_M128 r_m128)
	{
		return cmpps(xmm, r_m128, 4);
	}

	Encoding *Emulator::cmpnleps(OperandXMMREG xmmi, OperandXMMREG xmmj)
	{
		return cmpps(xmmi, xmmj, 6);
	}

	Encoding *Emulator::cmpnleps(OperandXMMREG xmm, OperandMEM128 mem128)
	{
		return cmpps(xmm, mem128, 6);
	}

	Encoding *Emulator::cmpnleps(OperandXMMREG xmm, OperandR_M128 r_m128)
	{
		return cmpps(xmm, r_m128, 6);
	}

	Encoding *Emulator::cmpnltps(OperandXMMREG xmmi, OperandXMMREG xmmj)
	{
		return cmpps(xmmi, xmmj, 5);
	}

	Encoding *Emulator::cmpnltps(OperandXMMREG xmm, OperandMEM128 mem128)
	{
		return cmpps(xmm, mem128, 5);
	}

	Encoding *Emulator::cmpnltps(OperandXMMREG xmm, OperandR_M128 r_m128)
	{
		return cmpps(xmm, r_m128, 5);
	}

	Encoding *Emulator::cmpordps(OperandXMMREG xmmi, OperandXMMREG xmmj)
	{
		return cmpps(xmmi, xmmj, 7);
	}

	Encoding *Emulator::cmpordps(OperandXMMREG xmm, OperandMEM128 mem128)
	{
		return cmpps(xmm, mem128, 7);
	}

	Encoding *Emulator::cmpordps(OperandXMMREG xmm, OperandR_M128 r_m128)
	{
		return cmpps(xmm, r_m128, 7);
	}

	Encoding *Emulator::cmpunordps(OperandXMMREG xmmi, OperandXMMREG xmmj)
	{
		return cmpps(xmmi, xmmj, 3);
	}

	Encoding *Emulator::cmpunordps(OperandXMMREG xmm, OperandMEM128 mem128)
	{
		return cmpps(xmm, mem128, 3);
	}

	Encoding *Emulator::cmpunordps(OperandXMMREG xmm, OperandR_M128 r_m128)
	{
		return cmpps(xmm, r_m128, 3);
	}

	Encoding *Emulator::cmpss(OperandXMMREG xmmi, OperandXMMREG xmmj, char c)
	{
		if(emulateSSE)
		{
			spillMMX();
			const int i = xmmi.reg;
			const int j = xmmj.reg;
			static float zero = 0;
			static float one = 1;
			fld(dword_ptr [&zero]);		// st2
			fld(dword_ptr [&one]);		// st1

			fld(dword_ptr [&sse[j][0]]);
			fld(dword_ptr [&sse[i][0]]);
			fcomip(st0, st1);
			switch(c)
			{
			case 0:   // CMPEQPS
				fcmove(st1);
				fcmovne(st2);
				break;
			case 1:   // CMPLTPS
				fcmovb(st1);
				fcmovnb(st2);
				break;
			case 2:   // CMPLEPS
				fcmovbe(st1);
				fcmovnbe(st2);
				break;
			case 3:   // CMPUNORDPS
				fcmovnu(st1);
				fcmovu(st2);
				break;
			case 4:   // CMPNEQPS
				fcmovne(st1);
				fcmove(st2);
				break;
			case 5:   // CMPNLTPS
				fcmovnb(st1);
				fcmovb(st2);
				break;
			case 6:   // CMPNLEPS
				fcmovnbe(st1);
				fcmovbe(st2);
				break;
			case 7:   // CMPORDPS
				fcmovnu(st1);
				fcmovu(st2);
				break;
			default:
				throw INTERNAL_ERROR;
			}
			fstp(dword_ptr [&sse[i][0]]);

			ffree(st0);
			ffree(st1);

			return 0;
		}
		
		return Optimizer::cmpss(xmmi, xmmj, c);
	}

	Encoding *Emulator::cmpss(OperandXMMREG xmm, OperandMEM32 mem32, char c)
	{
		if(emulateSSE)
		{
			spillMMX();
			const int i = xmm.reg;
			static float zero = 0;
			static float one = 1;
			fld(dword_ptr [&zero]);		// st2
			fld(dword_ptr [&one]);		// st1

			fld((OperandMEM32)(mem32+0));
			fld(dword_ptr [&sse[i][0]]);
			fcomip(st0, st1);
			switch(c)
			{
			case 0:   // CMPEQPS
				fcmove(st1);
				fcmovne(st2);
				break;
			case 1:   // CMPLTPS
				fcmovb(st1);
				fcmovnb(st2);
				break;
			case 2:   // CMPLEPS
				fcmovbe(st1);
				fcmovnbe(st2);
				break;
			case 3:   // CMPUNORDPS
				fcmovnu(st1);
				fcmovu(st2);
				break;
			case 4:   // CMPNEQPS
				fcmovne(st1);
				fcmove(st2);
				break;
			case 5:   // CMPNLTPS
				fcmovnb(st1);
				fcmovb(st2);
				break;
			case 6:   // CMPNLEPS
				fcmovnbe(st1);
				fcmovbe(st2);
				break;
			case 7:   // CMPORDPS
				fcmovnu(st1);
				fcmovu(st2);
				break;
			default:
				throw INTERNAL_ERROR;
			}
			fstp(dword_ptr [&sse[i][0]]);

			ffree(st0);
			ffree(st1);

			return 0;
		}
		
		return Optimizer::cmpss(xmm, mem32, c);
	}

	Encoding *Emulator::cmpss(OperandXMMREG xmm, OperandXMM32 xmm32, char c)
	{
		if(xmm32.type == Operand::OPERAND_XMMREG) return cmpss(xmm, (OperandXMMREG)xmm32, c);
		else                                      return cmpss(xmm, (OperandMEM32)xmm32, c);
	}

	Encoding *Emulator::cmpeqss(OperandXMMREG xmmi, OperandXMMREG xmmj)
	{
		return cmpss(xmmi, xmmj, 0);
	}

	Encoding *Emulator::cmpeqss(OperandXMMREG xmm, OperandMEM32 mem32)
	{
		return cmpss(xmm, mem32, 0);
	}

	Encoding *Emulator::cmpeqss(OperandXMMREG xmm, OperandXMM32 xmm32)
	{
		return cmpss(xmm, xmm32, 0);
	}

	Encoding *Emulator::cmpless(OperandXMMREG xmmi, OperandXMMREG xmmj)
	{
		return cmpss(xmmi, xmmj, 2);
	}

	Encoding *Emulator::cmpless(OperandXMMREG xmm, OperandMEM32 mem32)
	{
		return cmpss(xmm, mem32, 2);
	}

	Encoding *Emulator::cmpless(OperandXMMREG xmm, OperandXMM32 xmm32)
	{
		return cmpss(xmm, xmm32, 2);
	}

	Encoding *Emulator::cmpltss(OperandXMMREG xmmi, OperandXMMREG xmmj)
	{
		return cmpss(xmmi, xmmj, 1);
	}

	Encoding *Emulator::cmpltss(OperandXMMREG xmm, OperandMEM32 mem32)
	{
		return cmpss(xmm, mem32, 1);
	}

	Encoding *Emulator::cmpltss(OperandXMMREG xmm, OperandXMM32 xmm32)
	{
		return cmpss(xmm, xmm32, 1);
	}

	Encoding *Emulator::cmpneqss(OperandXMMREG xmmi, OperandXMMREG xmmj)
	{
		return cmpss(xmmi, xmmj, 4);
	}

	Encoding *Emulator::cmpneqss(OperandXMMREG xmm, OperandMEM32 mem32)
	{
		return cmpss(xmm, mem32, 4);
	}

	Encoding *Emulator::cmpneqss(OperandXMMREG xmm, OperandXMM32 xmm32)
	{
		return cmpss(xmm, xmm32, 4);
	}

	Encoding *Emulator::cmpnless(OperandXMMREG xmmi, OperandXMMREG xmmj)
	{
		return cmpss(xmmi, xmmj, 6);
	}

	Encoding *Emulator::cmpnless(OperandXMMREG xmm, OperandMEM32 mem32)
	{
		return cmpss(xmm, mem32, 6);
	}

	Encoding *Emulator::cmpnless(OperandXMMREG xmm, OperandXMM32 xmm32)
	{
		return cmpss(xmm, xmm32, 6);
	}

	Encoding *Emulator::cmpnltss(OperandXMMREG xmmi, OperandXMMREG xmmj)
	{
		return cmpss(xmmi, xmmj, 5);
	}

	Encoding *Emulator::cmpnltss(OperandXMMREG xmm, OperandMEM32 mem32)
	{
		return cmpss(xmm, mem32, 5);
	}

	Encoding *Emulator::cmpnltss(OperandXMMREG xmm, OperandXMM32 xmm32)
	{
		return cmpss(xmm, xmm32, 5);
	}

	Encoding *Emulator::cmpordss(OperandXMMREG xmmi, OperandXMMREG xmmj)
	{
		return cmpss(xmmi, xmmj, 7);
	}

	Encoding *Emulator::cmpordss(OperandXMMREG xmm, OperandMEM32 mem32)
	{
		return cmpss(xmm, mem32, 7);
	}

	Encoding *Emulator::cmpordss(OperandXMMREG xmm, OperandXMM32 xmm32)
	{
		return cmpss(xmm, xmm32, 7);
	}

	Encoding *Emulator::cmpunordss(OperandXMMREG xmmi, OperandXMMREG xmmj)
	{
		return cmpss(xmmi, xmmj, 3);
	}

	Encoding *Emulator::cmpunordss(OperandXMMREG xmm, OperandMEM32 mem32)
	{
		return cmpss(xmm, mem32, 3);
	}

	Encoding *Emulator::cmpunordss(OperandXMMREG xmm, OperandXMM32 xmm32)
	{
		return cmpss(xmm, xmm32, 3);
	}

	Encoding *Emulator::comiss(OperandXMMREG xmmi, OperandXMMREG xmmj)
	{
		if(emulateSSE)
		{
			spillMMX();
			const int i = xmmi.reg;
			const int j = xmmj.reg;

			fld(dword_ptr [&sse[j][0]]);
			fld(dword_ptr [&sse[i][0]]);
			fcomip(st0, st1);
			ffree(st0);

			return 0;
		}
		
		return Optimizer::comiss(xmmi, xmmj);
	}

	Encoding *Emulator::comiss(OperandXMMREG xmm, OperandMEM32 mem32)
	{
		if(emulateSSE)
		{
			spillMMX();
			const int i = xmm.reg;
			
			fld(mem32);
			fld(dword_ptr [&sse[i][0]]);
			fcomip(st0, st1);
			ffree(st0);

			return 0;
		}
		
		return Optimizer::comiss(xmm, mem32);
	}

	Encoding *Emulator::comiss(OperandXMMREG xmm, OperandXMM32 xmm32)
	{
		if(xmm32.type == Operand::OPERAND_XMMREG) return comiss(xmm, (OperandXMMREG)xmm32);
		else                                      return comiss(xmm, (OperandMEM32)xmm32);
	}

	Encoding *Emulator::cvtpi2ps(OperandXMMREG xmm, OperandMMREG mm)
	{
		if(emulateSSE)
		{
			static int dword[2];
			movq(qword_ptr [dword], mm);
			const int i = xmm.reg;
			spillMMX();

			fild(dword_ptr [&dword[0]]);
			fstp(dword_ptr [&sse[i][0]]);
			fild(dword_ptr [&dword[1]]);
			fstp(dword_ptr [&sse[i][1]]);

			return 0;
		}
		
		return Optimizer::cvtpi2ps(xmm, mm);
	}

	Encoding *Emulator::cvtpi2ps(OperandXMMREG xmm, OperandMEM64 mem64)
	{
		if(emulateSSE)
		{
			spillMMX();
			const int i = xmm.reg;

			fild((OperandMEM32)(mem64+0));
			fstp(dword_ptr [&sse[i][0]]);
			fild((OperandMEM32)(mem64+4));
			fstp(dword_ptr [&sse[i][1]]);

			return 0;
		}
		
		return Optimizer::cvtpi2ps(xmm, mem64);
	}

	Encoding *Emulator::cvtpi2ps(OperandXMMREG xmm, OperandMM64 r_m64)
	{
		if(r_m64.type == Operand::OPERAND_MMREG) return cvtpi2ps(xmm, (OperandMMREG)r_m64);
		else                                     return cvtpi2ps(xmm, (OperandMEM64)r_m64);
	}

	Encoding *Emulator::cvtps2pi(OperandMMREG mm, OperandXMMREG xmm)
	{
		if(emulateSSE)
		{
			static int dword[2];

			spillMMXcept(mm);
			const int i = xmm.reg;
		//	short fpuCW1;
		//	short fpuCW2;

		//	fldcw(word_ptr [&fpuCW1]);
		//	fldcw(word_ptr [&fpuCW2]);
		//	and(word_ptr [&fpuCW2], (short)0xF3FF);
		//	fstcw(word_ptr [&fpuCW2]);

			fld(dword_ptr [&sse[i][0]]);
			fistp(dword_ptr [&dword[0]]);
			fld(dword_ptr [&sse[i][1]]);
			fistp(dword_ptr [&dword[1]]);

		//	fstcw(word_ptr [&fpuCW1]);
			movq(mm, qword_ptr [dword]);

			return 0;
		}
		
		return Optimizer::cvtps2pi(mm, xmm);
	}

	Encoding *Emulator::cvtps2pi(OperandMMREG mm, OperandMEM64 mem64)
	{
		if(emulateSSE)
		{
			static int dword[2];

			spillMMXcept(mm);
		//	short fpuCW1;
		//	short fpuCW2;

		//	fldcw(word_ptr [&fpuCW1]);
		//	fldcw(word_ptr [&fpuCW2]);
		//	and(word_ptr [&fpuCW2], (short)0xF3FF);
		//	fstcw(word_ptr [&fpuCW2]);

			fld((OperandMEM32)(mem64+0));
			fistp(dword_ptr [&dword[0]]);
			fld((OperandMEM32)(mem64+4));
			fistp(dword_ptr [&dword[1]]);

		//	fstcw(word_ptr [&fpuCW1]);
			movq(mm, qword_ptr [dword]);

			return 0;
		}
		
		return Optimizer::cvtps2pi(mm, mem64);
	}

	Encoding *Emulator::cvtps2pi(OperandMMREG mm, OperandXMM64 xmm64)
	{
		if(xmm64.type == Operand::OPERAND_XMMREG) return cvtps2pi(mm, (OperandXMMREG)xmm64);
		else                                      return cvtps2pi(mm, (OperandMEM64)xmm64);
	}

	Encoding *Emulator::cvttps2pi(OperandMMREG mm, OperandXMMREG xmm)
	{
		if(emulateSSE)
		{
			static int dword[2];
			spillMMXcept(mm);
			const int i = xmm.reg;
			short fpuCW1;
			short fpuCW2;

			fstcw(word_ptr [&fpuCW1]);
			fstcw(word_ptr [&fpuCW2]);
			or(word_ptr [&fpuCW2], (unsigned short)0x0C00);
			fldcw(word_ptr [&fpuCW2]);

			fld(dword_ptr [&sse[i][0]]);
			fistp(dword_ptr [&dword[0]]);
			fld(dword_ptr [&sse[i][1]]);
			fistp(dword_ptr [&dword[1]]);

			fldcw(word_ptr [&fpuCW1]);
			movq(mm, qword_ptr [dword]);

			return 0;
		}
		
		return Optimizer::cvttps2pi(mm, xmm);
	}

	Encoding *Emulator::cvttps2pi(OperandMMREG mm, OperandMEM64 mem64)
	{
		if(emulateSSE)
		{
			static int dword[2];

			spillMMXcept(mm);
			static short fpuCW1;
			static short fpuCW2;

			fstcw(word_ptr [&fpuCW1]);
			fstcw(word_ptr [&fpuCW2]);
			or(word_ptr [&fpuCW2], (unsigned short)0x0C00);
			fldcw(word_ptr [&fpuCW2]);

			fld((OperandMEM32)(mem64+0));
			fistp(dword_ptr [&dword[0]]);
			fld((OperandMEM32)(mem64+4));
			fistp(dword_ptr [&dword[1]]);

			fldcw(word_ptr [&fpuCW1]);
			movq(mm, qword_ptr [dword]);

			return 0;
		}
		
		return Optimizer::cvttps2pi(mm, mem64);
	}

	Encoding *Emulator::cvttps2pi(OperandMMREG mm, OperandXMM64 xmm64)
	{
		if(xmm64.type == Operand::OPERAND_XMMREG) return cvttps2pi(mm, (OperandXMMREG)xmm64);
		else                                      return cvttps2pi(mm, (OperandMEM64)xmm64);
	}

	Encoding *Emulator::cvtsi2ss(OperandXMMREG xmm, OperandREG32 reg32)
	{
		if(emulateSSE)
		{
			spillMMX();
			const int i = xmm.reg;
			static int dword;

			mov(dword_ptr [&dword], reg32);
			fild(dword_ptr [&dword]);
			fstp(dword_ptr [&sse[i][0]]);

			return 0;
		}
		
		return Optimizer::cvtsi2ss(xmm, reg32);
	}

	Encoding *Emulator::cvtsi2ss(OperandXMMREG xmm, OperandMEM32 mem32)
	{
		if(emulateSSE)
		{
			spillMMX();
			const int i = xmm.reg;

			fild(mem32);
			fstp(dword_ptr [&sse[i][0]]);

			return 0;
		}
		
		return Optimizer::cvtsi2ss(xmm, mem32);
	}

	Encoding *Emulator::cvtsi2ss(OperandXMMREG xmm, OperandR_M32 r_m32)
	{
		if(r_m32.type == Operand::OPERAND_REG32) return cvtsi2ss(xmm, (OperandREG32)r_m32);
		else                                     return cvtsi2ss(xmm, (OperandMEM32)r_m32);
	}

	Encoding *Emulator::cvtss2si(OperandREG32 reg32, OperandXMMREG xmm)
	{
		if(emulateSSE)
		{
			spillMMX();
			const int i = xmm.reg;
		//	short fpuCW1;
		//	short fpuCW2;
			static int dword;

		//	fldcw(word_ptr [&fpuCW1]);
		//	fldcw(word_ptr [&fpuCW2]);
		//	and(word_ptr [&fpuCW2], (short)0xF3FF);
		//	fstcw(word_ptr [&fpuCW2]);

			fld(dword_ptr [&sse[i][0]]);
			fistp(dword_ptr [&dword]);
			mov(reg32, dword_ptr [&dword]);

		//	fstcw(word_ptr [&fpuCW1]);

			return 0;
		}
		
		return Optimizer::cvtss2si(reg32, xmm);
	}

	Encoding *Emulator::cvtss2si(OperandREG32 reg32, OperandMEM32 mem32)
	{
		if(emulateSSE)
		{
			spillMMX();
		//	short fpuCW1;
		//	short fpuCW2;
			static int dword;

		//	fldcw(word_ptr [&fpuCW1]);
		//	fldcw(word_ptr [&fpuCW2]);
		//	and(word_ptr [&fpuCW2], (short)0xF3FF);
		//	fstcw(word_ptr [&fpuCW2]);

			fld(mem32);
			fistp(dword_ptr [&dword]);
			mov(reg32, dword_ptr [&dword]);

		//	fstcw(word_ptr [&fpuCW1]);

			return 0;
		}
		
		return Optimizer::cvtss2si(reg32, mem32);
	}

	Encoding *Emulator::cvtss2si(OperandREG32 reg32, OperandXMM32 xmm32)
	{
		if(xmm32.type == Operand::OPERAND_XMMREG) return cvtss2si(reg32, (OperandXMMREG)xmm32);
		else                                      return cvtss2si(reg32, (OperandMEM32)xmm32);
	}

	Encoding *Emulator::cvttss2si(OperandREG32 reg32, OperandXMMREG xmm)
	{
		if(emulateSSE)
		{
			spillMMX();
			const int i = xmm.reg;
			static short fpuCW1;
			static short fpuCW2;
			static int dword;

			fstcw(word_ptr [&fpuCW1]);
			fstcw(word_ptr [&fpuCW2]);
			or(word_ptr [&fpuCW2], (unsigned short)0x0C00);
			fldcw(word_ptr [&fpuCW2]);

			fld(dword_ptr [&sse[i][0]]);
			fistp(dword_ptr [&dword]);
			mov(reg32, dword_ptr [&dword]);

			fldcw(word_ptr [&fpuCW1]);

			return 0;
		}
		
		return Optimizer::cvttss2si(reg32, xmm);
	}

	Encoding *Emulator::cvttss2si(OperandREG32 reg32, OperandMEM32 mem32)
	{
		if(emulateSSE)
		{
			spillMMX();
			static short fpuCW1;
			static short fpuCW2;
			static int dword;

			fstcw(word_ptr [&fpuCW1]);
			fstcw(word_ptr [&fpuCW2]);
			or(word_ptr [&fpuCW2], (unsigned short)0x0C00);
			fldcw(word_ptr [&fpuCW2]);

			fld(mem32);
			fistp(dword_ptr [&dword]);
			mov(reg32, dword_ptr [&dword]);

			fldcw(word_ptr [&fpuCW1]);

			return 0;
		}
		
		return Optimizer::cvttss2si(reg32, mem32);
	}

	Encoding *Emulator::cvttss2si(OperandREG32 reg32, OperandXMM32 xmm32)
	{
		if(xmm32.type == Operand::OPERAND_XMMREG) return cvttss2si(reg32, (OperandXMMREG)xmm32);
		else                                      return cvttss2si(reg32, (OperandMEM32)xmm32);
	}

	Encoding *Emulator::divps(OperandXMMREG xmmi, OperandXMMREG xmmj)
	{
		if(emulateSSE)
		{
			spillMMX();
			const int i = xmmi.reg;
			const int j = xmmj.reg;
			fld(dword_ptr [&sse[i][0]]);
			fdiv(dword_ptr [&sse[j][0]]);
			fstp(dword_ptr [&sse[i][0]]);
			fld(dword_ptr [&sse[i][1]]);
			fdiv(dword_ptr [&sse[j][1]]);
			fstp(dword_ptr [&sse[i][1]]);
			fld(dword_ptr [&sse[i][2]]);
			fdiv(dword_ptr [&sse[j][2]]);
			fstp(dword_ptr [&sse[i][2]]);
			fld(dword_ptr [&sse[i][3]]);
			fdiv(dword_ptr [&sse[j][3]]);
			fstp(dword_ptr [&sse[i][3]]);
			return 0;
		}
		
		return Optimizer::divps(xmmi, xmmj);
	}
	
	Encoding *Emulator::divps(OperandXMMREG xmm, OperandMEM128 mem128)
	{
		if(emulateSSE)
		{
			spillMMX();
			const int i = xmm.reg;
			fld(dword_ptr [&sse[i][0]]);
			fdiv((OperandMEM32)(mem128+0));
			fstp(dword_ptr [&sse[i][0]]);
			fld(dword_ptr [&sse[i][1]]);
			fdiv((OperandMEM32)(mem128+4));
			fstp(dword_ptr [&sse[i][1]]);
			fld(dword_ptr [&sse[i][2]]);
			fdiv((OperandMEM32)(mem128+8));
			fstp(dword_ptr [&sse[i][2]]);
			fld(dword_ptr [&sse[i][3]]);
			fdiv((OperandMEM32)(mem128+12));
			fstp(dword_ptr [&sse[i][3]]);
			return 0;
		}
		
		return Optimizer::divps(xmm, mem128);
	}
	
	Encoding *Emulator::divps(OperandXMMREG xmm, OperandR_M128 r_m128)
	{
		if(r_m128.type == Operand::OPERAND_XMMREG) return divps(xmm, (OperandXMMREG)r_m128);
		else                                       return divps(xmm, (OperandMEM128)r_m128);
	}

	Encoding *Emulator::divss(OperandXMMREG xmmi, OperandXMMREG xmmj)
	{
		if(emulateSSE)
		{
			spillMMX();
			const int i = xmmi.reg;
			const int j = xmmj.reg;
			fld(dword_ptr [&sse[i][0]]);
			fdiv(dword_ptr [&sse[j][0]]);
			fstp(dword_ptr [&sse[i][0]]);
			return 0;
		}
		
		return Optimizer::divss(xmmi, xmmj);
	}
	
	Encoding *Emulator::divss(OperandXMMREG xmm, OperandMEM32 mem32)
	{
		if(emulateSSE)
		{
			spillMMX();
			const int i = xmm.reg;
			fld(dword_ptr [&sse[i][0]]);
			fdiv((OperandMEM32)mem32);
			fstp(dword_ptr [&sse[i][0]]);
			return 0;
		}
		
		return Optimizer::divss(xmm, mem32);
	}
	
	Encoding *Emulator::divss(OperandXMMREG xmm, OperandXMM32 xmm32)
	{
		if(xmm32.type == Operand::OPERAND_XMMREG) return divss(xmm, (OperandXMMREG)xmm32);
		else                                      return divss(xmm, (OperandMEM32)xmm32);
	}

	Encoding *Emulator::ldmxcsr(OperandMEM32 mem32)
	{
		if(emulateSSE)
		{
			return 0;
		}
		
		return Optimizer::ldmxcsr(mem32);
	}

	Encoding *Emulator::maskmovq(OperandMMREG mmi, OperandMMREG mmj)
	{
		if(emulateSSE)
		{
			static short qword1[4];
			static short qword2[4];

			movq(qword_ptr [&qword1], mmi);
			movq(qword_ptr [&qword2], mmj);

			test(byte_ptr [&qword2+0], (char)0x80);
			mov(t8(0), byte_ptr [edi+0]);
			cmovnz(t32(0), dword_ptr [&qword1+0]);
			mov(byte_ptr [edi+0], t8(0));

			test(byte_ptr [&qword2+1], (char)0x80);
			mov(t8(0), byte_ptr [edi+1]);
			cmovnz(t32(0), dword_ptr [&qword1+1]);
			mov(byte_ptr [edi+1], t8(0));

			test(byte_ptr [&qword2+2], (char)0x80);
			mov(t8(0), byte_ptr [edi+2]);
			cmovnz(t32(0), dword_ptr [&qword1+2]);
			mov(byte_ptr [edi+2], t8(0));

			test(byte_ptr [&qword2+3], (char)0x80);
			mov(t8(0), byte_ptr [edi+3]);
			cmovnz(t32(0), dword_ptr [&qword1+3]);
			mov(byte_ptr [edi+3], t8(0));

			test(byte_ptr [&qword2+4], (char)0x80);
			mov(t8(0), byte_ptr [edi+4]);
			cmovnz(t32(0), dword_ptr [&qword1+4]);
			mov(byte_ptr [edi+4], t8(0));

			test(byte_ptr [&qword2+5], (char)0x80);
			mov(t8(0), byte_ptr [edi+5]);
			cmovnz(t32(0), dword_ptr [&qword1+5]);
			mov(byte_ptr [edi+5], t8(0));

			test(byte_ptr [&qword2+6], (char)0x80);
			mov(t8(0), byte_ptr [edi+6]);
			cmovnz(t32(0), dword_ptr [&qword1+6]);
			mov(byte_ptr [edi+6], t8(0));

			test(byte_ptr [&qword2+7], (char)0x80);
			mov(t8(0), byte_ptr [edi+7]);
			cmovnz(t32(0), dword_ptr [&qword1+7]);
			mov(byte_ptr [edi+7], t8(0));

			free((OperandREF)0);
			return 0;
		}
		
		return Optimizer::maskmovq(mmi, mmj);
	}

	Encoding *Emulator::maxps(OperandXMMREG xmmi, OperandXMMREG xmmj)
	{
		if(emulateSSE)
		{
			spillMMX();
			const int i = xmmi.reg;
			const int j = xmmj.reg;

			fld(dword_ptr [&sse[j][0]]);
			fld(dword_ptr [&sse[i][0]]);
			fcomi(st0, st1);
			fcmovb(st1);
			fstp(dword_ptr [&sse[i][0]]);
			ffree(st0);

			fld(dword_ptr [&sse[j][1]]);
			fld(dword_ptr [&sse[i][1]]);
			fcomi(st0, st1);
			fcmovb(st1);
			fstp(dword_ptr [&sse[i][1]]);
			ffree(st0);

			fld(dword_ptr [&sse[j][2]]);
			fld(dword_ptr [&sse[i][2]]);
			fcomi(st0, st1);
			fcmovb(st1);
			fstp(dword_ptr [&sse[i][2]]);
			ffree(st0);

			fld(dword_ptr [&sse[j][3]]);
			fld(dword_ptr [&sse[i][3]]);
			fcomi(st0, st1);
			fcmovb(st1);
			fstp(dword_ptr [&sse[i][3]]);
			ffree(st0);

			return 0;
		}
		
		return Optimizer::maxps(xmmi, xmmj);
	}

	Encoding *Emulator::maxps(OperandXMMREG xmm, OperandMEM128 mem128)
	{
		if(emulateSSE)
		{
			spillMMX();
			const int i = xmm.reg;

			fld((OperandMEM32)(mem128+0));
			fld(dword_ptr [&sse[i][0]]);
			fcomi(st0, st1);
			fcmovb(st1);
			fstp(dword_ptr [&sse[i][0]]);
			ffree(st0);

			fld((OperandMEM32)(mem128+4));
			fld(dword_ptr [&sse[i][1]]);
			fcomi(st0, st1);
			fcmovb(st1);
			fstp(dword_ptr [&sse[i][1]]);
			ffree(st0);

			fld((OperandMEM32)(mem128+8));
			fld(dword_ptr [&sse[i][2]]);
			fcomi(st0, st1);
			fcmovb(st1);
			fstp(dword_ptr [&sse[i][2]]);
			ffree(st0);

			fld((OperandMEM32)(mem128+0));
			fld(dword_ptr [&sse[i][3]]);
			fcomi(st0, st1);
			fcmovb(st1);
			fstp(dword_ptr [&sse[i][3]]);
			ffree(st0);

			return 0;
		}
		
		return Optimizer::maxps(xmm, mem128);
	}

	Encoding *Emulator::maxps(OperandXMMREG xmm, OperandR_M128 r_m128)
	{
		if(r_m128.type == Operand::OPERAND_XMMREG) return maxps(xmm, (OperandXMMREG)r_m128);
		else                                       return maxps(xmm, (OperandMEM128)r_m128);
	}

	Encoding *Emulator::maxss(OperandXMMREG xmmi, OperandXMMREG xmmj)
	{
		if(emulateSSE)
		{
			spillMMX();
			const int i = xmmi.reg;
			const int j = xmmj.reg;

			fld(dword_ptr [&sse[j][0]]);
			fld(dword_ptr [&sse[i][0]]);
			fcomi(st0, st1);
			fcmovb(st1);
			fstp(dword_ptr [&sse[i][0]]);
			ffree(st0);

			return 0;
		}
		
		return Optimizer::maxss(xmmi, xmmj);
	}

	Encoding *Emulator::maxss(OperandXMMREG xmm, OperandMEM32 mem32)
	{
		if(emulateSSE)
		{
			spillMMX();
			const int i = xmm.reg;

			fld(mem32);
			fld(dword_ptr [&sse[i][0]]);
			fcomi(st0, st1);
			fcmovb(st1);
			fstp(dword_ptr [&sse[i][0]]);
			ffree(st0);

			return 0;
		}
		
		return Optimizer::maxss(xmm, mem32);
	}

	Encoding *Emulator::maxss(OperandXMMREG xmm, OperandXMM32 xmm32)
	{
		if(xmm32.type == Operand::OPERAND_XMMREG) return maxss(xmm, (OperandXMMREG)xmm32);
		else                                      return maxss(xmm, (OperandMEM32)xmm32);
	}

	Encoding *Emulator::minps(OperandXMMREG xmmi, OperandXMMREG xmmj)
	{
		if(emulateSSE)
		{
			spillMMX();
			const int i = xmmi.reg;
			const int j = xmmj.reg;

			fld(dword_ptr [&sse[j][0]]);
			fld(dword_ptr [&sse[i][0]]);
			fcomi(st0, st1);
			fcmovnb(st1);
			fstp(dword_ptr [&sse[i][0]]);
			ffree(st0);

			fld(dword_ptr [&sse[j][1]]);
			fld(dword_ptr [&sse[i][1]]);
			fcomi(st0, st1);
			fcmovnb(st1);
			fstp(dword_ptr [&sse[i][1]]);
			ffree(st0);

			fld(dword_ptr [&sse[j][2]]);
			fld(dword_ptr [&sse[i][2]]);
			fcomi(st0, st1);
			fcmovnb(st1);
			fstp(dword_ptr [&sse[i][2]]);
			ffree(st0);

			fld(dword_ptr [&sse[j][3]]);
			fld(dword_ptr [&sse[i][3]]);
			fcomi(st0, st1);
			fcmovnb(st1);
			fstp(dword_ptr [&sse[i][3]]);
			ffree(st0);

			return 0;
		}
		
		return Optimizer::minps(xmmi, xmmj);
	}

	Encoding *Emulator::minps(OperandXMMREG xmm, OperandMEM128 mem128)
	{
		if(emulateSSE)
		{
			spillMMX();
			const int i = xmm.reg;

			fld((OperandMEM32)(mem128+0));
			fld(dword_ptr [&sse[i][0]]);
			fcomi(st0, st1);
			fcmovnb(st1);
			fstp(dword_ptr [&sse[i][0]]);
			ffree(st0);

			fld((OperandMEM32)(mem128+4));
			fld(dword_ptr [&sse[i][1]]);
			fcomi(st0, st1);
			fcmovnb(st1);
			fstp(dword_ptr [&sse[i][1]]);
			ffree(st0);

			fld((OperandMEM32)(mem128+8));
			fld(dword_ptr [&sse[i][2]]);
			fcomi(st0, st1);
			fcmovnb(st1);
			fstp(dword_ptr [&sse[i][2]]);
			ffree(st0);

			fld((OperandMEM32)(mem128+0));
			fld(dword_ptr [&sse[i][3]]);
			fcomi(st0, st1);
			fcmovnb(st1);
			fstp(dword_ptr [&sse[i][3]]);
			ffree(st0);

			return 0;
		}
		
		return Optimizer::minps(xmm, mem128);
	}

	Encoding *Emulator::minps(OperandXMMREG xmm, OperandR_M128 r_m128)
	{
		if(r_m128.type == Operand::OPERAND_XMMREG) return minps(xmm, (OperandXMMREG)r_m128);
		else                                       return minps(xmm, (OperandMEM128)r_m128);
	}

	Encoding *Emulator::minss(OperandXMMREG xmmi, OperandXMMREG xmmj)
	{
		if(emulateSSE)
		{
			spillMMX();
			const int i = xmmi.reg;
			const int j = xmmj.reg;

			fld(dword_ptr [&sse[j][0]]);
			fld(dword_ptr [&sse[i][0]]);
			fcomi(st0, st1);
			fcmovnb(st1);
			fstp(dword_ptr [&sse[i][0]]);
			ffree(st0);

			return 0;
		}
		
		return Optimizer::minss(xmmi, xmmj);
	}

	Encoding *Emulator::minss(OperandXMMREG xmm, OperandMEM32 mem32)
	{
		if(emulateSSE)
		{
			spillMMX();
			const int i = xmm.reg;

			fld(mem32);
			fld(dword_ptr [&sse[i][0]]);
			fucomi(st0, st1);
			fcmovnb(st1);
			fstp(dword_ptr [&sse[i][0]]);
			ffree(st0);

			return 0;
		}
		
		return Optimizer::minss(xmm, mem32);
	}

	Encoding *Emulator::minss(OperandXMMREG xmm, OperandXMM32 xmm32)
	{
		if(xmm32.type == Operand::OPERAND_XMMREG) return minss(xmm, (OperandXMMREG)xmm32);
		else                                      return minss(xmm, (OperandMEM32)xmm32);
	}

	Encoding *Emulator::movaps(OperandXMMREG xmmi, OperandXMMREG xmmj)
	{
		if(emulateSSE)
		{
			const int i = xmmi.reg;
			const int j = xmmj.reg;

			mov(t32(0), dword_ptr [&sse[j][0]]);
			mov(dword_ptr [&sse[i][0]], t32(0));

			mov(t32(0), dword_ptr [&sse[j][1]]);
			mov(dword_ptr [&sse[i][1]], t32(0));

			mov(t32(0), dword_ptr [&sse[j][2]]);
			mov(dword_ptr [&sse[i][2]], t32(0));

			mov(t32(0), dword_ptr [&sse[j][3]]);
			mov(dword_ptr [&sse[i][3]], t32(0));

			free((OperandREF)0);
			return 0;
		}

		return Optimizer::movaps(xmmi, xmmj);
	}

	Encoding *Emulator::movaps(OperandXMMREG xmm, OperandMEM128 m128)
	{
		if(emulateSSE)
		{
			const int i = xmm.reg;

			mov(t32(0), (OperandMEM32)(m128+0));
			mov(dword_ptr [&sse[i][0]], t32(0));

			mov(t32(0), (OperandMEM32)(m128+4));
			mov(dword_ptr [&sse[i][1]], t32(0));

			mov(t32(0), (OperandMEM32)(m128+8));
			mov(dword_ptr [&sse[i][2]], t32(0));

			mov(t32(0), (OperandMEM32)(m128+12));
			mov(dword_ptr [&sse[i][3]], t32(0));

			free((OperandREF)0);
			return 0;
		}
		
		return Optimizer::movaps(xmm, m128);
	}

	Encoding *Emulator::movaps(OperandXMMREG xmm, OperandR_M128 r_m128)
	{
		if(r_m128.type == Operand::OPERAND_XMMREG) return movaps(xmm, (OperandXMMREG)r_m128);
		else                                       return movaps(xmm, (OperandMEM128)r_m128);
	}

	Encoding *Emulator::movaps(OperandMEM128 m128, OperandXMMREG xmm)
	{
		if(emulateSSE)
		{
			const int i = xmm.reg;

			mov(t32(0), dword_ptr [&sse[i][0]]);
			mov((OperandMEM32)(m128+0), t32(0));

			mov(t32(0), dword_ptr [&sse[i][1]]);
			mov((OperandMEM32)(m128+4), t32(0));

			mov(t32(0), dword_ptr [&sse[i][2]]);
			mov((OperandMEM32)(m128+8), t32(0));

			mov(t32(0), dword_ptr [&sse[i][3]]);
			mov((OperandMEM32)(m128+12), t32(0));

			free((OperandREF)0);
			return 0;
		}
		
		return Optimizer::movaps(m128, xmm);
	}

	Encoding *Emulator::movaps(OperandR_M128 r_m128, OperandXMMREG xmm)
	{
		if(r_m128.type == Operand::OPERAND_XMMREG) return movaps((OperandXMMREG)r_m128, xmm);
		else                                       return movaps((OperandMEM128)r_m128, xmm);
	}

	Encoding *Emulator::movhlps(OperandXMMREG xmmi, OperandXMMREG xmmj)
	{
		if(emulateSSE)
		{
			const int i = xmmi.reg;
			const int j = xmmj.reg;

			mov(t32(0), dword_ptr [&sse[j][2]]);
			mov(dword_ptr [&sse[i][0]], t32(0));

			mov(t32(0), dword_ptr [&sse[j][3]]);
			mov(dword_ptr [&sse[i][1]], t32(0));

			free((OperandREF)0);
			return 0;
		}
		
		return Optimizer::movhlps(xmmi, xmmj);
	}

	Encoding *Emulator::movhps(OperandXMMREG xmm, OperandMEM64 m64)
	{
		if(emulateSSE)
		{
			const int i = xmm.reg;

			mov(t32(0), (OperandMEM32)(m64+0));
			mov(dword_ptr [&sse[i][2]], t32(0));

			mov(t32(0), (OperandMEM32)(m64+4));
			mov(dword_ptr [&sse[i][3]], t32(0));

			free((OperandREF)0);
			return 0;
		}
		
		return Optimizer::movhps(xmm, m64);
	}

	Encoding *Emulator::movhps(OperandMEM64 m64, OperandXMMREG xmm)
	{
		if(emulateSSE)
		{
			const int i = xmm.reg;

			mov(t32(0), dword_ptr [&sse[i][2]]);
			mov((OperandMEM32)(m64+0), t32(0));

			mov(t32(0), dword_ptr [&sse[i][3]]);
			mov((OperandMEM32)(m64+4), t32(0));

			free((OperandREF)0);
			return 0;
		}

		return Optimizer::movhps(m64, xmm);
	}

	Encoding *Emulator::movhps(OperandXMMREG xmmi, OperandXMMREG xmmj)
	{
		if(emulateSSE)
		{
			const int i = xmmi.reg;
			const int j = xmmj.reg;

			mov(t32(0), dword_ptr [&sse[j][2]]);
			mov(dword_ptr [&sse[i][2]], t32(0));

			mov(t32(0), dword_ptr [&sse[j][3]]);
			mov(dword_ptr [&sse[i][3]], t32(0));

			free((OperandREF)0);
			return 0;
		}
		
		return Optimizer::movhps(xmmi, xmmj);
	}

	Encoding *Emulator::movlhps(OperandXMMREG xmmi, OperandXMMREG xmmj)
	{
		if(emulateSSE)
		{
			const int i = xmmi.reg;
			const int j = xmmj.reg;

			mov(t32(0), dword_ptr [&sse[j][0]]);
			mov(dword_ptr [&sse[i][2]], t32(0));

			mov(t32(0), dword_ptr [&sse[j][1]]);
			mov(dword_ptr [&sse[i][3]], t32(0));

			free((OperandREF)0);
			return 0;
		}
		
		return Optimizer::movlhps(xmmi, xmmj);
	}

	Encoding *Emulator::movlps(OperandXMMREG xmm, OperandMEM64 m64)
	{
		if(emulateSSE)
		{
			const int i = xmm.reg;

			mov(t32(0), (OperandMEM32)(m64+0));
			mov(dword_ptr [&sse[i][0]], t32(0));

			mov(t32(0), (OperandMEM32)(m64+4));
			mov(dword_ptr [&sse[i][1]], t32(0));

			free((OperandREF)0);
			return 0;
		}
			
		return Optimizer::movlps(xmm, m64);
	}

	Encoding *Emulator::movlps(OperandMEM64 m64, OperandXMMREG xmm)
	{
		if(emulateSSE)
		{
			const int i = xmm.reg;

			mov(t32(0), dword_ptr [&sse[i][0]]);
			mov((OperandMEM32)(m64+0), t32(0));

			mov(t32(0), dword_ptr [&sse[i][1]]);
			mov((OperandMEM32)(m64+4), t32(0));

			free((OperandREF)0);
			return 0;
		}
		
		return Optimizer::movlps(m64, xmm);
	}

	Encoding *Emulator::movmskps(OperandREG32 reg32, OperandXMMREG xmm)
	{
		if(emulateSSE)
		{
			const int i = xmm.reg;

			mov(t32(0), dword_ptr [&sse[i][0]]);
			shr(t32(0), 31);
			mov(reg32, t32(0));

			mov(t32(0), dword_ptr [&sse[i][1]]);
			shr(t32(0), 31);
			shl(t32(0), 1);
			or(reg32, t32(0));

			mov(t32(0), dword_ptr [&sse[i][2]]);
			shr(t32(0), 31);
			shl(t32(0), 2);
			or(reg32, t32(0));

			mov(t32(0), dword_ptr [&sse[i][3]]);
			shr(t32(0), 31);
			shl(t32(0), 3);
			or(reg32, t32(0));

			free((OperandREF)0);
			return 0;
		}

		return Optimizer::movmskps(reg32, xmm);
	}

	Encoding *Emulator::movntps(OperandMEM128 m128, OperandXMMREG xmm)
	{
		if(emulateSSE)
		{
			const int i = xmm.reg;

			mov(t32(0), dword_ptr [&sse[i][0]]);
			mov((OperandMEM32)(m128+0), t32(0));

			mov(t32(0), dword_ptr [&sse[i][1]]);
			mov((OperandMEM32)(m128+4), t32(0));

			mov(t32(0), dword_ptr [&sse[i][2]]);
			mov((OperandMEM32)(m128+8), t32(0));

			mov(t32(0), dword_ptr [&sse[i][3]]);
			mov((OperandMEM32)(m128+12), t32(0));

			free((OperandREF)0);
			return 0;
		}
		
		return Optimizer::movntps(m128, xmm);
	}

	Encoding *Emulator::movntq(OperandMEM64 m64, OperandMMREG xmm)
	{
		if(emulateSSE)
		{
			const int i = xmm.reg;

			mov(t32(0), dword_ptr [&sse[i][0]]);
			mov((OperandMEM32)(m64+0), t32(0));

			mov(t32(0), dword_ptr [&sse[i][1]]);
			mov((OperandMEM32)(m64+4), t32(0));

			free((OperandREF)0);
			return 0;
		}

		return Optimizer::movntq(m64, xmm);
	}

	Encoding *Emulator::movss(OperandXMMREG xmmi, OperandXMMREG xmmj)
	{
		if(emulateSSE)
		{
			const int i = xmmi.reg;
			const int j = xmmj.reg;

			mov(t32(0), dword_ptr [&sse[j][0]]);
			mov(dword_ptr [&sse[i][0]], t32(0));

			free((OperandREF)0);
			return 0;
		}
		
		return Optimizer::movss(xmmi, xmmj);
	}

	Encoding *Emulator::movss(OperandXMMREG xmm, OperandMEM32 m32)
	{
		if(emulateSSE)
		{
			const int i = xmm.reg;

			mov(t32(0), m32);
			mov(dword_ptr [&sse[i][0]], t32(0));

			mov(dword_ptr [&sse[i][1]], 0);
			mov(dword_ptr [&sse[i][2]], 0);
			mov(dword_ptr [&sse[i][3]], 0);

			free((OperandREF)0);
			return 0;
		}
		
		return Optimizer::movss(xmm, m32);
	}

	Encoding *Emulator::movss(OperandXMMREG xmm, OperandXMM32 xmm32)
	{
		if(xmm32.type == Operand::OPERAND_XMMREG) return movss(xmm, (OperandXMMREG)xmm32);
		else                                      return movss(xmm, (OperandMEM32)xmm32);
	}

	Encoding *Emulator::movss(OperandMEM32 m32, OperandXMMREG xmm)
	{
		if(emulateSSE)
		{
			const int i = xmm.reg;

			mov(t32(0), dword_ptr [&sse[i][0]]);
			mov(m32, t32(0));

			free((OperandREF)0);
			return 0;
		}

		return Optimizer::movss(m32, xmm);
	}

	Encoding *Emulator::movss(OperandXMM32 xmm32, OperandXMMREG xmm)
	{
		if(xmm32.type == Operand::OPERAND_XMMREG) return movss((OperandXMMREG)xmm32, xmm);
		else                                      return movss((OperandMEM32)xmm32, xmm);
	}

	Encoding *Emulator::movups(OperandXMMREG xmmi, OperandXMMREG xmmj)
	{
		if(emulateSSE)
		{
			const int i = xmmi.reg;
			const int j = xmmj.reg;

			mov(t32(0), dword_ptr [&sse[j][0]]);
			mov(dword_ptr [&sse[i][0]], t32(0));

			mov(t32(0), dword_ptr [&sse[j][1]]);
			mov(dword_ptr [&sse[i][1]], t32(0));

			mov(t32(0), dword_ptr [&sse[j][2]]);
			mov(dword_ptr [&sse[i][2]], t32(0));

			mov(t32(0), dword_ptr [&sse[j][3]]);
			mov(dword_ptr [&sse[i][3]], t32(0));

			free((OperandREF)0);
			return 0;
		}

		return Optimizer::movups(xmmi, xmmj);
	}

	Encoding *Emulator::movups(OperandXMMREG xmm, OperandMEM128 m128)
	{
		if(emulateSSE)
		{
			const int i = xmm.reg;

			mov(t32(0), (OperandMEM32)(m128+0));
			mov(dword_ptr [&sse[i][0]], t32(0));

			mov(t32(0), (OperandMEM32)(m128+4));
			mov(dword_ptr [&sse[i][1]], t32(0));

			mov(t32(0), (OperandMEM32)(m128+8));
			mov(dword_ptr [&sse[i][2]], t32(0));

			mov(t32(0), (OperandMEM32)(m128+12));
			mov(dword_ptr [&sse[i][3]], t32(0));

			free((OperandREF)0);
			return 0;
		}
		
		return Optimizer::movups(xmm, m128);
	}

	Encoding *Emulator::movups(OperandXMMREG xmm, OperandR_M128 r_m128)
	{
		if(emulateSSE)
		{
			return movaps(xmm, r_m128);
		}
		else
		{
			return Optimizer::movups(xmm, r_m128);
		}
	}

	Encoding *Emulator::movups(OperandMEM128 m128, OperandXMMREG xmm)
	{
		if(emulateSSE)
		{
			return movaps(m128, xmm);
		}
		else
		{
			return Optimizer::movups(m128, xmm);
		}
	}

	Encoding *Emulator::movups(OperandR_M128 r_m128, OperandXMMREG xmm)
	{
		if(emulateSSE)
		{
			return movaps(r_m128, xmm);
		}
		else
		{
			return Optimizer::movups(r_m128, xmm);
		}
	}

	Encoding *Emulator::mulps(OperandXMMREG xmmi, OperandXMMREG xmmj)
	{
		if(emulateSSE)
		{
			spillMMX();
			const int i = xmmi.reg;
			const int j = xmmj.reg;
			fld(dword_ptr [&sse[i][0]]);
			fmul(dword_ptr [&sse[j][0]]);
			fstp(dword_ptr [&sse[i][0]]);
			fld(dword_ptr [&sse[i][1]]);
			fmul(dword_ptr [&sse[j][1]]);
			fstp(dword_ptr [&sse[i][1]]);
			fld(dword_ptr [&sse[i][2]]);
			fmul(dword_ptr [&sse[j][2]]);
			fstp(dword_ptr [&sse[i][2]]);
			fld(dword_ptr [&sse[i][3]]);
			fmul(dword_ptr [&sse[j][3]]);
			fstp(dword_ptr [&sse[i][3]]);
			return 0;
		}

		return Optimizer::mulps(xmmi, xmmj);
	}
	
	Encoding *Emulator::mulps(OperandXMMREG xmm, OperandMEM128 mem128)
	{
		if(emulateSSE)
		{
			spillMMX();
			const int i = xmm.reg;
			fld(dword_ptr [&sse[i][0]]);
			fmul((OperandMEM32)(mem128+0));
			fstp(dword_ptr [&sse[i][0]]);
			fld(dword_ptr [&sse[i][1]]);
			fmul((OperandMEM32)(mem128+4));
			fstp(dword_ptr [&sse[i][1]]);
			fld(dword_ptr [&sse[i][2]]);
			fmul((OperandMEM32)(mem128+8));
			fstp(dword_ptr [&sse[i][2]]);
			fld(dword_ptr [&sse[i][3]]);
			fmul((OperandMEM32)(mem128+12));
			fstp(dword_ptr [&sse[i][3]]);
			return 0;
		}
		
		return Optimizer::mulps(xmm, mem128);
	}
	
	Encoding *Emulator::mulps(OperandXMMREG xmm, OperandR_M128 r_m128)
	{
		if(r_m128.type == Operand::OPERAND_XMMREG) return mulps(xmm, (OperandXMMREG)r_m128);
		else                                       return mulps(xmm, (OperandMEM128)r_m128);
	}

	Encoding *Emulator::mulss(OperandXMMREG xmmi, OperandXMMREG xmmj)
	{
		if(emulateSSE)
		{
			spillMMX();
			const int i = xmmi.reg;
			const int j = xmmj.reg;
			fld(dword_ptr [&sse[i][0]]);
			fmul(dword_ptr [&sse[j][0]]);
			fstp(dword_ptr [&sse[i][0]]);
			return 0;
		}

		return Optimizer::mulss(xmmi, xmmj);
	}
	
	Encoding *Emulator::mulss(OperandXMMREG xmm, OperandMEM32 mem32)
	{
		if(emulateSSE)
		{
			spillMMX();
			const int i = xmm.reg;
			fld(dword_ptr [&sse[i][0]]);
			fmul((OperandMEM32)mem32);
			fstp(dword_ptr [&sse[i][0]]);
			return 0;
		}
		
		return Optimizer::mulss(xmm, mem32);
	}
	
	Encoding *Emulator::mulss(OperandXMMREG xmm, OperandXMM32 xmm32)
	{
		if(xmm32.type == Operand::OPERAND_XMMREG) return mulss(xmm, (OperandXMMREG)xmm32);
		else                                      return mulss(xmm, (OperandMEM32)xmm32);
	}

	Encoding *Emulator::orps(OperandXMMREG xmmi, OperandXMMREG xmmj)
	{
		if(emulateSSE)
		{
			const int i = xmmi.reg;
			const int j = xmmj.reg;

			mov(t32(0), dword_ptr [&sse[j][0]]);
			or(dword_ptr [&sse[i][0]], t32(0));

			mov(t32(0), dword_ptr [&sse[j][1]]);
			or(dword_ptr [&sse[i][1]], t32(0));

			mov(t32(0), dword_ptr [&sse[j][2]]);
			or(dword_ptr [&sse[i][2]], t32(0));

			mov(t32(0), dword_ptr [&sse[j][3]]);
			or(dword_ptr [&sse[i][3]], t32(0));

			free((OperandREF)0);
			return 0;
		}
		
		return Optimizer::orps(xmmi, xmmj);
	}
	
	Encoding *Emulator::orps(OperandXMMREG xmm, OperandMEM128 mem128)
	{
		if(emulateSSE)
		{
			const int i = xmm.reg;

			mov(t32(0), (OperandMEM32)(mem128+0));
			or(dword_ptr [&sse[i][0]], t32(0));

			mov(t32(0), (OperandMEM32)(mem128+4));
			or(dword_ptr [&sse[i][1]], t32(0));

			mov(t32(0), (OperandMEM32)(mem128+8));
			or(dword_ptr [&sse[i][2]], t32(0));

			mov(t32(0), (OperandMEM32)(mem128+12));
			or(dword_ptr [&sse[i][3]], t32(0));

			free((OperandREF)0);
			return 0;
		}
		
		return Optimizer::orps(xmm, mem128);
	}
	
	Encoding *Emulator::orps(OperandXMMREG xmm, OperandR_M128 r_m128)
	{
		if(r_m128.type == Operand::OPERAND_XMMREG) return orps(xmm, (OperandXMMREG)r_m128);
		else                                       return orps(xmm, (OperandMEM128)r_m128);
	}

	Encoding *Emulator::pavgb(OperandMMREG mmi, OperandMMREG mmj)
	{
		if(emulateSSE)
		{
			static unsigned char byte1[8];
			static unsigned char byte2[8];

			movq(qword_ptr [byte1], mmi);
			movq(qword_ptr [byte2], mmj);

			movzx(t32(0), byte_ptr [&byte1[0]]);
			movzx(t32(1), byte_ptr [&byte2[0]]);
			add(t32(0), t32(1));
			shr(t32(0), 1);
			mov(byte_ptr [&byte1[0]], t8(0));

			movzx(t32(0), byte_ptr [&byte1[1]]);
			movzx(t32(1), byte_ptr [&byte2[1]]);
			add(t32(0), t32(1));
			shr(t32(0), 1);
			mov(byte_ptr [&byte1[1]], t8(0));

			movzx(t32(0), byte_ptr [&byte1[2]]);
			movzx(t32(1), byte_ptr [&byte2[2]]);
			add(t32(0), t32(1));
			shr(t32(0), 1);
			mov(byte_ptr [&byte1[2]], t8(0));

			movzx(t32(0), byte_ptr [&byte1[3]]);
			movzx(t32(1), byte_ptr [&byte2[3]]);
			add(t32(0), t32(1));
			shr(t32(0), 1);
			mov(byte_ptr [&byte1[3]], t8(0));

			movzx(t32(0), byte_ptr [&byte1[4]]);
			movzx(t32(1), byte_ptr [&byte2[4]]);
			add(t32(0), t32(1));
			shr(t32(0), 1);
			mov(byte_ptr [&byte1[4]], t8(0));

			movzx(t32(0), byte_ptr [&byte1[5]]);
			movzx(t32(1), byte_ptr [&byte2[5]]);
			add(t32(0), t32(1));
			shr(t32(0), 1);
			mov(byte_ptr [&byte1[5]], t8(0));

			movzx(t32(0), byte_ptr [&byte1[6]]);
			movzx(t32(1), byte_ptr [&byte2[6]]);
			add(t32(0), t32(1));
			shr(t32(0), 1);
			mov(byte_ptr [&byte1[6]], t8(0));

			movzx(t32(0), byte_ptr [&byte1[7]]);
			movzx(t32(1), byte_ptr [&byte2[7]]);
			add(t32(0), t32(1));
			shr(t32(0), 1);
			mov(byte_ptr [&byte1[7]], t8(0));

			movq(mmi, qword_ptr [byte1]);

			free((OperandREF)0);
			free((OperandREF)1);
			return 0;
		}
		
		return Optimizer::pavgb(mmi, mmj);
	}

	Encoding *Emulator::pavgb(OperandMMREG mm, OperandMEM64 m64)
	{
		if(emulateSSE)
		{
			static unsigned char byte1[8];

			movq(qword_ptr [byte1], mm);

			static int t1;

			movzx(t32(0), byte_ptr [&byte1[0]]);
			movzx(t32(1), (OperandMEM8)(m64+0));
			add(t32(0), t32(1));
			shr(t32(0), 1);
			mov(byte_ptr [&byte1[0]], t8(0));

			movzx(t32(0), byte_ptr [&byte1[1]]);
			movzx(t32(1), (OperandMEM8)(m64+1));
			add(t32(0), t32(1));
			shr(t32(0), 1);
			mov(byte_ptr [&byte1[1]], t8(0));

			movzx(t32(0), byte_ptr [&byte1[2]]);
			movzx(t32(1), (OperandMEM8)(m64+2));
			add(t32(0), t32(1));
			shr(t32(0), 1);
			mov(byte_ptr [&byte1[2]], t8(0));

			movzx(t32(0), byte_ptr [&byte1[3]]);
			movzx(t32(1), (OperandMEM8)(m64+3));
			add(t32(0), t32(1));
			shr(t32(0), 1);
			mov(byte_ptr [&byte1[3]], t8(0));

			movzx(t32(0), byte_ptr [&byte1[4]]);
			movzx(t32(1), (OperandMEM8)(m64+4));
			add(t32(0), t32(1));
			shr(t32(0), 1);
			mov(byte_ptr [&byte1[4]], t8(0));

			movzx(t32(0), byte_ptr [&byte1[5]]);
			movzx(t32(1), (OperandMEM8)(m64+5));
			add(t32(0), t32(1));
			shr(t32(0), 1);
			mov(byte_ptr [&byte1[5]], t8(0));

			movzx(t32(0), byte_ptr [&byte1[6]]);
			movzx(t32(1), (OperandMEM8)(m64+6));
			add(t32(0), t32(1));
			shr(t32(0), 1);
			mov(byte_ptr [&byte1[6]], t8(0));

			movzx(t32(0), byte_ptr [&byte1[7]]);
			movzx(t32(1), (OperandMEM8)(m64+7));
			add(t32(0), t32(1));
			shr(t32(0), 1);
			mov(byte_ptr [&byte1[7]], t8(0));

			movq(mm, qword_ptr [byte1]);

			free((OperandREF)0);
			free((OperandREF)1);
			return 0;
		}
		
		return Optimizer::pavgb(mm, m64);
	}

	Encoding *Emulator::pavgb(OperandMMREG mm, OperandMM64 r_m64)
	{
		if(r_m64.type == Operand::OPERAND_MMREG) return pavgb(mm, (OperandMMREG)r_m64);
		else                                     return pavgb(mm, (OperandMEM64)r_m64);
	}

	Encoding *Emulator::pavgw(OperandMMREG mmi, OperandMMREG mmj)
	{
		if(emulateSSE)
		{
			static unsigned short word1[4];
			static unsigned short word2[4];

			movq(qword_ptr [word1], mmi);
			movq(qword_ptr [word2], mmj);

			movzx(t32(0), word_ptr [&word1[0]]);
			movzx(t32(1), word_ptr [&word2[0]]);
			add(t32(0), t32(1));
			shr(t32(0), 1);
			mov(word_ptr [&word1[0]], t16(0));

			movzx(t32(0), word_ptr [&word1[1]]);
			movzx(t32(1), word_ptr [&word2[1]]);
			add(t32(0), t32(1));
			shr(t32(0), 1);
			mov(word_ptr [&word1[1]], t16(0));

			movzx(t32(0), word_ptr [&word1[2]]);
			movzx(t32(1), word_ptr [&word2[2]]);
			add(t32(0), t32(1));
			shr(t32(0), 1);
			mov(word_ptr [&word1[2]], t16(0));

			movzx(t32(0), word_ptr [&word1[3]]);
			movzx(t32(1), word_ptr [&word2[3]]);
			add(t32(0), t32(1));
			shr(t32(0), 1);
			mov(word_ptr [&word1[3]], t16(0));

			movq(mmi, qword_ptr [word1]);

			free((OperandREF)0);
			free((OperandREF)1);
			return 0;
		}
		
		return Optimizer::pavgw(mmi, mmj);
	}

	Encoding *Emulator::pavgw(OperandMMREG mm, OperandMEM64 m64)
	{
		if(emulateSSE)
		{
			static unsigned char word1[8];

			movq(qword_ptr [word1], mm);

			movzx(t32(0), word_ptr [&word1[0]]);
			movzx(t32(1), (OperandMEM16)(m64+0));
			add(t32(0), t32(1));
			shr(t32(0), 1);
			mov(word_ptr [&word1[0]], t16(0));

			movzx(t32(0), word_ptr [&word1[1]]);
			movzx(t32(1), (OperandMEM16)(m64+2));
			add(t32(0), t32(1));
			shr(t32(0), 1);
			mov(word_ptr [&word1[1]], t16(0));

			movzx(t32(0), word_ptr [&word1[2]]);
			movzx(t32(1), (OperandMEM16)(m64+4));
			add(t32(0), t32(1));
			shr(t32(0), 1);
			mov(word_ptr [&word1[2]], t16(0));

			movzx(t32(0), word_ptr [&word1[3]]);
			movzx(t32(1), (OperandMEM16)(m64+6));
			add(t32(0), t32(1));
			shr(t32(0), 1);
			mov(word_ptr [&word1[3]], t16(0));

			movq(mm, qword_ptr [word1]);

			free((OperandREF)0);
			free((OperandREF)1);
			return 0;
		}
		
		return Optimizer::pavgw(mm, m64);
	}

	Encoding *Emulator::pavgw(OperandMMREG mm, OperandMM64 r_m64)
	{
		if(r_m64.type == Operand::OPERAND_MMREG) return pavgw(mm, (OperandMMREG)r_m64);
		else                                     return pavgw(mm, (OperandMEM64)r_m64);
	}

	Encoding *Emulator::pextrw(OperandREG32 r32, OperandMMREG mm, unsigned char c)
	{
		if(emulateSSE)
		{
			static short word[4];

			movq(qword_ptr [word], mm);
			xor(r32, r32);
			mov((OperandREG16)r32, word_ptr [&word[c & 0x03]]);

			return 0;
		}
		
		return Optimizer::pextrw(r32, mm, c);
	}

	Encoding *Emulator::pinsrw(OperandMMREG mm, OperandREG16 r16, unsigned char c)
	{
		if(emulateSSE)
		{
			static short word[4];

			movq(qword_ptr [word], mm);
			mov(word_ptr [&word[c & 0x03]], r16);
			movq(mm, qword_ptr [word]);

			return 0;
		}
		
		return Optimizer::pinsrw(mm, r16, c);
	}

	Encoding *Emulator::pinsrw(OperandMMREG mm, OperandMEM16 m16, unsigned char c)
	{
		if(emulateSSE)
		{
			static short word[4];

			movq(qword_ptr [word], mm);
			mov(t16(0), m16);
			mov(word_ptr [&word[c & 0x03]], t16(0));
			movq(mm, qword_ptr [word]);

			free((OperandREF)0);
			return 0;
		}
		
		return Optimizer::pinsrw(mm, m16, c);
	}

	Encoding *Emulator::pinsrw(OperandMMREG mm, OperandR_M16 r_m16, unsigned char c)
	{
		if(r_m16.type == Operand::OPERAND_REG16) return pinsrw(mm, (OperandREG16)r_m16, c);
		else                                     return pinsrw(mm, (OperandMEM16)r_m16, c);
	}

	Encoding *Emulator::pmaxsw(OperandMMREG mmi, OperandMMREG mmj)
	{
		if(emulateSSE)
		{
			throw Error("Unimplemented SSE instruction emulation");
		}
		
		return Optimizer::pmaxsw(mmi, mmj);
	}

	Encoding *Emulator::pmaxsw(OperandMMREG mm, OperandMEM64 m64)
	{
		if(emulateSSE)
		{
			throw Error("Unimplemented SSE instruction emulation");
		}
		
		return Optimizer::pmaxsw(mm, m64);
	}

	Encoding *Emulator::pmaxsw(OperandMMREG mm, OperandMM64 r_m64)
	{
		if(emulateSSE)
		{
			throw Error("Unimplemented SSE instruction emulation");
		}
		
		return Optimizer::pmaxsw(mm, r_m64);
	}

	Encoding *Emulator::pmaxub(OperandMMREG mmi, OperandMMREG mmj)
	{
		if(emulateSSE)
		{
			throw Error("Unimplemented SSE instruction emulation");
		}
		
		return Optimizer::pmaxub(mmi, mmj);
	}

	Encoding *Emulator::pmaxub(OperandMMREG mm, OperandMEM64 m64)
	{
		if(emulateSSE)
		{
			throw Error("Unimplemented SSE instruction emulation");
		}
		
		return Optimizer::pmaxub(mm, m64);
	}

	Encoding *Emulator::pmaxub(OperandMMREG mm, OperandMM64 r_m64)
	{
		if(emulateSSE)
		{
			throw Error("Unimplemented SSE instruction emulation");
		}
		
		return Optimizer::pmaxub(mm, r_m64);
	}

	Encoding *Emulator::pminsw(OperandMMREG mmi, OperandMMREG mmj)
	{
		if(emulateSSE)
		{
			throw Error("Unimplemented SSE instruction emulation");
		}
		
		return Optimizer::pminsw(mmi, mmj);
	}

	Encoding *Emulator::pminsw(OperandMMREG mm, OperandMEM64 m64)
	{
		if(emulateSSE)
		{
			throw Error("Unimplemented SSE instruction emulation");
		}
		
		return Optimizer::pminsw(mm, m64);
	}

	Encoding *Emulator::pminsw(OperandMMREG mm, OperandMM64 r_m64)
	{
		if(emulateSSE)
		{
			throw Error("Unimplemented SSE instruction emulation");
		}
		
		return Optimizer::pminsw(mm, r_m64);
	}

	Encoding *Emulator::pminub(OperandMMREG mmi, OperandMMREG mmj)
	{
		if(emulateSSE)
		{
			throw Error("Unimplemented SSE instruction emulation");
		}
		
		return Optimizer::pminub(mmi, mmj);
	}

	Encoding *Emulator::pminub(OperandMMREG mm, OperandMEM64 m64)
	{
		if(emulateSSE)
		{
			throw Error("Unimplemented SSE instruction emulation");
		}
		
		return Optimizer::pminub(mm, m64);
	}

	Encoding *Emulator::pminub(OperandMMREG mm, OperandMM64 r_m64)
	{
		if(emulateSSE)
		{
			throw Error("Unimplemented SSE instruction emulation");
		}
		
		return Optimizer::pminub(mm, r_m64);
	}

	Encoding *Emulator::pmulhuw(OperandMMREG mmi, OperandMMREG mmj)
	{
		if(emulateSSE)
		{
			static short word1[4];
			static short word2[4];

			movq(qword_ptr [word1], mmi);
			movq(qword_ptr [word2], mmj);
			push(eax);
			push(edx);

			mov(ax, word_ptr [&word1[0]]);
			mul(word_ptr [&word2[0]]);
			mov(word_ptr [&word1[0]], dx);

			mov(ax, word_ptr [&word1[1]]);
			mul(word_ptr [&word2[1]]);
			mov(word_ptr [&word1[1]], dx);

			mov(ax, word_ptr [&word1[2]]);
			mul(word_ptr [&word2[2]]);
			mov(word_ptr [&word1[2]], dx);

			mov(ax, word_ptr [&word1[3]]);
			mul(word_ptr [&word2[3]]);
			mov(word_ptr [&word1[3]], dx);

			pop(edx);
			pop(eax);
			movq(mmi, qword_ptr [word1]);

			return 0;
		}
		
		return Optimizer::pmulhuw(mmi, mmj);
	}

	Encoding *Emulator::pmulhuw(OperandMMREG mm, OperandMEM64 m64)
	{
		if(emulateSSE)
		{
			static short word1[4];
			static short word2[4];

			movq(qword_ptr [word1], mm);
			movq(mm, m64);
			movq(qword_ptr [word2], mm);
			push(eax);
			push(edx);

			mov(ax, word_ptr [&word1[0]]);
			mul(word_ptr [&word2[0]]);
			mov(word_ptr [&word1[0]], dx);

			mov(ax, word_ptr [&word1[1]]);
			mul(word_ptr [&word2[1]]);
			mov(word_ptr [&word1[1]], dx);

			mov(ax, word_ptr [&word1[2]]);
			mul(word_ptr [&word2[2]]);
			mov(word_ptr [&word1[2]], dx);

			mov(ax, word_ptr [&word1[3]]);
			mul(word_ptr [&word2[3]]);
			mov(word_ptr [&word1[3]], dx);

			pop(edx);
			pop(eax);
			movq(mm, qword_ptr [word1]);

			return 0;
		}
		
		return Optimizer::pmulhuw(mm, m64);
	}

	Encoding *Emulator::pmulhuw(OperandMMREG mm, OperandMM64 r_m64)
	{
		if(r_m64.type == Operand::OPERAND_MMREG) return pmulhuw(mm, (OperandMMREG)r_m64);
		else                                     return pmulhuw(mm, (OperandMEM64)r_m64);
	}

	Encoding *Emulator::prefetchnta(OperandMEM mem)
	{
		if(emulateSSE)
		{
			return 0;
		}
		
		return Optimizer::prefetchnta((OperandMEM8&)mem);
	}

	Encoding *Emulator::prefetcht0(OperandMEM mem)
	{
		if(emulateSSE)
		{
			return 0;
		}
		
		return Optimizer::prefetcht0((OperandMEM8&)mem);
	}

	Encoding *Emulator::prefetcht1(OperandMEM mem)
	{
		if(emulateSSE)
		{
			return 0;
		}
		
		return Optimizer::prefetcht1((OperandMEM8&)mem);
	}

	Encoding *Emulator::prefetcht2(OperandMEM mem)
	{
		if(emulateSSE)
		{
			return 0;
		}

		return Optimizer::prefetcht2((OperandMEM8&)mem);
	}

	Encoding *Emulator::pshufw(OperandMMREG mmi, OperandMMREG mmj, unsigned char c)
	{
		if(emulateSSE)
		{
			static short word1[4];
			static short word2[4];

			movq(qword_ptr [word1], mmj);

			mov(t16(0), word_ptr [&word1[(c >> 0) & 0x03]]);
			mov(word_ptr [&word2[0]], t16(0));

			mov(t16(0), word_ptr [&word1[(c >> 2) & 0x03]]);
			mov(word_ptr [&word2[1]], t16(0));

			mov(t16(0), word_ptr [&word1[(c >> 4) & 0x03]]);
			mov(word_ptr [&word2[2]], t16(0));

			mov(t16(0), word_ptr [&word1[(c >> 6) & 0x03]]);
			mov(word_ptr [&word2[3]], t16(0));

			movq(mmi, qword_ptr [word2]);

			free((OperandREF)0);
			return 0;
		}
		
		return Optimizer::pshufw(mmi, mmj, c);
	}

	Encoding *Emulator::pshufw(OperandMMREG mm, OperandMEM64 m64, unsigned char c)
	{
		if(emulateSSE)
		{
			static short word[4];

			mov(t16(0), (OperandMEM16)(m64+((c>>0)&0x03)*2));
			mov(word_ptr [&word[0]], t16(0));

			mov(t16(0), (OperandMEM16)(m64+((c>>2)&0x03)*2));
			mov(word_ptr [&word[1]], t16(0));

			mov(t16(0), (OperandMEM16)(m64+((c>>4)&0x03)*2));
			mov(word_ptr [&word[2]], t16(0));

			mov(t16(0), (OperandMEM16)(m64+((c>>6)&0x03)*2));
			mov(word_ptr [&word[3]], t16(0));

			movq(mm, qword_ptr [word]);

			free((OperandREF)0);
			return 0;
		}
		
		return Optimizer::pshufw(mm, m64, c);
	}

	Encoding *Emulator::pshufw(OperandMMREG mm, OperandMM64 r_m64, unsigned char c)
	{
		if(r_m64.type == Operand::OPERAND_MMREG) return pshufw(mm, (OperandMMREG)r_m64, c);
		else                                     return pshufw(mm, (OperandMEM64)r_m64, c);
	}

	Encoding *Emulator::rcpps(OperandXMMREG xmmi, OperandXMMREG xmmj)
	{
		if(emulateSSE)
		{
			spillMMX();
			const int i = xmmi.reg;
			const int j = xmmj.reg;
			static float one = 1.0f;
			fld(dword_ptr [&one]);
			fdiv(dword_ptr [&sse[j][0]]);
			fstp(dword_ptr [&sse[i][0]]);
			fld(dword_ptr [&one]);
			fdiv(dword_ptr [&sse[j][1]]);
			fstp(dword_ptr [&sse[i][1]]);
			fld(dword_ptr [&one]);
			fdiv(dword_ptr [&sse[j][2]]);
			fstp(dword_ptr [&sse[i][2]]);
			fld(dword_ptr [&one]);
			fdiv(dword_ptr [&sse[j][3]]);
			fstp(dword_ptr [&sse[i][3]]);
			return 0;
		}
		
		return Optimizer::rcpps(xmmi, xmmj);
	}
	
	Encoding *Emulator::rcpps(OperandXMMREG xmm, OperandMEM128 mem128)
	{
		if(emulateSSE)
		{
			spillMMX();
			const int i = xmm.reg;
			static float one = 1.0f;
			fld(dword_ptr [&one]);
			fdiv((OperandMEM32)(mem128+0));
			fstp(dword_ptr [&sse[i][0]]);
			fld(dword_ptr [&one]);
			fdiv((OperandMEM32)(mem128+4));
			fstp(dword_ptr [&sse[i][1]]);
			fld(dword_ptr [&one]);
			fdiv((OperandMEM32)(mem128+8));
			fstp(dword_ptr [&sse[i][2]]);
			fld(dword_ptr [&one]);
			fdiv((OperandMEM32)(mem128+12));
			fstp(dword_ptr [&sse[i][3]]);
			return 0;
		}
		
		return Optimizer::rcpps(xmm, mem128);
	}
	
	Encoding *Emulator::rcpps(OperandXMMREG xmm, OperandR_M128 r_m128)
	{
		if(r_m128.type == Operand::OPERAND_XMMREG) return rcpps(xmm, (OperandXMMREG)r_m128);
		else                                       return rcpps(xmm, (OperandMEM128)r_m128);
	}

	Encoding *Emulator::rcpss(OperandXMMREG xmmi, OperandXMMREG xmmj)
	{
		if(emulateSSE)
		{
			spillMMX();
			const int i = xmmi.reg;
			const int j = xmmj.reg;
			static float one = 1.0f;
			fld(dword_ptr [&one]);
			fdiv(dword_ptr [&sse[j][0]]);
			fstp(dword_ptr [&sse[i][0]]);
			return 0;
		}
		
		return Optimizer::rcpss(xmmi, xmmj);
	}
	
	Encoding *Emulator::rcpss(OperandXMMREG xmm, OperandMEM32 mem32)
	{
		if(emulateSSE)
		{
			spillMMX();
			const int i = xmm.reg;
			static float one = 1.0f;
			fld(dword_ptr [&one]);
			fdiv((OperandMEM32)mem32);
			fstp(dword_ptr [&sse[i][0]]);
			return 0;
		}
		
		return Optimizer::rcpss(xmm, mem32);
	}
	
	Encoding *Emulator::rcpss(OperandXMMREG xmm, OperandXMM32 xmm32)
	{
		if(xmm32.type == Operand::OPERAND_XMMREG) return rcpss(xmm, (OperandXMMREG)xmm32);
		else                                      return rcpss(xmm, (OperandMEM32)xmm32);
	}

	Encoding *Emulator::rsqrtps(OperandXMMREG xmmi, OperandXMMREG xmmj)
	{
		if(emulateSSE)
		{
			spillMMX();
			const int i = xmmi.reg;
			const int j = xmmj.reg;
			static float one = 1.0f;
			fld(dword_ptr [&one]);
			fdiv(dword_ptr [&sse[j][0]]);
			fsqrt();
			fstp(dword_ptr [&sse[i][0]]);
			fld(dword_ptr [&one]);
			fdiv(dword_ptr [&sse[j][1]]);
			fsqrt();
			fstp(dword_ptr [&sse[i][1]]);
			fld(dword_ptr [&one]);
			fdiv(dword_ptr [&sse[j][2]]);
			fsqrt();
			fstp(dword_ptr [&sse[i][2]]);
			fld(dword_ptr [&one]);
			fdiv(dword_ptr [&sse[j][3]]);
			fsqrt();
			fstp(dword_ptr [&sse[i][3]]);
			return 0;
		}
		
		return Optimizer::rsqrtps(xmmi, xmmj);
	}
	
	Encoding *Emulator::rsqrtps(OperandXMMREG xmm, OperandMEM128 mem128)
	{
		if(emulateSSE)
		{
			spillMMX();
			const int i = xmm.reg;
			static float one = 1.0f;
			fld(dword_ptr [&one]);
			fdiv((OperandMEM32)(mem128+0));
			fsqrt();
			fstp(dword_ptr [&sse[i][0]]);
			fld(dword_ptr [&one]);
			fdiv((OperandMEM32)(mem128+4));
			fsqrt();
			fstp(dword_ptr [&sse[i][1]]);
			fld(dword_ptr [&one]);
			fdiv((OperandMEM32)(mem128+8));
			fsqrt();
			fstp(dword_ptr [&sse[i][2]]);
			fld(dword_ptr [&one]);
			fdiv((OperandMEM32)(mem128+12));
			fsqrt();
			fstp(dword_ptr [&sse[i][3]]);
			return 0;
		}
		
		return Optimizer::rsqrtps(xmm, mem128);
	}
	
	Encoding *Emulator::rsqrtps(OperandXMMREG xmm, OperandR_M128 r_m128)
	{
		if(r_m128.type == Operand::OPERAND_XMMREG) return rsqrtps(xmm, (OperandXMMREG)r_m128);
		else                                       return rsqrtps(xmm, (OperandMEM128)r_m128);
	}

	Encoding *Emulator::rsqrtss(OperandXMMREG xmmi, OperandXMMREG xmmj)
	{
		if(emulateSSE)
		{
			spillMMX();
			const int i = xmmi.reg;
			const int j = xmmj.reg;
			static float one = 1.0f;
			fld(dword_ptr [&one]);
			fdiv(dword_ptr [&sse[j][0]]);
			fsqrt();
			fstp(dword_ptr [&sse[i][0]]);
			return 0;
		}
		
		return Optimizer::rsqrtss(xmmi, xmmj);
	}
	
	Encoding *Emulator::rsqrtss(OperandXMMREG xmm, OperandMEM32 mem32)
	{
		if(emulateSSE)
		{
			spillMMX();
			const int i = xmm.reg;
			static float one = 1.0f;
			fld(dword_ptr [&one]);
			fdiv((OperandMEM32)mem32);
			fsqrt();
			fstp(dword_ptr [&sse[i][0]]);
			return 0;
		}
		
		return Optimizer::rsqrtss(xmm, mem32);
	}
	
	Encoding *Emulator::rsqrtss(OperandXMMREG xmm, OperandXMM32 xmm32)
	{
		if(xmm32.type == Operand::OPERAND_XMMREG) return rsqrtss(xmm, (OperandXMMREG)xmm32);
		else                                      return rsqrtss(xmm, (OperandMEM32)xmm32);
	}

	Encoding *Emulator::sfence()
	{
		if(emulateSSE)
		{
			return 0;
		}
		
		return Optimizer::sfence();
	}

	Encoding *Emulator::shufps(OperandXMMREG xmmi, OperandXMMREG xmmj, unsigned char c)
	{
		if(emulateSSE)
		{
			const int i = xmmi.reg;
			const int j = xmmj.reg;

			mov(t32(0), dword_ptr [&sse[i][(c >> 0) & 0x03]]);
			mov(dword_ptr [&sse[i][0]], t32(0));

			mov(t32(0), dword_ptr [&sse[i][(c >> 2) & 0x03]]);
			mov(dword_ptr [&sse[i][1]], t32(0));

			mov(t32(0), dword_ptr [&sse[j][(c >> 4) & 0x03]]);
			mov(dword_ptr [&sse[i][2]], t32(0));

			mov(t32(0), dword_ptr [&sse[j][(c >> 6) & 0x03]]);
			mov(dword_ptr [&sse[i][3]], t32(0));

			free((OperandREF)0);
			return 0;
		}
		
		return Optimizer::shufps(xmmi, xmmj, c);
	}

	Encoding *Emulator::shufps(OperandXMMREG xmm, OperandMEM128 m128, unsigned char c)
	{
		if(emulateSSE)
		{
			const int i = xmm.reg;

			mov(t32(0), dword_ptr [&sse[i][(c >> 0) & 0x03]]);
			mov(dword_ptr [&sse[i][0]], t32(0));

			mov(t32(0), dword_ptr [&sse[i][(c >> 2) & 0x03]]);
			mov(dword_ptr [&sse[i][1]], t32(0));

			mov(t32(0), (OperandMEM32)(m128+((c>>4)&0x03)*4));
			mov(dword_ptr [&sse[i][2]], t32(0));

			mov(t32(0), (OperandMEM32)(m128+((c>>6)&0x03)*4));
			mov(dword_ptr [&sse[i][3]], t32(0));

			free((OperandREF)0);
			return 0;
		}
		
		return Optimizer::shufps(xmm, m128, c);
	}

	Encoding *Emulator::shufps(OperandXMMREG xmm, OperandR_M128 r_m128, unsigned char c)
	{
		if(r_m128.type == Operand::OPERAND_XMMREG) return shufps(xmm, (OperandXMMREG)r_m128, c);
		else                                       return shufps(xmm, (OperandMEM128)r_m128, c);
	}

	Encoding *Emulator::sqrtps(OperandXMMREG xmmi, OperandXMMREG xmmj)
	{
		if(emulateSSE)
		{
			spillMMX();
			const int i = xmmi.reg;
			const int j = xmmj.reg;
			fld(dword_ptr [&sse[j][0]]);
			fsqrt();
			fstp(dword_ptr [&sse[i][0]]);
			fld(dword_ptr [&sse[j][1]]);
			fsqrt();
			fstp(dword_ptr [&sse[i][1]]);
			fld(dword_ptr [&sse[j][2]]);
			fsqrt();
			fstp(dword_ptr [&sse[i][2]]);
			fld(dword_ptr [&sse[j][3]]);
			fsqrt();
			fstp(dword_ptr [&sse[i][3]]);
			return 0;
		}
		
		return Optimizer::sqrtps(xmmi, xmmj);
	}
	
	Encoding *Emulator::sqrtps(OperandXMMREG xmm, OperandMEM128 mem128)
	{
		if(emulateSSE)
		{
			spillMMX();
			const int i = xmm.reg;
			fld((OperandMEM32)(mem128+0));
			fsqrt();
			fstp(dword_ptr [&sse[i][0]]);
			fld((OperandMEM32)(mem128+4));
			fsqrt();
			fstp(dword_ptr [&sse[i][1]]);
			fld((OperandMEM32)(mem128+8));
			fsqrt();
			fstp(dword_ptr [&sse[i][2]]);
			fld((OperandMEM32)(mem128+12));
			fsqrt();
			fstp(dword_ptr [&sse[i][3]]);
			return 0;
		}
		
		return Optimizer::sqrtps(xmm, mem128);
	}
	
	Encoding *Emulator::sqrtps(OperandXMMREG xmm, OperandR_M128 r_m128)
	{
		if(r_m128.type == Operand::OPERAND_XMMREG) return sqrtps(xmm, (OperandXMMREG)r_m128);
		else                                       return sqrtps(xmm, (OperandMEM128)r_m128);
	}

	Encoding *Emulator::sqrtss(OperandXMMREG xmmi, OperandXMMREG xmmj)
	{
		if(emulateSSE)
		{
			spillMMX();
			const int i = xmmi.reg;
			const int j = xmmj.reg;
			fld(dword_ptr [&sse[j][0]]);
			fsqrt();
			fstp(dword_ptr [&sse[i][0]]);
			return 0;
		}
		
		return Optimizer::sqrtss(xmmi, xmmj);
	}
	
	Encoding *Emulator::sqrtss(OperandXMMREG xmm, OperandMEM32 mem32)
	{
		if(emulateSSE)
		{
			spillMMX();
			const int i = xmm.reg;
			static float one = 1.0f;
			fld(mem32);
			fsqrt();
			fstp(dword_ptr [&sse[i][0]]);
			return 0;
		}
		
		return Optimizer::sqrtss(xmm, mem32);
	}
	
	Encoding *Emulator::sqrtss(OperandXMMREG xmm, OperandXMM32 xmm32)
	{
		if(xmm32.type == Operand::OPERAND_XMMREG) return sqrtss(xmm, (OperandXMMREG)xmm32);
		else                                      return sqrtss(xmm, (OperandMEM32)xmm32);
	}

	Encoding *Emulator::stmxcsr(OperandMEM32 m32)
	{
		if(emulateSSE)
		{
			return 0;
		}
		
		return Optimizer::stmxcsr(m32);
	}

	Encoding *Emulator::subps(OperandXMMREG xmmi, OperandXMMREG xmmj)
	{
		if(emulateSSE)
		{
			spillMMX();
			const int i = xmmi.reg;
			const int j = xmmj.reg;
			fld(dword_ptr [&sse[i][0]]);
			fsub(dword_ptr [&sse[j][0]]);
			fstp(dword_ptr [&sse[i][0]]);
			fld(dword_ptr [&sse[i][1]]);
			fsub(dword_ptr [&sse[j][1]]);
			fstp(dword_ptr [&sse[i][1]]);
			fld(dword_ptr [&sse[i][2]]);
			fsub(dword_ptr [&sse[j][2]]);
			fstp(dword_ptr [&sse[i][2]]);
			fld(dword_ptr [&sse[i][3]]);
			fsub(dword_ptr [&sse[j][3]]);
			fstp(dword_ptr [&sse[i][3]]);
			return 0;
		}
		
		return Optimizer::subps(xmmi, xmmj);
	}
	
	Encoding *Emulator::subps(OperandXMMREG xmm, OperandMEM128 mem128)
	{
		if(emulateSSE)
		{
			spillMMX();
			const int i = xmm.reg;
			fld(dword_ptr [&sse[i][0]]);
			fsub((OperandMEM32)(mem128+0));
			fstp(dword_ptr [&sse[i][0]]);
			fld(dword_ptr [&sse[i][1]]);
			fsub((OperandMEM32)(mem128+4));
			fstp(dword_ptr [&sse[i][1]]);
			fld(dword_ptr [&sse[i][2]]);
			fsub((OperandMEM32)(mem128+8));
			fstp(dword_ptr [&sse[i][2]]);
			fld(dword_ptr [&sse[i][3]]);
			fsub((OperandMEM32)(mem128+12));
			fstp(dword_ptr [&sse[i][3]]);
			return 0;
		}
		
		return Optimizer::subps(xmm, mem128);
	}
	
	Encoding *Emulator::subps(OperandXMMREG xmm, OperandR_M128 r_m128)
	{
		if(r_m128.type == Operand::OPERAND_XMMREG) return subps(xmm, (OperandXMMREG)r_m128);
		else                                       return subps(xmm, (OperandMEM128)r_m128);
	}

	Encoding *Emulator::subss(OperandXMMREG xmmi, OperandXMMREG xmmj)
	{
		if(emulateSSE)
		{
			spillMMX();
			const int i = xmmi.reg;
			const int j = xmmj.reg;
			fld(dword_ptr [&sse[i][0]]);
			fsub(dword_ptr [&sse[j][0]]);
			fstp(dword_ptr [&sse[i][0]]);
			return 0;
		}
		
		return Optimizer::subss(xmmi, xmmj);
	}
	
	Encoding *Emulator::subss(OperandXMMREG xmm, OperandMEM32 mem32)
	{
		if(emulateSSE)
		{
			spillMMX();
			const int i = xmm.reg;
			fld(dword_ptr [&sse[i][0]]);
			fsub((OperandMEM32)mem32);
			fstp(dword_ptr [&sse[i][0]]);
			return 0;
		}
		
		return Optimizer::subss(xmm, mem32);
	}
	
	Encoding *Emulator::subss(OperandXMMREG xmm, OperandXMM32 xmm32)
	{
		if(xmm32.type == Operand::OPERAND_XMMREG) return subss(xmm, (OperandXMMREG)xmm32);
		else                                      return subss(xmm, (OperandMEM32)xmm32);
	}

	Encoding *Emulator::ucomiss(OperandXMMREG xmmi, OperandXMMREG xmmj)
	{
		if(emulateSSE)
		{
			spillMMX();
			const int i = xmmi.reg;
			const int j = xmmj.reg;

			fld(dword_ptr [&sse[j][0]]);
			fld(dword_ptr [&sse[i][0]]);
			fcomip(st0, st1);
			ffree(st0);

			return 0;
		}
		
		return Optimizer::ucomiss(xmmi, xmmj);
	}

	Encoding *Emulator::ucomiss(OperandXMMREG xmm, OperandMEM32 mem32)
	{
		if(emulateSSE)
		{
			spillMMX();
			const int i = xmm.reg;
			
			fld(mem32);
			fld(dword_ptr [&sse[i][0]]);
			fcomip(st0, st1);
			ffree(st0);

			return 0;
		}
		
		return Optimizer::ucomiss(xmm, mem32);
	}

	Encoding *Emulator::ucomiss(OperandXMMREG xmm, OperandXMM32 xmm32)
	{
		if(xmm32.type == Operand::OPERAND_XMMREG) return ucomiss(xmm, (OperandXMMREG)xmm32);
		else                                      return ucomiss(xmm, (OperandMEM32)xmm32);
	}

	Encoding *Emulator::unpckhps(OperandXMMREG xmmi, OperandXMMREG xmmj)
	{
		if(emulateSSE)
		{
			const int i = xmmi.reg;
			const int j = xmmj.reg;

			mov(t32(0), dword_ptr [&sse[i][2]]);
			mov(dword_ptr [&sse[i][0]], t32(0));

			mov(t32(0), dword_ptr [&sse[i][3]]);
			mov(dword_ptr [&sse[i][2]], t32(0));

			mov(t32(0), dword_ptr [&sse[j][2]]);
			mov(dword_ptr [&sse[i][1]], t32(0));

			mov(t32(0), dword_ptr [&sse[j][3]]);
			mov(dword_ptr [&sse[i][3]], t32(0));

			free((OperandREF)0);
			return 0;
		}
		
		return Optimizer::unpckhps(xmmi, xmmj);
	}
	
	Encoding *Emulator::unpckhps(OperandXMMREG xmm, OperandMEM128 mem128)
	{
		if(emulateSSE)
		{
			const int i = xmm.reg;

			mov(t32(0), dword_ptr [&sse[i][2]]);
			mov(dword_ptr [&sse[i][0]], t32(0));

			mov(t32(0), dword_ptr [&sse[i][3]]);
			mov(dword_ptr [&sse[i][2]], t32(0));

			mov(t32(0), (OperandMEM32)(mem128+8));
			mov(dword_ptr [&sse[i][1]], t32(0));

			mov(t32(0), (OperandMEM32)(mem128+12));
			mov(dword_ptr [&sse[i][3]], t32(0));

			free((OperandREF)0);
			return 0;
		}
		
		return Optimizer::unpckhps(xmm, mem128);
	}
	
	Encoding *Emulator::unpckhps(OperandXMMREG xmm, OperandR_M128 r_m128)
	{
		if(r_m128.type == Operand::OPERAND_XMMREG) return unpckhps(xmm, (OperandXMMREG)r_m128);
		else                                       return unpckhps(xmm, (OperandMEM128)r_m128);
	}

	Encoding *Emulator::unpcklps(OperandXMMREG xmmi, OperandXMMREG xmmj)
	{
		if(emulateSSE)
		{
			const int i = xmmi.reg;
			const int j = xmmj.reg;

			mov(t32(0), dword_ptr [&sse[i][0]]);
			mov(dword_ptr [&sse[i][0]], t32(0));

			mov(t32(0), dword_ptr [&sse[i][1]]);
			mov(dword_ptr [&sse[i][2]], t32(0));

			mov(t32(0), dword_ptr [&sse[j][0]]);
			mov(dword_ptr [&sse[i][1]], t32(0));

			mov(t32(0), dword_ptr [&sse[j][1]]);
			mov(dword_ptr [&sse[i][3]], t32(0));

			free((OperandREF)0);
			return 0;
		}
		
		return Optimizer::unpcklps(xmmi, xmmj);
	}
	
	Encoding *Emulator::unpcklps(OperandXMMREG xmm, OperandMEM128 mem128)
	{
		if(emulateSSE)
		{
			const int i = xmm.reg;

			mov(t32(0), dword_ptr [&sse[i][0]]);
			mov(dword_ptr [&sse[i][0]], t32(0));

			mov(t32(0), dword_ptr [&sse[i][1]]);
			mov(dword_ptr [&sse[i][2]], t32(0));

			mov(t32(0), (OperandMEM32)(mem128+0));
			mov(dword_ptr [&sse[i][1]], t32(0));

			mov(t32(0), (OperandMEM32)(mem128+4));
			mov(dword_ptr [&sse[i][3]], t32(0));

			free((OperandREF)0);
			return 0;
		}
		
		return Optimizer::unpcklps(xmm, mem128);
	}
	
	Encoding *Emulator::unpcklps(OperandXMMREG xmm, OperandR_M128 r_m128)
	{
		if(r_m128.type == Operand::OPERAND_XMMREG) return unpcklps(xmm, (OperandXMMREG)r_m128);
		else                                       return unpcklps(xmm, (OperandMEM128)r_m128);
	}

	Encoding *Emulator::xorps(OperandXMMREG xmmi, OperandXMMREG xmmj)
	{
		if(emulateSSE)
		{
			const int i = xmmi.reg;
			const int j = xmmj.reg;

			mov(t32(0), dword_ptr [&sse[j][0]]);
			xor(dword_ptr [&sse[i][0]], t32(0));

			mov(t32(0), dword_ptr [&sse[j][1]]);
			xor(dword_ptr [&sse[i][1]], t32(0));

			mov(t32(0), dword_ptr [&sse[j][2]]);
			xor(dword_ptr [&sse[i][2]], t32(0));

			mov(t32(0), dword_ptr [&sse[j][3]]);
			xor(dword_ptr [&sse[i][3]], t32(0));

			free((OperandREF)0);
			return 0;
		}
		
		return Optimizer::xorps(xmmi, xmmj);
	}
	
	Encoding *Emulator::xorps(OperandXMMREG xmm, OperandMEM128 mem128)
	{
		if(emulateSSE)
		{
			const int i = xmm.reg;

			mov(t32(0), (OperandMEM32)(mem128+0));
			xor(dword_ptr [&sse[i][0]], t32(0));

			mov(t32(0), (OperandMEM32)(mem128+4));
			xor(dword_ptr [&sse[i][1]], t32(0));

			mov(t32(0), (OperandMEM32)(mem128+8));
			xor(dword_ptr [&sse[i][2]], t32(0));

			mov(t32(0), (OperandMEM32)(mem128+12));
			xor(dword_ptr [&sse[i][3]], t32(0));

			free((OperandREF)0);
			return 0;
		}
		
		return Optimizer::xorps(xmm, mem128);
	}
	
	Encoding *Emulator::xorps(OperandXMMREG xmm, OperandR_M128 r_m128)
	{
		if(r_m128.type == Operand::OPERAND_XMMREG) return xorps(xmm, (OperandXMMREG)r_m128);
		else                                       return xorps(xmm, (OperandMEM128)r_m128);
	}

	void Emulator::enableEmulateSSE()
	{
		emulateSSE = true;
	}

	void Emulator::disableEmulateSSE()
	{
		emulateSSE = false;
	}

	void Emulator::dumpSSE()
	{
		pushad();
		emms();

		static float sse[8][4];

		movups(xword_ptr [sse[0]], xmm0);
		movups(xword_ptr [sse[1]], xmm1);
		movups(xword_ptr [sse[2]], xmm2);
		movups(xword_ptr [sse[3]], xmm3);
		movups(xword_ptr [sse[4]], xmm4);
		movups(xword_ptr [sse[5]], xmm5);
		movups(xword_ptr [sse[6]], xmm6);
		movups(xword_ptr [sse[7]], xmm7);

		static FILE *file;
		static char *perm = "a";
		static char *name;

		if(emulateSSE)
		{
			name = "dumpEmulate.txt";
		}
		else
		{
			name = "dumpNative.txt";
		}

		mov(eax, dword_ptr [&perm]); 
		push(eax);
		mov(ecx, dword_ptr [&name]); 
		push(ecx);
		call((int)fopen);
		add(esp, 8);
		mov(dword_ptr [&file], eax);

		static char *string0 = "xmm0: %f, %f, %f, %f\n";
		static char *string1 = "xmm1: %f, %f, %f, %f\n";
		static char *string2 = "xmm2: %f, %f, %f, %f\n";
		static char *string3 = "xmm3: %f, %f, %f, %f\n";
		static char *string4 = "xmm4: %f, %f, %f, %f\n";
		static char *string5 = "xmm5: %f, %f, %f, %f\n";
		static char *string6 = "xmm6: %f, %f, %f, %f\n";
		static char *string7 = "xmm7: %f, %f, %f, %f\n";
		static char *newline = "\n";

		// fprintf(file, string0, sse[0][0], sse[0][1], sse[0][2], sse[0][3]);
		fld(dword_ptr [&sse[0][3]]);
		sub(esp, 8);
		fstp(qword_ptr [esp]);
		fld(dword_ptr [&sse[0][2]]);
		sub(esp, 8); 
		fstp(qword_ptr [esp]);
		fld(dword_ptr [&sse[0][1]]);
		sub(esp, 8);
		fstp(qword_ptr [esp]);
		fld(dword_ptr [&sse[0][0]]);
		sub(esp, 8);
		fstp(qword_ptr [esp]);
		mov(eax, dword_ptr [&string0]); 
		push(eax);
		mov(ecx, dword_ptr [&file]); 
		push(ecx);
		call((int)fprintf); 
		add(esp, 0x28); 

		// fprintf(file, string1, sse[1][0], sse[1][1], sse[1][2], sse[1][3]);
		fld(dword_ptr [&sse[1][3]]);
		sub(esp, 8);
		fstp(qword_ptr [esp]);
		fld(dword_ptr [&sse[1][2]]);
		sub(esp, 8); 
		fstp(qword_ptr [esp]);
		fld(dword_ptr [&sse[1][1]]);
		sub(esp, 8);
		fstp(qword_ptr [esp]);
		fld(dword_ptr [&sse[1][0]]);
		sub(esp, 8);
		fstp(qword_ptr [esp]);
		mov(eax, dword_ptr [&string1]); 
		push(eax);
		mov(ecx, dword_ptr [&file]); 
		push(ecx);
		call((int)fprintf); 
		add(esp, 0x28); 

		// fprintf(file, string2, sse[2][0], sse[2][1], sse[2][2], sse[2][3]);
		fld(dword_ptr [&sse[2][3]]);
		sub(esp, 8);
		fstp(qword_ptr [esp]);
		fld(dword_ptr [&sse[2][2]]);
		sub(esp, 8); 
		fstp(qword_ptr [esp]);
		fld(dword_ptr [&sse[2][1]]);
		sub(esp, 8);
		fstp(qword_ptr [esp]);
		fld(dword_ptr [&sse[2][0]]);
		sub(esp, 8);
		fstp(qword_ptr [esp]);
		mov(eax, dword_ptr [&string2]); 
		push(eax);
		mov(ecx, dword_ptr [&file]); 
		push(ecx);
		call((int)fprintf); 
		add(esp, 0x28); 

		// fprintf(file, string3, sse[3][0], sse[3][1], sse[3][2], sse[3][3]);
		fld(dword_ptr [&sse[3][3]]);
		sub(esp, 8);
		fstp(qword_ptr [esp]);
		fld(dword_ptr [&sse[3][2]]);
		sub(esp, 8); 
		fstp(qword_ptr [esp]);
		fld(dword_ptr [&sse[3][1]]);
		sub(esp, 8);
		fstp(qword_ptr [esp]);
		fld(dword_ptr [&sse[3][0]]);
		sub(esp, 8);
		fstp(qword_ptr [esp]);
		mov(eax, dword_ptr [&string3]); 
		push(eax);
		mov(ecx, dword_ptr [&file]); 
		push(ecx);
		call((int)fprintf); 
		add(esp, 0x28); 

		// fprintf(file, string4, sse[4][0], sse[4][1], sse[4][2], sse[4][3]);
		fld(dword_ptr [&sse[4][3]]);
		sub(esp, 8);
		fstp(qword_ptr [esp]);
		fld(dword_ptr [&sse[4][2]]);
		sub(esp, 8); 
		fstp(qword_ptr [esp]);
		fld(dword_ptr [&sse[4][1]]);
		sub(esp, 8);
		fstp(qword_ptr [esp]);
		fld(dword_ptr [&sse[4][0]]);
		sub(esp, 8);
		fstp(qword_ptr [esp]);
		mov(eax, dword_ptr [&string4]); 
		push(eax);
		mov(ecx, dword_ptr [&file]); 
		push(ecx);
		call((int)fprintf); 
		add(esp, 0x28); 

		// fprintf(file, string5, sse[5][0], sse[5][1], sse[5][2], sse[5][3]);
		fld(dword_ptr [&sse[5][3]]);
		sub(esp, 8);
		fstp(qword_ptr [esp]);
		fld(dword_ptr [&sse[5][2]]);
		sub(esp, 8); 
		fstp(qword_ptr [esp]);
		fld(dword_ptr [&sse[5][1]]);
		sub(esp, 8);
		fstp(qword_ptr [esp]);
		fld(dword_ptr [&sse[5][0]]);
		sub(esp, 8);
		fstp(qword_ptr [esp]);
		mov(eax, dword_ptr [&string5]); 
		push(eax);
		mov(ecx, dword_ptr [&file]); 
		push(ecx);
		call((int)fprintf); 
		add(esp, 0x28);

		// fprintf(file, string6, sse[6][0], sse[6][1], sse[6][2], sse[6][3]);
		fld(dword_ptr [&sse[6][3]]);
		sub(esp, 8);
		fstp(qword_ptr [esp]);
		fld(dword_ptr [&sse[6][2]]);
		sub(esp, 8); 
		fstp(qword_ptr [esp]);
		fld(dword_ptr [&sse[6][1]]);
		sub(esp, 8);
		fstp(qword_ptr [esp]);
		fld(dword_ptr [&sse[6][0]]);
		sub(esp, 8);
		fstp(qword_ptr [esp]);
		mov(eax, dword_ptr [&string6]); 
		push(eax);
		mov(ecx, dword_ptr [&file]); 
		push(ecx);
		call((int)fprintf); 
		add(esp, 0x28); 

		// fprintf(file, string7, sse[7][0], sse[7][1], sse[7][2], sse[7][3]);
		fld(dword_ptr [&sse[7][3]]);
		sub(esp, 8);
		fstp(qword_ptr [esp]);
		fld(dword_ptr [&sse[7][2]]);
		sub(esp, 8); 
		fstp(qword_ptr [esp]);
		fld(dword_ptr [&sse[7][1]]);
		sub(esp, 8);
		fstp(qword_ptr [esp]);
		fld(dword_ptr [&sse[7][0]]);
		sub(esp, 8);
		fstp(qword_ptr [esp]);
		mov(eax, dword_ptr [&string7]); 
		push(eax);
		mov(ecx, dword_ptr [&file]); 
		push(ecx);
		call((int)fprintf); 
		add(esp, 0x28); 

		// fprintf(file, newline);
		mov(eax, dword_ptr [&newline]);
		push(eax);
		mov(ecx, dword_ptr [&file]);
		push(ecx);
		call((int)fprintf); 
		add(esp, 8);

		// fclose(file);
		mov(eax, dword_ptr [&file]); 
		push(eax);
		call((int)fclose);
		add(esp, 4);

		popad();

	//	int3();
	}
}
