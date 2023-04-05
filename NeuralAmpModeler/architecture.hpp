// From https://github.com/Dougal-s/Aether

#ifndef ARCHITECTURE_HPP
#define ARCHITECTURE_HPP

#include <cfenv>
#include <fenv.h>

// check cpu architecture

#if /* x86_64 */ \
	/* clang & gcc */ defined(__x86_64__) || \
	/* msvc        */ defined(_M_AMD64) \

	#define ARCH_X86
	#define ARCH_X86_64

#elif /* i386 */ \
	/* clang & gcc */ defined(__i386__) || \
	/* msvc        */ defined(_M_IX86) \

	#define ARCH_X86
	#define ARCH_I386

#elif /* Arm64 */ \
	/* clang & gcc */ defined(__aarch64__) || \
	/* msvc        */ defined(_M_ARM64) \

	#define ARCH_ARM
	#define ARCH_ARM64

#elif /* Arm */ \
	/* clang & gcc */ defined(__arm__) || \
	/* msvc        */ defined(_M_ARM) \

	#define ARCH_ARM
	#define ARCH_ARM32

#else
	#define ARCH_UNKNOWN
#endif


// check cpu extensions

/* clang & gcc */
#ifdef __SSE__
	#define ARCH_EXT_SSE
#endif

#ifdef __SSE2__
	#define ARCH_EXT_SSE2
#endif

#ifdef __SSE3__
	#define ARCH_EXT_SSE3
#endif

/* msvc */
#if defined(ARCH_X86_64)
	#define ARCH_EXT_SSE
	#define ARCH_EXT_SSE2

	// msvc doesn't seem to have anything for sse3 so I am just assuming
	// it is supported
	#define ARCH_EXT_SSE3
#elif defined(ARCH_I386)
	#if _M_IX86_FP > 0
		#define ARCH_EXT_SSE
	#elif _M_IX86_FP > 1
		#define ARCH_EXT_SSE3
		#define ARCH_EXT_SSE2
		#define ARCH_EXT_SSE
	#endif
#endif


// misc functions

#ifdef ARCH_EXT_SSE

	#include <cfenv>
	#ifndef FE_DFL_DISABLE_SSE_DENORMS_ENV
		#include <immintrin.h>
	#endif

#endif

inline void disable_denormals() noexcept {

	#if defined(ARCH_EXT_SSE)
		#ifdef FE_DFL_DISABLE_SSE_DENORMS_ENV
			std::fesetenv(FE_DFL_DISABLE_SSE_DENORMS_ENV);
		#else
			_mm_setcsr(_mm_getcsr() | 0x8040);
		#endif
	#elif defined(ARCH_ARM)
		#if defined __has_builtin
			#if __has_builtin(__builtin_arm_set_fpscr) && __has_builtin(__builtin_arm_get_fpscr)
				__builtin_arm_set_fpscr(__builtin_arm_get_fpscr() | (1 << 24));
			#endif
		#endif
	#endif

}

#endif
