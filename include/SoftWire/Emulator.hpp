#ifndef SoftWire_Emulator_hpp
#define SoftWire_Emulator_hpp

#include "Optimizer.hpp"

namespace SoftWire
{
	class Emulator : public Optimizer
	{
	public:
		// Emulation flags
		static void enableEmulateSSE();   // Default off
		static void disableEmulateSSE();

	protected:
		Emulator(bool x64);

		virtual ~Emulator();

		OperandREG8 t8(unsigned int i);
		OperandREG16 t16(unsigned int i);
		OperandREG32 t32(unsigned int i);

		// Overloaded to emulate
		Encoding *addps(OperandXMMREG xmmi, OperandXMMREG xmmj);
		Encoding *addps(OperandXMMREG xmm, OperandMEM128 mem128);
		Encoding *addps(OperandXMMREG xmm, OperandR_M128 r_m128);

		Encoding *addss(OperandXMMREG xmmi, OperandXMMREG xmmj);
		Encoding *addss(OperandXMMREG xmm, OperandMEM32 mem32);
		Encoding *addss(OperandXMMREG xmm, OperandXMM32 xmm32);

		Encoding *andnps(OperandXMMREG xmmi, OperandXMMREG xmmj);
		Encoding *andnps(OperandXMMREG xmm, OperandMEM128 mem128);
		Encoding *andnps(OperandXMMREG xmm, OperandR_M128 r_m128);

		Encoding *andps(OperandXMMREG xmmi, OperandXMMREG xmmj);
		Encoding *andps(OperandXMMREG xmm, OperandMEM128 mem128);
		Encoding *andps(OperandXMMREG xmm, OperandR_M128 r_m128);

		Encoding *cmpps(OperandXMMREG xmmi, OperandXMMREG xmmj, char c);
		Encoding *cmpps(OperandXMMREG xmm, OperandMEM128 mem128, char c);
		Encoding *cmpps(OperandXMMREG xmm, OperandR_M128 r_m128, char c);

		Encoding *cmpeqps(OperandXMMREG xmmi, OperandXMMREG xmmj);
		Encoding *cmpeqps(OperandXMMREG xmm, OperandMEM128 mem128);
		Encoding *cmpeqps(OperandXMMREG xmm, OperandR_M128 r_m128);
		Encoding *cmpleps(OperandXMMREG xmmi, OperandXMMREG xmmj);
		Encoding *cmpleps(OperandXMMREG xmm, OperandMEM128 mem128);
		Encoding *cmpleps(OperandXMMREG xmm, OperandR_M128 r_m128);
		Encoding *cmpltps(OperandXMMREG xmmi, OperandXMMREG xmmj);
		Encoding *cmpltps(OperandXMMREG xmm, OperandMEM128 mem128);
		Encoding *cmpltps(OperandXMMREG xmm, OperandR_M128 r_m128);
		Encoding *cmpneqps(OperandXMMREG xmmi, OperandXMMREG xmmj);
		Encoding *cmpneqps(OperandXMMREG xmm, OperandMEM128 mem128);
		Encoding *cmpneqps(OperandXMMREG xmm, OperandR_M128 r_m128);
		Encoding *cmpnleps(OperandXMMREG xmmi, OperandXMMREG xmmj);
		Encoding *cmpnleps(OperandXMMREG xmm, OperandMEM128 mem128);
		Encoding *cmpnleps(OperandXMMREG xmm, OperandR_M128 r_m128);
		Encoding *cmpnltps(OperandXMMREG xmmi, OperandXMMREG xmmj);
		Encoding *cmpnltps(OperandXMMREG xmm, OperandMEM128 mem128);
		Encoding *cmpnltps(OperandXMMREG xmm, OperandR_M128 r_m128);
		Encoding *cmpordps(OperandXMMREG xmmi, OperandXMMREG xmmj);
		Encoding *cmpordps(OperandXMMREG xmm, OperandMEM128 mem128);
		Encoding *cmpordps(OperandXMMREG xmm, OperandR_M128 r_m128);
		Encoding *cmpunordps(OperandXMMREG xmmi, OperandXMMREG xmmj);
		Encoding *cmpunordps(OperandXMMREG xmm, OperandMEM128 mem128);
		Encoding *cmpunordps(OperandXMMREG xmm, OperandR_M128 r_m128);

		Encoding *cmpss(OperandXMMREG xmmi, OperandXMMREG xmmj, char c);
		Encoding *cmpss(OperandXMMREG xmm, OperandMEM32 mem32, char c);
		Encoding *cmpss(OperandXMMREG xmm, OperandXMM32 xmm32, char c);

		Encoding *cmpeqss(OperandXMMREG xmmi, OperandXMMREG xmmj);
		Encoding *cmpeqss(OperandXMMREG xmm, OperandMEM32 mem32);
		Encoding *cmpeqss(OperandXMMREG xmm, OperandXMM32 xmm32);
		Encoding *cmpless(OperandXMMREG xmmi, OperandXMMREG xmmj);
		Encoding *cmpless(OperandXMMREG xmm, OperandMEM32 mem32);
		Encoding *cmpless(OperandXMMREG xmm, OperandXMM32 xmm32);
		Encoding *cmpltss(OperandXMMREG xmmi, OperandXMMREG xmmj);
		Encoding *cmpltss(OperandXMMREG xmm, OperandMEM32 mem32);
		Encoding *cmpltss(OperandXMMREG xmm, OperandXMM32 xmm32);
		Encoding *cmpneqss(OperandXMMREG xmmi, OperandXMMREG xmmj);
		Encoding *cmpneqss(OperandXMMREG xmm, OperandMEM32 mem32);
		Encoding *cmpneqss(OperandXMMREG xmm, OperandXMM32 xmm32);
		Encoding *cmpnless(OperandXMMREG xmmi, OperandXMMREG xmmj);
		Encoding *cmpnless(OperandXMMREG xmm, OperandMEM32 mem32);
		Encoding *cmpnless(OperandXMMREG xmm, OperandXMM32 xmm32);
		Encoding *cmpnltss(OperandXMMREG xmmi, OperandXMMREG xmmj);
		Encoding *cmpnltss(OperandXMMREG xmm, OperandMEM32 mem32);
		Encoding *cmpnltss(OperandXMMREG xmm, OperandXMM32 xmm32);
		Encoding *cmpordss(OperandXMMREG xmmi, OperandXMMREG xmmj);
		Encoding *cmpordss(OperandXMMREG xmm, OperandMEM32 mem32);
		Encoding *cmpordss(OperandXMMREG xmm, OperandXMM32 xmm32);
		Encoding *cmpunordss(OperandXMMREG xmmi, OperandXMMREG xmmj);
		Encoding *cmpunordss(OperandXMMREG xmm, OperandMEM32 mem32);
		Encoding *cmpunordss(OperandXMMREG xmm, OperandXMM32 xmm32);

		Encoding *comiss(OperandXMMREG xmmi, OperandXMMREG xmmj);
		Encoding *comiss(OperandXMMREG xmm, OperandMEM32 mem32);
		Encoding *comiss(OperandXMMREG xmm, OperandXMM32 xmm32);

		Encoding *cvtpi2ps(OperandXMMREG xmm, OperandMMREG mm);
		Encoding *cvtpi2ps(OperandXMMREG xmm, OperandMEM64 mem64);
		Encoding *cvtpi2ps(OperandXMMREG xmm, OperandMM64 r_m64);

		Encoding *cvtps2pi(OperandMMREG mm, OperandXMMREG xmm);
		Encoding *cvtps2pi(OperandMMREG mm, OperandMEM64 mem64);
		Encoding *cvtps2pi(OperandMMREG mm, OperandXMM64 xmm64);

		Encoding *cvttps2pi(OperandMMREG mm, OperandXMMREG xmm);
		Encoding *cvttps2pi(OperandMMREG mm, OperandMEM64 mem64);
		Encoding *cvttps2pi(OperandMMREG mm, OperandXMM64 xmm64);

		Encoding *cvtsi2ss(OperandXMMREG xmm, OperandREG32 reg32);
		Encoding *cvtsi2ss(OperandXMMREG xmm, OperandMEM32 mem32);
		Encoding *cvtsi2ss(OperandXMMREG xmm, OperandR_M32 r_m32);

		Encoding *cvtss2si(OperandREG32 reg32, OperandXMMREG xmm);
		Encoding *cvtss2si(OperandREG32 reg32, OperandMEM32 mem32);
		Encoding *cvtss2si(OperandREG32 reg32, OperandXMM32 xmm32);

		Encoding *cvttss2si(OperandREG32 reg32, OperandXMMREG xmm);
		Encoding *cvttss2si(OperandREG32 reg32, OperandMEM32 mem32);
		Encoding *cvttss2si(OperandREG32 reg32, OperandXMM32 xmm32);

		Encoding *divps(OperandXMMREG xmmi, OperandXMMREG xmmj);
		Encoding *divps(OperandXMMREG xmm, OperandMEM128 mem128);
		Encoding *divps(OperandXMMREG xmm, OperandR_M128 r_m128);

		Encoding *divss(OperandXMMREG xmmi, OperandXMMREG xmmj);
		Encoding *divss(OperandXMMREG xmm, OperandMEM32 mem32);
		Encoding *divss(OperandXMMREG xmm, OperandXMM32 xmm32);

		Encoding *ldmxcsr(OperandMEM32 mem32);

		Encoding *maskmovq(OperandMMREG mmi, OperandMMREG mmj);

		Encoding *maxps(OperandXMMREG xmmi, OperandXMMREG xmmj);
		Encoding *maxps(OperandXMMREG xmm, OperandMEM128 mem128);
		Encoding *maxps(OperandXMMREG xmm, OperandR_M128 r_m128);

		Encoding *maxss(OperandXMMREG xmmi, OperandXMMREG xmmj);
		Encoding *maxss(OperandXMMREG xmm, OperandMEM32 mem32);
		Encoding *maxss(OperandXMMREG xmm, OperandXMM32 xmm32);

		Encoding *minps(OperandXMMREG xmmi, OperandXMMREG xmmj);
		Encoding *minps(OperandXMMREG xmm, OperandMEM128 mem128);
		Encoding *minps(OperandXMMREG xmm, OperandR_M128 r_m128);

		Encoding *minss(OperandXMMREG xmmi, OperandXMMREG xmmj);
		Encoding *minss(OperandXMMREG xmm, OperandMEM32 mem32);
		Encoding *minss(OperandXMMREG xmm, OperandXMM32 xmm32);

		Encoding *movaps(OperandXMMREG xmmi, OperandXMMREG xmmj);
		Encoding *movaps(OperandXMMREG xmm, OperandMEM128 m128);
		Encoding *movaps(OperandXMMREG xmm, OperandR_M128 r_m128);
		Encoding *movaps(OperandMEM128 m128, OperandXMMREG xmm);
		Encoding *movaps(OperandR_M128 r_m128, OperandXMMREG xmm);

		Encoding *movhlps(OperandXMMREG xmmi, OperandXMMREG xmmj);
		Encoding *movhps(OperandXMMREG xmm, OperandMEM64 m64);
		Encoding *movhps(OperandMEM64 m64, OperandXMMREG xmm);
		Encoding *movhps(OperandXMMREG xmmi, OperandXMMREG xmmj);
		Encoding *movlhps(OperandXMMREG xmmi, OperandXMMREG xmmj);
		Encoding *movlps(OperandXMMREG xmm, OperandMEM64 m64);
		Encoding *movlps(OperandMEM64 m64, OperandXMMREG xmm);

		Encoding *movmskps(OperandREG32 r32, OperandXMMREG xmm);

		Encoding *movntps(OperandMEM128 m128, OperandXMMREG xmm);
		Encoding *movntq(OperandMEM64 m64, OperandMMREG mm);

		Encoding *movss(OperandXMMREG xmmi, OperandXMMREG xmmj);
		Encoding *movss(OperandXMMREG xmm, OperandMEM32 m32);
		Encoding *movss(OperandXMMREG xmm, OperandXMM32 xmm32);
		Encoding *movss(OperandMEM32 m32, OperandXMMREG xmm);
		Encoding *movss(OperandXMM32 xmm32, OperandXMMREG xmm);

		Encoding *movups(OperandXMMREG xmmi, OperandXMMREG xmmj);
		Encoding *movups(OperandXMMREG xmm, OperandMEM128 m128);
		Encoding *movups(OperandXMMREG xmm, OperandR_M128 r_m128);
		Encoding *movups(OperandMEM128 m128, OperandXMMREG xmm);
		Encoding *movups(OperandR_M128 r_m128, OperandXMMREG xmm);

		Encoding *mulps(OperandXMMREG xmmi, OperandXMMREG xmmj);
		Encoding *mulps(OperandXMMREG xmm, OperandMEM128 mem128);
		Encoding *mulps(OperandXMMREG xmm, OperandR_M128 r_m128);

		Encoding *mulss(OperandXMMREG xmmi, OperandXMMREG xmmj);
		Encoding *mulss(OperandXMMREG xmm, OperandMEM32 mem32);
		Encoding *mulss(OperandXMMREG xmm, OperandXMM32 xmm32);

		Encoding *orps(OperandXMMREG xmmi, OperandXMMREG xmmj);
		Encoding *orps(OperandXMMREG xmm, OperandMEM128 mem128);
		Encoding *orps(OperandXMMREG xmm, OperandR_M128 r_m128);

		Encoding *pavgb(OperandMMREG mmi, OperandMMREG mmj);
		Encoding *pavgb(OperandMMREG mm, OperandMEM64 m64);
		Encoding *pavgb(OperandMMREG mm, OperandMM64 r_m64);

		Encoding *pavgw(OperandMMREG mmi, OperandMMREG mmj);
		Encoding *pavgw(OperandMMREG mm, OperandMEM64 m64);
		Encoding *pavgw(OperandMMREG mm, OperandMM64 r_m64);

		Encoding *pextrw(OperandREG32 r32, OperandMMREG mm, unsigned char c);
		Encoding *pinsrw(OperandMMREG mm, OperandREG16 r16, unsigned char c);
		Encoding *pinsrw(OperandMMREG mm, OperandMEM16 m16, unsigned char c);
		Encoding *pinsrw(OperandMMREG mm, OperandR_M16 r_m16, unsigned char c);

		Encoding *pmaxsw(OperandMMREG mmi, OperandMMREG mmj);
		Encoding *pmaxsw(OperandMMREG mm, OperandMEM64 m64);
		Encoding *pmaxsw(OperandMMREG mm, OperandMM64 r_m64);

		Encoding *pmaxub(OperandMMREG mmi, OperandMMREG mmj);
		Encoding *pmaxub(OperandMMREG mm, OperandMEM64 m64);
		Encoding *pmaxub(OperandMMREG mm, OperandMM64 r_m64);

		Encoding *pminsw(OperandMMREG mmi, OperandMMREG mmj);
		Encoding *pminsw(OperandMMREG mm, OperandMEM64 m64);
		Encoding *pminsw(OperandMMREG mm, OperandMM64 r_m64);

		Encoding *pminub(OperandMMREG mmi, OperandMMREG mmj);
		Encoding *pminub(OperandMMREG mm, OperandMEM64 m64);
		Encoding *pminub(OperandMMREG mm, OperandMM64 r_m64);

		Encoding *pmulhuw(OperandMMREG mmi, OperandMMREG mmj);
		Encoding *pmulhuw(OperandMMREG mm, OperandMEM64 m64);
		Encoding *pmulhuw(OperandMMREG mm, OperandMM64 r_m64);

		Encoding *prefetchnta(OperandMEM mem);
		Encoding *prefetcht0(OperandMEM mem);
		Encoding *prefetcht1(OperandMEM mem);
		Encoding *prefetcht2(OperandMEM mem);

		Encoding *pshufw(OperandMMREG mmi, OperandMMREG mmj, unsigned char c);
		Encoding *pshufw(OperandMMREG mm, OperandMEM64 m64, unsigned char c);
		Encoding *pshufw(OperandMMREG mm, OperandMM64 r_m64, unsigned char c);

		Encoding *rcpps(OperandXMMREG xmmi, OperandXMMREG xmmj);
		Encoding *rcpps(OperandXMMREG xmm, OperandMEM128 mem128);
		Encoding *rcpps(OperandXMMREG xmm, OperandR_M128 r_m128);

		Encoding *rcpss(OperandXMMREG xmmi, OperandXMMREG xmmj);
		Encoding *rcpss(OperandXMMREG xmm, OperandMEM32 mem32);
		Encoding *rcpss(OperandXMMREG xmm, OperandXMM32 xmm32);

		Encoding *rsqrtps(OperandXMMREG xmmi, OperandXMMREG xmmj);
		Encoding *rsqrtps(OperandXMMREG xmm, OperandMEM128 mem128);
		Encoding *rsqrtps(OperandXMMREG xmm, OperandR_M128 r_m128);

		Encoding *rsqrtss(OperandXMMREG xmmi, OperandXMMREG xmmj);
		Encoding *rsqrtss(OperandXMMREG xmm, OperandMEM32 mem32);
		Encoding *rsqrtss(OperandXMMREG xmm, OperandXMM32 xmm32);

		Encoding *sfence();

		Encoding *shufps(OperandXMMREG xmmi, OperandXMMREG xmmj, unsigned char c);
		Encoding *shufps(OperandXMMREG xmm, OperandMEM128 m128, unsigned char c);
		Encoding *shufps(OperandXMMREG xmm, OperandR_M128 r_m128, unsigned char c);

		Encoding *sqrtps(OperandXMMREG xmmi, OperandXMMREG xmmj);
		Encoding *sqrtps(OperandXMMREG xmm, OperandMEM128 mem128);
		Encoding *sqrtps(OperandXMMREG xmm, OperandR_M128 r_m128);

		Encoding *sqrtss(OperandXMMREG xmmi, OperandXMMREG xmmj);
		Encoding *sqrtss(OperandXMMREG xmm, OperandMEM32 mem32);
		Encoding *sqrtss(OperandXMMREG xmm, OperandXMM32 xmm32);

		Encoding *stmxcsr(OperandMEM32 m32);

		Encoding *subps(OperandXMMREG xmmi, OperandXMMREG xmmj);
		Encoding *subps(OperandXMMREG xmm, OperandMEM128 mem128);
		Encoding *subps(OperandXMMREG xmm, OperandR_M128 r_m128);

		Encoding *subss(OperandXMMREG xmmi, OperandXMMREG xmmj);
		Encoding *subss(OperandXMMREG xmm, OperandMEM32 mem32);
		Encoding *subss(OperandXMMREG xmm, OperandXMM32 xmm32);

		Encoding *ucomiss(OperandXMMREG xmmi, OperandXMMREG xmmj);
		Encoding *ucomiss(OperandXMMREG xmm, OperandMEM32 mem32);
		Encoding *ucomiss(OperandXMMREG xmm, OperandXMM32 xmm32);

		Encoding *unpckhps(OperandXMMREG xmmi, OperandXMMREG xmmj);
		Encoding *unpckhps(OperandXMMREG xmm, OperandMEM128 mem128);
		Encoding *unpckhps(OperandXMMREG xmm, OperandR_M128 r_m128);

		Encoding *unpcklps(OperandXMMREG xmmi, OperandXMMREG xmmj);
		Encoding *unpcklps(OperandXMMREG xmm, OperandMEM128 mem128);
		Encoding *unpcklps(OperandXMMREG xmm, OperandR_M128 r_m128);

		Encoding *xorps(OperandXMMREG xmmi, OperandXMMREG xmmj);
		Encoding *xorps(OperandXMMREG xmm, OperandMEM128 mem128);
		Encoding *xorps(OperandXMMREG xmm, OperandR_M128 r_m128);

		// Debugging tools
		void dumpSSE();

	private:
		static float sse[8][4];   // Storage for SSE emulation registers

		static bool emulateSSE;
	};
}

#endif   // SoftWire_Emulator_hpp
