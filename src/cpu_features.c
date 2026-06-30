/*
cpu_features.c - runtime CPU feature tests for cl.exe.

GNU/Clang resolve b64_cpu_has_* via __builtin_cpu_supports (see
b64_internal.h), so this TU only carries real code under MSVC on x86, where
the SSE kernels need a runtime gate. MSVC ARM64 always has NEON, so no gate
is emitted there.
*/

#include "b64_internal.h"

#if defined(_MSC_VER) && !defined(__clang__)

#if defined(_M_X64) || defined(_M_IX86)

#include <intrin.h>
#include <immintrin.h>  /* _xgetbv */

int b64_cpu_has_sse41(void)
{
	int r[4];
	__cpuid(r, 1);
	return (r[2] >> 19) & 1;  /* ECX bit 19 */
}

int b64_cpu_has_avx2(void)
{
	int r[4];
	__cpuid(r, 1);
	/* AVX2 is only usable if the OS preserves YMM state across context
	   switches; confirm OSXSAVE + AVX, then that XCR0 enables XMM|YMM. */
	int osxsave = (r[2] >> 27) & 1;
	int avx = (r[2] >> 28) & 1;
	if (!(osxsave && avx))
		return 0;
	if ((_xgetbv(0) & 0x6) != 0x6)
		return 0;
	__cpuidex(r, 7, 0);
	return (r[1] >> 5) & 1;  /* EBX bit 5 */
}

#else  /* MSVC ARM64: NEON is unconditional, no symbols needed */

typedef int b64_cpu_features_dummy;

#endif

#else  /* GNU/Clang: feature tests are macros; nothing to emit here */

typedef int b64_cpu_features_dummy;

#endif
