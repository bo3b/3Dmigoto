#ifndef CRC32C_H
#define CRC32C_H

#ifndef CRC32C_STATIC
#ifdef CRC32C_EXPORTS
#define CRC32C_API __declspec(dllexport)
#else
#define CRC32C_API __declspec(dllimport)
#endif
#else
#define CRC32C_API
#endif

#include <stdint.h>

/*
    Computes CRC-32C using Castagnoli polynomial of 0x82f63b78.
    This polynomial is better at detecting errors than the more common CRC-32 polynomial.
    CRC-32C is implemented in hardware on newer Intel processors.
    This function will use the hardware if available and fall back to fast software implementation.
*/
extern "C" CRC32C_API uint32_t crc32c_append(
    uint32_t crc,               // initial CRC, typically 0, may be used to accumulate CRC from multiple buffers
    const uint8_t *input,       // data to be put through the CRC algorithm
    size_t length);             // length of the data in the input buffer

extern "C" CRC32C_API void crc32c_unittest();

#endif
