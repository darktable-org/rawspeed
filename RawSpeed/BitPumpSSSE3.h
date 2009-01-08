#pragma once
#include "bitpump.h"
#include "../include/softwire/CodeGenerator.hpp"
using namespace SoftWire; 

/********************************************
 * Fast Bitpump for SSSE3 enabled machines.
 * (c) Klaus Post 2008.
 * 
 * We avoid shifting and other bit-magic, by unpacking 
 * 1024 bytes into 7 preshifted arrays.
 * This means we only have a conditional to refill the buffer 
 * every 1024 bytes.
 *
 * We use very fast SSSE3 for shifting.
 *
 * This class is not re-entrant, since it's a streaming class.
 *
 * Someone keen on optimization could move unpacking into another thread
 * and use a buffer switch mechanism.
 *******************************************/


class BitPumpSSSE3 :
  public BitPump, public  CodeGenerator
{
public:
  BitPumpSSSE3(const unsigned char* _buffer, int _size);
  virtual ~BitPumpSSSE3(void);
	virtual int getBits(unsigned int nbits);
	virtual int getBit();
	virtual int getBitsSafe(unsigned int nbits);
	virtual int getBitSafe();
	virtual int peekBits(unsigned int nbits);
	virtual int peekBit();
  virtual void skipBits(unsigned int nbits);
private:
  void GenerateAssembly(Assembler &x86, bool aligned);
  void fillBuffers();
  void (*entry)();
  Assembler unpack; 
  unsigned char* mData[8];
};
