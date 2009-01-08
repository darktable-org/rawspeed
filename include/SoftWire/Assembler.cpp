#include "Assembler.hpp"

#include "Linker.hpp"
#include "Loader.hpp"
#include "Error.hpp"
#include "Operand.hpp"
#include "Synthesizer.hpp"
#include "InstructionSet.hpp"
#include "String.hpp"

#include <time.h>

namespace SoftWire
{
	const OperandAL Assembler::al;
	const OperandCL Assembler::cl;
	const OperandREG8 Assembler::dl(Encoding::DL);
	const OperandREG8 Assembler::bl(Encoding::BL);
	const OperandREG8 Assembler::ah(Encoding::AH);
	const OperandREG8 Assembler::ch(Encoding::CH);
	const OperandREG8 Assembler::dh(Encoding::DH);
	const OperandREG8 Assembler::bh(Encoding::BH);
	const OperandAL Assembler::r0b;
	const OperandCL Assembler::r1b;
	const OperandREG8 Assembler::r2b(Encoding::R2);
	const OperandREG8 Assembler::r3b(Encoding::R3);
	const OperandREG8 Assembler::r4b(Encoding::R4);
	const OperandREG8 Assembler::r5b(Encoding::R5);
	const OperandREG8 Assembler::r6b(Encoding::R6);
	const OperandREG8 Assembler::r7b(Encoding::R7);
	const OperandREG8 Assembler::r8b(Encoding::R8);
	const OperandREG8 Assembler::r9b(Encoding::R9);
	const OperandREG8 Assembler::r10b(Encoding::R10);
	const OperandREG8 Assembler::r11b(Encoding::R11);
	const OperandREG8 Assembler::r12b(Encoding::R12);
	const OperandREG8 Assembler::r13b(Encoding::R13);
	const OperandREG8 Assembler::r14b(Encoding::R14);
	const OperandREG8 Assembler::r15b(Encoding::R15);

	const OperandAX Assembler::ax;
	const OperandCX Assembler::cx;
	const OperandDX Assembler::dx;
	const OperandREG16 Assembler::bx(Encoding::BX);
	const OperandREG16 Assembler::sp(Encoding::SP);
	const OperandREG16 Assembler::bp(Encoding::BP);
	const OperandREG16 Assembler::si(Encoding::SI);
	const OperandREG16 Assembler::di(Encoding::DI);
	const OperandAX Assembler::r0w;
	const OperandCX Assembler::r1w;
	const OperandDX Assembler::r2w;
	const OperandREG16 Assembler::r3w(Encoding::R3);
	const OperandREG16 Assembler::r4w(Encoding::R4);
	const OperandREG16 Assembler::r5w(Encoding::R5);
	const OperandREG16 Assembler::r6w(Encoding::R6);
	const OperandREG16 Assembler::r7w(Encoding::R7);
	const OperandREG16 Assembler::r8w(Encoding::R8);
	const OperandREG16 Assembler::r9w(Encoding::R9);
	const OperandREG16 Assembler::r10w(Encoding::R10);
	const OperandREG16 Assembler::r11w(Encoding::R11);
	const OperandREG16 Assembler::r12w(Encoding::R12);
	const OperandREG16 Assembler::r13w(Encoding::R13);
	const OperandREG16 Assembler::r14w(Encoding::R14);
	const OperandREG16 Assembler::r15w(Encoding::R15);

	const OperandEAX Assembler::eax;
	const OperandECX Assembler::ecx;
	const OperandREG32 Assembler::edx(Encoding::EDX);
	const OperandREG32 Assembler::ebx(Encoding::EBX);
	const OperandREG32 Assembler::esp(Encoding::ESP);
	const OperandREG32 Assembler::ebp(Encoding::EBP);
	const OperandREG32 Assembler::esi(Encoding::ESI);
	const OperandREG32 Assembler::edi(Encoding::EDI);
	const OperandEAX Assembler::r0d;
	const OperandECX Assembler::r1d;
	const OperandREG32 Assembler::r2d(Encoding::R2);
	const OperandREG32 Assembler::r3d(Encoding::R3);
	const OperandREG32 Assembler::r4d(Encoding::R4);
	const OperandREG32 Assembler::r5d(Encoding::R5);
	const OperandREG32 Assembler::r6d(Encoding::R6);
	const OperandREG32 Assembler::r7d(Encoding::R7);
	const OperandREG32 Assembler::r8d(Encoding::R8);
	const OperandREG32 Assembler::r9d(Encoding::R9);
	const OperandREG32 Assembler::r10d(Encoding::R10);
	const OperandREG32 Assembler::r11d(Encoding::R11);
	const OperandREG32 Assembler::r12d(Encoding::R12);
	const OperandREG32 Assembler::r13d(Encoding::R13);
	const OperandREG32 Assembler::r14d(Encoding::R14);
	const OperandREG32 Assembler::r15d(Encoding::R15);

	const OperandREG64 Assembler::rax(Encoding::R0);
	const OperandREG64 Assembler::rcx(Encoding::R1);
	const OperandREG64 Assembler::rdx(Encoding::R2);
	const OperandREG64 Assembler::rbx(Encoding::R3);
	const OperandREG64 Assembler::rsp(Encoding::R4);
	const OperandREG64 Assembler::rbp(Encoding::R5);
	const OperandREG64 Assembler::rsi(Encoding::R6);
	const OperandREG64 Assembler::rdi(Encoding::R7);
	const OperandREG64 Assembler::r0(Encoding::R0);
	const OperandREG64 Assembler::r1(Encoding::R1);
	const OperandREG64 Assembler::r2(Encoding::R2);
	const OperandREG64 Assembler::r3(Encoding::R3);
	const OperandREG64 Assembler::r4(Encoding::R4);
	const OperandREG64 Assembler::r5(Encoding::R5);
	const OperandREG64 Assembler::r6(Encoding::R6);
	const OperandREG64 Assembler::r7(Encoding::R7);
	const OperandREG64 Assembler::r8(Encoding::R8);
	const OperandREG64 Assembler::r9(Encoding::R9);
	const OperandREG64 Assembler::r10(Encoding::R10);
	const OperandREG64 Assembler::r11(Encoding::R11);
	const OperandREG64 Assembler::r12(Encoding::R12);
	const OperandREG64 Assembler::r13(Encoding::R13);
	const OperandREG64 Assembler::r14(Encoding::R14);
	const OperandREG64 Assembler::r15(Encoding::R15);

	const OperandST0 Assembler::st;
	const OperandST0 Assembler::st0;
	const OperandFPUREG Assembler::st1(Encoding::ST1);
	const OperandFPUREG Assembler::st2(Encoding::ST2);
	const OperandFPUREG Assembler::st3(Encoding::ST3);
	const OperandFPUREG Assembler::st4(Encoding::ST4);
	const OperandFPUREG Assembler::st5(Encoding::ST5);
	const OperandFPUREG Assembler::st6(Encoding::ST6);
	const OperandFPUREG Assembler::st7(Encoding::ST7);

	const OperandMMREG Assembler::mm0(Encoding::MM0);
	const OperandMMREG Assembler::mm1(Encoding::MM1);
	const OperandMMREG Assembler::mm2(Encoding::MM2);
	const OperandMMREG Assembler::mm3(Encoding::MM3);
	const OperandMMREG Assembler::mm4(Encoding::MM4);
	const OperandMMREG Assembler::mm5(Encoding::MM5);
	const OperandMMREG Assembler::mm6(Encoding::MM6);
	const OperandMMREG Assembler::mm7(Encoding::MM7);

	const OperandXMMREG Assembler::xmm0(Encoding::XMM0);
	const OperandXMMREG Assembler::xmm1(Encoding::XMM1);
	const OperandXMMREG Assembler::xmm2(Encoding::XMM2);
	const OperandXMMREG Assembler::xmm3(Encoding::XMM3);
	const OperandXMMREG Assembler::xmm4(Encoding::XMM4);
	const OperandXMMREG Assembler::xmm5(Encoding::XMM5);
	const OperandXMMREG Assembler::xmm6(Encoding::XMM6);
	const OperandXMMREG Assembler::xmm7(Encoding::XMM7);

	const OperandMEM8 Assembler::byte_ptr;
	const OperandMEM16 Assembler::word_ptr;
	const OperandMEM32 Assembler::dword_ptr;
	const OperandMEM64 Assembler::mmword_ptr;
	const OperandMEM64 Assembler::qword_ptr;
	const OperandMEM128 Assembler::xmmword_ptr;
	const OperandMEM128 Assembler::xword_ptr;

	InstructionSet *Assembler::instructionSet = 0;
	int Assembler::referenceCount = 0;
	bool Assembler::listingEnabled = true;

	Assembler::Assembler(bool x64) : x64(x64)
	{
		echoFile = 0;
		entryLabel = 0;

		if(!instructionSet)
		{
			instructionSet = new InstructionSet();
		}
	
		referenceCount++;

		linker = new Linker();
		loader = new Loader(*linker, x64);
		synthesizer = new Synthesizer(x64);
	}

	Assembler::~Assembler()
	{
		delete[] entryLabel;
		entryLabel = 0;

		delete linker;
		linker = 0;

		delete loader;
		loader = 0;

		delete synthesizer;
		synthesizer = 0;

		referenceCount--;
		if(!referenceCount)
		{
			delete instructionSet;
			instructionSet = 0;
		}

		delete[] echoFile;
		echoFile = 0;
	}

	void (*Assembler::callable(const char *entryLabel))()
	{
		if(!loader) return 0;

		if(entryLabel)
		{
			return loader->callable(entryLabel);
		}
		else
		{
			return loader->callable(this->entryLabel);
		}
	}

	void (*Assembler::finalize(const char *entryLabel))()
	{
		if(!loader) throw Error("Assembler could not be finalized (cannot re-finalize)");

		delete linker;
		linker = 0;

		delete synthesizer;
		synthesizer = 0;

		delete[] echoFile;
		echoFile = 0;

		if(entryLabel)
		{
			delete[] this->entryLabel;
			this->entryLabel = 0;

			return loader->finalize(entryLabel);
		}
		else
		{
			return loader->finalize(this->entryLabel);
		}
	}

	void *Assembler::acquire()
	{
		if(!loader) return 0;

		return loader->acquire();
	}

	const char *Assembler::getListing() const
	{
		return loader->getListing();
	}

	void Assembler::clearListing() const
	{
		loader->clearListing();
	}

	void Assembler::setEchoFile(const char *echoFile, const char *mode)
	{
		if(!listingEnabled) return;

		if(this->echoFile)
		{
			delete[] this->echoFile;
			this->echoFile = 0;
		}

		if(echoFile)
		{
			this->echoFile = strdup(echoFile);

			FILE *file = fopen(echoFile, mode);
			const time_t t = time(0);
			fprintf(file, "\n;%s\n", ctime(&t));
			fclose(file);
		}
	}

	void Assembler::annotate(const char *format, ...)
	{
		if(!echoFile) return;

		char buffer[256];
		va_list argList;

		va_start(argList, format);
		vsnprintf(buffer, 256, format, argList);
		va_end(argList);

		FILE *file = fopen(echoFile, "at");
		fprintf(file, "; ");
		fprintf(file, buffer);
		fprintf(file, "\n");
		fclose(file);
	}

	void Assembler::reset()
	{
		if(!loader) return;

		loader->reset();
	}

	int Assembler::instructionCount()
	{
		if(!loader)
		{
			return 0;
		}

		return loader->instructionCount();
	}

	void Assembler::enableListing()
	{
		listingEnabled = true;
	}

	void Assembler::disableListing()
	{
		listingEnabled = false;
	}

	Encoding *Assembler::x86(int instructionID, const Operand &firstOperand, const Operand &secondOperand, const Operand &thirdOperand)
	{
		if(!loader || !synthesizer || !instructionSet) throw INTERNAL_ERROR;

		const Instruction *instruction = instructionSet->instruction(instructionID);

		if(echoFile)
		{
			FILE *file = fopen(echoFile, "at");

			fprintf(file, "\t%s", instruction->getMnemonic());
			if(!Operand::isVoid(firstOperand)) fprintf(file, "\t%s", firstOperand.string());
			if(!Operand::isVoid(secondOperand)) fprintf(file, ",\t%s", secondOperand.string());
			if(!Operand::isVoid(thirdOperand)) fprintf(file, ",\t%s", thirdOperand.string());
			fprintf(file, "\n");

			fclose(file);
		}

		synthesizer->reset();

		synthesizer->encodeFirstOperand(firstOperand);
		synthesizer->encodeSecondOperand(secondOperand);
		synthesizer->encodeThirdOperand(thirdOperand);
		const Encoding &encoding = synthesizer->encodeInstruction(instruction);

		return loader->appendEncoding(encoding);
	}

	void Assembler::label(const char *label)
	{
		if(!loader || !synthesizer) return;

		if(echoFile)
		{
			FILE *file = fopen(echoFile, "at");
			fprintf(file, "%s:\n", label);
			fclose(file);
		}

		synthesizer->reset();

		synthesizer->defineLabel(label);
		const Encoding &encoding = synthesizer->encodeInstruction(0);

		loader->appendEncoding(encoding);
	}
};
