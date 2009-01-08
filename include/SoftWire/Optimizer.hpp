#ifndef SoftWire_Optimizer_hpp
#define SoftWire_Optimizer_hpp

#include "RegisterAllocator.hpp"

namespace SoftWire
{
	class Optimizer : public RegisterAllocator
	{
	protected:
		Optimizer(bool x64);

		virtual ~Optimizer();
	};
}

#endif   // SoftWire_Optimizer_hpp
