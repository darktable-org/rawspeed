#ifndef SoftWire_Encoding_hpp
#define SoftWire_Encoding_hpp

namespace SoftWire
{
	class Synthesizer;
	class Instruction;

	class Encoding
	{
		friend class Synthesizer;

	public:
		enum Reg
		{
			REG_UNKNOWN = -1,

			R0 = 0, AL = 0, AX = 0, EAX = 0, RAX = 0, ST0 = 0, MM0 = 0, XMM0 = 0,
			R1 = 1, CL = 1, CX = 1, ECX = 1, RCX = 1, ST1 = 1, MM1 = 1, XMM1 = 1,
			R2 = 2, DL = 2, DX = 2, EDX = 2, RDX = 2, ST2 = 2, MM2 = 2, XMM2 = 2,
			R3 = 3, BL = 3, BX = 3, EBX = 3, RBX = 3, ST3 = 3, MM3 = 3, XMM3 = 3,
			R4 = 4, AH = 4, SP = 4, ESP = 4, RSP = 4, ST4 = 4, MM4 = 4, XMM4 = 4,
			R5 = 5, CH = 5, BP = 5, EBP = 5, RBP = 5, ST5 = 5, MM5 = 5, XMM5 = 5,
			R6 = 6, DH = 6, SI = 6, ESI = 6, RSI = 6, ST6 = 6, MM6 = 6, XMM6 = 6,
			R7 = 7, BH = 7, DI = 7, EDI = 7, RDI = 7, ST7 = 7, MM7 = 7, XMM7 = 7,
			R8 = 8,
			R9 = 9,
			R10 = 10,
			R11 = 11,
			R12 = 12,
			R13 = 13,
			R14 = 14,
			R15 = 15
		};

		enum Mod
		{
			MOD_NO_DISP = 0,
			MOD_BYTE_DISP = 1,
			MOD_DWORD_DISP = 2,
			MOD_REG = 3
		};

		enum Scale
		{
			SCALE_UNKNOWN = 0,
			SCALE_1 = 0,
			SCALE_2 = 1,
			SCALE_4 = 2,
			SCALE_8 = 3
		};

		Encoding(const Instruction *instruction = 0);
		Encoding(const Encoding &encoding);

		~Encoding();

		Encoding &operator=(const Encoding &encoding);

		void reset();

		const char *getLabel() const;
		const char *getReference() const;
		const char *getLiteral() const;
		int getImmediate() const;
		__int64 getDisplacement() const;

		void addPrefix(unsigned char p);

		int length(const unsigned char *buffer) const;   // Length of encoded instruction in bytes
		int writeCode(unsigned char *buffer, bool write = true) const;

		void setImmediate(int immediate);
		void setDisplacement(__int64 displacement);
		void addDisplacement(__int64 displacement);
		void setJumpOffset(int offset);
		void setCallOffset(int offset);
		void setLabel(const char *label);
		void setReference(const char *label);

		bool relativeReference() const;
		bool absoluteReference() const;
		bool hasDisplacement() const;
		bool hasImmediate() const;
		bool isRipRelative() const;

		void setAddress(const unsigned char *address);
		const unsigned char *getAddress() const;

		// Prevent or enable writing to output
		Encoding *reserve();
		void retain();
		bool isEmitting();

		int printCode(char *buffer) const;

	protected:
		const Instruction *instruction;

		char *label;
		union
		{
			char *reference;
			char *literal;
		};
		bool relative;

		struct
		{
			bool P1 : 1;
			bool P2 : 1;
			bool P3 : 1;
			bool P4 : 1;
			bool REX : 1;
			bool O4 : 1;
			bool O3 : 1;
			bool O2 : 1;
			bool O1 : 1;
			bool modRM : 1;
			bool SIB : 1;
			bool D1 : 1;
			bool D2 : 1;
			bool D3 : 1;
			bool D4 : 1;
			bool I1 : 1;
			bool I2 : 1;
			bool I3 : 1;
			bool I4 : 1;
		} format;

		unsigned char P1;   // Prefixes
		unsigned char P2;
		unsigned char P3;
		unsigned char P4;
		struct
		{
			union
			{
				struct
				{
					unsigned char B : 1;
					unsigned char X : 1;
					unsigned char R : 1;
					unsigned char W : 1;
					unsigned char prefix : 4;
				};

				unsigned char b;
			};
		} REX;
		unsigned char O1;   // Opcode
		unsigned char O2;
		unsigned char O3;   
		unsigned char O4;
		struct
		{
			union
			{
				struct
				{
					unsigned char r_m : 3;
					unsigned char reg : 3;
					unsigned char mod : 2;
				};

				unsigned char b;
			};
		} modRM;
		struct
		{
			union
			{
				struct
				{
					unsigned char base : 3;
					unsigned char index : 3;
					unsigned char scale : 2;
				};

				unsigned char b;
			};
		} SIB;
		union
		{
			__int64 displacement;

			struct
			{
				unsigned char D1;
				unsigned char D2;
				unsigned char D3;
				unsigned char D4;
			};
		};
		union
		{
			int immediate;

			struct
			{
				unsigned char I1;
				unsigned char I2;
				unsigned char I3;
				unsigned char I4;
			};
		};

		const unsigned char *address;

		bool emit;  // false for eliminated instructions

		static int align(unsigned char *output, int alignment, bool write);
	};
}

#endif   // SoftWire_Encoding_hpp
