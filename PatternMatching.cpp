#include <immintrin.h>
#include <string>
#include <iostream>
#include <cstring>

// =====================
// AVX2 Low-Level strchr
// =====================
const char *avx2_strchr(const char *haystack, char needle)
{
    __m256i vneedle = _mm256_set1_epi8(needle);
    const char *ptr = haystack;

    while (true)
    {
        __m256i chunk = _mm256_loadu_si256((const __m256i *)ptr);
        __m256i zeros = _mm256_cmpeq_epi8(chunk, _mm256_setzero_si256());
        int zero_mask = _mm256_movemask_epi8(zeros);

        __m256i cmp = _mm256_cmpeq_epi8(chunk, vneedle);
        int match_mask = _mm256_movemask_epi8(cmp);

        if (match_mask)
        {
            int pos = __builtin_ctz(match_mask);
            return ptr + pos;
        }
        if (zero_mask)
        {
            return nullptr;
        }

        ptr += 32;
    }
}

// =====================
// AVX2 Low-Level strstr
// =====================
const char *avx2_strstr(const char *haystack, size_t hay_len,
                        const char *needle, size_t needle_len)
{
    if (needle_len == 0)
        return haystack;
    if (hay_len < needle_len)
        return nullptr;

    const unsigned char first = (unsigned char)needle[0];
    __m256i vfirst = _mm256_set1_epi8(first);

    size_t pos = 0;
    size_t last_possible = hay_len - needle_len;

    while (pos <= last_possible)
    {
        __m256i chunk = _mm256_loadu_si256((const __m256i *)(haystack + pos));
        __m256i cmp = _mm256_cmpeq_epi8(chunk, vfirst);
        unsigned mask = _mm256_movemask_epi8(cmp);

        while (mask)
        {
            unsigned bit = __builtin_ctz(mask);
            size_t idx = pos + bit;
            if (idx + needle_len <= hay_len &&
                std::memcmp(haystack + idx, needle, needle_len) == 0)
            {
                return haystack + idx;
            }
            mask &= mask - 1;
        }

        pos += 32;
    }

    for (; pos <= last_possible; ++pos)
    {
        if (haystack[pos] == needle[0] &&
            std::memcmp(haystack + pos, needle, needle_len) == 0)
            return haystack + pos;
    }
    return nullptr;
}

// ==========================
// C++ std::string Wrappers
// ==========================
inline const char *fast_strchr(const std::string &s, char needle)
{
    return avx2_strchr(s.c_str(), needle);
}

inline const char *fast_strstr(const std::string &hay, const std::string &needle)
{
    return avx2_strstr(hay.c_str(), hay.length(), needle.c_str(), needle.length());
}

// ==========================
// Test Main
// ==========================
int main()
{
    std::string hay = "This is a very fast AVX2 string search test string!";
    std::string needle = "AVX2";

    const char *found1 = fast_strchr(hay, 'f');
    const char *found2 = fast_strstr(hay, needle);

    if (found1)
        std::cout << "Found 'f' at position: " << (found1 - hay.c_str()) << "\n";
    else
        std::cout << "'f' not found\n";

    if (found2)
        std::cout << "Found \"AVX2\" at position: " << (found2 - hay.c_str()) << "\n";
    else
        std::cout << "\"AVX2\" not found\n";

    return 0;
}