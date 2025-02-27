
#if _typ_int64_t

/*
 * Aaron D. Gifford's SHA {256,384,512} code transcribed into a -lsum method
 * with bitcount[] order reversed to allow a single noalias buffer copy
 */

/*
 * Copyright (c) 2000-2001, Aaron D. Gifford
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. Neither the name of the copyright holder nor the names of contributors
 *    may be used to endorse or promote products derived from this software
 *    without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR AND CONTRIBUTOR(S) ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE AUTHOR OR CONTRIBUTOR(S) BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 */

/*
 * ASSERT NOTE:
 * Some sanity checking code is included using assert().
 * It can be removed by compiling with NDEBUG defined. 
 *
 * UNROLLED TRANSFORM LOOP NOTE:
 * You can define SHA2_UNROLL_TRANSFORM to use the unrolled transform
 * loop version for the hash transform rounds (defined using macros
 * later in this file).  Either define on the command line, for example:
 *
 *   cc -DSHA2_UNROLL_TRANSFORM -o sha2 sha2.c sha2prog.c
 *
 * or define below:
 *
 *   #define SHA2_UNROLL_TRANSFORM
 *
 */

/*** SHA-256/384/512 Machine Architecture Definitions *****************/

#ifndef __USE_BSD
#define __undef__USE_BSD
#define __USE_BSD
#endif
#include <endian.h>
#ifdef	__undef__USE_BSD
#undef	__undef__USE_BSD
#undef	__USE_BSD
#endif

typedef  uint8_t sha2_byte;	/* Exactly 1 byte */
typedef uint32_t sha2_word32;	/* Exactly 4 bytes */
typedef uint64_t sha2_word64;	/* Exactly 8 bytes */

#if _AST_release
#define NDEBUG
#endif
#include <assert.h>

#undef	R
#undef	S32
#undef	S64

/*** SHA-256/384/512 Various Length Definitions ***********************/

#define SHA256_BLOCK_LENGTH		64
#define SHA256_DIGEST_LENGTH		32
#define SHA384_BLOCK_LENGTH		128
#define SHA384_DIGEST_LENGTH		48
#define SHA512_BLOCK_LENGTH		128
#define SHA512_DIGEST_LENGTH		64

#define SHA256_SHORT_BLOCK_LENGTH	(SHA256_BLOCK_LENGTH - 8)
#define SHA384_SHORT_BLOCK_LENGTH	(SHA384_BLOCK_LENGTH - 16)
#define SHA512_SHORT_BLOCK_LENGTH	(SHA512_BLOCK_LENGTH - 16)

/*** ENDIAN REVERSAL MACROS *******************************************/
#if BYTE_ORDER == LITTLE_ENDIAN
#define REVERSE32(w,x)	{ \
	sha2_word32 tmp = (w); \
	tmp = (tmp >> 16) | (tmp << 16); \
	(x) = ((tmp & 0xff00ff00UL) >> 8) | ((tmp & 0x00ff00ffUL) << 8); \
}
#if _ast_LL
#define REVERSE64(w,x)	{ \
	sha2_word64 tmp = (w); \
	tmp = (tmp >> 32) | (tmp << 32); \
	tmp = ((tmp & 0xff00ff00ff00ff00ULL) >> 8) | \
	      ((tmp & 0x00ff00ff00ff00ffULL) << 8); \
	(x) = ((tmp & 0xffff0000ffff0000ULL) >> 16) | \
	      ((tmp & 0x0000ffff0000ffffULL) << 16); \
}
#else
#define REVERSE64(w,x)	{ \
	sha2_word64 tmp = (w); \
	tmp = (tmp >> 32) | (tmp << 32); \
	tmp = ((tmp & ((sha2_word64)0xff00ff00ff00ff00)) >> 8) | \
	      ((tmp & ((sha2_word64)0x00ff00ff00ff00ff)) << 8); \
	(x) = ((tmp & ((sha2_word64)0xffff0000ffff0000)) >> 16) | \
	      ((tmp & ((sha2_word64)0x0000ffff0000ffff)) << 16); \
}
#endif
#endif /* BYTE_ORDER == LITTLE_ENDIAN */

/*
 * Macro for incrementally adding the unsigned 64-bit integer n to the
 * unsigned 128-bit integer (represented using a two-element array of
 * 64-bit words):
 */

#define ADDINC128(w,n)	{ \
	(w)[1] += (sha2_word64)(n); \
	if ((w)[1] < (n)) { \
		(w)[0]++; \
	} \
}

/*
 * Macros for copying blocks of memory and for zeroing out ranges
 * of memory.  Using these macros makes it easy to switch from
 * using memset()/memcpy() and using bzero()/bcopy().
 *
 * Please define either SHA2_USE_MEMSET_MEMCPY or define
 * SHA2_USE_BZERO_BCOPY depending on which function set you
 * choose to use:
 */

#if !defined(SHA2_USE_MEMSET_MEMCPY) && !defined(SHA2_USE_BZERO_BCOPY)
/* Default to memset()/memcpy() if no option is specified */
#define SHA2_USE_MEMSET_MEMCPY	1
#endif
#if defined(SHA2_USE_MEMSET_MEMCPY) && defined(SHA2_USE_BZERO_BCOPY)
/* Abort with an error if BOTH options are defined */
#error Define either SHA2_USE_MEMSET_MEMCPY or SHA2_USE_BZERO_BCOPY, not both!
#endif

#ifdef SHA2_USE_MEMSET_MEMCPY
#define MEMSET_BZERO(p,l)	memset((p), 0, (l))
#define MEMCPY_BCOPY(d,s,l)	memcpy((d), (s), (l))
#endif
#ifdef SHA2_USE_BZERO_BCOPY
#define MEMSET_BZERO(p,l)	bzero((p), (l))
#define MEMCPY_BCOPY(d,s,l)	bcopy((s), (d), (l))
#endif


/*** THE SIX LOGICAL FUNCTIONS ****************************************/
/*
 * Bit shifting and rotation (used by the six SHA-XYZ logical functions:
 *
 *   NOTE:  The naming of R and S appears backwards here (R is a SHIFT and
 *   S is a ROTATION) because the SHA-256/384/512 description document
 *   uses this same "backwards" definition:
 *   https://web.archive.org/web/20050907174740/http://csrc.nist.gov/cryptval/shs/sha256-384-512.pdf
 */

/* Shift-right (used in SHA-256, SHA-384, and SHA-512): */
#define R(b,x) 		((x) >> (b))
/* 32-bit Rotate-right (used in SHA-256): */
#define S32(b,x)	(((x) >> (b)) | ((x) << (32 - (b))))
/* 64-bit Rotate-right (used in SHA-384 and SHA-512): */
#define S64(b,x)	(((x) >> (b)) | ((x) << (64 - (b))))

/* Two of six logical functions used in SHA-256, SHA-384, and SHA-512: */
#define Ch(x,y,z)	(((x) & (y)) ^ ((~(x)) & (z)))
#define Maj(x,y,z)	(((x) & (y)) ^ ((x) & (z)) ^ ((y) & (z)))

/* Four of six logical functions used in SHA-256: */
#define Sigma0_256(x)	(S32(2,  (x)) ^ S32(13, (x)) ^ S32(22, (x)))
#define Sigma1_256(x)	(S32(6,  (x)) ^ S32(11, (x)) ^ S32(25, (x)))
#define sigma0_256(x)	(S32(7,  (x)) ^ S32(18, (x)) ^ R(3 ,   (x)))
#define sigma1_256(x)	(S32(17, (x)) ^ S32(19, (x)) ^ R(10,   (x)))

/* Four of six logical functions used in SHA-384 and SHA-512: */
#define Sigma0_512(x)	(S64(28, (x)) ^ S64(34, (x)) ^ S64(39, (x)))
#define Sigma1_512(x)	(S64(14, (x)) ^ S64(18, (x)) ^ S64(41, (x)))
#define sigma0_512(x)	(S64( 1, (x)) ^ S64( 8, (x)) ^ R( 7,   (x)))
#define sigma1_512(x)	(S64(19, (x)) ^ S64(61, (x)) ^ R( 6,   (x)))

/*** SHA-XYZ INITIAL HASH VALUES AND CONSTANTS ************************/
/* Hash constant words K for SHA-256: */
static const sha2_word32 K256[64] = {
	0x428a2f98UL, 0x71374491UL, 0xb5c0fbcfUL, 0xe9b5dba5UL,
	0x3956c25bUL, 0x59f111f1UL, 0x923f82a4UL, 0xab1c5ed5UL,
	0xd807aa98UL, 0x12835b01UL, 0x243185beUL, 0x550c7dc3UL,
	0x72be5d74UL, 0x80deb1feUL, 0x9bdc06a7UL, 0xc19bf174UL,
	0xe49b69c1UL, 0xefbe4786UL, 0x0fc19dc6UL, 0x240ca1ccUL,
	0x2de92c6fUL, 0x4a7484aaUL, 0x5cb0a9dcUL, 0x76f988daUL,
	0x983e5152UL, 0xa831c66dUL, 0xb00327c8UL, 0xbf597fc7UL,
	0xc6e00bf3UL, 0xd5a79147UL, 0x06ca6351UL, 0x14292967UL,
	0x27b70a85UL, 0x2e1b2138UL, 0x4d2c6dfcUL, 0x53380d13UL,
	0x650a7354UL, 0x766a0abbUL, 0x81c2c92eUL, 0x92722c85UL,
	0xa2bfe8a1UL, 0xa81a664bUL, 0xc24b8b70UL, 0xc76c51a3UL,
	0xd192e819UL, 0xd6990624UL, 0xf40e3585UL, 0x106aa070UL,
	0x19a4c116UL, 0x1e376c08UL, 0x2748774cUL, 0x34b0bcb5UL,
	0x391c0cb3UL, 0x4ed8aa4aUL, 0x5b9cca4fUL, 0x682e6ff3UL,
	0x748f82eeUL, 0x78a5636fUL, 0x84c87814UL, 0x8cc70208UL,
	0x90befffaUL, 0xa4506cebUL, 0xbef9a3f7UL, 0xc67178f2UL
};

/* Initial hash value H for SHA-256: */
static const sha2_word32 sha256_initial_hash_value[8] = {
	0x6a09e667UL,
	0xbb67ae85UL,
	0x3c6ef372UL,
	0xa54ff53aUL,
	0x510e527fUL,
	0x9b05688cUL,
	0x1f83d9abUL,
	0x5be0cd19UL
};

/* Hash constant words K for SHA-384 and SHA-512: */
static const sha2_word64 K512[80] = {
#if _ast_LL
	0x428a2f98d728ae22ULL, 0x7137449123ef65cdULL,
	0xb5c0fbcfec4d3b2fULL, 0xe9b5dba58189dbbcULL,
	0x3956c25bf348b538ULL, 0x59f111f1b605d019ULL,
	0x923f82a4af194f9bULL, 0xab1c5ed5da6d8118ULL,
	0xd807aa98a3030242ULL, 0x12835b0145706fbeULL,
	0x243185be4ee4b28cULL, 0x550c7dc3d5ffb4e2ULL,
	0x72be5d74f27b896fULL, 0x80deb1fe3b1696b1ULL,
	0x9bdc06a725c71235ULL, 0xc19bf174cf692694ULL,
	0xe49b69c19ef14ad2ULL, 0xefbe4786384f25e3ULL,
	0x0fc19dc68b8cd5b5ULL, 0x240ca1cc77ac9c65ULL,
	0x2de92c6f592b0275ULL, 0x4a7484aa6ea6e483ULL,
	0x5cb0a9dcbd41fbd4ULL, 0x76f988da831153b5ULL,
	0x983e5152ee66dfabULL, 0xa831c66d2db43210ULL,
	0xb00327c898fb213fULL, 0xbf597fc7beef0ee4ULL,
	0xc6e00bf33da88fc2ULL, 0xd5a79147930aa725ULL,
	0x06ca6351e003826fULL, 0x142929670a0e6e70ULL,
	0x27b70a8546d22ffcULL, 0x2e1b21385c26c926ULL,
	0x4d2c6dfc5ac42aedULL, 0x53380d139d95b3dfULL,
	0x650a73548baf63deULL, 0x766a0abb3c77b2a8ULL,
	0x81c2c92e47edaee6ULL, 0x92722c851482353bULL,
	0xa2bfe8a14cf10364ULL, 0xa81a664bbc423001ULL,
	0xc24b8b70d0f89791ULL, 0xc76c51a30654be30ULL,
	0xd192e819d6ef5218ULL, 0xd69906245565a910ULL,
	0xf40e35855771202aULL, 0x106aa07032bbd1b8ULL,
	0x19a4c116b8d2d0c8ULL, 0x1e376c085141ab53ULL,
	0x2748774cdf8eeb99ULL, 0x34b0bcb5e19b48a8ULL,
	0x391c0cb3c5c95a63ULL, 0x4ed8aa4ae3418acbULL,
	0x5b9cca4f7763e373ULL, 0x682e6ff3d6b2b8a3ULL,
	0x748f82ee5defb2fcULL, 0x78a5636f43172f60ULL,
	0x84c87814a1f0ab72ULL, 0x8cc702081a6439ecULL,
	0x90befffa23631e28ULL, 0xa4506cebde82bde9ULL,
	0xbef9a3f7b2c67915ULL, 0xc67178f2e372532bULL,
	0xca273eceea26619cULL, 0xd186b8c721c0c207ULL,
	0xeada7dd6cde0eb1eULL, 0xf57d4f7fee6ed178ULL,
	0x06f067aa72176fbaULL, 0x0a637dc5a2c898a6ULL,
	0x113f9804bef90daeULL, 0x1b710b35131c471bULL,
	0x28db77f523047d84ULL, 0x32caab7b40c72493ULL,
	0x3c9ebe0a15c9bebcULL, 0x431d67c49c100d4cULL,
	0x4cc5d4becb3e42b6ULL, 0x597f299cfc657e2aULL,
	0x5fcb6fab3ad6faecULL, 0x6c44198c4a475817ULL
#else
	((sha2_word64)0x428a2f98d728ae22), ((sha2_word64)0x7137449123ef65cd),
	((sha2_word64)0xb5c0fbcfec4d3b2f), ((sha2_word64)0xe9b5dba58189dbbc),
	((sha2_word64)0x3956c25bf348b538), ((sha2_word64)0x59f111f1b605d019),
	((sha2_word64)0x923f82a4af194f9b), ((sha2_word64)0xab1c5ed5da6d8118),
	((sha2_word64)0xd807aa98a3030242), ((sha2_word64)0x12835b0145706fbe),
	((sha2_word64)0x243185be4ee4b28c), ((sha2_word64)0x550c7dc3d5ffb4e2),
	((sha2_word64)0x72be5d74f27b896f), ((sha2_word64)0x80deb1fe3b1696b1),
	((sha2_word64)0x9bdc06a725c71235), ((sha2_word64)0xc19bf174cf692694),
	((sha2_word64)0xe49b69c19ef14ad2), ((sha2_word64)0xefbe4786384f25e3),
	((sha2_word64)0x0fc19dc68b8cd5b5), ((sha2_word64)0x240ca1cc77ac9c65),
	((sha2_word64)0x2de92c6f592b0275), ((sha2_word64)0x4a7484aa6ea6e483),
	((sha2_word64)0x5cb0a9dcbd41fbd4), ((sha2_word64)0x76f988da831153b5),
	((sha2_word64)0x983e5152ee66dfab), ((sha2_word64)0xa831c66d2db43210),
	((sha2_word64)0xb00327c898fb213f), ((sha2_word64)0xbf597fc7beef0ee4),
	((sha2_word64)0xc6e00bf33da88fc2), ((sha2_word64)0xd5a79147930aa725),
	((sha2_word64)0x06ca6351e003826f), ((sha2_word64)0x142929670a0e6e70),
	((sha2_word64)0x27b70a8546d22ffc), ((sha2_word64)0x2e1b21385c26c926),
	((sha2_word64)0x4d2c6dfc5ac42aed), ((sha2_word64)0x53380d139d95b3df),
	((sha2_word64)0x650a73548baf63de), ((sha2_word64)0x766a0abb3c77b2a8),
	((sha2_word64)0x81c2c92e47edaee6), ((sha2_word64)0x92722c851482353b),
	((sha2_word64)0xa2bfe8a14cf10364), ((sha2_word64)0xa81a664bbc423001),
	((sha2_word64)0xc24b8b70d0f89791), ((sha2_word64)0xc76c51a30654be30),
	((sha2_word64)0xd192e819d6ef5218), ((sha2_word64)0xd69906245565a910),
	((sha2_word64)0xf40e35855771202a), ((sha2_word64)0x106aa07032bbd1b8),
	((sha2_word64)0x19a4c116b8d2d0c8), ((sha2_word64)0x1e376c085141ab53),
	((sha2_word64)0x2748774cdf8eeb99), ((sha2_word64)0x34b0bcb5e19b48a8),
	((sha2_word64)0x391c0cb3c5c95a63), ((sha2_word64)0x4ed8aa4ae3418acb),
	((sha2_word64)0x5b9cca4f7763e373), ((sha2_word64)0x682e6ff3d6b2b8a3),
	((sha2_word64)0x748f82ee5defb2fc), ((sha2_word64)0x78a5636f43172f60),
	((sha2_word64)0x84c87814a1f0ab72), ((sha2_word64)0x8cc702081a6439ec),
	((sha2_word64)0x90befffa23631e28), ((sha2_word64)0xa4506cebde82bde9),
	((sha2_word64)0xbef9a3f7b2c67915), ((sha2_word64)0xc67178f2e372532b),
	((sha2_word64)0xca273eceea26619c), ((sha2_word64)0xd186b8c721c0c207),
	((sha2_word64)0xeada7dd6cde0eb1e), ((sha2_word64)0xf57d4f7fee6ed178),
	((sha2_word64)0x06f067aa72176fba), ((sha2_word64)0x0a637dc5a2c898a6),
	((sha2_word64)0x113f9804bef90dae), ((sha2_word64)0x1b710b35131c471b),
	((sha2_word64)0x28db77f523047d84), ((sha2_word64)0x32caab7b40c72493),
	((sha2_word64)0x3c9ebe0a15c9bebc), ((sha2_word64)0x431d67c49c100d4c),
	((sha2_word64)0x4cc5d4becb3e42b6), ((sha2_word64)0x597f299cfc657e2a),
	((sha2_word64)0x5fcb6fab3ad6faec), ((sha2_word64)0x6c44198c4a475817)
#endif
};

/* Initial hash value H for SHA-384 */
static const sha2_word64 sha384_initial_hash_value[8] = {
#if _ast_LL
	0xcbbb9d5dc1059ed8ULL,
	0x629a292a367cd507ULL,
	0x9159015a3070dd17ULL,
	0x152fecd8f70e5939ULL,
	0x67332667ffc00b31ULL,
	0x8eb44a8768581511ULL,
	0xdb0c2e0d64f98fa7ULL,
	0x47b5481dbefa4fa4ULL
#else
	((sha2_word64)0xcbbb9d5dc1059ed8),
	((sha2_word64)0x629a292a367cd507),
	((sha2_word64)0x9159015a3070dd17),
	((sha2_word64)0x152fecd8f70e5939),
	((sha2_word64)0x67332667ffc00b31),
	((sha2_word64)0x8eb44a8768581511),
	((sha2_word64)0xdb0c2e0d64f98fa7),
	((sha2_word64)0x47b5481dbefa4fa4)
#endif
};

/* Initial hash value H for SHA-512 */
static const sha2_word64 sha512_initial_hash_value[8] = {
#if _ast_LL
	0x6a09e667f3bcc908ULL,
	0xbb67ae8584caa73bULL,
	0x3c6ef372fe94f82bULL,
	0xa54ff53a5f1d36f1ULL,
	0x510e527fade682d1ULL,
	0x9b05688c2b3e6c1fULL,
	0x1f83d9abfb41bd6bULL,
	0x5be0cd19137e2179ULL
#else
	((sha2_word64)0x6a09e667f3bcc908),
	((sha2_word64)0xbb67ae8584caa73b),
	((sha2_word64)0x3c6ef372fe94f82b),
	((sha2_word64)0xa54ff53a5f1d36f1),
	((sha2_word64)0x510e527fade682d1),
	((sha2_word64)0x9b05688c2b3e6c1f),
	((sha2_word64)0x1f83d9abfb41bd6b),
	((sha2_word64)0x5be0cd19137e2179)
#endif
};

/*** SHA-256: *********************************************************/

#define sha256_description "FIPS SHA-256 secure hash algorithm."
#define sha256_options	"\
[+(version)?sha-256 (FIPS) 2000-01-01]\
[+(author)?Aaron D. Gifford]\
"
#define sha256_match	"sha256|sha-256|SHA256|SHA-256"
#define sha256_scale	0

#define sha256_padding	md5_pad

#define SHA256_CTX	Sha256_t

typedef struct Sha256_s
{
	_SUM_PUBLIC_
	_SUM_PRIVATE_
	sha2_byte	digest[SHA256_DIGEST_LENGTH];
	sha2_byte	digest_sum[SHA256_DIGEST_LENGTH];
	sha2_word32	state[8];
	sha2_word64	bitcount;
	sha2_byte	buffer[SHA256_BLOCK_LENGTH];
} Sha256_t;

#ifdef SHA2_UNROLL_TRANSFORM

/* Unrolled SHA-256 round macros: */

#if BYTE_ORDER == LITTLE_ENDIAN

#define ROUND256_0_TO_15(a,b,c,d,e,f,g,h)	\
	REVERSE32(*data++, W256[j]); \
	T1 = (h) + Sigma1_256(e) + Ch((e), (f), (g)) + \
	     K256[j] + W256[j]; \
	(d) += T1; \
	(h) = T1 + Sigma0_256(a) + Maj((a), (b), (c)); \
	j++


#else /* BYTE_ORDER == LITTLE_ENDIAN */

#define ROUND256_0_TO_15(a,b,c,d,e,f,g,h)	\
	T1 = (h) + Sigma1_256(e) + Ch((e), (f), (g)) + \
	     K256[j] + (W256[j] = *data++); \
	(d) += T1; \
	(h) = T1 + Sigma0_256(a) + Maj((a), (b), (c)); \
	j++

#endif /* BYTE_ORDER == LITTLE_ENDIAN */

#define ROUND256(a,b,c,d,e,f,g,h)	\
	s0 = W256[(j+1)&0x0f]; \
	s0 = sigma0_256(s0); \
	s1 = W256[(j+14)&0x0f]; \
	s1 = sigma1_256(s1); \
	T1 = (h) + Sigma1_256(e) + Ch((e), (f), (g)) + K256[j] + \
	     (W256[j&0x0f] += s1 + W256[(j+9)&0x0f] + s0); \
	(d) += T1; \
	(h) = T1 + Sigma0_256(a) + Maj((a), (b), (c)); \
	j++

static void SHA256_Transform(SHA256_CTX* sha, const sha2_word32* data) {
	sha2_word32	a, b, c, d, e, f, g, h, s0, s1;
	sha2_word32	T1, *W256;
	int		j;

	W256 = (sha2_word32*)sha->buffer;

	/* Initialize registers with the prev. intermediate value */
	a = sha->state[0];
	b = sha->state[1];
	c = sha->state[2];
	d = sha->state[3];
	e = sha->state[4];
	f = sha->state[5];
	g = sha->state[6];
	h = sha->state[7];

	j = 0;
	do {
		/* Rounds 0 to 15 (unrolled): */
		ROUND256_0_TO_15(a,b,c,d,e,f,g,h);
		ROUND256_0_TO_15(h,a,b,c,d,e,f,g);
		ROUND256_0_TO_15(g,h,a,b,c,d,e,f);
		ROUND256_0_TO_15(f,g,h,a,b,c,d,e);
		ROUND256_0_TO_15(e,f,g,h,a,b,c,d);
		ROUND256_0_TO_15(d,e,f,g,h,a,b,c);
		ROUND256_0_TO_15(c,d,e,f,g,h,a,b);
		ROUND256_0_TO_15(b,c,d,e,f,g,h,a);
	} while (j < 16);

	/* Now for the remaining rounds to 64: */
	do {
		ROUND256(a,b,c,d,e,f,g,h);
		ROUND256(h,a,b,c,d,e,f,g);
		ROUND256(g,h,a,b,c,d,e,f);
		ROUND256(f,g,h,a,b,c,d,e);
		ROUND256(e,f,g,h,a,b,c,d);
		ROUND256(d,e,f,g,h,a,b,c);
		ROUND256(c,d,e,f,g,h,a,b);
		ROUND256(b,c,d,e,f,g,h,a);
	} while (j < 64);

	/* Compute the current intermediate hash value */
	sha->state[0] += a;
	sha->state[1] += b;
	sha->state[2] += c;
	sha->state[3] += d;
	sha->state[4] += e;
	sha->state[5] += f;
	sha->state[6] += g;
	sha->state[7] += h;

	/* Clean up */
	a = b = c = d = e = f = g = h = T1 = 0;
}

#else /* SHA2_UNROLL_TRANSFORM */

static void SHA256_Transform(SHA256_CTX* sha, const sha2_word32* data) {
	sha2_word32	a, b, c, d, e, f, g, h, s0, s1;
	sha2_word32	T1, T2, *W256;
	int		j;

	W256 = (sha2_word32*)sha->buffer;

	/* Initialize registers with the prev. intermediate value */
	a = sha->state[0];
	b = sha->state[1];
	c = sha->state[2];
	d = sha->state[3];
	e = sha->state[4];
	f = sha->state[5];
	g = sha->state[6];
	h = sha->state[7];

	j = 0;
	do {
#if BYTE_ORDER == LITTLE_ENDIAN
		/* Copy data while converting to host byte order */
		REVERSE32(*data++,W256[j]);
		/* Apply the SHA-256 compression function to update a..h */
		T1 = h + Sigma1_256(e) + Ch(e, f, g) + K256[j] + W256[j];
#else /* BYTE_ORDER == LITTLE_ENDIAN */
		/* Apply the SHA-256 compression function to update a..h with copy */
		T1 = h + Sigma1_256(e) + Ch(e, f, g) + K256[j] + (W256[j] = *data++);
#endif /* BYTE_ORDER == LITTLE_ENDIAN */
		T2 = Sigma0_256(a) + Maj(a, b, c);
		h = g;
		g = f;
		f = e;
		e = d + T1;
		d = c;
		c = b;
		b = a;
		a = T1 + T2;

		j++;
	} while (j < 16);

	do {
		/* Part of the message block expansion: */
		s0 = W256[(j+1)&0x0f];
		s0 = sigma0_256(s0);
		s1 = W256[(j+14)&0x0f];
		s1 = sigma1_256(s1);

		/* Apply the SHA-256 compression function to update a..h */
		T1 = h + Sigma1_256(e) + Ch(e, f, g) + K256[j] +
		     (W256[j&0x0f] += s1 + W256[(j+9)&0x0f] + s0);
		T2 = Sigma0_256(a) + Maj(a, b, c);
		h = g;
		g = f;
		f = e;
		e = d + T1;
		d = c;
		c = b;
		b = a;
		a = T1 + T2;

		j++;
	} while (j < 64);

	/* Compute the current intermediate hash value */
	sha->state[0] += a;
	sha->state[1] += b;
	sha->state[2] += c;
	sha->state[3] += d;
	sha->state[4] += e;
	sha->state[5] += f;
	sha->state[6] += g;
	sha->state[7] += h;

	/* Clean up */
	a = b = c = d = e = f = g = h = T1 = T2 = 0;
}

#endif /* SHA2_UNROLL_TRANSFORM */

static int
sha256_block(Sum_t* p, const void* s, size_t len)
{
	Sha256_t*	sha = (Sha256_t*)p;
	sha2_byte*	data = (sha2_byte*)s;
	unsigned int	freespace, usedspace;

	if (!len)
		return 0;
	usedspace = (sha->bitcount >> 3) % SHA256_BLOCK_LENGTH;
	if (usedspace > 0) {
		/* Calculate how much free space is available in the buffer */
		freespace = SHA256_BLOCK_LENGTH - usedspace;

		if (len >= freespace) {
			/* Fill the buffer completely and process it */
			MEMCPY_BCOPY(&sha->buffer[usedspace], data, freespace);
			sha->bitcount += freespace << 3;
			len -= freespace;
			data += freespace;
			SHA256_Transform(sha, (sha2_word32*)sha->buffer);
		} else {
			/* The buffer is not yet full */
			MEMCPY_BCOPY(&sha->buffer[usedspace], data, len);
			sha->bitcount += len << 3;
			/* Clean up: */
			usedspace = freespace = 0;
			return 0;
		}
	}
	while (len >= SHA256_BLOCK_LENGTH) {
		/* Process as many complete blocks as we can */
		SHA256_Transform(sha, (sha2_word32*)data);
		sha->bitcount += SHA256_BLOCK_LENGTH << 3;
		len -= SHA256_BLOCK_LENGTH;
		data += SHA256_BLOCK_LENGTH;
	}
	if (len > 0) {
		/* There's leftovers, so save 'em */
		MEMCPY_BCOPY(sha->buffer, data, len);
		sha->bitcount += len << 3;
	}
	/* Clean up: */
	usedspace = freespace = 0;

	return 0;
}

static int
sha256_init(Sum_t* p)
{
	Sha256_t*	sha = (Sha256_t*)p;

	MEMCPY_BCOPY(sha->state, sha256_initial_hash_value, SHA256_DIGEST_LENGTH);
	MEMSET_BZERO(sha->buffer, SHA256_BLOCK_LENGTH);
	sha->bitcount = 0;

	return 0;
}

static Sum_t*
sha256_open(const Method_t* method, const char* name)
{
	Sha256_t*	sha;

	if (sha = newof(0, Sha256_t, 1, 0))
	{
		sha->method = (Method_t*)method;
		sha->name = name;
		sha256_init((Sum_t*)sha);
	}
	return (Sum_t*)sha;
}

static int
sha256_done(Sum_t* p)
{
	Sha256_t*	sha = (Sha256_t*)p;
	unsigned int	usedspace;
	int		i;

	/* Sanity check: */
	assert(sha != NULL);

	usedspace = (sha->bitcount >> 3) % SHA256_BLOCK_LENGTH;
#if BYTE_ORDER == LITTLE_ENDIAN
	/* Convert FROM host byte order */
	REVERSE64(sha->bitcount,sha->bitcount);
#endif
	if (usedspace > 0) {
		/* Begin padding with a 1 bit: */
		sha->buffer[usedspace++] = 0x80;

		if (usedspace <= SHA256_SHORT_BLOCK_LENGTH) {
			/* Set-up for the last transform: */
			MEMSET_BZERO(&sha->buffer[usedspace], SHA256_SHORT_BLOCK_LENGTH - usedspace);
		} else {
			if (usedspace < SHA256_BLOCK_LENGTH) {
				MEMSET_BZERO(&sha->buffer[usedspace], SHA256_BLOCK_LENGTH - usedspace);
			}
			/* Do second-to-last transform: */
			SHA256_Transform(sha, (sha2_word32*)sha->buffer);

			/* And set-up for the last transform: */
			MEMSET_BZERO(sha->buffer, SHA256_SHORT_BLOCK_LENGTH);
		}
	} else {
		/* Set-up for the last transform: */
		MEMSET_BZERO(sha->buffer, SHA256_SHORT_BLOCK_LENGTH);

		/* Begin padding with a 1 bit: */
		*sha->buffer = 0x80;
	}
	/* Store the length of input data (in bits): */
	MEMCPY_BCOPY(&sha->buffer[SHA256_SHORT_BLOCK_LENGTH], &sha->bitcount, 8);

	/* Final transform: */
	SHA256_Transform(sha, (sha2_word32*)sha->buffer);

#if BYTE_ORDER == LITTLE_ENDIAN
	{
		/* Convert TO host byte order */
		int		j;
		sha2_word32*	d = (sha2_word32*)sha->digest;
		for (j = 0; j < 8; j++) {
			REVERSE32(sha->state[j],sha->state[j]);
			*d++ = sha->state[j];
		}
	}
#else
	MEMCPY_BCOPY(sha->digest, sha->state, SHA256_DIGEST_LENGTH);
#endif

	/* accumulate the digests */
	for (i = 0; i < SHA256_DIGEST_LENGTH; i++)
		sha->digest_sum[i] ^= sha->digest[i];

	/* Clean up state data: */
	MEMSET_BZERO(&sha->state, sizeof(*sha) - offsetof(Sha256_t, state));
	usedspace = 0;

	return 0;
}

static int
sha256_print(Sum_t* p, Sfio_t* sp, int flags, size_t scale)
{
	Sha256_t*	sha = (Sha256_t*)p;
	sha2_byte*	d;
	sha2_byte*	e;

	NOT_USED(scale);
	d = (flags & SUM_TOTAL) ? sha->digest_sum : sha->digest;
	e = d + SHA256_DIGEST_LENGTH;
	while (d < e)
		sfprintf(sp, "%02x", *d++);
	return 0;
}

static int
sha256_data(Sum_t* p, Sumdata_t* data)
{
	Sha256_t*	sha = (Sha256_t*)p;

	data->size = SHA256_DIGEST_LENGTH;
	data->num = 0;
	data->buf = sha->digest;
	return 0;
}

/*** SHA-512: *********************************************************/

#define sha512_description "FIPS SHA-512 secure hash algorithm."
#define sha512_options	"\
[+(version)?sha-512 (FIPS) 2000-01-01]\
[+(author)?Aaron D. Gifford]\
"
#define sha512_match	"sha512|sha-512|SHA512|SHA-512"
#define sha512_scale	0

#define sha512_padding	md5_pad

#define SHA512_CTX	Sha512_t

typedef struct Sha512_s
{
	_SUM_PUBLIC_
	_SUM_PRIVATE_
	sha2_byte	digest[SHA512_DIGEST_LENGTH];
	sha2_byte	digest_sum[SHA512_DIGEST_LENGTH];
	sha2_word64	state[8];
	sha2_word64	bitcount[2];
	sha2_byte	buffer[SHA512_BLOCK_LENGTH];
} Sha512_t;

#ifdef SHA2_UNROLL_TRANSFORM

/* Unrolled SHA-512 round macros: */
#if BYTE_ORDER == LITTLE_ENDIAN

#define ROUND512_0_TO_15(a,b,c,d,e,f,g,h)	\
	REVERSE64(*data++, W512[j]); \
	T1 = (h) + Sigma1_512(e) + Ch((e), (f), (g)) + \
	     K512[j] + W512[j]; \
	(d) += T1, \
	(h) = T1 + Sigma0_512(a) + Maj((a), (b), (c)), \
	j++


#else /* BYTE_ORDER == LITTLE_ENDIAN */

#define ROUND512_0_TO_15(a,b,c,d,e,f,g,h)	\
	T1 = (h) + Sigma1_512(e) + Ch((e), (f), (g)) + \
	     K512[j] + (W512[j] = *data++); \
	(d) += T1; \
	(h) = T1 + Sigma0_512(a) + Maj((a), (b), (c)); \
	j++

#endif /* BYTE_ORDER == LITTLE_ENDIAN */

#define ROUND512(a,b,c,d,e,f,g,h)	\
	s0 = W512[(j+1)&0x0f]; \
	s0 = sigma0_512(s0); \
	s1 = W512[(j+14)&0x0f]; \
	s1 = sigma1_512(s1); \
	T1 = (h) + Sigma1_512(e) + Ch((e), (f), (g)) + K512[j] + \
	     (W512[j&0x0f] += s1 + W512[(j+9)&0x0f] + s0); \
	(d) += T1; \
	(h) = T1 + Sigma0_512(a) + Maj((a), (b), (c)); \
	j++

static void SHA512_Transform(SHA512_CTX* sha, const sha2_word64* data) {
	sha2_word64	a, b, c, d, e, f, g, h, s0, s1;
	sha2_word64	T1, *W512 = (sha2_word64*)sha->buffer;
	int		j;

	/* Initialize registers with the prev. intermediate value */
	a = sha->state[0];
	b = sha->state[1];
	c = sha->state[2];
	d = sha->state[3];
	e = sha->state[4];
	f = sha->state[5];
	g = sha->state[6];
	h = sha->state[7];

	j = 0;
	do {
		ROUND512_0_TO_15(a,b,c,d,e,f,g,h);
		ROUND512_0_TO_15(h,a,b,c,d,e,f,g);
		ROUND512_0_TO_15(g,h,a,b,c,d,e,f);
		ROUND512_0_TO_15(f,g,h,a,b,c,d,e);
		ROUND512_0_TO_15(e,f,g,h,a,b,c,d);
		ROUND512_0_TO_15(d,e,f,g,h,a,b,c);
		ROUND512_0_TO_15(c,d,e,f,g,h,a,b);
		ROUND512_0_TO_15(b,c,d,e,f,g,h,a);
	} while (j < 16);

	/* Now for the remaining rounds up to 79: */
	do {
		ROUND512(a,b,c,d,e,f,g,h);
		ROUND512(h,a,b,c,d,e,f,g);
		ROUND512(g,h,a,b,c,d,e,f);
		ROUND512(f,g,h,a,b,c,d,e);
		ROUND512(e,f,g,h,a,b,c,d);
		ROUND512(d,e,f,g,h,a,b,c);
		ROUND512(c,d,e,f,g,h,a,b);
		ROUND512(b,c,d,e,f,g,h,a);
	} while (j < 80);

	/* Compute the current intermediate hash value */
	sha->state[0] += a;
	sha->state[1] += b;
	sha->state[2] += c;
	sha->state[3] += d;
	sha->state[4] += e;
	sha->state[5] += f;
	sha->state[6] += g;
	sha->state[7] += h;

	/* Clean up */
	a = b = c = d = e = f = g = h = T1 = 0;
}

#else /* SHA2_UNROLL_TRANSFORM */

static void SHA512_Transform(SHA512_CTX* sha, const sha2_word64* data) {
	sha2_word64	a, b, c, d, e, f, g, h, s0, s1;
	sha2_word64	T1, T2, *W512 = (sha2_word64*)sha->buffer;
	int		j;

	/* Initialize registers with the prev. intermediate value */
	a = sha->state[0];
	b = sha->state[1];
	c = sha->state[2];
	d = sha->state[3];
	e = sha->state[4];
	f = sha->state[5];
	g = sha->state[6];
	h = sha->state[7];

	j = 0;
	do {
#if BYTE_ORDER == LITTLE_ENDIAN
		/* Convert TO host byte order */
		REVERSE64(*data++, W512[j]);
		/* Apply the SHA-512 compression function to update a..h */
		T1 = h + Sigma1_512(e) + Ch(e, f, g) + K512[j] + W512[j];
#else /* BYTE_ORDER == LITTLE_ENDIAN */
		/* Apply the SHA-512 compression function to update a..h with copy */
		T1 = h + Sigma1_512(e) + Ch(e, f, g) + K512[j] + (W512[j] = *data++);
#endif /* BYTE_ORDER == LITTLE_ENDIAN */
		T2 = Sigma0_512(a) + Maj(a, b, c);
		h = g;
		g = f;
		f = e;
		e = d + T1;
		d = c;
		c = b;
		b = a;
		a = T1 + T2;

		j++;
	} while (j < 16);

	do {
		/* Part of the message block expansion: */
		s0 = W512[(j+1)&0x0f];
		s0 = sigma0_512(s0);
		s1 = W512[(j+14)&0x0f];
		s1 =  sigma1_512(s1);

		/* Apply the SHA-512 compression function to update a..h */
		T1 = h + Sigma1_512(e) + Ch(e, f, g) + K512[j] +
		     (W512[j&0x0f] += s1 + W512[(j+9)&0x0f] + s0);
		T2 = Sigma0_512(a) + Maj(a, b, c);
		h = g;
		g = f;
		f = e;
		e = d + T1;
		d = c;
		c = b;
		b = a;
		a = T1 + T2;

		j++;
	} while (j < 80);

	/* Compute the current intermediate hash value */
	sha->state[0] += a;
	sha->state[1] += b;
	sha->state[2] += c;
	sha->state[3] += d;
	sha->state[4] += e;
	sha->state[5] += f;
	sha->state[6] += g;
	sha->state[7] += h;

	/* Clean up */
	a = b = c = d = e = f = g = h = T1 = T2 = 0;
}

#endif /* SHA2_UNROLL_TRANSFORM */

static int
sha512_block(Sum_t* p, const void* s, size_t len)
{
	Sha512_t*	sha = (Sha512_t*)p;
	sha2_byte*	data = (sha2_byte*)s;
	unsigned int	freespace, usedspace;

	if (!len)
		return 0;
	usedspace = (sha->bitcount[1] >> 3) % SHA512_BLOCK_LENGTH;
	if (usedspace > 0) {
		/* Calculate how much free space is available in the buffer */
		freespace = SHA512_BLOCK_LENGTH - usedspace;

		if (len >= freespace) {
			/* Fill the buffer completely and process it */
			MEMCPY_BCOPY(&sha->buffer[usedspace], data, freespace);
			ADDINC128(sha->bitcount, freespace << 3);
			len -= freespace;
			data += freespace;
			SHA512_Transform(sha, (sha2_word64*)sha->buffer);
		} else {
			/* The buffer is not yet full */
			MEMCPY_BCOPY(&sha->buffer[usedspace], data, len);
			ADDINC128(sha->bitcount, len << 3);
			/* Clean up: */
			usedspace = freespace = 0;
			return 0;
		}
	}
	while (len >= SHA512_BLOCK_LENGTH) {
		/* Process as many complete blocks as we can */
		SHA512_Transform(sha, (sha2_word64*)data);
		ADDINC128(sha->bitcount, SHA512_BLOCK_LENGTH << 3);
		len -= SHA512_BLOCK_LENGTH;
		data += SHA512_BLOCK_LENGTH;
	}
	if (len > 0) {
		/* There's leftovers, so save 'em */
		MEMCPY_BCOPY(sha->buffer, data, len);
		ADDINC128(sha->bitcount, len << 3);
	}
	/* Clean up: */
	usedspace = freespace = 0;

	return 0;
}

static int
sha512_init(Sum_t* p)
{
	Sha512_t*	sha = (Sha512_t*)p;

	MEMCPY_BCOPY(sha->state, sha512_initial_hash_value, SHA512_DIGEST_LENGTH);
	MEMSET_BZERO(sha->buffer, SHA512_BLOCK_LENGTH);
	sha->bitcount[0] = sha->bitcount[1] =  0;

	return 0;
}

static Sum_t*
sha512_open(const Method_t* method, const char* name)
{
	Sha512_t*	sha;

	if (sha = newof(0, Sha512_t, 1, 0))
	{
		sha->method = (Method_t*)method;
		sha->name = name;
		sha512_init((Sum_t*)sha);
	}
	return (Sum_t*)sha;
}

static int
sha512_done(Sum_t* p)
{
	Sha512_t*	sha = (Sha512_t*)p;
	unsigned int	usedspace;
	int		i;

	usedspace = (sha->bitcount[1] >> 3) % SHA512_BLOCK_LENGTH;
#if BYTE_ORDER == LITTLE_ENDIAN
	/* Convert FROM host byte order */
	REVERSE64(sha->bitcount[0],sha->bitcount[0]);
	REVERSE64(sha->bitcount[1],sha->bitcount[1]);
#endif
	if (usedspace > 0) {
		/* Begin padding with a 1 bit: */
		sha->buffer[usedspace++] = 0x80;

		if (usedspace <= SHA512_SHORT_BLOCK_LENGTH) {
			/* Set-up for the last transform: */
			MEMSET_BZERO(&sha->buffer[usedspace], SHA512_SHORT_BLOCK_LENGTH - usedspace);
		} else {
			if (usedspace < SHA512_BLOCK_LENGTH) {
				MEMSET_BZERO(&sha->buffer[usedspace], SHA512_BLOCK_LENGTH - usedspace);
			}
			/* Do second-to-last transform: */
			SHA512_Transform(sha, (sha2_word64*)sha->buffer);

			/* And set-up for the last transform: */
			MEMSET_BZERO(sha->buffer, SHA512_BLOCK_LENGTH - 2);
		}
	} else {
		/* Prepare for final transform: */
		MEMSET_BZERO(sha->buffer, SHA512_SHORT_BLOCK_LENGTH);

		/* Begin padding with a 1 bit: */
		*sha->buffer = 0x80;
	}
	/* Store the length of input data (in bits): */
	MEMCPY_BCOPY(&sha->buffer[SHA512_SHORT_BLOCK_LENGTH], &sha->bitcount[0], 16);

	/* Final transform: */
	SHA512_Transform(sha, (sha2_word64*)sha->buffer);

#if BYTE_ORDER == LITTLE_ENDIAN
	{
		/* Convert TO host byte order */
		sha2_word64*	d = (sha2_word64*)sha->digest;
		int		j;
		for (j = 0; j < 8; j++) {
			REVERSE64(sha->state[j],sha->state[j]);
			*d++ = sha->state[j];
		}
	}
#else
	MEMCPY_BCOPY(sha->digest, sha->state, SHA512_DIGEST_LENGTH);
#endif

	/* accumulate the digests */
	for (i = 0; i < SHA512_DIGEST_LENGTH; i++)
		sha->digest_sum[i] ^= sha->digest[i];

	/* Clean up state data: */
	MEMSET_BZERO(&sha->state, sizeof(*sha) - offsetof(Sha512_t, state));
	usedspace = 0;

	return 0;
}

static int
sha512_print(Sum_t* p, Sfio_t* sp, int flags, size_t scale)
{
	Sha512_t*	sha = (Sha512_t*)p;
	sha2_byte*	d;
	sha2_byte*	e;

	NOT_USED(scale);
	d = (flags & SUM_TOTAL) ? sha->digest_sum : sha->digest;
	e = d + SHA512_DIGEST_LENGTH;
	while (d < e)
		sfprintf(sp, "%02x", *d++);
	return 0;
}

static int
sha512_data(Sum_t* p, Sumdata_t* data)
{
	Sha512_t*	sha = (Sha512_t*)p;

	data->size = SHA512_DIGEST_LENGTH;
	data->num = 0;
	data->buf = sha->digest;
	return 0;
}

/*** SHA-384: *********************************************************/

#define sha384_description "FIPS SHA-384 secure hash algorithm."
#define sha384_options	"\
[+(version)?sha-384 (FIPS) 2000-01-01]\
[+(author)?Aaron D. Gifford]\
"
#define sha384_match	"sha384|sha-384|SHA384|SHA-384"
#define sha384_scale	0
#define sha384_block	sha512_block
#define sha384_done	sha512_done

#define sha384_padding	md5_pad

#define Sha384_t		Sha512_t
#define SHA384_CTX		Sha384_t
#define SHA384_DIGEST_LENGTH	48

static int
sha384_init(Sum_t* p)
{
	Sha384_t*	sha = (Sha384_t*)p;

	MEMCPY_BCOPY(sha->state, sha384_initial_hash_value, SHA512_DIGEST_LENGTH);
	MEMSET_BZERO(sha->buffer, SHA384_BLOCK_LENGTH);
	sha->bitcount[0] = sha->bitcount[1] = 0;

	return 0;
}

static Sum_t*
sha384_open(const Method_t* method, const char* name)
{
	Sha384_t*	sha;

	if (sha = newof(0, Sha384_t, 1, 0))
	{
		sha->method = (Method_t*)method;
		sha->name = name;
		sha384_init((Sum_t*)sha);
	}
	return (Sum_t*)sha;
}

static int
sha384_print(Sum_t* p, Sfio_t* sp, int flags, size_t scale)
{
	Sha384_t*	sha = (Sha384_t*)p;
	sha2_byte*	d;
	sha2_byte*	e;

	NOT_USED(scale);
	d = (flags & SUM_TOTAL) ? sha->digest_sum : sha->digest;
	e = d + SHA384_DIGEST_LENGTH;
	while (d < e)
		sfprintf(sp, "%02x", *d++);
	return 0;
}

static int
sha384_data(Sum_t* p, Sumdata_t* data)
{
	Sha384_t*	sha = (Sha384_t*)p;

	data->size = SHA384_DIGEST_LENGTH;
	data->num = 0;
	data->buf = sha->digest;
	return 0;
}

#endif /* _typ_int64_t */
