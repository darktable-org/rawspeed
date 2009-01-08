#include "CodeGenerator.hpp"

#include "Error.hpp"

#include <stdio.h>

namespace SoftWire
{
	int CodeGenerator::stack = -128;
	int CodeGenerator::stackTop = -128;
	Encoding *CodeGenerator::stackUpdate = 0;

	CodeGenerator *CodeGenerator::cg = 0;

	CodeGenerator::Variable::Variable(int size) : size(size)
	{
		previous = stack;
		reference = (stack + size - 1) & ~(size - 1);
		stack = reference + size;

		// Grow stack when required
		if(stack > stackTop)
		{
			if(stackUpdate)
			{
				stackTop += 16;

				if(!cg->x64)
				{
					stackUpdate->setImmediate(stackTop);
				}
				else
				{
					stackUpdate->setImmediate(32+stackTop+128);
				}
			}
			else if(stackTop != -128)   // Skip arg
			{
				throw Error("Stack used without prologue");
			}
		}
	}

	CodeGenerator::Variable::~Variable()
	{
		if(reference == 0xDEADC0DE) return;   // Already freed

		cg->free((OperandREF)(ebp + reference));
		reference = 0xDEADC0DE;

		for(int i = 0; i < 8; i++)
		{
			if(GPR[i].reference.baseReg == Encoding::EBP && GPR[i].reference.displacement > previous) return;
			if(MMX[i].reference.baseReg == Encoding::EBP && MMX[i].reference.displacement > previous) return;
			if(XMM[i].reference.baseReg == Encoding::EBP && XMM[i].reference.displacement > previous) return;
		}
		
		stack = previous;   // Free stack space when allocated at top of stack
	}

	void CodeGenerator::Variable::free()
	{
		// Explicitely destruct
		this->~Variable();
	}

	int CodeGenerator::Variable::ref() const
	{
		if(reference == 0xDEADC0DE)
		{
			throw Error("Freed variables can no longer be accessed!");
		}

		return reference;
	}

	CodeGenerator::Byte::Byte() : Variable(1)
	{
	}

	CodeGenerator::Byte::operator OperandREG8() const
	{
		return cg->r8(ebp + ref());
	}

	CodeGenerator::Char::Char()
	{
	}

	CodeGenerator::Char::Char(unsigned char c)
	{
		cg->mov(*this, c);
	}

	CodeGenerator::Char::Char(const Char &c)
	{
		cg->mov(*this, c);
	}

	CodeGenerator::Char &CodeGenerator::Char::operator=(const Char &c)
	{
		cg->mov(*this, cg->m8(ebp + c.ref()));
		return *this;
	}

	CodeGenerator::Char &CodeGenerator::Char::operator+=(const Char &c)
	{
		cg->add(*this, cg->m8(ebp + c.ref()));
		return *this;
	}

	CodeGenerator::Char &CodeGenerator::Char::operator-=(const Char &c)
	{
		cg->sub(*this, cg->m8(ebp + c.ref()));
		return *this;
	}

	CodeGenerator::Char &CodeGenerator::Char::operator*=(const Char &c)
	{
		cg->exclude(eax);
		cg->mov(al, cg->m8(ebp + ref()));
		cg->imul(c);
		cg->mov(*this, al);
		return *this;
	}

	CodeGenerator::Char &CodeGenerator::Char::operator/=(const Char &c)
	{
		cg->exclude(eax);
		cg->exclude(edx);
		cg->mov(al, cg->m8(ebp + ref()));
		cg->mov(dl, cg->m8(ebp + c.ref()));
		cg->idiv(dl);
		cg->mov(*this, al);
		return *this;
	}

	CodeGenerator::Char &CodeGenerator::Char::operator%=(const Char &c)
	{
		cg->exclude(eax);
		cg->exclude(edx);
		cg->mov(al, cg->m8(ebp + ref()));
		cg->mov(dl, cg->m8(ebp + c.ref()));
		cg->idiv(dl);
		cg->mov(*this, dl);
		return *this;
	}

	CodeGenerator::Char &CodeGenerator::Char::operator<<=(const Char &c)
	{
		cg->exclude(ecx);
		cg->mov(cl, cg->m8(ebp + c.ref()));
		cg->shl(*this, cl);
		return *this;
	}

	CodeGenerator::Char &CodeGenerator::Char::operator>>=(const Char &c)
	{
		cg->exclude(ecx);
		cg->mov(cl, cg->m8(ebp + c.ref()));
		cg->shr(*this, cl);
		return *this;
	}

	CodeGenerator::Char &CodeGenerator::Char::operator&=(const Char &c)
	{
		cg->and(*this, cg->m8(ebp + c.ref()));
		return *this;
	}

	CodeGenerator::Char &CodeGenerator::Char::operator^=(const Char &c)
	{
		cg->xor(*this, cg->m8(ebp + c.ref()));
		return *this;
	}

	CodeGenerator::Char &CodeGenerator::Char::operator|=(const Char &c)
	{
		cg->or(*this, cg->m8(ebp + c.ref()));
		return *this;
	}

	CodeGenerator::Char CodeGenerator::Char::operator+(const Char &c)
	{
		Char temp;
		temp = *this;
		temp += c;
		return temp;
	}

	CodeGenerator::Char CodeGenerator::Char::operator-(const Char &c)
	{
		Char temp;
		temp = *this;
		temp -= c;
		return temp;		
	}

	CodeGenerator::Char CodeGenerator::Char::operator*(const Char &c)
	{
		Char temp;
		temp = *this;
		temp *= c;
		return temp;
	}

	CodeGenerator::Char CodeGenerator::Char::operator/(const Char &c)
	{
		Char temp;
		temp = *this;
		temp /= c;
		return temp;
	}

	CodeGenerator::Char CodeGenerator::Char::operator%(const Char &c)
	{
		Char temp;
		temp = *this;
		temp %= c;
		return temp;
	}

	CodeGenerator::Char CodeGenerator::Char::operator<<(const Char &c)
	{
		Char temp;
		temp = *this;
		temp <<= c;
		return temp;
	}

	CodeGenerator::Char CodeGenerator::Char::operator>>(const Char &c)
	{
		Char temp;
		temp = *this;
		temp >>= c;
		return temp;
	}

	CodeGenerator::Char CodeGenerator::Char::operator&(const Char &c)
	{
		Char temp;
		temp = *this;
		temp &= c;
		return temp;
	}

	CodeGenerator::Char CodeGenerator::Char::operator^(const Char &c)
	{
		Char temp;
		temp = *this;
		temp ^= c;
		return temp;
	}

	CodeGenerator::Char CodeGenerator::Char::operator|(const Char &c)
	{
		Char temp;
		temp = *this;
		temp |= c;
		return temp;
	}

	CodeGenerator::Char &CodeGenerator::Char::operator+=(unsigned char c)
	{
		cg->add(*this, c);
		return *this;
	}

	CodeGenerator::Char &CodeGenerator::Char::operator-=(unsigned char c)
	{
		cg->sub(*this, c);
		return *this;
	}

	CodeGenerator::Char &CodeGenerator::Char::operator*=(unsigned char c)
	{
		cg->exclude(eax);
		cg->mov(al, *this);
		cg->imul(eax, c);
		cg->mov(*this, al);
		return *this;
	}

	CodeGenerator::Char &CodeGenerator::Char::operator/=(unsigned char c)
	{
		cg->exclude(eax);
		cg->exclude(edx);
		cg->mov(al, *this);
		cg->mov(dl, c);
		cg->idiv(dl);
		cg->mov(*this, al);
		return *this;
	}

	CodeGenerator::Char &CodeGenerator::Char::operator%=(unsigned char c)
	{
		cg->exclude(eax);
		cg->exclude(edx);
		cg->mov(al, *this);
		cg->mov(dl, c);
		cg->idiv(dl);
		cg->mov(*this, dl);
		return *this;
	}

	CodeGenerator::Char &CodeGenerator::Char::operator<<=(unsigned char c)
	{
		cg->exclude(ecx);
		cg->exclude(edx);
		cg->mov(cl, c);
		cg->mov(cl, *this);
		cg->shl(*this, cl);
		return *this;
	}

	CodeGenerator::Char &CodeGenerator::Char::operator>>=(unsigned char c)
	{
		cg->exclude(ecx);
		cg->exclude(edx);
		cg->mov(cl, c);
		cg->mov(cl, *this);
		cg->shr(*this, cl);
		return *this;
	}

	CodeGenerator::Char &CodeGenerator::Char::operator&=(unsigned char c)
	{
		cg->and(*this, c);
		return *this;
	}

	CodeGenerator::Char &CodeGenerator::Char::operator^=(unsigned char c)
	{
		cg->xor(*this, c);
		return *this;
	}

	CodeGenerator::Char &CodeGenerator::Char::operator|=(unsigned char c)
	{
		cg->or(*this, c);
		return *this;
	}

	CodeGenerator::Char CodeGenerator::Char::operator+(unsigned char c)
	{
		Char temp;
		temp = *this;
		temp += c;
		return temp;
	}

	CodeGenerator::Char CodeGenerator::Char::operator-(unsigned char c)
	{
		Char temp;
		temp = *this;
		temp -= c;
		return temp;		
	}

	CodeGenerator::Char CodeGenerator::Char::operator*(unsigned char c)
	{
		Char temp;
		temp = *this;
		temp *= c;
		return temp;
	}

	CodeGenerator::Char CodeGenerator::Char::operator/(unsigned char c)
	{
		Char temp;
		temp = *this;
		temp /= c;
		return temp;
	}

	CodeGenerator::Char CodeGenerator::Char::operator%(unsigned char c)
	{
		Char temp;
		temp = *this;
		temp %= c;
		return temp;
	}

	CodeGenerator::Char CodeGenerator::Char::operator<<(unsigned char c)
	{
		Char temp;
		temp = *this;
		temp <<= c;
		return temp;
	}

	CodeGenerator::Char CodeGenerator::Char::operator>>(unsigned char c)
	{
		Char temp;
		temp = *this;
		temp >>= c;
		return temp;
	}

	CodeGenerator::Char CodeGenerator::Char::operator&(unsigned char c)
	{
		Char temp;
		temp = *this;
		temp &= c;
		return temp;
	}

	CodeGenerator::Char CodeGenerator::Char::operator^(unsigned char c)
	{
		Char temp;
		temp = *this;
		temp ^= c;
		return temp;
	}

	CodeGenerator::Char CodeGenerator::Char::operator|(unsigned char c)
	{
		Char temp;
		temp = *this;
		temp |= c;
		return temp;
	}

	CodeGenerator::Word::Word() : Variable(2)
	{
	}

	CodeGenerator::Word::operator OperandREG16() const
	{
		return cg->r16(ebp + ref());
	}

	CodeGenerator::Short::Short()
	{
	}

	CodeGenerator::Short::Short(unsigned short s)
	{
		cg->mov(*this, s);
	}

	CodeGenerator::Short::Short(const Short &s)
	{
		cg->mov(*this, s);
	}

	CodeGenerator::Short &CodeGenerator::Short::operator=(const Short &s)
	{
		cg->mov(*this, cg->m16(ebp + s.ref()));
		return *this;
	}

	CodeGenerator::Short &CodeGenerator::Short::operator+=(const Short &s)
	{
		cg->add(*this, cg->m16(ebp + s.ref()));
		return *this;
	}

	CodeGenerator::Short &CodeGenerator::Short::operator-=(const Short &s)
	{
		cg->sub(*this, cg->m16(ebp + s.ref()));
		return *this;
	}

	CodeGenerator::Short &CodeGenerator::Short::operator*=(const Short &s)
	{
		cg->imul(*this, cg->m16(ebp + s.ref()));
		return *this;
	}

	CodeGenerator::Short &CodeGenerator::Short::operator/=(const Short &s)
	{
		cg->exclude(eax);
		cg->exclude(edx);
		cg->mov(ax, cg->m16(ebp + ref()));
		cg->mov(dx, cg->m16(ebp + s.ref()));
		cg->idiv(dx);
		cg->mov(*this, ax);
		return *this;
	}

	CodeGenerator::Short &CodeGenerator::Short::operator%=(const Short &s)
	{
		cg->exclude(eax);
		cg->exclude(edx);
		cg->mov(ax, cg->m16(ebp + ref()));
		cg->mov(dx, cg->m16(ebp + s.ref()));
		cg->idiv(dx);
		cg->mov(*this, dx);
		return *this;
	}

	CodeGenerator::Short &CodeGenerator::Short::operator<<=(const Short &s)
	{
		cg->exclude(ecx);
		cg->mov(cx, cg->m16(ebp + s.ref()));
		cg->shl(*this, cl);
		return *this;
	}

	CodeGenerator::Short &CodeGenerator::Short::operator>>=(const Short &s)
	{
		cg->exclude(ecx);
		cg->mov(cx, cg->m16(ebp + s.ref()));
		cg->shr(*this, cl);
		return *this;
	}

	CodeGenerator::Short &CodeGenerator::Short::operator&=(const Short &s)
	{
		cg->and(*this, cg->m16(ebp + s.ref()));
		return *this;
	}

	CodeGenerator::Short &CodeGenerator::Short::operator^=(const Short &s)
	{
		cg->xor(*this, cg->m16(ebp + s.ref()));
		return *this;
	}

	CodeGenerator::Short &CodeGenerator::Short::operator|=(const Short &s)
	{
		cg->or(*this, cg->m16(ebp + s.ref()));
		return *this;
	}

	CodeGenerator::Short CodeGenerator::Short::operator+(const Short &s)
	{
		Short temp;
		temp = *this;
		temp += s;
		return temp;
	}

	CodeGenerator::Short CodeGenerator::Short::operator-(const Short &s)
	{
		Short temp;
		temp = *this;
		temp -= s;
		return temp;		
	}

	CodeGenerator::Short CodeGenerator::Short::operator*(const Short &s)
	{
		Short temp;
		temp = *this;
		temp *= s;
		return temp;
	}

	CodeGenerator::Short CodeGenerator::Short::operator/(const Short &s)
	{
		Short temp;
		temp = *this;
		temp /= s;
		return temp;
	}

	CodeGenerator::Short CodeGenerator::Short::operator%(const Short &s)
	{
		Short temp;
		temp = *this;
		temp %= s;
		return temp;
	}

	CodeGenerator::Short CodeGenerator::Short::operator<<(const Short &s)
	{
		Short temp;
		temp = *this;
		temp <<= s;
		return temp;
	}

	CodeGenerator::Short CodeGenerator::Short::operator>>(const Short &s)
	{
		Short temp;
		temp = *this;
		temp >>= s;
		return temp;
	}

	CodeGenerator::Short CodeGenerator::Short::operator&(const Short &s)
	{
		Short temp;
		temp = *this;
		temp &= s;
		return temp;
	}

	CodeGenerator::Short CodeGenerator::Short::operator^(const Short &s)
	{
		Short temp;
		temp = *this;
		temp ^= s;
		return temp;
	}

	CodeGenerator::Short CodeGenerator::Short::operator|(const Short &s)
	{
		Short temp;
		temp = *this;
		temp |= s;
		return temp;
	}

	CodeGenerator::Short &CodeGenerator::Short::operator+=(unsigned short s)
	{
		cg->add(*this, s);
		return *this;
	}

	CodeGenerator::Short &CodeGenerator::Short::operator-=(unsigned short s)
	{
		cg->sub(*this, s);
		return *this;
	}

	CodeGenerator::Short &CodeGenerator::Short::operator*=(unsigned short s)
	{
		cg->imul(*this, s);
		return *this;
	}

	CodeGenerator::Short &CodeGenerator::Short::operator/=(unsigned short s)
	{
		cg->exclude(eax);
		cg->exclude(edx);
		cg->mov(ax, *this);
		cg->mov(dx, s);
		cg->idiv(dx);
		cg->mov(*this, ax);
		return *this;
	}

	CodeGenerator::Short &CodeGenerator::Short::operator%=(unsigned short s)
	{
		cg->exclude(eax);
		cg->exclude(edx);
		cg->mov(ax, *this);
		cg->mov(dx, s);
		cg->idiv(dx);
		cg->mov(*this, dx);
		return *this;
	}

	CodeGenerator::Short &CodeGenerator::Short::operator<<=(unsigned short s)
	{
		cg->exclude(ecx);
		cg->exclude(edx);
		cg->mov(cx, s);
		cg->mov(dx, *this);
		cg->shl(*this, cl);
		return *this;
	}

	CodeGenerator::Short &CodeGenerator::Short::operator>>=(unsigned short s)
	{
		cg->exclude(ecx);
		cg->exclude(edx);
		cg->mov(cx, s);
		cg->mov(dx, *this);
		cg->shr(*this, cl);
		return *this;
	}

	CodeGenerator::Short &CodeGenerator::Short::operator&=(unsigned short s)
	{
		cg->and(*this, s);
		return *this;
	}

	CodeGenerator::Short &CodeGenerator::Short::operator^=(unsigned short s)
	{
		cg->xor(*this, s);
		return *this;
	}

	CodeGenerator::Short &CodeGenerator::Short::operator|=(unsigned short s)
	{
		cg->or(*this, s);
		return *this;
	}

	CodeGenerator::Short CodeGenerator::Short::operator+(unsigned short s)
	{
		Short temp;
		temp = *this;
		temp += s;
		return temp;
	}

	CodeGenerator::Short CodeGenerator::Short::operator-(unsigned short s)
	{
		Short temp;
		temp = *this;
		temp -= s;
		return temp;		
	}

	CodeGenerator::Short CodeGenerator::Short::operator*(unsigned short s)
	{
		Short temp;
		temp = *this;
		temp *= s;
		return temp;
	}

	CodeGenerator::Short CodeGenerator::Short::operator/(unsigned short s)
	{
		Short temp;
		temp = *this;
		temp /= s;
		return temp;
	}

	CodeGenerator::Short CodeGenerator::Short::operator%(unsigned short s)
	{
		Short temp;
		temp = *this;
		temp %= s;
		return temp;
	}

	CodeGenerator::Short CodeGenerator::Short::operator<<(unsigned short s)
	{
		Short temp;
		temp = *this;
		temp <<= s;
		return temp;
	}

	CodeGenerator::Short CodeGenerator::Short::operator>>(unsigned short s)
	{
		Short temp;
		temp = *this;
		temp >>= s;
		return temp;
	}

	CodeGenerator::Short CodeGenerator::Short::operator&(unsigned short s)
	{
		Short temp;
		temp = *this;
		temp &= s;
		return temp;
	}

	CodeGenerator::Short CodeGenerator::Short::operator^(unsigned short s)
	{
		Short temp;
		temp = *this;
		temp ^= s;
		return temp;
	}

	CodeGenerator::Short CodeGenerator::Short::operator|(unsigned short s)
	{
		Short temp;
		temp = *this;
		temp |= s;
		return temp;
	}

	CodeGenerator::Dword::Dword() : Variable(4)
	{
	}

	CodeGenerator::Dword::operator OperandREG32() const
	{
		return cg->r32(ebp + ref());
	}

	CodeGenerator::Int::Int()
	{
	}

	CodeGenerator::Int::Int(unsigned int i)
	{
		cg->mov(*this, i);
	}

	CodeGenerator::Int::Int(const Int &i)
	{
		cg->mov(*this, i);
	}

	CodeGenerator::Int &CodeGenerator::Int::operator=(const Int &i)
	{
		cg->mov(*this, cg->m32(ebp + i.ref()));
		return *this;
	}

	CodeGenerator::Int &CodeGenerator::Int::operator+=(const Int &i)
	{
		cg->add(*this, cg->m32(ebp + i.ref()));
		return *this;
	}

	CodeGenerator::Int &CodeGenerator::Int::operator-=(const Int &i)
	{
		cg->sub(*this, cg->m32(ebp + i.ref()));
		return *this;
	}

	CodeGenerator::Int &CodeGenerator::Int::operator*=(const Int &i)
	{
		cg->imul(*this, cg->m32(ebp + i.ref()));
		return *this;
	}

	CodeGenerator::Int &CodeGenerator::Int::operator/=(const Int &i)
	{
		cg->exclude(eax);
		cg->exclude(edx);
		cg->mov(eax, cg->m32(ebp + ref()));
		cg->mov(edx, cg->m32(ebp + i.ref()));
		cg->idiv(edx);
		cg->mov(*this, eax);
		return *this;
	}

	CodeGenerator::Int &CodeGenerator::Int::operator%=(const Int &i)
	{
		cg->exclude(eax);
		cg->exclude(edx);
		cg->mov(eax, cg->m32(ebp + ref()));
		cg->mov(edx, cg->m32(ebp + i.ref()));
		cg->idiv(edx);
		cg->mov(*this, edx);
		return *this;
	}

	CodeGenerator::Int &CodeGenerator::Int::operator<<=(const Int &i)
	{
		cg->exclude(ecx);
		cg->mov(ecx, cg->m32(ebp + ref()));
		cg->shl(*this, cl);
		return *this;
	}

	CodeGenerator::Int &CodeGenerator::Int::operator>>=(const Int &i)
	{
		cg->exclude(ecx);
		cg->mov(ecx, cg->m32(ebp + ref()));
		cg->shr(*this, cl);
		return *this;
	}

	CodeGenerator::Int &CodeGenerator::Int::operator&=(const Int &i)
	{
		cg->and(*this, cg->m32(ebp + i.ref()));
		return *this;
	}

	CodeGenerator::Int &CodeGenerator::Int::operator^=(const Int &i)
	{
		cg->xor(*this, cg->m32(ebp + i.ref()));
		return *this;
	}

	CodeGenerator::Int &CodeGenerator::Int::operator|=(const Int &i)
	{
		cg->or(*this, cg->m32(ebp + i.ref()));
		return *this;
	}

	CodeGenerator::Int CodeGenerator::Int::operator+(const Int &i)
	{
		Int temp;
		temp = *this;
		temp += i;
		return temp;
	}

	CodeGenerator::Int CodeGenerator::Int::operator-(const Int &i)
	{
		Int temp;
		temp = *this;
		temp -= i;
		return temp;		
	}

	CodeGenerator::Int CodeGenerator::Int::operator*(const Int &i)
	{
		Int temp;
		temp = *this;
		temp *= i;
		return temp;
	}

	CodeGenerator::Int CodeGenerator::Int::operator/(const Int &i)
	{
		Int temp;
		temp = *this;
		temp /= i;
		return temp;
	}

	CodeGenerator::Int CodeGenerator::Int::operator%(const Int &i)
	{
		Int temp;
		temp = *this;
		temp %= i;
		return temp;
	}

	CodeGenerator::Int CodeGenerator::Int::operator<<(const Int &i)
	{
		Int temp;
		temp = *this;
		temp <<= i;
		return temp;
	}

	CodeGenerator::Int CodeGenerator::Int::operator>>(const Int &i)
	{
		Int temp;
		temp = *this;
		temp >>= i;
		return temp;
	}

	CodeGenerator::Int CodeGenerator::Int::operator&(const Int &i)
	{
		Int temp;
		temp = *this;
		temp &= i;
		return temp;
	}

	CodeGenerator::Int CodeGenerator::Int::operator^(const Int &i)
	{
		Int temp;
		temp = *this;
		temp ^= i;
		return temp;
	}

	CodeGenerator::Int CodeGenerator::Int::operator|(const Int &i)
	{
		Int temp;
		temp = *this;
		temp |= i;
		return temp;
	}

	CodeGenerator::Int &CodeGenerator::Int::operator+=(unsigned int i)
	{
		cg->add(*this, i);
		return *this;
	}

	CodeGenerator::Int &CodeGenerator::Int::operator-=(unsigned int i)
	{
		cg->sub(*this, i);
		return *this;
	}

	CodeGenerator::Int &CodeGenerator::Int::operator*=(unsigned int i)
	{
		cg->imul(*this, i);
		return *this;
	}

	CodeGenerator::Int &CodeGenerator::Int::operator/=(unsigned int i)
	{
		cg->exclude(eax);
		cg->exclude(edx);
		cg->mov(eax, *this);
		cg->mov(edx, i);
		cg->idiv(edx);
		cg->mov(*this, eax);
		return *this;
	}

	CodeGenerator::Int &CodeGenerator::Int::operator%=(unsigned int i)
	{
		cg->exclude(eax);
		cg->exclude(edx);
		cg->mov(eax, *this);
		cg->mov(edx, i);
		cg->idiv(edx);
		cg->mov(*this, edx);
		return *this;
	}

	CodeGenerator::Int &CodeGenerator::Int::operator<<=(unsigned int i)
	{
		cg->exclude(ecx);
		cg->exclude(edx);
		cg->mov(ecx, i);
		cg->mov(edx, *this);
		cg->shl(*this, cl);
		return *this;
	}

	CodeGenerator::Int &CodeGenerator::Int::operator>>=(unsigned int i)
	{
		cg->exclude(ecx);
		cg->exclude(edx);
		cg->mov(ecx, i);
		cg->mov(edx, *this);
		cg->shr(*this, cl);
		return *this;
	}

	CodeGenerator::Int &CodeGenerator::Int::operator&=(unsigned int i)
	{
		cg->and(*this, i);
		return *this;
	}

	CodeGenerator::Int &CodeGenerator::Int::operator^=(unsigned int i)
	{
		cg->xor(*this, i);
		return *this;
	}

	CodeGenerator::Int &CodeGenerator::Int::operator|=(unsigned int i)
	{
		cg->or(*this, i);
		return *this;
	}

	CodeGenerator::Int CodeGenerator::Int::operator+(unsigned int i)
	{
		Int temp;
		temp = *this;
		temp += i;
		return temp;
	}

	CodeGenerator::Int CodeGenerator::Int::operator-(unsigned int i)
	{
		Int temp;
		temp = *this;
		temp -= i;
		return temp;		
	}

	CodeGenerator::Int CodeGenerator::Int::operator*(unsigned int i)
	{
		Int temp;
		temp = *this;
		temp *= i;
		return temp;
	}

	CodeGenerator::Int CodeGenerator::Int::operator/(unsigned int i)
	{
		Int temp;
		temp = *this;
		temp /= i;
		return temp;
	}

	CodeGenerator::Int CodeGenerator::Int::operator%(unsigned int i)
	{
		Int temp;
		temp = *this;
		temp %= i;
		return temp;
	}

	CodeGenerator::Int CodeGenerator::Int::operator<<(unsigned int i)
	{
		Int temp;
		temp = *this;
		temp <<= i;
		return temp;
	}

	CodeGenerator::Int CodeGenerator::Int::operator>>(unsigned int i)
	{
		Int temp;
		temp = *this;
		temp >>= i;
		return temp;
	}

	CodeGenerator::Int CodeGenerator::Int::operator&(unsigned int i)
	{
		Int temp;
		temp = *this;
		temp &= i;
		return temp;
	}

	CodeGenerator::Int CodeGenerator::Int::operator^(unsigned int i)
	{
		Int temp;
		temp = *this;
		temp ^= i;
		return temp;
	}

	CodeGenerator::Int CodeGenerator::Int::operator|(unsigned int i)
	{
		Int temp;
		temp = *this;
		temp |= i;
		return temp;
	}

	CodeGenerator::Qword::Qword() : Variable(8)
	{
	}

	CodeGenerator::Qword::Qword(const Qword &qword) : Variable(8)
	{
		cg->movq(*this, qword);
	}

	CodeGenerator::Qword::operator OperandMMREG() const
	{
		return cg->r64(ebp + ref());
	}

	CodeGenerator::Qword &CodeGenerator::Qword::operator=(const Qword &qword)
	{
		cg->movq(*this, cg->m64(ebp + qword.ref()));
		return *this;
	}

	CodeGenerator::Qword &CodeGenerator::Qword::operator+=(const Qword &qword)
	{
		cg->paddq(*this, cg->m64(ebp + qword.ref()));
		return *this;
	}

	CodeGenerator::Qword &CodeGenerator::Qword::operator-=(const Qword &qword)
	{
		cg->psubq(*this, cg->m64(ebp + qword.ref()));
		return *this;
	}

	CodeGenerator::Qword &CodeGenerator::Qword::operator<<=(const Qword &qword)
	{
		cg->psllq(*this, cg->m64(ebp + qword.ref()));
		return *this;
	}

	CodeGenerator::Qword &CodeGenerator::Qword::operator&=(const Qword &qword)
	{
		cg->pand(*this, cg->m64(ebp + qword.ref()));
		return *this;
	}

	CodeGenerator::Qword &CodeGenerator::Qword::operator^=(const Qword &qword)
	{
		cg->pxor(*this, cg->m64(ebp + qword.ref()));
		return *this;
	}

	CodeGenerator::Qword &CodeGenerator::Qword::operator|=(const Qword &qword)
	{
		cg->por(*this, cg->m64(ebp + qword.ref()));
		return *this;
	}

	CodeGenerator::Qword CodeGenerator::Qword::operator+(const Qword &qword)
	{
		Qword temp;
		temp = *this;
		temp += qword;
		return temp;
	}

	CodeGenerator::Qword CodeGenerator::Qword::operator-(const Qword &qword)
	{
		Qword temp;
		temp = *this;
		temp -= qword;
		return temp;
	}

	CodeGenerator::Qword CodeGenerator::Qword::operator<<(const Qword &qword)
	{
		Qword temp;
		temp = *this;
		temp <<= qword;
		return temp;
	}

	CodeGenerator::Qword CodeGenerator::Qword::operator&(const Qword &qword)
	{
		Qword temp;
		temp = *this;
		temp &= qword;
		return temp;
	}

	CodeGenerator::Qword CodeGenerator::Qword::operator^(const Qword &qword)
	{
		Qword temp;
		temp = *this;
		temp ^= qword;
		return temp;
	}

	CodeGenerator::Qword CodeGenerator::Qword::operator|(const Qword &qword)
	{
		Qword temp;
		temp = *this;
		temp |= qword;
		return temp;
	}

	CodeGenerator::Qword &CodeGenerator::Qword::operator<<=(char imm)
	{
		cg->psllq(*this, imm);
		return *this;
	}

	CodeGenerator::Qword CodeGenerator::Qword::operator<<(char imm)
	{
		Qword temp;
		temp = *this;
		temp <<= imm;
		return temp;
	}

	CodeGenerator::Word4::Word4()
	{
	}

	CodeGenerator::Word4::Word4(const Word4 &word4)
	{
		cg->movq(*this, word4);
	}

	CodeGenerator::Word4::operator OperandMMREG() const
	{
		return cg->r64(ebp + ref());
	}

	CodeGenerator::Word4 &CodeGenerator::Word4::operator=(const Word4 &word4)
	{
		cg->movq(*this, cg->m64(ebp + word4.ref()));
		return *this;
	}

	CodeGenerator::Word4 &CodeGenerator::Word4::operator+=(const Word4 &word4)
	{
		cg->paddw(*this, cg->m64(ebp + word4.ref()));
		return *this;
	}

	CodeGenerator::Word4 &CodeGenerator::Word4::operator-=(const Word4 &word4)
	{
		cg->psubw(*this, cg->m64(ebp + word4.ref()));
		return *this;
	}

	CodeGenerator::Word4 &CodeGenerator::Word4::operator<<=(const Qword &qword)
	{
		cg->psllw(*this, cg->m64(ebp + qword.ref()));
		return *this;
	}

	CodeGenerator::Word4 &CodeGenerator::Word4::operator>>=(const Qword &qword)
	{
		cg->psraw(*this, cg->m64(ebp + qword.ref()));
		return *this;
	}

	CodeGenerator::Word4 &CodeGenerator::Word4::operator&=(const Word4 &word4)
	{
		cg->pand(*this, cg->m64(ebp + word4.ref()));
		return *this;
	}

	CodeGenerator::Word4 &CodeGenerator::Word4::operator^=(const Word4 &word4)
	{
		cg->pxor(*this, cg->m64(ebp + word4.ref()));
		return *this;
	}

	CodeGenerator::Word4 &CodeGenerator::Word4::operator|=(const Word4 &word4)
	{
		cg->por(*this, cg->m64(ebp + word4.ref()));
		return *this;
	}

	CodeGenerator::Word4 CodeGenerator::Word4::operator+(const Word4 &word4)
	{
		Word4 temp;
		temp = *this;
		temp += word4;
		return temp;
	}

	CodeGenerator::Word4 CodeGenerator::Word4::operator-(const Word4 &word4)
	{
		Word4 temp;
		temp = *this;
		temp -= word4;
		return temp;
	}

	CodeGenerator::Word4 CodeGenerator::Word4::operator<<(const Qword &qword)
	{
		Word4 temp;
		temp = *this;
		temp <<= qword;
		return temp;
	}

	CodeGenerator::Word4 CodeGenerator::Word4::operator>>(const Qword &qword)
	{
		Word4 temp;
		temp = *this;
		temp >>= qword;
		return temp;
	}

	CodeGenerator::Word4 CodeGenerator::Word4::operator&(const Word4 &word4)
	{
		Word4 temp;
		temp = *this;
		temp &= word4;
		return temp;
	}

	CodeGenerator::Word4 CodeGenerator::Word4::operator^(const Word4 &word4)
	{
		Word4 temp;
		temp = *this;
		temp ^= word4;
		return temp;
	}

	CodeGenerator::Word4 CodeGenerator::Word4::operator|(const Word4 &word4)
	{
		Word4 temp;
		temp = *this;
		temp |= word4;
		return temp;
	}

	CodeGenerator::Word4 &CodeGenerator::Word4::operator<<=(char imm)
	{
		cg->psllw(*this, imm);
		return *this;
	}

	CodeGenerator::Word4 &CodeGenerator::Word4::operator>>=(char imm)
	{
		cg->psraw(*this, imm);
		return *this;
	}

	CodeGenerator::Word4 CodeGenerator::Word4::operator<<(char imm)
	{
		Word4 temp;
		temp = *this;
		temp <<= imm;
		return temp;
	}

	CodeGenerator::Word4 CodeGenerator::Word4::operator>>(char imm)
	{
		Word4 temp;
		temp = *this;
		temp >>= imm;
		return temp;
	}

	CodeGenerator::Dword2::Dword2()
	{
	}

	CodeGenerator::Dword2::Dword2(const Dword2 &dword2)
	{
		cg->movq(*this, dword2);
	}

	CodeGenerator::Dword2::operator OperandMMREG() const
	{
		return cg->r64(ebp + ref());
	}

	CodeGenerator::Dword2 &CodeGenerator::Dword2::operator=(const Dword2 &dword2)
	{
		cg->movq(*this, cg->m64(ebp + dword2.ref()));
		return *this;
	}

	CodeGenerator::Dword2 &CodeGenerator::Dword2::operator+=(const Dword2 &dword2)
	{
		cg->paddd(*this, cg->m64(ebp + dword2.ref()));
		return *this;
	}

	CodeGenerator::Dword2 &CodeGenerator::Dword2::operator-=(const Dword2 &dword2)
	{
		cg->psubd(*this, cg->m64(ebp + dword2.ref()));
		return *this;
	}

	CodeGenerator::Dword2 &CodeGenerator::Dword2::operator<<=(const Qword &qword)
	{
		cg->pslld(*this, cg->m64(ebp + qword.ref()));
		return *this;
	}

	CodeGenerator::Dword2 &CodeGenerator::Dword2::operator>>=(const Qword &qword)
	{
		cg->psrad(*this, cg->m64(ebp + qword.ref()));
		return *this;
	}

	CodeGenerator::Dword2 &CodeGenerator::Dword2::operator&=(const Dword2 &dword2)
	{
		cg->pand(*this, cg->m64(ebp + dword2.ref()));
		return *this;
	}

	CodeGenerator::Dword2 &CodeGenerator::Dword2::operator^=(const Dword2 &dword2)
	{
		cg->pxor(*this, cg->m64(ebp + dword2.ref()));
		return *this;
	}

	CodeGenerator::Dword2 &CodeGenerator::Dword2::operator|=(const Dword2 &dword2)
	{
		cg->por(*this, cg->m64(ebp + dword2.ref()));
		return *this;
	}

	CodeGenerator::Dword2 CodeGenerator::Dword2::operator+(const Dword2 &dword2)
	{
		Dword2 temp;
		temp = *this;
		temp += dword2;
		return temp;
	}

	CodeGenerator::Dword2 CodeGenerator::Dword2::operator-(const Dword2 &dword2)
	{
		Dword2 temp;
		temp = *this;
		temp -= dword2;
		return temp;
	}

	CodeGenerator::Dword2 CodeGenerator::Dword2::operator<<(const Qword &qword)
	{
		Dword2 temp;
		temp = *this;
		temp <<= qword;
		return temp;
	}

	CodeGenerator::Dword2 CodeGenerator::Dword2::operator>>(const Qword &qword)
	{
		Dword2 temp;
		temp = *this;
		temp >>= qword;
		return temp;
	}

	CodeGenerator::Dword2 CodeGenerator::Dword2::operator&(const Dword2 &dword2)
	{
		Dword2 temp;
		temp = *this;
		temp &= dword2;
		return temp;
	}

	CodeGenerator::Dword2 CodeGenerator::Dword2::operator^(const Dword2 &dword2)
	{
		Dword2 temp;
		temp = *this;
		temp ^= dword2;
		return temp;
	}

	CodeGenerator::Dword2 CodeGenerator::Dword2::operator|(const Dword2 &dword2)
	{
		Dword2 temp;
		temp = *this;
		temp |= dword2;
		return temp;
	}

	CodeGenerator::Dword2 &CodeGenerator::Dword2::operator<<=(char imm)
	{
		cg->pslld(*this, imm);
		return *this;
	}

	CodeGenerator::Dword2 &CodeGenerator::Dword2::operator>>=(char imm)
	{
		cg->psrad(*this, imm);
		return *this;
	}

	CodeGenerator::Dword2 CodeGenerator::Dword2::operator<<(char imm)
	{
		Dword2 temp;
		temp = *this;
		temp <<= imm;
		return temp;
	}

	CodeGenerator::Dword2 CodeGenerator::Dword2::operator>>(char imm)
	{
		Dword2 temp;
		temp = *this;
		temp >>= imm;
		return temp;
	}

	CodeGenerator::Float::Float() : Variable(4)
	{
	}

	CodeGenerator::Float::Float(const Float &f) : Variable(4)
	{
		cg->movss(*this, f);
	}

	CodeGenerator::Float::operator OperandXMMREG() const
	{
		return cg->rSS(ebp + ref());
	}

	CodeGenerator::Float &CodeGenerator::Float::operator=(const Float &f)
	{
		cg->movss(*this, cg->mSS(ebp + f.ref()));
		return *this;
	}

	CodeGenerator::Float &CodeGenerator::Float::operator+=(const Float &f)
	{
		cg->movss(*this, cg->mSS(ebp + f.ref()));
		return *this;
	}

	CodeGenerator::Float &CodeGenerator::Float::operator-=(const Float &f)
	{
		cg->subss(*this, cg->mSS(ebp + f.ref()));
		return *this;
	}

	CodeGenerator::Float &CodeGenerator::Float::operator*=(const Float &f)
	{
		cg->mulss(*this, cg->mSS(ebp + f.ref()));
		return *this;
	}

	CodeGenerator::Float &CodeGenerator::Float::operator/=(const Float &f)
	{
		cg->divss(*this, cg->mSS(ebp + f.ref()));
		return *this;
	}

	CodeGenerator::Float CodeGenerator::Float::operator+(const Float &f)
	{
		Float temp;
		temp = *this;
		temp += f;
		return temp;
	}

	CodeGenerator::Float CodeGenerator::Float::operator-(const Float &f)
	{
		Float temp;
		temp = *this;
		temp -= f;
		return temp;
	}

	CodeGenerator::Float CodeGenerator::Float::operator*(const Float &f)
	{
		Float temp;
		temp = *this;
		temp *= f;
		return temp;
	}

	CodeGenerator::Float CodeGenerator::Float::operator/(const Float &f)
	{
		Float temp;
		temp = *this;
		temp /= f;
		return temp;
	}

	CodeGenerator::Xword::Xword() : Variable(16)
	{
	}

	CodeGenerator::Xword::operator OperandXMMREG() const
	{
		return cg->r128(ebp + ref());
	}

	CodeGenerator::Float4::Float4()
	{
	}

	CodeGenerator::Float4::Float4(const Float4 &float4)
	{
		cg->movaps(*this, float4);
	}

	CodeGenerator::Float4::Float4(const Float &f)
	{
		cg->movss(*this, f);
		cg->shufps(*this, *this, 0x00);
	}

	CodeGenerator::Float4 &CodeGenerator::Float4::operator=(const Float4 &float4)
	{
		cg->movaps(*this, cg->m128(ebp + float4.ref()));
		return *this;
	}

	CodeGenerator::Float4 &CodeGenerator::Float4::operator+=(const Float4 &float4)
	{
		cg->addps(*this, cg->m128(ebp + float4.ref()));
		return *this;
	}

	CodeGenerator::Float4 &CodeGenerator::Float4::operator-=(const Float4 &float4)
	{
		cg->subps(*this, cg->m128(ebp + float4.ref()));
		return *this;
	}

	CodeGenerator::Float4 &CodeGenerator::Float4::operator*=(const Float4 &float4)
	{
		cg->mulps(*this, cg->m128(ebp + float4.ref()));
		return *this;
	}

	CodeGenerator::Float4 &CodeGenerator::Float4::operator/=(const Float4 &float4)
	{
		cg->divps(*this, cg->m128(ebp + float4.ref()));
		return *this;
	}

	CodeGenerator::Float4 &CodeGenerator::Float4::operator&=(const Float4 &float4)
	{
		cg->andps(*this, cg->m128(ebp + float4.ref()));
		return *this;
	}

	CodeGenerator::Float4 &CodeGenerator::Float4::operator^=(const Float4 &float4)
	{
		cg->xorps(*this, cg->m128(ebp + float4.ref()));
		return *this;
	}

	CodeGenerator::Float4 &CodeGenerator::Float4::operator|=(const Float4 &float4)
	{
		cg->orps(*this, cg->m128(ebp + float4.ref()));
		return *this;
	}

	CodeGenerator::Float4 CodeGenerator::Float4::operator+(const Float4 &float4)
	{
		Float4 temp;
		temp = *this;
		temp += float4;
		return temp;
	}

	CodeGenerator::Float4 CodeGenerator::Float4::operator-(const Float4 &float4)
	{
		Float4 temp;
		temp = *this;
		temp -= float4;
		return temp;
	}

	CodeGenerator::Float4 CodeGenerator::Float4::operator*(const Float4 &float4)
	{
		Float4 temp;
		temp = *this;
		temp *= float4;
		return temp;
	}

	CodeGenerator::Float4 CodeGenerator::Float4::operator/(const Float4 &float4)
	{
		Float4 temp;
		temp = *this;
		temp /= float4;
		return temp;
	}

	CodeGenerator::Float4 CodeGenerator::Float4::operator&(const Float4 &float4)
	{
		Float4 temp;
		temp = *this;
		temp &= float4;
		return temp;
	}
	
	CodeGenerator::Float4 CodeGenerator::Float4::operator^(const Float4 &float4)
	{
		Float4 temp;
		temp = *this;
		temp ^= float4;
		return temp;
	}

	CodeGenerator::Float4 CodeGenerator::Float4::operator|(const Float4 &float4)
	{
		Float4 temp;
		temp = *this;
		temp |= float4;
		return temp;
	}

	CodeGenerator::CodeGenerator(bool x64) : Emulator(x64)
	{
		cg = this;
	}

	CodeGenerator::~CodeGenerator()
	{
		// Reset stack
		stack = -128;
		stackTop = -128;
		stackUpdate = 0;
	}

	void CodeGenerator::prologue(int functionArguments)
	{
		cg = this;

		if(!x64)
		{
			mov(arg, esp);

			push(edi);
			push(esi);
			push(ebx);

			push(ebp);
			mov(ebp, esp);
			stackUpdate =
			sub(ebp, stackTop);
			lea(esp, dword_ptr [ebp-128-12]);
			and(ebp, 0xFFFFFFF0);
		}
		else
		{
			push(rbp);
			push(rbx);
			push(r12);
			push(r13);
			push(r14);
			push(r15);
			stackUpdate =
			sub(rsp, 32+stackTop+128);
		}
	};

	OperandMEM32 CodeGenerator::argument(int i)
	{
		return dword_ptr [arg + 4 * i + 4];
	}

	void CodeGenerator::epilogue()
	{
		cg = this;

		if(!x64)
		{
			add(esp, stackTop+128+12);
			pop(ebp);

			pop(ebx);
			pop(esi);
			pop(edi);
		}
		else
		{
			add(rsp, 32+stackTop+128);
			pop(r15);
			pop(r14);
			pop(r13);
			pop(r12);
			pop(rbx);
			pop(rbp);
		}

		ret();
	}

	void CodeGenerator::free(Variable &var1)
	{
		var1.free();
	}

	void CodeGenerator::free(Variable &var1, Variable &var2)
	{
		var1.free();
		var2.free();
	}

	void CodeGenerator::free(Variable &var1, Variable &var2, Variable &var3)
	{
		var1.free();
		var2.free();
		var3.free();
	}

	void CodeGenerator::free(Variable &var1, Variable &var2, Variable &var3, Variable &var4)
	{
		var1.free();
		var2.free();
		var3.free();
		var4.free();
	}

	void CodeGenerator::free(Variable &var1, Variable &var2, Variable &var3, Variable &var4, Variable &var5)
	{
		var1.free();
		var2.free();
		var3.free();
		var4.free();
		var5.free();
	}
}
