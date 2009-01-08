#include "RegisterAllocator.hpp"

#include "Error.hpp"

namespace SoftWire
{
	RegisterAllocator::Allocation RegisterAllocator::GPR[16];
	RegisterAllocator::Allocation RegisterAllocator::MMX[16];
	RegisterAllocator::Allocation RegisterAllocator::XMM[16];

	bool RegisterAllocator::autoEMMS = false;
	bool RegisterAllocator::copyPropagation = true;
	bool RegisterAllocator::loadElimination = true;
	bool RegisterAllocator::spillElimination = true;
	bool RegisterAllocator::minimalRestore = true;
	bool RegisterAllocator::dropUnmodified = true;

	RegisterAllocator::RegisterAllocator(bool x64) : Assembler(x64)
	{
		// Completely eraze allocation state
		{for(int i = 0; i < 16; i++)
		{
			GPR[i].free();
			XMM[i].free();
		}}

		{for(int i = 0; i < 8; i++)
		{
			MMX[i].free();
		}}
	}

	RegisterAllocator::~RegisterAllocator()
	{
		// Completely eraze allocation state
		for(int i = 0; i < 8; i++)
		{
			GPR[i].free();
			MMX[i].free();
			XMM[i].free();
		}
	}

	const OperandREG8 RegisterAllocator::r8(const OperandREF &ref, bool copy)
	{
		OperandREG32 reg = r32(ref, copy);

		// Make sure we only have al, cl, dl or bl
		if(reg.reg >= 4)
		{
			spill(reg);

			// Need to spill one of al, cl, dl or bl
			int candidate = 0;
			unsigned int priority = 0xFFFFFFFF;

			for(int i = 0; i < 4; i++)
			{
				if(GPR[i].priority < priority)
				{
					priority = GPR[i].priority;
					candidate = i;
				}
			}

			spill(OperandREG32(candidate));

			return (OperandREG8)allocate32(candidate, ref, copy, 1);
		}

		return (OperandREG8)reg;
	}

	const OperandR_M8 RegisterAllocator::m8(const OperandREF &ref)
	{
		return (OperandR_M8)m32(ref, 1);
	}

	const OperandREG16 RegisterAllocator::r16(const OperandREF &ref, bool copy)
	{
		return (OperandREG16)r32(ref, copy, 2);
	}

	const OperandR_M16 RegisterAllocator::m16(const OperandREF &ref)
	{
		return (OperandR_M16)m32(ref, 2);
	}

	OperandREG32 RegisterAllocator::r32(const OperandREF &ref, bool copy, int partial)
	{
		if(ref == 0 && copy) throw Error("Cannot dereference 0");

		// Check if already allocated
		{for(int i = 0; i < 8; i++)
		{
			if(i == Encoding::ESP || i == Encoding::EBP) continue;

			if(GPR[i].reference == ref)
			{
				return prioritize32(i);
			}
		}}

		// Check spilled but unused registers
		if(spillElimination)
		{
			for(int i = 0; i < 8; i++)
			{
				if(i == Encoding::ESP || i == Encoding::EBP) continue;

				if(GPR[i].priority == 0 && GPR[i].spill.reference == ref)
				{
					if(GPR[i].spillInstruction)
					{
						GPR[i].spillInstruction->reserve();
					}

					GPR[i].reference = GPR[i].spill.reference;
					GPR[i].partial = GPR[i].spill.partial;
					GPR[i].priority = GPR[i].spill.priority;
					GPR[i].copyInstruction = GPR[i].spill.copyInstruction;
					GPR[i].loadInstruction = GPR[i].spill.loadInstruction;
					GPR[i].spillInstruction = GPR[i].spill.spillInstruction;

					GPR[i].spill.free();

					return prioritize32(i);
				}
			}
		}

		// Search for free registers
		{for(int i = 0; i < 8; i++)
		{
			if(i == Encoding::ESP || i == Encoding::EBP) continue;

			if(GPR[i].priority == 0 && GPR[i].spill.priority == 0)
			{
				return allocate32(i, ref, copy, partial);
			}
		}}
		
		{for(int i = 0; i < 8; i++)
		{
			if(i == Encoding::ESP || i == Encoding::EBP) continue;

			if(GPR[i].priority == 0)
			{
				return allocate32(i, ref, copy, partial);
			}
		}}

		// Need to spill one
		int candidate = 0;
		int betterCandidate = -1;
		unsigned int priority = 0xFFFFFFFF;

		{for(int i = 0; i < 8; i++)
		{
			if(i == Encoding::ESP || i == Encoding::EBP) continue;

			if(GPR[i].priority < priority)
			{
				priority = GPR[i].priority;
				candidate = i;

				if(!GPR[i].modified && GPR[i].priority < 0xFFFFFFFF - 2)
				{
					betterCandidate = i;
				}
			}
		}}

		if(betterCandidate != -1)
		{
			candidate = betterCandidate;
		}

		Encoding *spillInstruction = spill32(candidate);

		GPR[candidate].spill.reference = GPR[candidate].reference;
		GPR[candidate].spill.priority = GPR[candidate].priority;
		GPR[candidate].spill.partial = GPR[candidate].partial;
		GPR[candidate].spill.copyInstruction = GPR[candidate].copyInstruction;
		GPR[candidate].spill.loadInstruction = GPR[candidate].loadInstruction;
		GPR[candidate].spill.spillInstruction = GPR[candidate].spillInstruction;

		GPR[candidate].reference = 0;
		GPR[candidate].priority = 0;
		GPR[candidate].partial = 0;
		GPR[candidate].copyInstruction = 0;
		GPR[candidate].loadInstruction = 0;
		GPR[candidate].spillInstruction = spillInstruction;

		return allocate32(candidate, ref, copy, partial);
	}

	OperandR_M32 RegisterAllocator::m32(const OperandREF &ref, int partial)
	{
		if(ref == 0) throw Error("Cannot dereference 0");

		// Check if already allocated
		for(int i = 0; i < 8; i++)
		{
			if(i == Encoding::ESP || i == Encoding::EBP) continue;

			if(GPR[i].reference == ref)
			{
				return prioritize32(i);
			}
		}

		// Check spilled but unused registers
		if(spillElimination)
		{
			for(int i = 0; i < 8; i++)
			{
				if(i == Encoding::ESP || i == Encoding::EBP) continue;

				if(GPR[i].priority == 0 && GPR[i].spill.reference == ref)
				{
					if(GPR[i].spillInstruction)
					{
						GPR[i].spillInstruction->reserve();
					}

					GPR[i].reference = GPR[i].spill.reference;
					GPR[i].partial = GPR[i].spill.partial;
					GPR[i].priority = GPR[i].spill.priority;
					GPR[i].copyInstruction = GPR[i].spill.copyInstruction;
					GPR[i].loadInstruction = GPR[i].spill.loadInstruction;
					GPR[i].spillInstruction = GPR[i].spill.spillInstruction;

					GPR[i].spill.free();

					return prioritize32(i);
				}
			}
		}

		return (OperandR_M32)dword_ptr [ref];
	}

	OperandREG32 RegisterAllocator::allocate32(int i, const OperandREF &ref, bool copy, int partial)
	{
		GPR[i].reference = ref;
		GPR[i].partial = partial;

		prioritize32(i);

		Encoding *loadInstruction = 0;
		Encoding *spillInstruction = GPR[i].spillInstruction;
		AllocationData spillAllocation = GPR[i].spill;

		if(copy)
		{
			     if(partial == 1) loadInstruction = mov(OperandREG8(i), byte_ptr [ref]);
			else if(partial == 2) loadInstruction = mov(OperandREG16(i), word_ptr [ref]); 
			else                  loadInstruction = mov(OperandREG32(i), dword_ptr [ref]);
		}

		GPR[i].loadInstruction = loadInstruction;
		GPR[i].spillInstruction = spillInstruction;
		GPR[i].spill = spillAllocation;
		GPR[i].modified = false;

		return OperandREG32(i);
	}

	OperandREG32 RegisterAllocator::prioritize32(int i)
	{
		// Give highest priority
		GPR[i].priority = 0xFFFFFFFF;

		// Decrease priority of other registers
		for(int j = 0; j < 8; j++)
		{
			if(i == Encoding::ESP || i == Encoding::EBP) continue;

			if(j != i && GPR[j].priority)
			{
				GPR[j].priority--;
			}
		}

		return OperandREG32(i);
	}

	void RegisterAllocator::free32(int i)
	{
		if(GPR[i].loadInstruction && loadElimination)
		{
			GPR[i].loadInstruction->reserve();
			GPR[i].loadInstruction = 0;
		}

		if(GPR[i].copyInstruction && copyPropagation)
		{
			GPR[i].copyInstruction->reserve();
			GPR[i].copyInstruction = 0;
		}

		GPR[i].reference = 0;
		GPR[i].partial = 0;
		GPR[i].priority = 0;
	}

	Encoding *RegisterAllocator::spill32(int i)
	{
		// Register loaded but not used, eliminate load and don't spill
		if(GPR[i].loadInstruction && loadElimination)
		{
			GPR[i].loadInstruction->reserve();
			GPR[i].loadInstruction = 0;
	
			GPR[i].reference = 0;
			GPR[i].priority = 0;
			GPR[i].partial = 0;
			GPR[i].copyInstruction = 0;
			GPR[i].loadInstruction = 0;
		//	GPR[i].spillInstruction = 0;   // NOTE: Keep previous spill info
	
			return 0;
		}

		Encoding *spillInstruction = 0;

		if(GPR[i].reference != 0 && (GPR[i].modified || !dropUnmodified))
		{
			     if(GPR[i].partial == 1) spillInstruction = mov(byte_ptr [GPR[i].reference], OperandREG8(i));
			else if(GPR[i].partial == 2) spillInstruction = mov(word_ptr [GPR[i].reference], OperandREG16(i));
			else                         spillInstruction = mov(dword_ptr [GPR[i].reference], OperandREG32(i));
		}

		GPR[i].free();

		return spillInstruction;
	}

	void RegisterAllocator::free(const OperandREG32 &r32)
	{
		free32(r32.reg);
	}

	void RegisterAllocator::spill(const OperandREG32 &r32)
	{
		spill32(r32.reg);
	}

	OperandMMREG RegisterAllocator::r64(const OperandREF &ref, bool copy)
	{
		if(ref == 0 && copy) throw Error("Cannot dereference 0");

		// Check if already allocated
		{for(int i = 0; i < 8; i++)
		{
			if(MMX[i].reference == ref)
			{
				return prioritize64(i);
			}
		}}

		// Check spilled but unused registers
		if(spillElimination)
		{
			for(int i = 0; i < 8; i++)
			{
				if(MMX[i].priority == 0 && MMX[i].spill.reference == ref)
				{
					if(MMX[i].spillInstruction)
					{
						MMX[i].spillInstruction->reserve();
					}

					MMX[i].reference = MMX[i].spill.reference;
					MMX[i].partial = MMX[i].spill.partial;
					MMX[i].priority = MMX[i].spill.priority;
					MMX[i].copyInstruction = MMX[i].spill.copyInstruction;
					MMX[i].loadInstruction = MMX[i].spill.loadInstruction;
					MMX[i].spillInstruction = MMX[i].spill.spillInstruction;

					MMX[i].spill.free();

					return prioritize64(i);
				}
			}
		}

		// Search for free registers
		{for(int i = 0; i < 8; i++)
		{
			if(MMX[i].priority == 0 && MMX[i].spill.priority == 0)
			{
				return allocate64(i, ref, copy);
			}
		}}

		{for(int i = 0; i < 8; i++)
		{
			if(MMX[i].priority == 0)
			{
				return allocate64(i, ref, copy);
			}
		}}

		// Need to spill one
		int candidate = 0;
		int betterCandidate = -1;
		unsigned int priority = 0xFFFFFFFF;

		{for(int i = 0; i < 8; i++)
		{
			if(MMX[i].priority < priority)
			{
				priority = MMX[i].priority;
				candidate = i;

				if(!MMX[i].modified && MMX[i].priority < 0xFFFFFFFF - 2)
				{
					betterCandidate = i;
				}
			}
		}}

		if(betterCandidate != -1)
		{
			candidate = betterCandidate;
		}

		Encoding *spillInstruction = spill64(candidate);

		MMX[candidate].spill.reference = MMX[candidate].reference;
		MMX[candidate].spill.priority = MMX[candidate].priority;
		MMX[candidate].spill.partial = MMX[candidate].partial;
		MMX[candidate].spill.copyInstruction = MMX[candidate].copyInstruction;
		MMX[candidate].spill.loadInstruction = MMX[candidate].loadInstruction;
		MMX[candidate].spill.spillInstruction = MMX[candidate].spillInstruction;

		MMX[candidate].reference = 0;
		MMX[candidate].priority = 0;
		MMX[candidate].partial = 0;
		MMX[candidate].copyInstruction = 0;
		MMX[candidate].loadInstruction = 0;
		MMX[candidate].spillInstruction = spillInstruction;

		return allocate64(candidate, ref, copy);
	}

	OperandMM64 RegisterAllocator::m64(const OperandREF &ref)
	{
		if(ref == 0) throw Error("Cannot dereference 0");

		// Check if already allocated
		for(int i = 0; i < 8; i++)
		{
			if(MMX[i].reference == ref)
			{
				return prioritize64(i);
			}
		}

		// Check spilled but unused registers
		if(spillElimination)
		{
			for(int i = 0; i < 8; i++)
			{
				if(MMX[i].priority == 0 && MMX[i].spill.reference == ref)
				{
					if(MMX[i].spillInstruction)
					{
						MMX[i].spillInstruction->reserve();
					}

					MMX[i].reference = MMX[i].spill.reference;
					MMX[i].partial = MMX[i].spill.partial;
					MMX[i].priority = MMX[i].spill.priority;
					MMX[i].copyInstruction = MMX[i].spill.copyInstruction;
					MMX[i].loadInstruction = MMX[i].spill.loadInstruction;
					MMX[i].spillInstruction = MMX[i].spill.spillInstruction;

					MMX[i].spill.free();

					return prioritize64(i);
				}
			}
		}

		return (OperandMM64)qword_ptr [ref];
	}

	OperandMMREG RegisterAllocator::allocate64(int i, const OperandREF &ref, bool copy)
	{
		MMX[i].reference = ref;

		prioritize64(i);

		Encoding *loadInstruction = 0;
		Encoding *spillInstruction = MMX[i].spillInstruction;
		AllocationData spillAllocation = MMX[i].spill;

		if(copy)
		{
			loadInstruction = movq(OperandMMREG(i), qword_ptr [ref]);
		}

		MMX[i].loadInstruction = loadInstruction;
		MMX[i].spillInstruction = spillInstruction;
		MMX[i].spill = spillAllocation;
		MMX[i].modified = false;

		return OperandMMREG(i);
	}

	OperandMMREG RegisterAllocator::prioritize64(int i)
	{
		// Give highest priority
		MMX[i].priority = 0xFFFFFFFF;

		// Decrease priority of other registers
		for(int j = 0; j < 8; j++)
		{
			if(j != i && MMX[j].priority)
			{
				MMX[j].priority--;
			}
		}

		return OperandMMREG(i);
	}

	void RegisterAllocator::free64(int i)
	{
		bool free = (MMX[i].priority != 0);

		if(MMX[i].loadInstruction && loadElimination)
		{
			MMX[i].loadInstruction->reserve();
			MMX[i].loadInstruction = 0;
		}

		if(MMX[i].copyInstruction && copyPropagation)
		{
			MMX[i].copyInstruction->reserve();
			MMX[i].copyInstruction = 0;
		}

		MMX[i].reference = 0;
		MMX[i].partial = 0;
		MMX[i].priority = 0;

		if(free && autoEMMS)
		{
			{for(int i = 0; i < 8; i++)
			{
				if(MMX[i].priority != 0)
				{
					return;
				}
			}}

			// Last one freed
			emms();

			// Completely eraze MMX allocation state
			{for(int i = 0; i < 8; i++)
			{
				MMX[i].free();
			}}
		}
	}

	Encoding *RegisterAllocator::spill64(int i)
	{
		// Register loaded but not used, eliminate load and don't spill
		if(MMX[i].loadInstruction && loadElimination)
		{
			MMX[i].loadInstruction->reserve();
			MMX[i].loadInstruction = 0;
	
			MMX[i].reference = 0;
			MMX[i].priority = 0;
			MMX[i].partial = 0;
			MMX[i].copyInstruction = 0;
			MMX[i].loadInstruction = 0;
		//	MMX[i].spillInstruction = 0;   // NOTE: Keep previous spill info
	
			return 0;
		}

		Encoding *spillInstruction = 0;

		if(MMX[i].reference != 0 && (MMX[i].modified || !dropUnmodified))
		{
			spillInstruction = movq(qword_ptr [MMX[i].reference], OperandMMREG(i));
		}

		MMX[i].free();

		return spillInstruction;
	}

	void RegisterAllocator::free(const OperandMMREG &r64)
	{
		free64(r64.reg);
	}

	void RegisterAllocator::spill(const OperandMMREG &r64)
	{
		spill64(r64.reg);
	}

	OperandXMMREG RegisterAllocator::r128(const OperandREF &ref, bool copy, bool ss)
	{
		if(ref == 0 && copy) throw Error("Cannot dereference 0");

		// Check if already allocated
		{for(int i = 0; i < 8; i++)
		{
			if(XMM[i].reference == ref)
			{
				return prioritize128(i);
			}
		}}

		// Check spilled but unused registers
		if(spillElimination)
		{
			for(int i = 0; i < 8; i++)
			{
				if(XMM[i].priority == 0 && XMM[i].spill.reference == ref)
				{
					if(XMM[i].spillInstruction)
					{
						XMM[i].spillInstruction->reserve();
					}

					XMM[i].reference = XMM[i].spill.reference;
					XMM[i].partial = XMM[i].spill.partial;
					XMM[i].priority = XMM[i].spill.priority;
					XMM[i].copyInstruction = XMM[i].spill.copyInstruction;
					XMM[i].loadInstruction = XMM[i].spill.loadInstruction;
					XMM[i].spillInstruction = XMM[i].spill.spillInstruction;

					XMM[i].spill.free();

					return prioritize128(i);
				}
			}
		}

		// Search for free registers
		{for(int i = 0; i < 8; i++)
		{
			if(XMM[i].priority == 0 && XMM[i].spill.priority == 0)
			{
				return allocate128(i, ref, copy, ss);
			}
		}}
		
		{for(int i = 0; i < 8; i++)
		{
			if(XMM[i].priority == 0)
			{
				return allocate128(i, ref, copy, ss);
			}
		}}

		// Need to spill one
		int candidate = 0;
		int betterCandidate = -1;
		unsigned int priority = 0xFFFFFFFF;

		{for(int i = 0; i < 8; i++)
		{
			if(XMM[i].priority < priority)
			{
				priority = XMM[i].priority;
				candidate = i;

				if(!XMM[i].modified && XMM[i].priority < 0xFFFFFFFF - 2)
				{
					betterCandidate = i;
				}
			}
		}}

		if(betterCandidate != -1)
		{
			candidate = betterCandidate;
		}

		Encoding *spillInstruction = spill128(candidate);

		XMM[candidate].spill.reference = XMM[candidate].reference;
		XMM[candidate].spill.priority = XMM[candidate].priority;
		XMM[candidate].spill.partial = XMM[candidate].partial;
		XMM[candidate].spill.copyInstruction = XMM[candidate].copyInstruction;
		XMM[candidate].spill.loadInstruction = XMM[candidate].loadInstruction;
		XMM[candidate].spill.spillInstruction = XMM[candidate].spillInstruction;

		XMM[candidate].reference = 0;
		XMM[candidate].priority = 0;
		XMM[candidate].partial = 0;
		XMM[candidate].copyInstruction = 0;
		XMM[candidate].loadInstruction = 0;
		XMM[candidate].spillInstruction = spillInstruction;

		return allocate128(candidate, ref, copy, ss);
	}

	OperandR_M128 RegisterAllocator::m128(const OperandREF &ref, bool ss)
	{
		if(ref == 0) throw Error("Cannot dereference 0");

		// Check if already allocated
		for(int i = 0; i < 8; i++)
		{
			if(XMM[i].reference == ref)
			{
				return prioritize128(i);
			}
		}

		// Check spilled but unused registers
		if(spillElimination)
		{
			for(int i = 0; i < 8; i++)
			{
				if(XMM[i].priority == 0 && XMM[i].spill.reference == ref)
				{
					if(XMM[i].spillInstruction)
					{
						XMM[i].spillInstruction->reserve();
					}

					XMM[i].reference = XMM[i].spill.reference;
					XMM[i].partial = XMM[i].spill.partial;
					XMM[i].priority = XMM[i].spill.priority;
					XMM[i].copyInstruction = XMM[i].spill.copyInstruction;
					XMM[i].loadInstruction = XMM[i].spill.loadInstruction;
					XMM[i].spillInstruction = XMM[i].spill.spillInstruction;

					XMM[i].spill.free();

					return prioritize128(i);
				}
			}
		}

		return (OperandR_M128)xword_ptr [ref];
	}

	OperandXMMREG RegisterAllocator::allocate128(int i, const OperandREF &ref, bool copy, bool ss)
	{
		XMM[i].reference = ref;
		XMM[i].partial = ss ? 4 : 0;

		prioritize128(i);

		Encoding *loadInstruction = 0;
		Encoding *spillInstruction = XMM[i].spillInstruction;
		AllocationData spillAllocation = XMM[i].spill;

		if(copy)
		{
			if(ss) loadInstruction = movss(OperandXMMREG(i), dword_ptr [ref]);
			else   loadInstruction = movaps(OperandXMMREG(i), xword_ptr [ref]);
		}

		XMM[i].loadInstruction = loadInstruction;
		XMM[i].spillInstruction = spillInstruction;
		XMM[i].spill = spillAllocation;
		XMM[i].modified = false;

		return OperandXMMREG(i);
	}

	OperandXMMREG RegisterAllocator::prioritize128(int i)
	{
		// Give highest priority
		XMM[i].priority = 0xFFFFFFFF;

		// Decrease priority of other registers
		for(int j = 0; j < 8; j++)
		{
			if(j != i && XMM[j].priority)
			{
				XMM[j].priority--;
			}
		}

		return OperandXMMREG(i);
	}

	void RegisterAllocator::free128(int i)
	{
		if(XMM[i].loadInstruction && loadElimination)
		{
			XMM[i].loadInstruction->reserve();
			XMM[i].loadInstruction = 0;
		}

		if(XMM[i].copyInstruction && copyPropagation)
		{
			XMM[i].copyInstruction->reserve();
			XMM[i].copyInstruction = 0;
		}

		XMM[i].reference = 0;
		XMM[i].partial = 0;
		XMM[i].priority = 0;
	}

	Encoding *RegisterAllocator::spill128(int i)
	{
		// Register loaded but not used, eliminate load and don't spill
		if(XMM[i].loadInstruction && loadElimination)
		{
			XMM[i].loadInstruction->reserve();
			XMM[i].loadInstruction = 0;
	
			XMM[i].reference = 0;
			XMM[i].priority = 0;
			XMM[i].partial = 0;
			XMM[i].copyInstruction = 0;
			XMM[i].loadInstruction = 0;
		//	XMM[i].spillInstruction = 0;   // NOTE: Keep previous spill info
	
			return 0;
		}

		Encoding *spillInstruction = 0;

		if(XMM[i].reference != 0 && (XMM[i].modified || !dropUnmodified))
		{
			if(XMM[i].partial) spillInstruction = movss(dword_ptr [XMM[i].reference], OperandXMMREG(i));
			else               spillInstruction = movaps(xword_ptr [XMM[i].reference], OperandXMMREG(i));
		}
		
		XMM[i].free();

		return spillInstruction;
	}

	void RegisterAllocator::free(const OperandXMMREG &r128)
	{
		free128(r128.reg);
	}

	void RegisterAllocator::spill(const OperandXMMREG &r128)
	{
		spill128(r128.reg);
	}

	OperandXMMREG RegisterAllocator::rSS(const OperandREF &ref, bool copy, bool ss)
	{
		return r128(ref, copy, ss);
	}

	OperandXMM32 RegisterAllocator::mSS(const OperandREF &ref, bool ss)
	{
		return (OperandXMM32)m128(ref, ss);
	}

	void RegisterAllocator::free(const OperandREF &ref)
	{
		{for(int i = 0; i < 8; i++)
		{
			if(i == Encoding::ESP || i == Encoding::EBP) continue;

			if(GPR[i].reference == ref)
			{
				free32(i);
			}
		}}

		{for(int i = 0; i < 8; i++)
		{
			if(MMX[i].reference == ref)
			{
				free64(i);
			}
		}}

		{for(int i = 0; i < 8; i++)
		{
			if(XMM[i].reference == ref)
			{
				free128(i);
			}
		}}
	}

	void RegisterAllocator::spill(const OperandREF &ref)
	{
		{for(int i = 0; i < 8; i++)
		{
			if(i == Encoding::ESP || i == Encoding::EBP) continue;

			if(GPR[i].reference == ref)
			{
				spill32(i);
			}
		}}

		{for(int i = 0; i < 8; i++)
		{
			if(MMX[i].reference == ref)
			{
				spill64(i);
			}
		}}

		{for(int i = 0; i < 8; i++)
		{
			if(XMM[i].reference == ref)
			{
				spill128(i);
			}
		}}
	}

	void RegisterAllocator::freeAll()
	{
		for(int i = 0; i < 8; i++)
		{
			if(i == Encoding::ESP || i == Encoding::EBP) continue;

			free32(i);
		}

		{for(int i = 0; i < 8; i++)
		{
			free64(i);
		}}

		{for(int i = 0; i < 8; i++)
		{
			free128(i);
		}}
	}

	void RegisterAllocator::spillAll()
	{
		{for(int i = 0; i < 8; i++)
		{
			// Prevent optimizations
			markModified(OperandREG32(i));
			markModified(OperandMMREG(i));
			markModified(OperandXMMREG(i));
		}}

		{for(int i = 0; i < 8; i++)
		{
			spill32(i);
			spill64(i);
			spill128(i);
		}}

		{for(int i = 0; i < 8; i++)
		{
			// Prevent optimizations
			markModified(OperandREG32(i));
			markModified(OperandMMREG(i));
			markModified(OperandXMMREG(i));
		}}
	}

	void RegisterAllocator::spillMMX()
	{
		for(int i = 0; i < 8; i++)
		{
			spill64(i);
		}
	}

	void RegisterAllocator::spillMMXcept(const OperandMMREG &r64)
	{
		for(int i = 0; i < 8; i++)
		{
			if(r64.reg != i)
			{
				spill64(i);
			}
		}

		emms();
	}

	const RegisterAllocator::State RegisterAllocator::capture()
	{
		State state;

		if(!minimalRestore)
		{
			spillAll();
			return state;   // Empty state
		}

		{for(int i = 0; i < 8; i++)
		{
			// Prevent optimizations
			markModified(OperandREG32(i));
			markModified(OperandMMREG(i));
			markModified(OperandXMMREG(i));
		}}

		{for(int i = 0; i < 8; i++)
		{
			state.GPR[i] = GPR[i];
			state.MMX[i] = MMX[i];
			state.XMM[i] = XMM[i];
		}}

		return state;
	}

	void RegisterAllocator::restore(const State &state)
	{
		if(!minimalRestore)
		{
			spillAll();
			return;
		}

		{for(int i = 0; i < 8; i++)
		{
			if(GPR[i].reference != state.GPR[i].reference)
			{
				spill32(i);
			}

			if(MMX[i].reference != state.MMX[i].reference)
			{
				spill64(i);
			}

			if(XMM[i].reference != state.XMM[i].reference)
			{
				spill128(i);
			}
		}}

		{for(int i = 0; i < 8; i++)
		{
			if(GPR[i].reference != state.GPR[i].reference && state.GPR[i].reference != 0)
			{
				allocate32(i, state.GPR[i].reference, true, state.GPR[i].partial);
			}

			if(MMX[i].reference != state.MMX[i].reference && state.MMX[i].reference != 0)
			{
				allocate64(i, state.MMX[i].reference, true);
			}

			if(XMM[i].reference != state.XMM[i].reference && state.XMM[i].reference != 0)
			{
				allocate128(i, state.XMM[i].reference, true, state.XMM[i].partial != 0);
			}
		}}

		{for(int i = 0; i < 8; i++)
		{
			// Prevent optimizations
			markModified(OperandREG32(i));
			markModified(OperandMMREG(i));
			markModified(OperandXMMREG(i));
		}}
	}

	void RegisterAllocator::exclude(const OperandREG32 &r32)
	{
		spill(r32);
		prioritize32(r32.reg);
	}

	Encoding *RegisterAllocator::mov(OperandREG32 r32i, OperandREG32 r32j)
	{
		if(r32i == r32j) return 0;

		// Register overwritten, when not used, eliminate load instruction
		if(GPR[r32i.reg].loadInstruction && loadElimination)
		{
			GPR[r32i.reg].loadInstruction->reserve();
			GPR[r32i.reg].loadInstruction = 0;
		}

		// Register overwritten, when not used, eliminate copy instruction
		if(GPR[r32i.reg].copyInstruction && copyPropagation)
		{
			GPR[r32i.reg].copyInstruction->reserve();
			GPR[r32i.reg].copyInstruction = 0;
		}

		Encoding *spillInstruction = GPR[r32i.reg].spillInstruction;
		AllocationData spillAllocation = GPR[r32i.reg].spill;

		Encoding *mov = Assembler::mov(r32i, r32j);
		
		if(GPR[r32i.reg].reference == 0 || GPR[r32j.reg].reference == 0)   // Return if not in allocation table
		{
			return mov;
		}

		// Attempt copy propagation
		if(mov && copyPropagation)
		{
			swap32(r32i.reg, r32j.reg);
			GPR[r32i.reg].copyInstruction = mov;
		}

		GPR[r32i.reg].spillInstruction = spillInstruction;
		GPR[r32i.reg].spill = spillAllocation;
		
		return mov;
	}

	Encoding *RegisterAllocator::mov(OperandREG32 r32, OperandMEM32 m32)
	{
		if(r32.reg == Encoding::ESP || r32.reg == Encoding::EBP)
		{
			return Assembler::mov(r32, m32);
		}

		// Register overwritten, when not used, eliminate load instruction
		if(GPR[r32.reg].loadInstruction && loadElimination)
		{
			GPR[r32.reg].loadInstruction->reserve();
			GPR[r32.reg].loadInstruction = 0;
		}

		// Register overwritten, when not used, eliminate copy instruction
		if(GPR[r32.reg].copyInstruction && copyPropagation)
		{
			GPR[r32.reg].copyInstruction->reserve();
			GPR[r32.reg].copyInstruction = 0;
		}

		Encoding *spillInstruction = GPR[r32.reg].spillInstruction;
		AllocationData spillAllocation = GPR[r32.reg].spill;

		Encoding *mov = Assembler::mov(r32, m32);

		GPR[r32.reg].spillInstruction = spillInstruction;
		GPR[r32.reg].spill = spillAllocation;

		return mov;
	}

	Encoding *RegisterAllocator::mov(OperandREG32 r32, OperandR_M32 r_m32)
	{
		if(r_m32.isSubtypeOf(Operand::OPERAND_REG32))
		{
			return mov(r32, (OperandREG32)r_m32);
		}
		else
		{
			return mov(r32, (OperandMEM32)r_m32);
		}
	}

	Encoding *RegisterAllocator::movq(OperandMMREG r64i, OperandMMREG r64j)
	{
		if(r64i == r64j) return 0;

		// Register overwritten, when not used, eliminate load instruction
		if(MMX[r64i.reg].loadInstruction && loadElimination)
		{
			MMX[r64i.reg].loadInstruction->reserve();
			MMX[r64i.reg].loadInstruction = 0;
		}

		// Register overwritten, when not used, eliminate copy instruction
		if(MMX[r64i.reg].copyInstruction && copyPropagation)
		{
			MMX[r64i.reg].copyInstruction->reserve();
			MMX[r64i.reg].copyInstruction = 0;
		}

		Encoding *spillInstruction = MMX[r64i.reg].spillInstruction;
		AllocationData spillAllocation = MMX[r64i.reg].spill;

		Encoding *movq = Assembler::movq(r64i, r64j);
		
		if(MMX[r64i.reg].reference == 0 || MMX[r64j.reg].reference == 0)   // Return if not in allocation table
		{
			return movq;
		}

		// Attempt copy propagation
		if(movq && copyPropagation)
		{
			swap64(r64i.reg, r64j.reg);
			MMX[r64i.reg].copyInstruction = movq;
		}

		MMX[r64i.reg].spillInstruction = spillInstruction;
		MMX[r64i.reg].spill = spillAllocation;
		
		return movq;
	}

	Encoding *RegisterAllocator::movq(OperandMMREG r64, OperandMEM64 m64)
	{
		// Register overwritten, when not used, eliminate load instruction
		if(MMX[r64.reg].loadInstruction && loadElimination)
		{
			MMX[r64.reg].loadInstruction->reserve();
			MMX[r64.reg].loadInstruction = 0;
		}

		// Register overwritten, when not used, eliminate copy instruction
		if(MMX[r64.reg].copyInstruction && copyPropagation)
		{
			MMX[r64.reg].copyInstruction->reserve();
			MMX[r64.reg].copyInstruction = 0;
		}

		Encoding *spillInstruction = MMX[r64.reg].spillInstruction;
		AllocationData spillAllocation = MMX[r64.reg].spill;

		Encoding *movq = Assembler::movq(r64, m64);

		MMX[r64.reg].spillInstruction = spillInstruction;
		MMX[r64.reg].spill = spillAllocation;

		return movq;
	}

	Encoding *RegisterAllocator::movq(OperandMMREG r64, OperandMM64 r_m64)
	{
		if(r_m64.isSubtypeOf(Operand::OPERAND_MMREG))
		{
			return movq(r64, (OperandMMREG)r_m64);
		}
		else
		{
			return movq(r64, (OperandMEM64)r_m64);
		}
	}

	Encoding *RegisterAllocator::movaps(OperandXMMREG r128i, OperandXMMREG r128j)
	{
		if(r128i == r128j) return 0;

		// Register overwritten, when not used, eliminate load instruction
		if(XMM[r128i.reg].loadInstruction && loadElimination)
		{
			XMM[r128i.reg].loadInstruction->reserve();
			XMM[r128i.reg].loadInstruction = 0;
		}

		// Register overwritten, when not used, eliminate copy instruction
		if(XMM[r128i.reg].copyInstruction && copyPropagation)
		{
			XMM[r128i.reg].copyInstruction->reserve();
			XMM[r128i.reg].copyInstruction = 0;
		}

		Encoding *spillInstruction = XMM[r128i.reg].spillInstruction;
		AllocationData spillAllocation = XMM[r128i.reg].spill;

		Encoding *movaps = Assembler::movaps(r128i, r128j);
		
		if(XMM[r128i.reg].reference == 0 || XMM[r128j.reg].reference == 0)   // Return if not in allocation table
		{
			return movaps;
		}

		// Attempt copy propagation
		if(movaps && copyPropagation)
		{
			swap128(r128i.reg, r128j.reg);
			XMM[r128i.reg].copyInstruction = movaps;
		}

		XMM[r128i.reg].spillInstruction = spillInstruction;
		XMM[r128i.reg].spill = spillAllocation;
		
		return movaps;
	}

	Encoding *RegisterAllocator::movaps(OperandXMMREG r128, OperandMEM128 m128)
	{
		// Register overwritten, when not used, eliminate load instruction
		if(XMM[r128.reg].loadInstruction && loadElimination)
		{
			XMM[r128.reg].loadInstruction->reserve();
			XMM[r128.reg].loadInstruction = 0;
		}

		// Register overwritten, when not used, eliminate copy instruction
		if(XMM[r128.reg].copyInstruction && copyPropagation)
		{
			XMM[r128.reg].copyInstruction->reserve();
			XMM[r128.reg].copyInstruction = 0;
		}

		Encoding *spillInstruction = XMM[r128.reg].spillInstruction;
		AllocationData spillAllocation = XMM[r128.reg].spill;

		Encoding *movaps = Assembler::movaps(r128, m128);

		XMM[r128.reg].spillInstruction = spillInstruction;
		XMM[r128.reg].spill = spillAllocation;

		return movaps;
	}

	Encoding *RegisterAllocator::movaps(OperandXMMREG r128, OperandR_M128 r_m128)
	{
		if(r_m128.isSubtypeOf(Operand::OPERAND_XMMREG))
		{
			return movaps(r128, (OperandXMMREG)r_m128);
		}
		else
		{
			return movaps(r128, (OperandMEM128)r_m128);
		}
	}

	void RegisterAllocator::enableAutoEMMS()
	{
		autoEMMS = true;
	}

	void RegisterAllocator::disableAutoEMMS()
	{
		autoEMMS = false;
	}

	void RegisterAllocator::enableCopyPropagation()
	{
		copyPropagation = true;
	}

	void RegisterAllocator::disableCopyPropagation()
	{
		copyPropagation = false;
	}

	void RegisterAllocator::enableLoadElimination()
	{
		loadElimination = true;
	}

	void RegisterAllocator::disableLoadElimination()
	{
		loadElimination = false;
	}

	void RegisterAllocator::enableSpillElimination()
	{
		spillElimination = true;
	}

	void RegisterAllocator::disableSpillElimination()
	{
		spillElimination = false;
	}

	void RegisterAllocator::enableMinimalRestore()
	{
		minimalRestore = true;
	}

	void RegisterAllocator::disableMinimalRestore()
	{
		minimalRestore = false;
	}

	void RegisterAllocator::enableDropUnmodified()
	{
		dropUnmodified = true;
	}

	void RegisterAllocator::disableDropUnmodified()
	{
		dropUnmodified = false;
	}

	Encoding *RegisterAllocator::x86(int instructionID, const Operand &firstOperand, const Operand &secondOperand, const Operand &thirdOperand)
	{
		markModified(firstOperand);
		markReferenced(secondOperand);

		return Assembler::x86(instructionID, firstOperand, secondOperand, thirdOperand);
	}

	void RegisterAllocator::markModified(const Operand &op)
	{
		if(Operand::isReg(op))
		{
			if(op.type == Operand::OPERAND_REG64 ||
			   op.type == Operand::OPERAND_REG32 ||
			   op.type == Operand::OPERAND_REG16 ||
			   op.type == Operand::OPERAND_REG8 ||
			   op.type == Operand::OPERAND_EAX ||
			   op.type == Operand::OPERAND_ECX ||
			   op.type == Operand::OPERAND_AX ||
			   op.type == Operand::OPERAND_DX ||
			   op.type == Operand::OPERAND_CX ||
			   op.type == Operand::OPERAND_AL ||
			   op.type == Operand::OPERAND_CL)
			{
				if(op.reg == Encoding::ESP || op.reg == Encoding::EBP) return;

				if(GPR[op.reg].copyInstruction)
				{
					GPR[op.reg].copyInstruction->retain();
					GPR[op.reg].copyInstruction = 0;
				}

				if(GPR[op.reg].loadInstruction)
				{
					GPR[op.reg].loadInstruction->retain();
					GPR[op.reg].loadInstruction = 0;
				}

				if(GPR[op.reg].spillInstruction)
				{
					GPR[op.reg].spillInstruction->retain();
					GPR[op.reg].spillInstruction = 0;

					GPR[op.reg].spill.free();
				}

				GPR[op.reg].modified = true;
			}
			else if(op.type == Operand::OPERAND_MMREG)
			{
				if(MMX[op.reg].copyInstruction)
				{
					MMX[op.reg].copyInstruction->retain();
					MMX[op.reg].copyInstruction = 0;
				}

				if(MMX[op.reg].loadInstruction)
				{
					MMX[op.reg].loadInstruction->retain();
					MMX[op.reg].loadInstruction = 0;
				}

				if(MMX[op.reg].spillInstruction)
				{
					MMX[op.reg].spillInstruction->retain();
					MMX[op.reg].spillInstruction = 0;

					MMX[op.reg].spill.free();
				}

				MMX[op.reg].modified = true;
			}
			else if(op.type == Operand::OPERAND_XMMREG)
			{
				if(XMM[op.reg].copyInstruction)
				{
					XMM[op.reg].copyInstruction->retain();
					XMM[op.reg].copyInstruction = 0;
				}

				if(XMM[op.reg].loadInstruction)
				{
					XMM[op.reg].loadInstruction->retain();
					XMM[op.reg].loadInstruction = 0;
				}
			
				if(XMM[op.reg].spillInstruction)
				{
					XMM[op.reg].spillInstruction->retain();
					XMM[op.reg].spillInstruction = 0;

					XMM[op.reg].spill.free();
				}

				XMM[op.reg].modified = true;
			}
			else if(op.isSubtypeOf(Operand::OPERAND_FPUREG))
			{
			}
			else
			{
				throw INTERNAL_ERROR;
			}
		}
		else if(Operand::isMem(op))
		{
			if(op.baseReg != Encoding::REG_UNKNOWN)
			{
				markReferenced(OperandREG32(op.baseReg));
			}

			if(op.indexReg != Encoding::REG_UNKNOWN)
			{
				markReferenced(OperandREG32(op.indexReg));
			}
		}
	}

	void RegisterAllocator::markReferenced(const Operand &op)
	{
		if(Operand::isReg(op))
		{
			if(op.type == Operand::OPERAND_REG64 ||
			   op.type == Operand::OPERAND_REG32 ||
			   op.type == Operand::OPERAND_REG16 ||
			   op.type == Operand::OPERAND_REG8 ||
			   op.type == Operand::OPERAND_EAX ||
			   op.type == Operand::OPERAND_ECX ||
			   op.type == Operand::OPERAND_AX ||
			   op.type == Operand::OPERAND_DX ||
			   op.type == Operand::OPERAND_CX ||
			   op.type == Operand::OPERAND_AL ||
			   op.type == Operand::OPERAND_CL)
			{
				if(op.reg == Encoding::ESP || op.reg == Encoding::EBP) return;

				if(GPR[op.reg].copyInstruction)
				{
					GPR[op.reg].copyInstruction->retain();
					GPR[op.reg].copyInstruction = 0;
				}

				if(GPR[op.reg].loadInstruction)
				{
					GPR[op.reg].loadInstruction->retain();
					GPR[op.reg].loadInstruction = 0;
				}

				if(GPR[op.reg].spillInstruction)
				{
					GPR[op.reg].spillInstruction->retain();
					GPR[op.reg].spillInstruction = 0;

					GPR[op.reg].spill.free();
				}
			}
			else if(op.type == Operand::OPERAND_MMREG)
			{
				if(MMX[op.reg].copyInstruction)
				{
					MMX[op.reg].copyInstruction->retain();
					MMX[op.reg].copyInstruction = 0;
				}

				if(MMX[op.reg].loadInstruction)
				{
					MMX[op.reg].loadInstruction->retain();
					MMX[op.reg].loadInstruction = 0;
				}

				if(MMX[op.reg].spillInstruction)
				{
					MMX[op.reg].spillInstruction->retain();
					MMX[op.reg].spillInstruction = 0;

					MMX[op.reg].spill.free();
				}
			}
			else if(op.type == Operand::OPERAND_XMMREG)
			{
				if(XMM[op.reg].copyInstruction)
				{
					XMM[op.reg].copyInstruction->retain();
					XMM[op.reg].copyInstruction = 0;
				}

				if(XMM[op.reg].loadInstruction)
				{
					XMM[op.reg].loadInstruction->retain();
					XMM[op.reg].loadInstruction = 0;
				}

				if(XMM[op.reg].spillInstruction)
				{
					XMM[op.reg].spillInstruction->retain();
					XMM[op.reg].spillInstruction = 0;

					XMM[op.reg].spill.free();
				}
			}
			else if(op.isSubtypeOf(Operand::OPERAND_FPUREG))
			{
			}
			else
			{
				throw INTERNAL_ERROR;
			}
		}
		else if(Operand::isMem(op))
		{
			if(op.baseReg != Encoding::REG_UNKNOWN)
			{
				markReferenced(OperandREG32(op.baseReg));
			}

			if(op.indexReg != Encoding::REG_UNKNOWN)
			{
				markReferenced(OperandREG32(op.indexReg));
			}
		}
	}

	void RegisterAllocator::swap32(int i, int j)
	{
		Allocation *source = &GPR[j];
		Allocation *destination = &GPR[i];

		// Swap references, priorities, etc.
		OperandREF swapRef = source->reference;
		source->reference = destination->reference;
		destination->reference = swapRef;

		int swapPriority = source->priority;
		source->priority = destination->priority;
		destination->priority = swapPriority;

		int swapPartial = source->partial;
		source->partial = destination->partial;
		destination->partial = swapPartial;

		bool swapModified = source->modified;
		source->modified = destination->modified;
		destination->modified = swapModified;
	}

	void RegisterAllocator::swap64(int i, int j)
	{
		Allocation *source = &MMX[j];
		Allocation *destination = &MMX[i];

		// Swap references, priorities, etc.
		OperandREF swapRef = source->reference;
		source->reference = destination->reference;
		destination->reference = swapRef;

		int swapPriority = source->priority;
		source->priority = destination->priority;
		destination->priority = swapPriority;

		int swapPartial = source->partial;
		source->partial = destination->partial;
		destination->partial = swapPartial;

		bool swapModified = source->modified;
		source->modified = destination->modified;
		destination->modified = swapModified;
	}

	void RegisterAllocator::swap128(int i, int j)
	{
		Allocation *source = &XMM[j];
		Allocation *destination = &XMM[i];

		// Swap references, priorities, etc.
		OperandREF swapRef = source->reference;
		source->reference = destination->reference;
		destination->reference = swapRef;

		int swapPriority = source->priority;
		source->priority = destination->priority;
		destination->priority = swapPriority;

		int swapPartial = source->partial;
		source->partial = destination->partial;
		destination->partial = swapPartial;

		bool swapModified = source->modified;
		source->modified = destination->modified;
		destination->modified = swapModified;
	}
}
