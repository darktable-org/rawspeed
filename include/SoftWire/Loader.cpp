#include "Loader.hpp"

#include "Encoding.hpp"
#include "Error.hpp"
#include "Linker.hpp"

#include <string.h>

#ifdef WIN32
	#include <windows.h>
#elif __unix__
	#include <sys/mman.h>
#endif

namespace SoftWire
{
	Loader::Loader(const Linker &linker, bool x64) : linker(linker), x64(x64)
	{
		machineCode = 0;
		instructions = 0;
		listing = 0;
		possession = true;

		reset();
	}

	Loader::~Loader()
	{
		if(possession)
		{
			delete[] machineCode;
			machineCode = 0;
		}

		delete instructions;
		instructions = 0;

		delete[] listing;
		listing = 0;
	}

	void (*Loader::callable(const char *entryLabel))()
	{
		if(finalized) throw Error("Cannot retrieve callable from finalized code");

		if(!machineCode)
		{
			loadCode();
		}

		if(!entryLabel)
		{
			return (void(*)())machineCode;
		}

		const unsigned char *entryPoint = resolveLocal(entryLabel, 0);

		if(!entryPoint)
		{
			throw Error("Entry point '%s' not found", entryLabel);
		}

		return (void(*)())entryPoint;
	}

	void (*Loader::finalize(const char *entryLabel))()
	{
		if(!machineCode)
		{
			loadCode();
		}

		finalized = true;

		delete instructions;
		instructions = 0;

		delete[] listing;
		listing = 0;

		if(!entryLabel)
		{
			return (void(*)())machineCode;
		}

		const unsigned char *entryPoint = resolveLocal(entryLabel, 0);

		if(!entryPoint)
		{
			throw Error("Entry point '%s' not found", entryLabel);
		}

		return (void(*)())entryPoint;
	}

	void *Loader::acquire()
	{
		possession = false;

		return machineCode;
	}

	Encoding *Loader::appendEncoding(const Encoding &encoding)
	{
		if(!instructions) instructions = new Instruction();

		instructions->append(encoding);

		return instructions->tail();
	}

	void Loader::loadCode(const char *entryLabel)
	{
		int length = codeLength() + 64;   // NOTE: Code length is not accurate due to alignment issues

		machineCode = new unsigned char[length];
		unsigned char *currentCode = machineCode;

		#ifdef WIN32
			unsigned long oldProtection;
			VirtualProtect(machineCode, length, PAGE_EXECUTE_READWRITE, &oldProtection);
		#elif __unix__
			mprotect(machineCode, length, PROT_READ | PROT_WRITE | PROT_EXEC);
		#endif

		Instruction *instruction = instructions;

		while(instruction)
		{
			Encoding &encoding = *instruction;
			encoding.setAddress(currentCode);
			const char *reference = encoding.getReference();
			const char *label = encoding.getLabel();

			if(reference)
			{
				if(encoding.relativeReference())
				{
					int offset = resolveReference(reference, instruction) - currentCode - encoding.length(currentCode);
					encoding.setJumpOffset(offset);
				}
				else
				{
					int address = (int)resolveReference(reference, instruction);

					// Encoded as memory reference or immediate?
					if(encoding.hasDisplacement())
					{
						encoding.addDisplacement(address);
					}
					else if(encoding.hasImmediate())
					{
						encoding.setImmediate(address);
					}
					else
					{
						INTERNAL_ERROR;
					}
				}
			}
			else if(encoding.hasImmediate() && encoding.relativeReference())
			{
				__int64 offset = encoding.getImmediate() - (__int64)currentCode - encoding.length(currentCode);
				encoding.setCallOffset((int)offset);
			}

			if(x64 && encoding.isRipRelative())
			{
				__int64 displacement = encoding.getDisplacement() - (__int64)currentCode - encoding.length(currentCode);
				encoding.setDisplacement(displacement);
			}

			currentCode += encoding.writeCode(currentCode);

			instruction = instruction->next();
		}
	}

	const unsigned char *Loader::resolveReference(const char *name, const Instruction *position) const
	{
		const unsigned char *reference = resolveLocal(name, position);
		
		if(reference)
		{
			return reference;
		}
		else
		{
			reference = resolveExternal(name);
		}

		if(reference)
		{
			return reference;
		}
		else
		{
			throw Error("Unresolved identifier '%s'", name);
		}
	}

	const unsigned char *Loader::resolveLocal(const char *name, const Instruction *position) const
	{
		const Instruction *instruction = instructions;

		unsigned char *target = machineCode;
		unsigned char *local = 0;   // Closest match before, or after position

		while(instruction)
		{
			if(local && (instruction == position || position == 0))
			{
				return local;
			}
			else if(instruction == position)
			{
				position = 0;   // Return first match following position
			}

			if(instruction->getLabel() && strcmp(instruction->getLabel(), name) == 0)
			{
				local = target;
			}
			else
			{
				target += instruction->length(target);
			}

			instruction = instruction->next();
		}

		if(local)
		{
			return local;
		}

		return 0;
	}

	const unsigned char *Loader::resolveExternal(const char *name) const
	{
		return (unsigned char*)linker.resolveExternal(name);
	}

	int Loader::codeLength() const
	{
		const Instruction *instruction = instructions;
		unsigned char *end = machineCode;

		while(instruction)
		{
			end += instruction->length(end);

			instruction = instruction->next();
		}

		return end - machineCode;
	}

	const char *Loader::getListing()
	{
		if(!machineCode)
		{
			return 0;
		}

		if(listing)
		{
			return listing;
		}

		listing = new char[codeLength() * 4];
		char *buffer = listing;

		for(Instruction *instruction = instructions; instruction; instruction = instruction->next())
		{
			buffer += instruction->printCode(buffer);
		}

		*(buffer - 1) = '\0';

		return listing;
	}

	void Loader::clearListing()
	{
		delete[] listing;
		listing = 0;
	}

	void Loader::reset()
	{
		if(possession)
		{
			delete[] machineCode;
			machineCode = 0;
		}

		machineCode = 0;

		delete instructions;
		instructions = 0;

		delete[] listing;
		listing = 0;

		possession = true;
		finalized = false;
	}

	int Loader::instructionCount()
	{
		Instruction *instruction = instructions;

		int count = 0;

		while(instruction)
		{
			if(instruction->isEmitting())
			{
				count++;
			}
			
			instruction = instruction->next();
		}

		return count;
	}
}
