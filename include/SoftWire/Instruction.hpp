#ifndef SoftWire_Instruction_hpp
#define SoftWire_Instruction_hpp

#include "Operand.hpp"

namespace SoftWire
{
	enum
	{
		/*
			+r Add register value to opcode
			/# Value for Mod R/M register field encoding
			/r Effective address encoding
			ib Byte immediate
			iw Word immediate
			id Dword immediate
			-b Byte relative address
			-i Word or dword relative address
			p0 LOCK instruction prefix (F0h)
			p2 REPNE/REPNZ instruction prefix (F2h)
			p3 REP/REPE/REPZ instruction prefix (F3h) (also SSE prefix)
			po Offset override prefix (66h)
			pa Address override prefix (67h) 
		*/

		ADD_REG		= ('+' << 8) | 'r',
		EFF_ADDR	= ('/' << 8) | 'r',
		MOD_RM_0	= ('/' << 8) | '0',
		MOD_RM_1	= ('/' << 8) | '1',
		MOD_RM_2	= ('/' << 8) | '2',
		MOD_RM_3	= ('/' << 8) | '3',
		MOD_RM_4	= ('/' << 8) | '4',
		MOD_RM_5	= ('/' << 8) | '5',
		MOD_RM_6	= ('/' << 8) | '6',
		MOD_RM_7	= ('/' << 8) | '7',
		BYTE_IMM	= ('i' << 8) | 'b',
		WORD_IMM	= ('i' << 8) | 'w',
		DWORD_IMM	= ('i' << 8) | 'd',
		QWORD_IMM	= ('i' << 8) | 'q',
		BYTE_REL	= ('-' << 8) | 'b',
		DWORD_REL	= ('-' << 8) | 'i',
		LOCK_PRE	= ('p' << 8) | '0',
		CONST_PRE	= ('p' << 8) | '1',
		REPNE_PRE	= ('p' << 8) | '2',
		REP_PRE		= ('p' << 8) | '3',
		OFF_PRE		= ('p' << 8) | 'o',
		ADDR_PRE	= ('p' << 8) | 'a'
	};

	class Instruction
	{
	public:
		enum
		{
			CPU_UNKNOWN		= 0x00000000,

			CPU_8086		= 0x00000001,
			CPU_186			= 0x00000002 | CPU_8086,
			CPU_286			= 0x00000004 | CPU_186,
			CPU_386			= 0x00000008 | CPU_286,
			CPU_486			= 0x00000010 | CPU_386,
			CPU_P5			= 0x00000020 | CPU_486,      // Pentium
			CPU_PENTIUM		= CPU_P5,
			CPU_P6			= 0x00000040 | CPU_PENTIUM,   // Pentium Pro

			CPU_FPU			= 0x00000080,
			CPU_MMX			= 0x00000100 | CPU_PENTIUM,
			CPU_KATMAI		= 0x00000200 | CPU_MMX,
			CPU_SSE			= 0x00000400 | CPU_KATMAI,

			CPU_P7			= 0x00000800 | CPU_SSE,   // Pentium 4
			CPU_WILLAMETTE	= CPU_P7,
			CPU_SSE2		= 0x00001000 | CPU_WILLAMETTE,
			CPU_PNI			= 0x00002000,

			CPU_AMD			= 0x00004000,   // AMD specific system calls
			CPU_CYRIX		= 0x00008000,
			CPU_3DNOW		= 0x00010000 | CPU_AMD,
			CPU_ATHLON		= 0x00020000 | CPU_3DNOW,
			CPU_SMM			= 0x00040000,   // System Management Mode, standby mode

			CPU_UNDOC		= 0x00080000,   // Undocumented, also not supported by Visual Studio inline assembler
			CPU_PRIV		= 0x00100000,   // Priviledged

			CPU_X64			= 0x00200000 | CPU_SSE2,    // x86-64
			CPU_INVALID64	= 0x00400000,    // Invalid instruction in x86-64 long mode

			CPU_SSSE3		= 0x00800000 | CPU_PNI,
			CPU_SSE4_1		= 0x01000000 | CPU_SSSE3,
			CPU_SSE4_2	= 0x02000000 | CPU_SSE4_1
		};

		struct Syntax
		{
			char *mnemonic;
			char *operands;
			char *encoding;
			int flags;
		};

		Instruction();
		Instruction(const Syntax *syntax);

		~Instruction();

		Instruction &operator=(const Instruction &instruction);
		
		Operand::Type getFirstOperand() const;
		Operand::Type getSecondOperand() const;
		Operand::Type getThirdOperand() const;

		const char *getMnemonic() const;
		const char *getOperandSyntax() const;
		const char *getEncoding() const;
		
		bool is16Bit() const;
		bool is32Bit() const;
		bool is64Bit() const;
		bool isInvalid64() const;

		int approximateSize() const;

	private:
		const Syntax *syntax;
		Specifier::Type specifier;
		Operand::Type firstOperand;
		Operand::Type secondOperand;
		Operand::Type thirdOperand;

		void extractOperands(const char *syntax);
	};
}

#endif   // SoftWire_Instruction_hpp
