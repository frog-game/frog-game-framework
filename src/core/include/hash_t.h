#pragma once

#include <stdint.h>
#include <stdbool.h>

// This is the Hash128to64 function from Google's cityhash (available
// under the MIT License).  We use it to reduce multiple 64 bit hashes
// into a single hash.
static inline uint64_t hash_128_to_64(const uint64_t upper, const uint64_t lower)
{
    // Murmur-inspired hashing.
    const uint64_t kMul = 0x9ddfea08eb382d69ULL;
    uint64_t       a    = (lower ^ upper) * kMul;
    a ^= (a >> 47);
    uint64_t b = (upper ^ a) * kMul;
    b ^= (b >> 47);
    b *= kMul;
    return b;
}

//////////////////////////////////////////////////////////////////////
/*
 * Thomas Wang 64 bit mix hash function
 */

static inline uint64_t twang_mix64(uint64_t key)
{
    key = (~key) + (key << 21);   // key *= (1 << 21) - 1; key -= 1;
    key = key ^ (key >> 24);
    key = key + (key << 3) + (key << 8);   // key *= 1 + (1 << 3) + (1 << 8)
    key = key ^ (key >> 14);
    key = key + (key << 2) + (key << 4);   // key *= 1 + (1 << 2) + (1 << 4)
    key = key ^ (key >> 28);
    key = key + (key << 31);   // key *= 1 + (1 << 31)
    return key;
}

/*
 * Inverse of twang_mix64
 *
 * Note that twang_unmix64 is significantly slower than twang_mix64.
 */

static inline uint64_t twang_unmix64(uint64_t key)
{
    // See the comments in jenkins_rev_unmix32 for an explanation as to how this
    // was generated
    key *= 4611686016279904257U;
    key ^= (key >> 28) ^ (key >> 56);
    key *= 14933078535860113213U;
    key ^= (key >> 14) ^ (key >> 28) ^ (key >> 42) ^ (key >> 56);
    key *= 15244667743933553977U;
    key ^= (key >> 24) ^ (key >> 48);
    key = (key + 1) * 9223367638806167551U;
    return key;
}

/*
 * Thomas Wang downscaling hash function
 */

static inline uint32_t twang_32from64(uint64_t key)
{
    key = (~key) + (key << 18);
    key = key ^ (key >> 31);
    key = key * 21;
    key = key ^ (key >> 11);
    key = key + (key << 6);
    key = key ^ (key >> 22);
    return (uint32_t)key;
}

/*
 * Robert Jenkins' reversible 32 bit mix hash function
 */

static inline uint32_t jenkins_rev_mix32(uint32_t key)
{
    key += (key << 12);   // key *= (1 + (1 << 12))
    key ^= (key >> 22);
    key += (key << 4);   // key *= (1 + (1 << 4))
    key ^= (key >> 9);
    key += (key << 10);   // key *= (1 + (1 << 10))
    key ^= (key >> 2);
    // key *= (1 + (1 << 7)) * (1 + (1 << 12))
    key += (key << 7);
    key += (key << 12);
    return key;
}

/*
 * Inverse of jenkins_rev_mix32
 *
 * Note that jenkinks_rev_unmix32 is significantly slower than
 * jenkins_rev_mix32.
 */

static inline uint32_t jenkins_rev_unmix32(uint32_t key)
{
    // These are the modular multiplicative inverses (in Z_2^32) of the
    // multiplication factors in jenkins_rev_mix32, in reverse order.  They were
    // computed using the Extended Euclidean algorithm, see
    // http://en.wikipedia.org/wiki/Modular_multiplicative_inverse
    key *= 2364026753U;

    // The inverse of a ^= (a >> n) is
    // b = a
    // for (int i = n; i < 32; i += n) {
    //   b ^= (a >> i);
    // }
    key ^= (key >> 2) ^ (key >> 4) ^ (key >> 6) ^ (key >> 8) ^ (key >> 10) ^ (key >> 12) ^
           (key >> 14) ^ (key >> 16) ^ (key >> 18) ^ (key >> 20) ^ (key >> 22) ^ (key >> 24) ^
           (key >> 26) ^ (key >> 28) ^ (key >> 30);
    key *= 3222273025U;
    key ^= (key >> 9) ^ (key >> 18) ^ (key >> 27);
    key *= 4042322161U;
    key ^= (key >> 22);
    key *= 16773121U;
    return key;
}

/*
 * Fowler / Noll / Vo (FNV) Hash
 *     http://www.isthe.com/chongo/tech/comp/fnv/
 */

static const uint32_t FNV_32_HASH_START  = 2166136261UL;
static const uint64_t FNV_64_HASH_START  = 14695981039346656037ULL;
static const uint64_t FNVA_64_HASH_START = 14695981039346656037ULL;

static inline uint32_t fnv32(const char* buf, uint32_t hash /*= FNV_32_HASH_START*/)
{
    const char* s = (const char*)buf;

    for (; *s; ++s) {
        hash += (hash << 1) + (hash << 4) + (hash << 7) + (hash << 8) + (hash << 24);
        hash ^= *s;
    }
    return hash;
}

static inline uint32_t fnv32_buf(const void* buf, size_t n, uint32_t hash /*= FNV_32_HASH_START*/)
{
    const char* char_buf = (const char*)buf;

    for (size_t i = 0; i < n; ++i) {
        hash += (hash << 1) + (hash << 4) + (hash << 7) + (hash << 8) + (hash << 24);
        hash ^= char_buf[i];
    }
    return hash;
}

static inline uint64_t fnv64(const char* buf, uint64_t hash /*= FNV_64_HASH_START*/)
{
    const char* s = (const char*)buf;

    for (; *s; ++s) {
        hash += (hash << 1) + (hash << 4) + (hash << 5) + (hash << 7) + (hash << 8) + (hash << 40);
        hash ^= *s;
    }
    return hash;
}

static inline uint64_t fnv64_buf(const void* buf, size_t n, uint64_t hash /*= FNV_64_HASH_START*/)
{
    const char* char_buf = (const char*)buf;

    for (size_t i = 0; i < n; ++i) {
        hash += (hash << 1) + (hash << 4) + (hash << 5) + (hash << 7) + (hash << 8) + (hash << 40);
        hash ^= char_buf[i];
    }
    return hash;
}