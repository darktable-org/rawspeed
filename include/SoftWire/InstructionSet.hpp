#ifndef SoftWire_InstructionSet_hpp
#define SoftWire_InstructionSet_hpp

#include "Instruction.hpp"

namespace SoftWire
{
	class TokenList;

	class InstructionSet
	{
	public:
		InstructionSet();

		virtual ~InstructionSet();

		const Instruction *instruction(int i);

	private:
		struct Entry
		{
			~Entry() {delete instruction;};

			const char *mnemonic;

			Instruction *instruction;
		};

		Instruction *intrinsicMap;

		static const Instruction::Syntax instructionSet[];
		static const int numInstructions;

		void generateIntrinsics();

		static int strcmp(const char *string1, const char *string2);
	};
}

#endif   // SoftWire_InstructionSet_hpp
