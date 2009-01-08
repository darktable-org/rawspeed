#include "StdAfx.h"
#pragma warning (disable: 4311)     // pointer truncation
#include "BitPumpSSSE3.h"



#define UNPACK_LOOPS 64 // 1024 bytes

BitPumpSSSE3::BitPumpSSSE3(const unsigned char* _buffer, int _size) : BitPump(_buffer, _size)
{
  mData[0] = (unsigned char*)buffer;
  for (int i = 1; i<8; i++) {
    // mData[0] is set to input.
    mData[i] = (unsigned char*)_aligned_malloc(UNPACK_LOOPS*16,16);
  }
  GenerateAssembly(unpack, !((int)buffer&15));
  entry = (void(*)())unpack.callable();
  fillBuffers();
}

BitPumpSSSE3::~BitPumpSSSE3(void)
{
  for (int i = 1; i<8; i++) {
    _aligned_free(mData[i]);
  }
}

int BitPumpSSSE3::getBit() {
#ifdef _DEBUG
  if (mData[0] + ((off+1)>>3) >= end)
     throw BitPumpException("Out of buffer read");
#endif
  if (off >= UNPACK_LOOPS * 16 * 8)
    fillBuffers();
  int ret = mData[off&7][off>>3]&1;
  off++;
  return ret;
}

int BitPumpSSSE3::getBits(unsigned int nbits) {
  _ASSERT(nbits<32 && nbits>0);
#ifdef _DEBUG
  if (mData[0] + ((off+nbits)>>3) >= end)
     throw BitPumpException("Out of buffer read");
#endif
	if (off+nbits >= (UNPACK_LOOPS * 16 * 8)) { // We have too few - very rare case
    int left = (UNPACK_LOOPS * 16 * 8 - 1) - nbits;
    int ret = 0;
    if (left) {
      ret = getBits(left);
    }
    nbits -= left;
    fillBuffers();
    return ret | (getBits(nbits)<<left);
	}
  // We need to cast to int, because we may be returning more than 8 bits.
	int ret = (*(unsigned int*)&mData[off&7][off>>3]) & masks[nbits];
  off += nbits;
	return ret;
}

int BitPumpSSSE3::peekBit() {
#ifdef _DEBUG
  if (mData[0] + ((off+1)>>3) >= end)
     throw BitPumpException("Out of buffer read");
#endif
  if (off >= UNPACK_LOOPS * 16 * 8) {
    return mData[0][UNPACK_LOOPS * 16]&1;
  }
  int ret = mData[off&7][off>>3]&1;
  return ret;
}

int BitPumpSSSE3::peekBits(unsigned int nbits) {
     throw BitPumpException("Not implemented");
/*  _ASSERT(nbits<32 && nbits>0);
#ifdef _DEBUG
  if (mData[0] + ((off+nbits)>>3) >= end)
     throw BitPumpException("Out of buffer read");
#endif
	if (off+nbits >= (UNPACK_LOOPS * 16 * 8)) { // We have too few - very rare case
    int left = (UNPACK_LOOPS * 16 * 8 - 1) - nbits;
    int ret = 0;
    if (left) {
      ret = getBits(left);
    }
    nbits -= left;
    fillBuffers();
    return ret | (getBits(nbits)<<left);
	}
  // We need to cast to int, because we may be returning more than 8 bits.*/
	int ret = (*(unsigned int*)&mData[off&7][off>>3]) & masks[nbits];
  off += nbits;
	return ret;
}

void BitPumpSSSE3::skipBits(unsigned int nbits) {
  off+=nbits;
}

int BitPumpSSSE3::getBitSafe() {
  if (mData[0] + ((off+1)>>3) >= end)
     throw BitPumpException("Out of buffer read");
  return getBit();
}

int BitPumpSSSE3::getBitsSafe(unsigned int nbits) {
  if (mData[0] + ((off+nbits)>>3) >= end)
     throw BitPumpException("Out of buffer read");
  return getBits(nbits);
}

void BitPumpSSSE3::fillBuffers() {
  off = 0;
  mData[0] += UNPACK_LOOPS * 16;
  entry();
}

void BitPumpSSSE3::GenerateAssembly(Assembler &x86, bool aligned) {
    x86.push(esi);
    x86.push(edi);
    x86.push(eax);
    x86.push(ebx);
    x86.push(ecx);
    x86.push(edx);
    x86.push(ebp);

    x86.mov(eax, (int)&mData[0]);       // Move pointer to mData[0] to eax.
    x86.mov(esi, dword_ptr[eax]);       // Move source pointer to esi.
    x86.xor(ebp, ebp);                  // ebp is offset counter

    x86.mov(eax, (int)mData[1]);             // eax caches mData[1]
    x86.mov(ebx, (int)mData[2]);             // ebx caches mData[2]
    x86.mov(ecx, (int)mData[3]);             // ecx caches mData[3]
    x86.mov(edx, (int)mData[4]);             // edx caches mData[4]

    if (aligned) {
      x86.movdqa(xmm0, xmmword_ptr[esi]);
    } else {
      x86.movdqu(xmm0, xmmword_ptr[esi]);
    }
    x86.align(16);
    x86.label("loopback");
    if (aligned) {
      x86.movdqa(xmm1, xmmword_ptr[esi+16]);
    } else {
      x86.movdqu(xmm1, xmmword_ptr[esi+16]);
    }
    x86.movdqa(xmm2,xmm0);
    x86.movdqa(xmm3,xmm0);
    x86.movdqa(xmm4,xmm0);
    x86.palignr(xmm2,xmm1, 1);
    x86.palignr(xmm3,xmm1, 2);
    x86.palignr(xmm4,xmm1, 3);
    x86.movdqa(xmm5,xmm0);
    x86.movdqa(xmm6,xmm0);
    x86.movdqa(xmm7,xmm0);
    x86.movdqa(xmmword_ptr[eax+ebp], xmm2);
    x86.movdqa(xmmword_ptr[ebx+ebp], xmm3);
    x86.movdqa(xmmword_ptr[ecx+ebp], xmm4);
    x86.palignr(xmm5,xmm1, 4);
    x86.palignr(xmm6,xmm1, 5);
    x86.movdqa(xmmword_ptr[edx+ebp], xmm5);
    x86.mov(edi, (int)mData[5]);
    x86.palignr(xmm7,xmm1, 6);
    x86.movdqa(xmmword_ptr[edi+ebp], xmm6);
    x86.mov(edi, (int)mData[6]);
    x86.palignr(xmm0,xmm1, 7);
    x86.movdqa(xmmword_ptr[edi+ebp], xmm7);
    x86.mov(edi, (int)mData[7]);
    x86.movdqa(xmmword_ptr[edi+ebp], xmm0);
    x86.add(ebp, 16);
    x86.add(esi, 16);
    x86.movdqa(xmm0,xmm1);
    x86.cmp(ebp, UNPACK_LOOPS*16);
    x86.jl("loopback");

    x86.pop(ebp);
    x86.pop(edx);
    x86.pop(ecx);
    x86.pop(ebx);
    x86.pop(eax);
    x86.pop(edi);
    x86.pop(esi);
    x86.ret();
}
