#ifndef SoftWire_Linker_hpp
#define SoftWire_Linker_hpp

#include "Link.hpp"

namespace SoftWire
{
	class Linker
	{
	public:
		Linker();

		virtual ~Linker();

		static void *resolveExternal(const char *name);
		static void defineExternal(void *pointer, const char *name);
		static void clearExternals();

	private:
		struct Identifier
		{
			Identifier(void *pointer = 0, const char *name = 0) : pointer(pointer), name(name) {};

			void *pointer;
			const char *name;
		};

		typedef Link<Identifier> External;
		static External *externals;
	};
}

#endif   // SoftWire_Linker_hpp
