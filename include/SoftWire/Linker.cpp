#include "Linker.hpp"

#include "String.hpp"

namespace SoftWire
{
	Linker::External *Linker::externals;

	Linker::Linker()
	{
	}

	Linker::~Linker()
	{
		clearExternals();
	}

	void *Linker::resolveExternal(const char *name)
	{
		const External *external = externals;

		while(external)
		{
			if(external->name && strcmp(external->name, name) == 0)
			{
				return external->pointer;
			}

			external = external->next();
		}

		return 0;
	}

	void Linker::defineExternal(void *pointer, const char *name)
	{
		if(!externals)
		{
			externals = new External();
		}

		External *external = externals;

		do
		{
			if(external->name && strcmp(external->name, name) == 0)
			{
				external->pointer = pointer;
				return;
			}

			external = external->next();
		}
		while(external);

		externals->append(Identifier(pointer, name));
	}

	void Linker::clearExternals()
	{
		delete externals;
		externals = 0;
	}
}
