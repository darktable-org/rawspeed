#include "Operand.hpp"

#include "String.hpp"

namespace SoftWire
{
	const Specifier Specifier::specifierSet[] =
	{
		{TYPE_UNKNOWN,	""},

		{TYPE_NEAR,		"NEAR"},
		{TYPE_SHORT,	"SHORT"},
	//	{TYPE_FAR,		"FAR"},

		{TYPE_BYTE,		"BYTE"},
		{TYPE_WORD,		"WORD"},
		{TYPE_DWORD,	"DWORD"},
	//	{TYPE_TWORD,	"TWORD"},
		{TYPE_QWORD,	"QWORD"},
		{TYPE_MMWORD,	"MMWORD"},
		{TYPE_XMMWORD,	"XMMWORD"},
		{TYPE_XWORD,	"XWORD"},
		{TYPE_OWORD,	"OWORD"},

		{TYPE_PTR,		"PTR"}
	};

	Specifier::Type Specifier::scan(const char *string)
	{
		if(string)
		{
			for(int i = 0; i < sizeof(specifierSet) / sizeof(Specifier); i++)
			{
				if(stricmp(string, specifierSet[i].notation) == 0)
				{
					return specifierSet[i].type;
				}		
			}
		}

		return TYPE_UNKNOWN;
	}

	bool Operand::operator==(Operand &op)
	{
		return type == op.type &&
		       baseReg == op.baseReg &&
			   indexReg == op.indexReg &&
			   scale == op.scale &&
			   displacement == op.displacement;
	}

	bool Operand::operator!=(Operand &op)
	{
		return type != op.type ||
		       baseReg != op.baseReg ||
			   indexReg != op.indexReg ||
			   scale != op.scale ||
			   displacement != op.displacement;
	}

	bool Operand::isSubtypeOf(Type type, Type baseType)
	{
		return (type & baseType) == type;
	}

	bool Operand::isSubtypeOf(Type baseType) const
	{
		return isSubtypeOf(type, baseType);
	}

	const char *Operand::string() const
	{
		static char string[256];

		if(isVoid(type))
		{
			return 0;
		}
		else if(isImm(type))
		{
			if(reference)
			{
				return reference;
			}
			else
			{
				if(value <= 127 && value >= -128)
				{
					snprintf(string, 255, "0x%0.2X", value & 0xFF);
				}
				else if(value <= 32767 && value -32768)
				{
					snprintf(string, 255, "0x%0.4X", value & 0xFFFF);
				}
				else
				{
					snprintf(string, 255, "0x%0.8X", value);
				}
			}
		}
		else if(isReg(type))
		{
			return regName();
		}
		else if(isMem(type))
		{
			switch(type)
			{
			case OPERAND_MEM8:
				snprintf(string, 255, "byte ptr [");
				break;
			case OPERAND_MEM16:
				snprintf(string, 255, "word ptr [");
				break;
			case OPERAND_MEM32:
				snprintf(string, 255, "dword ptr [");
				break;
			case OPERAND_MEM64:
				snprintf(string, 255, "qword ptr [");
				break;
			case OPERAND_MEM128:
				snprintf(string, 255, "xmmword ptr [");
				break;
			case OPERAND_MEM:
			default:
				snprintf(string, 255, "byte ptr [");
			}

			if(baseReg != Encoding::REG_UNKNOWN)
			{
				snprintf(string, 255, "%s%s", string, regName());

				if(indexReg != Encoding::REG_UNKNOWN)
				{
					snprintf(string, 255, "%s+", string);
				}
			}

			if(indexReg != Encoding::REG_UNKNOWN)
			{
				snprintf(string, 255, "%s%s", string, indexName());
			}

			switch(scale)
			{
			case 0:
			case 1:
				break;
			case 2:
				snprintf(string, 255, "%s*2", string);
				break;
			case 4:
				snprintf(string, 255, "%s*4", string);
				break;
			case 8:
				snprintf(string, 255, "%s*8", string);
				break;
			default:
				throw INTERNAL_ERROR;
			}

			if(displacement)
			{
				if(baseReg != Encoding::REG_UNKNOWN ||
				   indexReg != Encoding::REG_UNKNOWN)
				{
					snprintf(string, 255, "%s+", string);
				}

				if(reference)
				{
					snprintf(string, 255, "%s%s", string, reference);
				}
				else
				{
					if(displacement <= 32767 && displacement >= -32768)
					{
						snprintf(string, 255, "%s%d", string, displacement);
					}
					else
					{					
						snprintf(string, 255, "%s0x%0.8X", string, displacement);
					}
				}
			}

			snprintf(string, 255, "%s]", string);
		}
		else
		{
			throw INTERNAL_ERROR;
		}

		return strlwr(string);
	}

	bool Operand::isVoid(Type type)
	{
		return type == OPERAND_VOID;
	}

	bool Operand::isImm(Type type)
	{
		return (type & OPERAND_IMM) == type;
	}

	bool Operand::isReg(Type type)
	{
		return (type & OPERAND_REG) == type;
	}

	bool Operand::isMem(Type type)
	{
		return (type & OPERAND_MEM) == type;
	}

	bool Operand::isR_M(Type type)
	{
		return (type & OPERAND_R_M) == type;
	}

	bool Operand::isVoid(const Operand &operand)
	{
		return isVoid(operand.type);
	}

	bool Operand::isImm(const Operand &operand)
	{
		return isImm(operand.type);
	}

	bool Operand::isReg(const Operand &operand)
	{
		return isReg(operand.type);
	}

	bool Operand::isMem(const Operand &operand)
	{
		return isMem(operand.type);
	}

	bool Operand::isR_M(const Operand &operand)
	{
		return isR_M(operand.type);
	}

	const Operand::Register Operand::registerSet[] =
	{
		{OPERAND_VOID,		""},

		{OPERAND_AL,		"al", Encoding::AL},
		{OPERAND_CL,		"cl", Encoding::CL},
		{OPERAND_REG8,		"dl", Encoding::DL},
		{OPERAND_REG8,		"bl", Encoding::BL},
		{OPERAND_REG8,		"ah", Encoding::AH},
		{OPERAND_REG8,		"ch", Encoding::CH},
		{OPERAND_REG8,		"dh", Encoding::DH},
		{OPERAND_REG8,		"bh", Encoding::BH},

		{OPERAND_AX,		"ax", Encoding::AX},
		{OPERAND_CX,		"cx", Encoding::CX},
		{OPERAND_DX,		"dx", Encoding::DX},
		{OPERAND_REG16,		"bx", Encoding::BX},
		{OPERAND_REG16,		"sp", Encoding::SP},
		{OPERAND_REG16,		"bp", Encoding::BP},
		{OPERAND_REG16,		"si", Encoding::SI},
		{OPERAND_REG16,		"di", Encoding::DI},

		{OPERAND_EAX,		"eax", Encoding::EAX},
		{OPERAND_ECX,		"ecx", Encoding::ECX},
		{OPERAND_REG32,		"edx", Encoding::EDX},
		{OPERAND_REG32,		"ebx", Encoding::EBX},
		{OPERAND_REG32,		"esp", Encoding::ESP},
		{OPERAND_REG32,		"ebp", Encoding::EBP},
		{OPERAND_REG32,		"esi", Encoding::ESI},
		{OPERAND_REG32,		"edi", Encoding::EDI},

		{OPERAND_ST0,		"st",  Encoding::ST0},
		{OPERAND_ST0,		"st0", Encoding::ST0},
		{OPERAND_FPUREG,	"st1", Encoding::ST1},
		{OPERAND_FPUREG,	"st2", Encoding::ST2},
		{OPERAND_FPUREG,	"st3", Encoding::ST3},
		{OPERAND_FPUREG,	"st4", Encoding::ST4},
		{OPERAND_FPUREG,	"st5", Encoding::ST5},
		{OPERAND_FPUREG,	"st6", Encoding::ST6},
		{OPERAND_FPUREG,	"st7", Encoding::ST7},

		{OPERAND_MMREG,		"mm0", Encoding::MM0},
		{OPERAND_MMREG,		"mm1", Encoding::MM1},
		{OPERAND_MMREG,		"mm2", Encoding::MM2},
		{OPERAND_MMREG,		"mm3", Encoding::MM3},
		{OPERAND_MMREG,		"mm4", Encoding::MM4},
		{OPERAND_MMREG,		"mm5", Encoding::MM5},
		{OPERAND_MMREG,		"mm6", Encoding::MM6},
		{OPERAND_MMREG,		"mm7", Encoding::MM7},

		{OPERAND_XMMREG,	"xmm0", Encoding::XMM0},
		{OPERAND_XMMREG,	"xmm1", Encoding::XMM1},
		{OPERAND_XMMREG,	"xmm2", Encoding::XMM2},
		{OPERAND_XMMREG,	"xmm3", Encoding::XMM3},
		{OPERAND_XMMREG,	"xmm4", Encoding::XMM4},
		{OPERAND_XMMREG,	"xmm5", Encoding::XMM5},
		{OPERAND_XMMREG,	"xmm6", Encoding::XMM6},
		{OPERAND_XMMREG,	"xmm7", Encoding::XMM7}
	};

	const char *Operand::regName() const
	{
		for(int i = 0; i < sizeof(registerSet) / sizeof(Operand::Register); i++)
		{
			if(reg == registerSet[i].reg)
			{
				if(isSubtypeOf(OPERAND_MEM) && Operand::isSubtypeOf(registerSet[i].type, OPERAND_REG32) ||
				   Operand::isSubtypeOf(registerSet[i].type, type) && reg == registerSet[i].reg)
				{
					return registerSet[i].notation;
				}
			}
		}

		throw INTERNAL_ERROR;
	}

	const char *Operand::indexName() const
	{
		for(int i = 0; i < sizeof(registerSet) / sizeof(Operand::Register); i++)
		{
			if(indexReg == registerSet[i].reg && Operand::isSubtypeOf(registerSet[i].type, OPERAND_REG32))
			{
				return registerSet[i].notation;
			}
		}

		throw INTERNAL_ERROR;
	}

	const Operand::Register Operand::syntaxSet[] =
	{
		{OPERAND_VOID,		""},

		{OPERAND_ONE,		"1"},
		{OPERAND_IMM,		"imm"},
		{OPERAND_IMM8,		"imm8"},
		{OPERAND_IMM16,		"imm16"},
		{OPERAND_IMM32,		"imm32"},
	//	{OPERAND_IMM64,		"imm64"},

		{OPERAND_AL,		"AL"},
		{OPERAND_AX,		"AX"},
		{OPERAND_EAX,		"EAX"},
		{OPERAND_RAX,		"RAX"},
		{OPERAND_DX,		"DX"},
		{OPERAND_CL,		"CL"},
		{OPERAND_CX,		"CX"},
		{OPERAND_ECX,		"ECX"},
		{OPERAND_ST0,		"ST0"},

		{OPERAND_REG8,		"reg8"},
		{OPERAND_REG16,		"reg16"},
		{OPERAND_REG32,		"reg32"},
		{OPERAND_REG64,		"reg64"},
		{OPERAND_FPUREG,	"fpureg"},
		{OPERAND_MMREG,		"mmreg"},
		{OPERAND_XMMREG,	"xmmreg"},

		{OPERAND_MEM,		"mem"},
		{OPERAND_MEM8,		"mem8"},
		{OPERAND_MEM16,		"mem16"},
		{OPERAND_MEM32,		"mem32"},
		{OPERAND_MEM64,		"mem64"},
		{OPERAND_MEM128,	"mem128"},

		{OPERAND_R_M8,		"r/m8"},
		{OPERAND_R_M16,		"r/m16"},
		{OPERAND_R_M32,		"r/m32"},
		{OPERAND_R_M64,		"r/m64"},
		{OPERAND_R_M128,	"r/m128"},

		{OPERAND_XMM32,		"xmm32"},
		{OPERAND_XMM64,		"xmm64"},
		{OPERAND_MM64,		"mm64"}
	};

	Operand::Type Operand::scanSyntax(const char *string)
	{
		if(string)
		{
			for(int i = 0; i < sizeof(syntaxSet) / sizeof(Operand::Register); i++)
			{
				if(stricmp(string, syntaxSet[i].notation) == 0)
				{
					return syntaxSet[i].type;
				}
			}
		}

		return OPERAND_UNKNOWN;
	}
}
