#ifndef SoftWire_Operand_hpp
#define SoftWire_Operand_hpp

#include "Encoding.hpp"
#include "Error.hpp"

namespace SoftWire
{
	struct Specifier
	{
		enum Type
		{
			TYPE_UNKNOWN = 0,

			TYPE_NEAR,
			TYPE_SHORT = TYPE_NEAR,
		//	TYPE_FAR,
			
			TYPE_BYTE,
			TYPE_WORD,
			TYPE_DWORD,
		//	TYPE_TWORD,   // 80-bit long double not supported
			TYPE_QWORD,
			TYPE_MMWORD = TYPE_QWORD,
			TYPE_XMMWORD,
			TYPE_XWORD = TYPE_XMMWORD,
			TYPE_OWORD = TYPE_XMMWORD,

			TYPE_PTR
		};

		Type type;
		union
		{
			const char *reference;
			const char *notation;
		};

		static Specifier::Type scan(const char *string);

		static const Specifier specifierSet[];
	};

	struct OperandREG;

	struct Operand
	{
		enum Type
		{
			OPERAND_UNKNOWN	= 0,

			OPERAND_VOID	= 0x00000001,

			OPERAND_ONE		= 0x00000002,
			OPERAND_EXT8	= 0x00000004 | OPERAND_ONE,   // Sign extended
			OPERAND_REF		= 0x00000008,
			OPERAND_IMM8	= 0x00000010 | OPERAND_EXT8 | OPERAND_ONE,
			OPERAND_IMM16	= 0x00000020 | OPERAND_IMM8 | OPERAND_EXT8 | OPERAND_ONE,
			OPERAND_IMM32	= 0x00000040 | OPERAND_REF | OPERAND_IMM16 | OPERAND_IMM8 | OPERAND_EXT8 | OPERAND_ONE,
			OPERAND_IMM		= OPERAND_IMM32 | OPERAND_IMM16 | OPERAND_IMM8 | OPERAND_EXT8 | OPERAND_ONE,

			OPERAND_AL		= 0x00000080,
			OPERAND_CL		= 0x00000100,
			OPERAND_REG8	= OPERAND_CL | OPERAND_AL,

			OPERAND_AX		= 0x00000200,
			OPERAND_DX		= 0x00000400,
			OPERAND_CX		= 0x00000800,
			OPERAND_REG16	= OPERAND_CX | OPERAND_DX | OPERAND_AX,

			OPERAND_EAX		= 0x00001000,
			OPERAND_ECX		= 0x00002000,
			OPERAND_REG32	= OPERAND_ECX | OPERAND_EAX,

			OPERAND_RAX		= 0x00004000,
			OPERAND_REG64	= 0x00008000 | OPERAND_RAX,

			OPERAND_CS		= 0,   // No need to touch these in protected mode
			OPERAND_DS		= 0,
			OPERAND_ES		= 0,
			OPERAND_SS		= 0,
			OPERAND_FS		= 0,
			OPERAND_GS		= 0,
			OPERAND_SEGREG	= OPERAND_GS | OPERAND_FS | OPERAND_SS | OPERAND_ES | OPERAND_DS | OPERAND_CS,

			OPERAND_ST0		= 0x00010000,
			OPERAND_FPUREG	= 0x00020000 | OPERAND_ST0,
			
			OPERAND_CR		= 0,   // You won't need these in a JIT assembler
			OPERAND_DR		= 0,
			OPERAND_TR		= 0,

			OPERAND_MMREG	= 0x00040000,
			OPERAND_XMMREG	= 0x00080000,

			OPERAND_REG		= OPERAND_XMMREG | OPERAND_MMREG | OPERAND_TR | OPERAND_DR | OPERAND_CR | OPERAND_FPUREG | OPERAND_SEGREG | OPERAND_REG32 | OPERAND_REG64 | OPERAND_REG16 | OPERAND_REG8,

			OPERAND_MEM8	= 0x00100000,
			OPERAND_MEM16	= 0x00200000,
			OPERAND_MEM32	= 0x00400000,
			OPERAND_MEM64	= 0x00800000,
			OPERAND_MEM128	= 0x01000000,
			OPERAND_MEM		= OPERAND_MEM128 | OPERAND_MEM64 | OPERAND_MEM32 | OPERAND_MEM16 | OPERAND_MEM8,
		
			OPERAND_XMM32	= OPERAND_MEM32 | OPERAND_XMMREG,
			OPERAND_XMM64	= OPERAND_MEM64 | OPERAND_XMMREG,

			OPERAND_R_M8	= OPERAND_MEM8 | OPERAND_REG8,
			OPERAND_R_M16	= OPERAND_MEM16 | OPERAND_REG16,
			OPERAND_R_M32	= OPERAND_MEM32 | OPERAND_REG32,
			OPERAND_R_M64	= OPERAND_MEM64 | OPERAND_REG64,
			OPERAND_MM64	= OPERAND_MEM64 | OPERAND_MMREG,
			OPERAND_R_M128	= OPERAND_MEM128 | OPERAND_XMMREG,
			OPERAND_R_M		= OPERAND_MEM | OPERAND_REG
		};

		Operand(Type type = OPERAND_VOID)
		{
			this->type = type;
		}

		Type type;
		union
		{
			const char *reference;
			const char *notation;
		};

		union
		{
			int value;     // For immediates
			int reg;       // For registers
			int baseReg;   // For memory references;
		};

		int indexReg;
		int scale;
		int displacement;

		bool operator==(Operand &op);
		bool operator!=(Operand &op);

		static bool isSubtypeOf(Type type, Type baseType);
		bool isSubtypeOf(Type baseType) const;

		const char *string() const;

		static bool isVoid(Type type);
		static bool isImm(Type type);
		static bool isReg(Type type);
		static bool isMem(Type type);
		static bool isR_M(Type type);

		static bool isVoid(const Operand &operand);
		static bool isImm(const Operand &operand);
		static bool isReg(const Operand &operand);
		static bool isMem(const Operand &operand);
		static bool isR_M(const Operand &operand);

		const char *regName() const;
		const char *indexName() const;

		static Operand::Type scanSyntax(const char *string);

		struct Register
		{
			Type type;
			const char *notation;
			int reg;   // Index
		};

		static const Register registerSet[];
		static const Register syntaxSet[];
	};

	struct OperandVOID : virtual Operand
	{
		OperandVOID()
		{
			type = OPERAND_VOID;
		}
	};

	struct OperandIMM : virtual Operand
	{
		OperandIMM(int imm = 0)
		{
			type = OPERAND_IMM;
			value = imm;
			reference = 0;
		}
	};

	struct OperandREF : virtual Operand
	{
		OperandREF(const void *ref = 0)
		{
			type = OPERAND_REF;
			baseReg = Encoding::REG_UNKNOWN;
			indexReg = Encoding::REG_UNKNOWN;
			scale = 0;
			displacement = (int)ref;
			reference = 0;
		}

		OperandREF(const char *ref)
		{
			type = OPERAND_IMM;
			baseReg = Encoding::REG_UNKNOWN;
			indexReg = Encoding::REG_UNKNOWN;
			scale = 0;
			displacement = 0;
			reference = ref;
		}

		OperandREF(int ref)
		{
			type = OPERAND_REF;
			baseReg = Encoding::REG_UNKNOWN;
			indexReg = Encoding::REG_UNKNOWN;
			scale = 0;
			displacement = ref;
			reference = 0;
		}

		const OperandREF operator+(const void *disp) const
		{
			OperandREF returnReg;

			returnReg.baseReg = baseReg;
			returnReg.indexReg = indexReg;
			returnReg.scale = scale;
			returnReg.displacement = displacement + (int)disp;

			return returnReg;
		}

		const OperandREF operator+(int disp) const
		{
			OperandREF returnReg;

			returnReg.baseReg = baseReg;
			returnReg.indexReg = indexReg;
			returnReg.scale = scale;
			returnReg.displacement = displacement + disp;

			return returnReg;
		}

		const OperandREF operator-(int disp) const
		{
			OperandREF returnReg;

			returnReg.baseReg = baseReg;
			returnReg.indexReg = indexReg;
			returnReg.scale = scale;
			returnReg.displacement = displacement - disp;

			return returnReg;
		}

		bool operator==(const OperandREF &ref) const
		{
			return baseReg == ref.baseReg &&
			       indexReg == ref.indexReg &&
				   scale == ref.scale &&
				   displacement == ref.displacement;
		}

		bool operator!=(const OperandREF &ref) const
		{
			return !(*this == ref);
		}
	};

	struct OperandMEM : virtual Operand
	{
		OperandMEM()
		{
			type = OPERAND_MEM;
			baseReg = Encoding::REG_UNKNOWN;
			indexReg = Encoding::REG_UNKNOWN;
			scale = 0;
			displacement = 0;
			reference = 0;
		}

		OperandMEM(const OperandREF &ref)
		{
			type = OPERAND_MEM;
			baseReg = ref.baseReg;
			indexReg = ref.indexReg;
			scale = ref.scale;
			displacement = ref.displacement;
			reference = ref.reference;
		}

		OperandMEM operator[](const OperandREF &ref) const
		{
			return OperandMEM(ref);
		}

		const OperandMEM operator+(int disp) const
		{
			OperandMEM returnMem;

			returnMem.baseReg = baseReg;
			returnMem.indexReg = indexReg;
			returnMem.scale = scale;
			returnMem.displacement = displacement + disp;
			returnMem.reference = reference;

			return returnMem;
		}

		const OperandMEM operator-(int disp) const
		{
			OperandMEM returnMem;

			returnMem.baseReg = baseReg;
			returnMem.indexReg = indexReg;
			returnMem.scale = scale;
			returnMem.displacement = displacement - disp;
			returnMem.reference = reference;

			return returnMem;
		}
	};

	struct OperandMEM8 : OperandMEM
	{
		OperandMEM8() {};

		explicit OperandMEM8(const OperandREF &ref)
		{
			type = OPERAND_MEM8;
			baseReg = ref.baseReg;
			indexReg = ref.indexReg;
			scale = ref.scale;
			displacement = ref.displacement;
			reference = ref.reference;
		}

		explicit OperandMEM8(const OperandMEM &mem)
		{
			type = OPERAND_MEM8;
			baseReg = mem.baseReg;
			indexReg = mem.indexReg;
			scale = mem.scale;
			displacement = mem.displacement;
			reference = mem.reference;
		}

		explicit OperandMEM8(const Operand &r_m8)
		{
			type = OPERAND_MEM8;
			baseReg = r_m8.baseReg;
			indexReg = r_m8.indexReg;
			scale = r_m8.scale;
			displacement = r_m8.displacement;
			reference = r_m8.reference;
		}

		OperandMEM8 operator[](const OperandREF &ref) const
		{
			return OperandMEM8(ref);
		}

		const OperandMEM8 operator+(int disp) const
		{
			OperandMEM8 returnMem;

			returnMem.baseReg = baseReg;
			returnMem.indexReg = indexReg;
			returnMem.scale = scale;
			returnMem.displacement = displacement + disp;
			returnMem.reference = reference;

			return returnMem;
		}

		const OperandMEM8 operator-(int disp) const
		{
			OperandMEM8 returnMem;

			returnMem.baseReg = baseReg;
			returnMem.indexReg = indexReg;
			returnMem.scale = scale;
			returnMem.displacement = displacement - disp;
			returnMem.reference = reference;

			return returnMem;
		}
	};

	struct OperandMEM16 : OperandMEM
	{
		OperandMEM16() {};

		explicit OperandMEM16(const OperandREF &ref)
		{
			type = OPERAND_MEM16;
			baseReg = ref.baseReg;
			indexReg = ref.indexReg;
			scale = ref.scale;
			displacement = ref.displacement;
			reference = ref.reference;
		}

		explicit OperandMEM16(const OperandMEM &mem)
		{
			type = OPERAND_MEM16;
			baseReg = mem.baseReg;
			indexReg = mem.indexReg;
			scale = mem.scale;
			displacement = mem.displacement;
			reference = mem.reference;
		}

		explicit OperandMEM16(const Operand &r_m16)
		{
			type = OPERAND_MEM16;
			baseReg = r_m16.baseReg;
			indexReg = r_m16.indexReg;
			scale = r_m16.scale;
			displacement = r_m16.displacement;
			reference = r_m16.reference;
		}

		OperandMEM16 operator[](const OperandREF &ref) const
		{
			return OperandMEM16(ref);
		}

		const OperandMEM16 operator+(int disp) const
		{
			OperandMEM16 returnMem;

			returnMem.baseReg = baseReg;
			returnMem.indexReg = indexReg;
			returnMem.scale = scale;
			returnMem.displacement = displacement + disp;
			returnMem.reference = reference;

			return returnMem;
		}

		const OperandMEM16 operator-(int disp) const
		{
			OperandMEM16 returnMem;

			returnMem.baseReg = baseReg;
			returnMem.indexReg = indexReg;
			returnMem.scale = scale;
			returnMem.displacement = displacement - disp;
			returnMem.reference = reference;

			return returnMem;
		}
	};

	struct OperandMEM32 : OperandMEM
	{
		OperandMEM32() {};

		explicit OperandMEM32(const OperandMEM &mem)
		{
			type = OPERAND_MEM32;
			baseReg = mem.baseReg;
			indexReg = mem.indexReg;
			scale = mem.scale;
			displacement = mem.displacement;
			reference = mem.reference;
		}

		explicit OperandMEM32(const OperandREF &ref)
		{
			type = OPERAND_MEM32;
			baseReg = ref.baseReg;
			indexReg = ref.indexReg;
			scale = ref.scale;
			displacement = ref.displacement;
			reference = ref.reference;
		}

		explicit OperandMEM32(const Operand &r32)
		{
			type = OPERAND_MEM32;
			baseReg = r32.baseReg;
			indexReg = r32.indexReg;
			scale = r32.scale;
			displacement = r32.displacement;
			reference = r32.reference;
		}		

		OperandMEM32 operator[](const OperandREF &ref) const
		{
			return OperandMEM32(ref);
		}

		const OperandMEM32 operator+(int disp) const
		{
			OperandMEM32 returnMem;

			returnMem.baseReg = baseReg;
			returnMem.indexReg = indexReg;
			returnMem.scale = scale;
			returnMem.displacement = displacement + disp;
			returnMem.reference = reference;

			return returnMem;
		}

		const OperandMEM32 operator-(int disp) const
		{
			OperandMEM32 returnMem;

			returnMem.baseReg = baseReg;
			returnMem.indexReg = indexReg;
			returnMem.scale = scale;
			returnMem.displacement = displacement - disp;
			returnMem.reference = reference;

			return returnMem;
		}
	};

	struct OperandMEM64 : OperandMEM
	{
		OperandMEM64() {};

		explicit OperandMEM64(const OperandMEM &mem)
		{
			type = OPERAND_MEM64;
			baseReg = mem.baseReg;
			indexReg = mem.indexReg;
			scale = mem.scale;
			displacement = mem.displacement;
			reference = mem.reference;
		}

		explicit OperandMEM64(const OperandREF &ref)
		{
			type = OPERAND_MEM64;
			baseReg = ref.baseReg;
			indexReg = ref.indexReg;
			scale = ref.scale;
			displacement = ref.displacement;
			reference = ref.reference;
		}

		explicit OperandMEM64(const Operand &r_m64)
		{
			type = OPERAND_MEM64;
			baseReg = r_m64.baseReg;
			indexReg = r_m64.indexReg;
			scale = r_m64.scale;
			displacement = r_m64.displacement;
			reference = r_m64.reference;
		}

		OperandMEM64 operator[](const OperandREF &ref) const
		{
			return OperandMEM64(ref);
		}

		const OperandMEM64 operator+(int disp) const
		{
			OperandMEM64 returnMem;

			returnMem.baseReg = baseReg;
			returnMem.indexReg = indexReg;
			returnMem.scale = scale;
			returnMem.displacement = displacement + disp;
			returnMem.reference = reference;

			return returnMem;
		}

		const OperandMEM64 operator-(int disp) const
		{
			OperandMEM64 returnMem;

			returnMem.baseReg = baseReg;
			returnMem.indexReg = indexReg;
			returnMem.scale = scale;
			returnMem.displacement = displacement - disp;
			returnMem.reference = reference;

			return returnMem;
		}
	};

	struct OperandMEM128 : OperandMEM
	{
		OperandMEM128() {};

		explicit OperandMEM128(const OperandMEM &mem)
		{
			type = OPERAND_MEM128;
			baseReg = mem.baseReg;
			indexReg = mem.indexReg;
			scale = mem.scale;
			displacement = mem.displacement;
			reference = mem.reference;
		}

		explicit OperandMEM128(const OperandREF &ref)
		{
			type = OPERAND_MEM128;
			baseReg = ref.baseReg;
			indexReg = ref.indexReg;
			scale = ref.scale;
			displacement = ref.displacement;
			reference = ref.reference;
		}

		explicit OperandMEM128(const Operand &r_m128)
		{
			type = OPERAND_MEM128;
			baseReg = r_m128.baseReg;
			indexReg = r_m128.indexReg;
			scale = r_m128.scale;
			displacement = r_m128.displacement;
			reference = r_m128.reference;
		}

		OperandMEM128 operator[](const OperandREF &ref) const
		{
			return OperandMEM128(ref);
		}

		const OperandMEM128 operator+(int disp) const
		{
			OperandMEM128 returnMem;

			returnMem.baseReg = baseReg;
			returnMem.indexReg = indexReg;
			returnMem.scale = scale;
			returnMem.displacement = displacement + disp;
			returnMem.reference = reference;

			return returnMem;
		}

		const OperandMEM128 operator-(int disp) const
		{
			OperandMEM128 returnMem;

			returnMem.baseReg = baseReg;
			returnMem.indexReg = indexReg;
			returnMem.scale = scale;
			returnMem.displacement = displacement - disp;
			returnMem.reference = reference;

			return returnMem;
		}
	};

	struct OperandR_M32 : virtual Operand
	{
		OperandR_M32()
		{
			type = OPERAND_R_M32;
			baseReg = Encoding::REG_UNKNOWN;
			indexReg = Encoding::REG_UNKNOWN;
			scale = 0;
			displacement = 0;
			reference = 0;
		}

		explicit OperandR_M32(const Operand &reg)
		{
			type = reg.type;
			baseReg = reg.baseReg;
			indexReg = reg.indexReg;
			scale = reg.scale;
			displacement = reg.displacement;
			reference = reg.reference;
		}
	};

	struct OperandR_M16 : virtual Operand
	{
		OperandR_M16()
		{
			type = OPERAND_R_M16;
			baseReg = Encoding::REG_UNKNOWN;
			indexReg = Encoding::REG_UNKNOWN;
			scale = 0;
			displacement = 0;
			reference = 0;
		}

		explicit OperandR_M16(const Operand &reg)
		{
			type = reg.type;
			baseReg = reg.baseReg;
			indexReg = reg.indexReg;
			scale = reg.scale;
			displacement = reg.displacement;
			reference = reg.reference;
		}

		explicit OperandR_M16(const OperandR_M32 &r_m32)
		{
			type = OPERAND_R_M16;
			baseReg = r_m32.baseReg;
			indexReg = r_m32.indexReg;
			scale = r_m32.scale;
			displacement = r_m32.displacement;
			reference = r_m32.reference;
		}
	};

	struct OperandR_M8 : virtual Operand
	{
		OperandR_M8()
		{
			type = OPERAND_R_M8;
			baseReg = Encoding::REG_UNKNOWN;
			indexReg = Encoding::REG_UNKNOWN;
			scale = 0;
			displacement = 0;
			reference = 0;
		}

		explicit OperandR_M8(const Operand &reg)
		{
			type = reg.type;
			baseReg = reg.baseReg;
			indexReg = reg.indexReg;
			scale = reg.scale;
			displacement = reg.displacement;
			reference = reg.reference;
		}

		explicit OperandR_M8(const OperandR_M16 &r_m16)
		{
			type = OPERAND_R_M8;
			baseReg = r_m16.baseReg;
			indexReg = r_m16.indexReg;
			scale = r_m16.scale;
			displacement = r_m16.displacement;
			reference = r_m16.reference;
		}

		explicit OperandR_M8(const OperandR_M32 &r_m32)
		{
			type = OPERAND_R_M8;
			baseReg = r_m32.baseReg;
			indexReg = r_m32.indexReg;
			scale = r_m32.scale;
			displacement = r_m32.displacement;
			reference = r_m32.reference;
		}
	};

	struct OperandR_M64 : virtual Operand
	{
		OperandR_M64()
		{
			type = OPERAND_R_M64;
			baseReg = Encoding::REG_UNKNOWN;
			indexReg = Encoding::REG_UNKNOWN;
			scale = 0;
			displacement = 0;
			reference = 0;
		}

		explicit OperandR_M64(const Operand &reg)
		{
			type = reg.type;
			baseReg = reg.baseReg;
			indexReg = reg.indexReg;
			scale = reg.scale;
			displacement = reg.displacement;
			reference = reg.reference;
		}
	};

	struct OperandR_M128 : virtual Operand
	{
		OperandR_M128()
		{
			type = OPERAND_R_M128;
			baseReg = Encoding::REG_UNKNOWN;
			indexReg = Encoding::REG_UNKNOWN;
			scale = 0;
			displacement = 0;
			reference = 0;
		}

		explicit OperandR_M128(const Operand &reg)
		{
			type = reg.type;
			baseReg = reg.baseReg;
			indexReg = reg.indexReg;
			scale = reg.scale;
			displacement = reg.displacement;
			reference = reg.reference;
		}
	};

	struct OperandXMM32 : virtual Operand
	{
		OperandXMM32()
		{
			type = OPERAND_XMM32;
			baseReg = Encoding::REG_UNKNOWN;
			indexReg = Encoding::REG_UNKNOWN;
			scale = 0;
			displacement = 0;
			reference = 0;
		}

		explicit OperandXMM32(const Operand &reg)
		{
			type = reg.type;
			baseReg = reg.baseReg;
			indexReg = reg.indexReg;
			scale = reg.scale;
			displacement = reg.displacement;
			reference = reg.reference;
		}
	};

	struct OperandXMM64 : virtual Operand
	{
		OperandXMM64()
		{
			type = OPERAND_XMM64;
			baseReg = Encoding::REG_UNKNOWN;
			indexReg = Encoding::REG_UNKNOWN;
			scale = 0;
			displacement = 0;
			reference = 0;
		}

		explicit OperandXMM64(const Operand &reg)
		{
			type = reg.type;
			baseReg = reg.baseReg;
			indexReg = reg.indexReg;
			scale = reg.scale;
			displacement = reg.displacement;
			reference = reg.reference;
		}
	};

	struct OperandMM64 : virtual Operand
	{
		OperandMM64()
		{
			type = OPERAND_MM64;
			baseReg = Encoding::REG_UNKNOWN;
			indexReg = Encoding::REG_UNKNOWN;
			scale = 0;
			displacement = 0;
			reference = 0;
		}

		explicit OperandMM64(const Operand &reg)
		{
			type = reg.type;
			baseReg = reg.baseReg;
			indexReg = reg.indexReg;
			scale = reg.scale;
			displacement = reg.displacement;
			reference = reg.reference;
		}
	};

	struct OperandREG : virtual Operand
	{
		OperandREG()
		{
			type = OPERAND_VOID;
		}

		OperandREG(const Operand &reg)
		{
			type = reg.type;
			this->reg = reg.reg;
			reference = reg.reference;
		}
	};

	struct OperandREG32;
	struct OperandREG64;

	struct OperandREGxX : OperandREF
	{
		OperandREGxX(int reg = Encoding::REG_UNKNOWN)
		{
			type = OPERAND_REG32;
			this->reg = reg;
			indexReg = Encoding::REG_UNKNOWN;
			scale = 0;
			displacement = 0;
			reference = 0;
		}

		friend const OperandREF operator+(const OperandREGxX &ref1, const OperandREG32 &ref2);
		friend const OperandREF operator+(const OperandREGxX &ref1, const OperandREG64 &ref2);
		friend const OperandREGxX operator+(const OperandREGxX &ref, void *disp);
		friend const OperandREGxX operator+(const OperandREGxX &ref, int disp);
		friend const OperandREGxX operator-(const OperandREGxX &ref, int disp);

		friend const OperandREF operator+(const OperandREG32 &ref2, const OperandREGxX &ref1);
		friend const OperandREF operator+(const OperandREG64 &ref2, const OperandREGxX &ref1);
		friend const OperandREGxX operator+(void *disp, const OperandREGxX &ref);
		friend const OperandREGxX operator+(int disp, const OperandREGxX &ref);
		friend const OperandREGxX operator-(int disp, const OperandREGxX &ref);
	};

	struct OperandREG32 : OperandR_M32, OperandREF, OperandREG
	{
		OperandREG32(int reg = Encoding::REG_UNKNOWN)
		{
			type = OPERAND_REG32;
			this->reg = reg;
			indexReg = Encoding::REG_UNKNOWN;
			scale = 0;
			displacement = 0;
			reference = 0;
		}

		explicit OperandREG32(const OperandR_M32 &r_m32)
		{
			type = OPERAND_REG32;
			reg = r_m32.reg;
			indexReg = Encoding::REG_UNKNOWN;
			scale = 0;
			displacement = 0;
			reference = 0;
		}

		friend const OperandREF operator+(const OperandREG32 ref, const OperandREG32 &ref2);

		friend const OperandREG32 operator+(const OperandREG32 ref, void *disp);
		friend const OperandREG32 operator+(const OperandREG32 ref, int disp);
		friend const OperandREG32 operator-(const OperandREG32 ref, int disp);
		friend const OperandREGxX operator*(const OperandREG32 ref, int scale);

		friend const OperandREG32 operator+(void *disp, const OperandREG32 ref);
		friend const OperandREG32 operator+(int disp, const OperandREG32 ref);
		friend const OperandREG32 operator-(int disp, const OperandREG32 ref);
		friend const OperandREGxX operator*(int scale, const OperandREG32 ref);
	};

	struct OperandREG64 : OperandR_M64, OperandREF, OperandREG
	{
		OperandREG64(int reg = Encoding::REG_UNKNOWN)
		{
			type = OPERAND_REG64;
			this->reg = reg;
			indexReg = Encoding::REG_UNKNOWN;
			scale = 0;
			displacement = 0;
			reference = 0;
		}

		explicit OperandREG64(const OperandR_M64 &r_m64)
		{
			type = OPERAND_REG64;
			reg = r_m64.reg;
			indexReg = Encoding::REG_UNKNOWN;
			scale = 0;
			displacement = 0;
			reference = 0;
		}

		friend const OperandREF operator+(const OperandREG64 ref, const OperandREG64 &ref2);

		friend const OperandREG64 operator+(const OperandREG64 ref, void *disp);
		friend const OperandREG64 operator+(const OperandREG64 ref, int disp);
		friend const OperandREG64 operator-(const OperandREG64 ref, int disp);
		friend const OperandREGxX operator*(const OperandREG64 ref, int scale);

		friend const OperandREG64 operator+(void *disp, const OperandREG64 ref);
		friend const OperandREG64 operator+(int disp, const OperandREG64 ref);
		friend const OperandREG64 operator-(int disp, const OperandREG64 ref);
		friend const OperandREGxX operator*(int scale, const OperandREG64 ref);
	};

	struct OperandREG16 : OperandR_M16, OperandREG
	{
		OperandREG16(int reg = Encoding::REG_UNKNOWN)
		{
			type = OPERAND_REG16;
			this->reg = reg;
			reference = 0;
		}

		explicit OperandREG16(const OperandREG32 &r32)
		{
			type = OPERAND_REG16;
			reg = r32.reg;
			reference = 0;
		}

		explicit OperandREG16(const OperandR_M16 &r_m16)
		{
			type = OPERAND_REG16;
			reg = r_m16.reg;
			reference = 0;
		}
	};

	struct OperandREG8 : OperandR_M8, OperandREG
	{
		OperandREG8(int reg = Encoding::REG_UNKNOWN)
		{
			type = OPERAND_REG8;
			this->reg = reg;
			reference = 0;
		}

		explicit OperandREG8(const OperandREG16 &r16)
		{
			type = OPERAND_REG8;
			reg = r16.reg;
			reference = 0;
		}

		explicit OperandREG8(const OperandREG32 &r32)
		{
			type = OPERAND_REG8;
			reg = r32.reg;
			reference = 0;
		}

		explicit OperandREG8(const OperandR_M8 &r_m8)
		{
			type = OPERAND_REG8;
			reg = r_m8.reg;
			reference = 0;
		}
	};

	struct OperandFPUREG : virtual Operand, OperandREG
	{
		OperandFPUREG(int reg = Encoding::REG_UNKNOWN)
		{
			type = OPERAND_FPUREG;
			this->reg = reg;
			reference = 0;
		}
	};

	struct OperandMMREG : OperandMM64, OperandREG
	{
		OperandMMREG(int reg = Encoding::REG_UNKNOWN)
		{
			type = OPERAND_MMREG;
			this->reg = reg;
			reference = 0;
		}

		explicit OperandMMREG(const OperandMM64 r_m64)
		{
			type = OPERAND_MMREG;
			reg = r_m64.reg;
			reference = 0;
		}
	};

	struct OperandXMMREG : OperandR_M128, OperandXMM32, OperandXMM64, OperandREG
	{
		OperandXMMREG(int reg = Encoding::REG_UNKNOWN)
		{
			type = OPERAND_XMMREG;
			this->reg = reg;
			reference = 0;
		}

		explicit OperandXMMREG(const OperandXMM32 &xmm32)
		{
			type = OPERAND_XMMREG;
			reg = xmm32.reg;
			reference = 0;
		}

		explicit OperandXMMREG(const OperandXMM64 &xmm64)
		{
			type = OPERAND_XMMREG;
			reg = xmm64.reg;
			reference = 0;
		}

		explicit OperandXMMREG(const OperandR_M128 &r_m128)
		{
			type = OPERAND_XMMREG;
			reg = r_m128.reg;
			reference = 0;
		}
	};

	struct OperandAL : OperandREG8
	{
		OperandAL()
		{
			type = OPERAND_AL;
			reg = Encoding::AL;
			reference = 0;
		}
	};

	struct OperandCL : OperandREG8
	{
		OperandCL()
		{
			type = OPERAND_CL;
			reg = Encoding::CL;
			reference = 0;
		}
	};

	struct OperandAX : OperandREG16
	{
		OperandAX()
		{
			type = OPERAND_AX;
			reg = Encoding::AX;
			reference = 0;
		}
	};

	struct OperandDX : OperandREG16
	{
		OperandDX()
		{
			type = OPERAND_DX;
			reg = Encoding::DX;
			reference = 0;
		}
	};

	struct OperandCX : OperandREG16
	{
		OperandCX()
		{
			type = OPERAND_CX;
			reg = Encoding::CX;
			reference = 0;
		}
	};

	struct OperandEAX : OperandREG32
	{
		OperandEAX()
		{
			type = OPERAND_EAX;
			reg = Encoding::EAX;
			reference = 0;
		}
	};

	struct OperandRAX : OperandREG32
	{
		OperandRAX()
		{
			type = OPERAND_RAX;
			reg = Encoding::RAX;
			reference = 0;
		}
	};

	struct OperandECX : OperandREG32
	{
		OperandECX()
		{
			type = OPERAND_ECX;
			reg = Encoding::ECX;
			reference = 0;
		}
	};

	struct OperandST0 : OperandFPUREG
	{
		OperandST0()
		{
			type = OPERAND_ST0;
			reg = Encoding::ST0;
			reference = 0;
		}
	};
}

namespace SoftWire
{
	inline const OperandREF operator+(const OperandREGxX &ref1, const OperandREG32 &ref2)
	{
		OperandREF returnReg;

		returnReg.baseReg = ref2.baseReg;
		returnReg.indexReg = ref1.indexReg;
		returnReg.scale = ref1.scale;
		returnReg.displacement = ref1.displacement + ref2.displacement;

		return returnReg;
	}

	inline const OperandREF operator+(const OperandREGxX &ref1, const OperandREG64 &ref2)
	{
		OperandREF returnReg;

		returnReg.baseReg = ref2.baseReg;
		returnReg.indexReg = ref1.indexReg;
		returnReg.scale = ref1.scale;
		returnReg.displacement = ref1.displacement + ref2.displacement;

		return returnReg;
	}

	inline const OperandREGxX operator+(const OperandREGxX &ref, void *disp)
	{
		OperandREGxX returnReg;

		returnReg.baseReg = ref.baseReg;
		returnReg.indexReg = ref.indexReg;
		returnReg.scale = ref.scale;
		returnReg.displacement = ref.displacement + (int)disp;

		return returnReg;
	}

	inline const OperandREGxX operator+(const OperandREGxX &ref, int disp)
	{
		OperandREGxX returnReg;

		returnReg.baseReg = ref.baseReg;
		returnReg.indexReg = ref.indexReg;
		returnReg.scale = ref.scale;
		returnReg.displacement = ref.displacement + disp;

		return returnReg;
	}

	inline const OperandREGxX operator-(const OperandREGxX &ref, int disp)
	{
		OperandREGxX returnReg;

		returnReg.baseReg = ref.baseReg;
		returnReg.indexReg = ref.indexReg;
		returnReg.scale = ref.scale;
		returnReg.displacement = ref.displacement - disp;

		return returnReg;
	}

	inline const OperandREF operator+(const OperandREG32 &ref2, const OperandREGxX &ref1)
	{
		return ref1 + ref2;
	}

	inline const OperandREF operator+(const OperandREG64 &ref2, const OperandREGxX &ref1)
	{
		return ref1 + ref2;
	}

	inline const OperandREGxX operator+(void *disp, const OperandREGxX &ref)
	{
		return ref + disp;
	}

	inline const OperandREGxX operator+(int disp, const OperandREGxX &ref)
	{
		return ref + disp;
	}

	inline const OperandREGxX operator-(int disp, const OperandREGxX &ref)
	{
		return ref + disp;
	}

	inline const OperandREF operator+(const OperandREG32 ref1, const OperandREG32 &ref2)
	{
		OperandREF returnReg;

		returnReg.baseReg = ref1.reg;
		returnReg.indexReg = ref2.reg;
		returnReg.scale = 1;
		returnReg.displacement = ref1.displacement + ref2.displacement;

		return returnReg;
	}

	inline const OperandREG32 operator+(const OperandREG32 ref, void *disp)
	{
		OperandREG32 returnReg;

		returnReg.baseReg = ref.baseReg;
		returnReg.indexReg = ref.indexReg;
		returnReg.scale = ref.scale;
		returnReg.displacement = ref.displacement + (int)disp;

		return returnReg;
	}

	inline const OperandREG32 operator+(const OperandREG32 ref, int disp)
	{
		OperandREG32 returnReg;

		returnReg.baseReg = ref.baseReg;
		returnReg.indexReg = ref.indexReg;
		returnReg.scale = ref.scale;
		returnReg.displacement = ref.displacement + disp;

		return returnReg;
	}

	inline const OperandREG32 operator-(const OperandREG32 ref, int disp)
	{
		OperandREG32 returnReg;

		returnReg.baseReg = ref.baseReg;
		returnReg.indexReg = ref.indexReg;
		returnReg.scale = ref.scale;
		returnReg.displacement = ref.displacement - disp;

		return returnReg;
	}

	inline const OperandREGxX operator*(const OperandREG32 ref, int scale)
	{
		OperandREGxX returnReg;

		returnReg.baseReg = Encoding::REG_UNKNOWN;
		returnReg.indexReg = ref.baseReg;
		returnReg.scale = scale;
		returnReg.displacement = ref.displacement;

		return returnReg;
	}

	inline const OperandREG32 operator+(void *disp, const OperandREG32 ref)
	{
		return ref + disp;
	}

	inline const OperandREG32 operator+(int disp, const OperandREG32 ref)
	{
		return ref + disp;
	}

	inline const OperandREG32 operator-(int disp, const OperandREG32 ref)
	{
		return ref - disp;
	}

	inline const OperandREGxX operator*(int scale, const OperandREG32 ref)
	{
		return ref * scale;
	}

	inline const OperandREF operator+(const OperandREG64 ref1, const OperandREG64 &ref2)
	{
		OperandREF returnReg;

		returnReg.baseReg = ref1.reg;
		returnReg.indexReg = ref2.reg;
		returnReg.scale = 1;
		returnReg.displacement = ref1.displacement + ref2.displacement;

		return returnReg;
	}

	inline const OperandREG64 operator+(const OperandREG64 ref, void *disp)
	{
		OperandREG64 returnReg;

		returnReg.baseReg = ref.baseReg;
		returnReg.indexReg = ref.indexReg;
		returnReg.scale = ref.scale;
		returnReg.displacement = ref.displacement + (int)disp;

		return returnReg;
	}

	inline const OperandREG64 operator+(const OperandREG64 ref, int disp)
	{
		OperandREG64 returnReg;

		returnReg.baseReg = ref.baseReg;
		returnReg.indexReg = ref.indexReg;
		returnReg.scale = ref.scale;
		returnReg.displacement = ref.displacement + disp;

		return returnReg;
	}

	inline const OperandREG64 operator-(const OperandREG64 ref, int disp)
	{
		OperandREG64 returnReg;

		returnReg.baseReg = ref.baseReg;
		returnReg.indexReg = ref.indexReg;
		returnReg.scale = ref.scale;
		returnReg.displacement = ref.displacement - disp;

		return returnReg;
	}

	inline const OperandREGxX operator*(const OperandREG64 ref, int scale)
	{
		OperandREGxX returnReg;

		returnReg.baseReg = Encoding::REG_UNKNOWN;
		returnReg.indexReg = ref.baseReg;
		returnReg.scale = scale;
		returnReg.displacement = ref.displacement;

		return returnReg;
	}

	inline const OperandREG64 operator+(void *disp, const OperandREG64 ref)
	{
		return ref + disp;
	}

	inline const OperandREG64 operator+(int disp, const OperandREG64 ref)
	{
		return ref + disp;
	}

	inline const OperandREG64 operator-(int disp, const OperandREG64 ref)
	{
		return ref - disp;
	}

	inline const OperandREGxX operator*(int scale, const OperandREG64 ref)
	{
		return ref * scale;
	}
}

#endif   // SoftWire_Operand_hpp
