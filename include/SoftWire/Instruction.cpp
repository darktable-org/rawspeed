#include "Instruction.hpp"

#include "Error.hpp"
#include "String.hpp"

namespace SoftWire
{
	Instruction::Instruction()
	{
	}

	Instruction::Instruction(const Syntax *syntax)
	{
		this->syntax = syntax;

		extractOperands(syntax->operands);

		if(secondOperand == Operand::OPERAND_IMM8)
		{
			if(Operand::isSubtypeOf(firstOperand, Operand::OPERAND_R_M16) ||
			   Operand::isSubtypeOf(firstOperand, Operand::OPERAND_R_M32))
			{
				secondOperand = Operand::OPERAND_EXT8;
			}
		}
	}

	Instruction::~Instruction()
	{
	}

	Instruction &Instruction::operator=(const Instruction &instruction)
	{
		syntax = instruction.syntax;

		specifier = instruction.specifier;
		firstOperand = instruction.firstOperand;
		secondOperand = instruction.secondOperand;
		thirdOperand = instruction.thirdOperand;

		return *this;
	}

	void Instruction::extractOperands(const char *syntax)
	{
		if(!syntax)
		{
			throw INTERNAL_ERROR;
		}

		specifier = Specifier::TYPE_UNKNOWN;
		firstOperand = Operand::OPERAND_VOID;
		secondOperand = Operand::OPERAND_VOID;
		thirdOperand = Operand::OPERAND_VOID;

		char string[256];
		strncpy(string, syntax, 255);
		const char *token = strtok(string, " ,");

		if(!token)
		{
			return;
		}

		specifier = Specifier::scan(token);

		if(specifier != Specifier::TYPE_UNKNOWN)
		{
			token = strtok(0, " ,");

			if(!token)
			{
				return;
			}
		}

		firstOperand = Operand::scanSyntax(token);

		if(firstOperand != Operand::OPERAND_UNKNOWN)
		{
			token = strtok(0, " ,");

			if(token == 0)
			{
				return;
			}
		}

		secondOperand = Operand::scanSyntax(token);

		if(secondOperand != Operand::OPERAND_UNKNOWN)
		{
			token = strtok(0, " ,");

			if(token == 0)
			{
				return;
			}
		}

		thirdOperand = Operand::scanSyntax(token);

		if(thirdOperand != Operand::OPERAND_UNKNOWN)
		{
			token = strtok(0, " ,");

			if(token == 0)
			{
				return;
			}
		}

		if(token == 0)
		{
			return;
		}
		else
		{
			throw Error("Invalid operand encoding '%s'", syntax);
		}
	}

	const char *Instruction::getMnemonic() const
	{
		return syntax->mnemonic;
	}

	Operand::Type Instruction::getFirstOperand() const
	{
		return firstOperand;
	}

	Operand::Type Instruction::getSecondOperand() const
	{
		return secondOperand;
	}

	Operand::Type Instruction::getThirdOperand() const
	{
		return thirdOperand;
	}

	const char *Instruction::getOperandSyntax() const
	{
		return syntax->operands;
	}

	const char *Instruction::getEncoding() const
	{
		return syntax->encoding;
	}

	bool Instruction::is16Bit() const
	{
		return (syntax->flags & CPU_386) != CPU_386;
	}

	bool Instruction::is32Bit() const
	{
		return (syntax->flags & CPU_386) == CPU_386;
	}

	bool Instruction::is64Bit() const
	{
		return (syntax->flags & CPU_X64) == CPU_X64;
	}

	bool Instruction::isInvalid64() const
	{
		return (syntax->flags & CPU_INVALID64) == CPU_INVALID64;
	}

	int Instruction::approximateSize() const
	{
		const char *format = syntax->encoding;

		if(!format)
		{
			throw INTERNAL_ERROR;
		}

		int size = 0;

		while(*format)
		{
			switch((format[0] << 8) | format[1])
			{
			case LOCK_PRE:
			case CONST_PRE:
			case REPNE_PRE:
			case REP_PRE:
				size += 1;
				break;
			case OFF_PRE:
				if(!is32Bit())
				{
					size += 1;
				}
				break;
			case ADDR_PRE:
				if(!is32Bit())
				{
					size += 1;
				}
				break;
			case ADD_REG:
				break;
			case EFF_ADDR:
			case MOD_RM_0:
			case MOD_RM_1:
			case MOD_RM_2:
			case MOD_RM_3:
			case MOD_RM_4:
			case MOD_RM_5:
			case MOD_RM_6:
			case MOD_RM_7:
				size += 1;
				break;
			case DWORD_IMM:
			case DWORD_REL:
				size += 4;
				break;
			case WORD_IMM:
				size += 2;
				break;
			case BYTE_IMM:
			case BYTE_REL:
				size += 1;
				break;
			default:
				size += 1;
			}

			format += 2;

			if(*format == ' ')	
			{
				format++;
			}
			else if(*format == '\0')
			{
				break;
			}
			else
			{
				throw INTERNAL_ERROR;
			}
		}

		return size;
	}
}
