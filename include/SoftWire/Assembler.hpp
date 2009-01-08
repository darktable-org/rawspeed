#ifndef SoftWire_Assembler_hpp
#define SoftWire_Assembler_hpp

#include "Operand.hpp"

namespace SoftWire
{
	class Synthesizer;
	class Instruction;
	class Linker;
	class Loader;
	class Error;
	class InstructionSet;

	class Assembler
	{
	public:
		Assembler(bool x64 = false);

		virtual ~Assembler();

		// Run-time intrinsics
		void label(const char *label);
		#include "Intrinsics.hpp"

		// Retrieve binary code
		void (*callable(const char *entryLabel = 0))();
		void (*finalize(const char *entryLable = 0))();
		void *acquire();

		// Error and debugging methods
		const char *getListing() const;
		void clearListing() const;
		void setEchoFile(const char *echoFile, const char *mode = "wt");
		void annotate(const char *format, ...);
		void reset();
		int instructionCount();

		static void enableListing();   // Default on
		static void disableListing();

		static const OperandAL al;
		static const OperandCL cl;
		static const OperandREG8 dl;
		static const OperandREG8 bl;
		static const OperandREG8 ah;
		static const OperandREG8 ch;
		static const OperandREG8 dh;
		static const OperandREG8 bh;
		static const OperandAL r0b;
		static const OperandCL r1b;
		static const OperandREG8 r2b;
		static const OperandREG8 r3b;
		static const OperandREG8 r4b;
		static const OperandREG8 r5b;
		static const OperandREG8 r6b;
		static const OperandREG8 r7b;
		static const OperandREG8 r8b;
		static const OperandREG8 r9b;
		static const OperandREG8 r10b;
		static const OperandREG8 r11b;
		static const OperandREG8 r12b;
		static const OperandREG8 r13b;
		static const OperandREG8 r14b;
		static const OperandREG8 r15b;

		static const OperandAX ax;
		static const OperandCX cx;
		static const OperandDX dx;
		static const OperandREG16 bx;
		static const OperandREG16 sp;
		static const OperandREG16 bp;
		static const OperandREG16 si;
		static const OperandREG16 di;
		static const OperandAX r0w;
		static const OperandCX r1w;
		static const OperandDX r2w;
		static const OperandREG16 r3w;
		static const OperandREG16 r4w;
		static const OperandREG16 r5w;
		static const OperandREG16 r6w;
		static const OperandREG16 r7w;
		static const OperandREG16 r8w;
		static const OperandREG16 r9w;
		static const OperandREG16 r10w;
		static const OperandREG16 r11w;
		static const OperandREG16 r12w;
		static const OperandREG16 r13w;
		static const OperandREG16 r14w;
		static const OperandREG16 r15w;

		static const OperandEAX eax;
		static const OperandECX ecx;
		static const OperandREG32 edx;
		static const OperandREG32 ebx;
		static const OperandREG32 esp;
		static const OperandREG32 ebp;
		static const OperandREG32 esi;
		static const OperandREG32 edi;
		static const OperandEAX r0d;
		static const OperandECX r1d;
		static const OperandREG32 r2d;
		static const OperandREG32 r3d;
		static const OperandREG32 r4d;
		static const OperandREG32 r5d;
		static const OperandREG32 r6d;
		static const OperandREG32 r7d;
		static const OperandREG32 r8d;
		static const OperandREG32 r9d;
		static const OperandREG32 r10d;
		static const OperandREG32 r11d;
		static const OperandREG32 r12d;
		static const OperandREG32 r13d;
		static const OperandREG32 r14d;
		static const OperandREG32 r15d;

		static const OperandREG64 rax;
		static const OperandREG64 rcx;
		static const OperandREG64 rdx;
		static const OperandREG64 rbx;
		static const OperandREG64 rsp;
		static const OperandREG64 rbp;
		static const OperandREG64 rsi;
		static const OperandREG64 rdi;
		static const OperandREG64 r0;
		static const OperandREG64 r1;
		static const OperandREG64 r2;
		static const OperandREG64 r3;
		static const OperandREG64 r4;
		static const OperandREG64 r5;
		static const OperandREG64 r6;
		static const OperandREG64 r7;
		static const OperandREG64 r8;
		static const OperandREG64 r9;
		static const OperandREG64 r10;
		static const OperandREG64 r11;
		static const OperandREG64 r12;
		static const OperandREG64 r13;
		static const OperandREG64 r14;
		static const OperandREG64 r15;

		static const OperandST0 st;
		static const OperandST0 st0;
		static const OperandFPUREG st1;
		static const OperandFPUREG st2;
		static const OperandFPUREG st3;
		static const OperandFPUREG st4;
		static const OperandFPUREG st5;
		static const OperandFPUREG st6;
		static const OperandFPUREG st7;

		static const OperandMMREG mm0;
		static const OperandMMREG mm1;
		static const OperandMMREG mm2;
		static const OperandMMREG mm3;
		static const OperandMMREG mm4;
		static const OperandMMREG mm5;
		static const OperandMMREG mm6;
		static const OperandMMREG mm7;

		static const OperandXMMREG xmm0;
		static const OperandXMMREG xmm1;
		static const OperandXMMREG xmm2;
		static const OperandXMMREG xmm3;
		static const OperandXMMREG xmm4;
		static const OperandXMMREG xmm5;
		static const OperandXMMREG xmm6;
		static const OperandXMMREG xmm7;

		static const OperandMEM8 byte_ptr;
		static const OperandMEM16 word_ptr;
		static const OperandMEM32 dword_ptr;
		static const OperandMEM64 mmword_ptr;
		static const OperandMEM64 qword_ptr;
		static const OperandMEM128 xmmword_ptr;
		static const OperandMEM128 xword_ptr;

	protected:
		virtual Encoding *x86(int instructionID,
		                      const Operand &firstOperand = Operand::OPERAND_VOID,
		                      const Operand &secondOperand = Operand::OPERAND_VOID,
		                      const Operand &thirdOperand = Operand::OPERAND_VOID);   // Assemble run-time intrinsic

		const bool x64;

	private:
		char *entryLabel;

		static InstructionSet *instructionSet;
		static int referenceCount;

		Synthesizer *synthesizer;
		Linker *linker;
		Loader *loader;

		char *echoFile;

		static bool listingEnabled;
	};
}

#endif   // SoftWire_Assembler_hpp
