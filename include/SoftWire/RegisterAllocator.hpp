#ifndef SoftWire_RegisterAllocator_hpp
#define SoftWire_RegisterAllocator_hpp

#include "Assembler.hpp"

namespace SoftWire
{
	class RegisterAllocator : public Assembler
	{
	protected:
		struct AllocationData
		{
			AllocationData()
			{
				free();
			}

			void free()
			{
				reference = 0;
				priority = 0;
				partial = 0;

				copyInstruction = 0;
				loadInstruction = 0;
				spillInstruction = 0;
			}

			OperandREF reference;
			unsigned int priority;
			int partial;   // Number of bytes used, 0/1/2 for general-purpose, 0/4 for SSE, 0 means all

			Encoding *copyInstruction;
			Encoding *loadInstruction;
			Encoding *spillInstruction;
		};

		struct Allocation : AllocationData
		{
			Allocation()
			{
				free();
			}

			void free()
			{
				AllocationData::free();
				spill.free();
				modified = false;
			}

			AllocationData spill;
			bool modified;
		};

		struct State
		{
			Allocation GPR[16];
			Allocation MMX[16];
			Allocation XMM[16];
		};

	public:
		RegisterAllocator(bool x64);

		virtual ~RegisterAllocator();

		// Register allocation
		const OperandREG8 r8(const OperandREF &ref, bool copy = true);
		const OperandR_M8 m8(const OperandREF &ref);

		const OperandREG16 r16(const OperandREF &ref, bool copy = true);
		const OperandR_M16 m16(const OperandREF &ref);

		OperandREG32 r32(const OperandREF &ref, bool copy = true, int partial = 0);
		OperandR_M32 m32(const OperandREF &ref, int partial = 0);
		void free(const OperandREG32 &r32);
		void spill(const OperandREG32 &r32);

		OperandMMREG r64(const OperandREF &ref, bool copy = true);
		OperandMM64 m64(const OperandREF &ref);
		void free(const OperandMMREG &r64);
		void spill(const OperandMMREG &r64);

		OperandXMMREG r128(const OperandREF &ref, bool copy = true, bool ss = false);
		OperandR_M128 m128(const OperandREF &ref, bool ss = false);
		void free(const OperandXMMREG &r128);
		void spill(const OperandXMMREG &r128);

		OperandXMMREG rSS(const OperandREF &ref, bool copy = true, bool ss = true);
		OperandXMM32 mSS(const OperandREF &ref, bool ss = true);

		void free(const OperandREF &ref);
		void spill(const OperandREF &ref);

		void freeAll();
		void spillAll();
		void spillMMX();   // Specifically for using FPU after MMX
		void spillMMXcept(const OperandMMREG &r64);   // Empty MMX state but leave one associated

		const State capture();              // Capture register allocation state
		void restore(const State &state);   // Restore state to minimize spills

		// Temporarily exclude register from allocation (spill, then prioritize)
		void exclude(const OperandREG32 &r32);

		using Assembler::mov;
		Encoding *mov(OperandREG32 r32i, OperandREG32 r32j);
		Encoding *mov(OperandREG32 r32, OperandMEM32 m32);
		Encoding *mov(OperandREG32 r32, OperandR_M32 r_m32);

		using Assembler::movq;
		Encoding *movq(OperandMMREG r64i, OperandMMREG r64j);
		Encoding *movq(OperandMMREG r64, OperandMEM64 m64);
		Encoding *movq(OperandMMREG r64, OperandMM64 r_m64);

		using Assembler::movaps;
		Encoding *movaps(OperandXMMREG r128i, OperandXMMREG r128j);
		Encoding *movaps(OperandXMMREG r128, OperandMEM128 m128);
		Encoding *movaps(OperandXMMREG r128, OperandR_M128 r_m128);

		// Automatically emit emms when all MMX registers freed
		void enableAutoEMMS();   // Default off
		void disableAutoEMMS();

		// Optimization flags
		static void enableCopyPropagation();   // Default on
		static void disableCopyPropagation();

		static void enableLoadElimination();   // Default on
		static void disableLoadElimination();

		static void enableSpillElimination();   // Default on
		static void disableSpillElimination();

		static void enableMinimalRestore();   // Default on
		static void disableMinimalRestore();

		static void enableDropUnmodified();   // Default on
		static void disableDropUnmodified();

	protected:
		Encoding *x86(int instructionID, const Operand &firstOperand, const Operand &secondOperand, const Operand &thirdOperand);

		// Current allocation data
		static Allocation GPR[16];
		static Allocation MMX[16];
		static Allocation XMM[16];

	private:
		void markModified(const Operand &op);
		void markReferenced(const Operand &op);

		OperandREG32 allocate32(int i, const OperandREF &ref, bool copy, int partial);
		OperandREG32 prioritize32(int i);
		void free32(int i);
		Encoding *spill32(int i);
		void swap32(int i, int j);
		
		OperandMMREG allocate64(int i, const OperandREF &ref, bool copy);
		OperandMMREG prioritize64(int i);
		void free64(int i);
		Encoding *spill64(int i);
		void swap64(int i, int j);
		
		OperandXMMREG allocate128(int i, const OperandREF &ref, bool copy, bool ss);
		OperandXMMREG prioritize128(int i);
		void free128(int i);
		Encoding *spill128(int i);
		void swap128(int i, int j);

		static bool autoEMMS;
		static bool copyPropagation;
		static bool loadElimination;
		static bool spillElimination;
		static bool minimalRestore;
		static bool dropUnmodified;
	};
}

#endif   // SoftWire_RegisterAllocator_hpp
