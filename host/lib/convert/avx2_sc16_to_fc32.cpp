//
// Copyright 2024 Ettus Research, a National Instruments Company
//
// SPDX-License-Identifier: GPL-3.0-or-later
//

#include "convert_common.hpp"
#include <uhd/utils/byteswap.hpp>
#include <immintrin.h>

using namespace uhd::convert;

DECLARE_CONVERTER(sc16_item32_le, 1, fc32, 1, PRIORITY_SIMD)
{
    const item32_t* input = reinterpret_cast<const item32_t*>(inputs[0]);
    fc32_t* output        = reinterpret_cast<fc32_t*>(outputs[0]);

    const __m256 scalar = _mm256_set1_ps(float(scale_factor) / (1 << 16));
    const __m256i zeroi = _mm256_setzero_si256();

// this macro converts values faster by using SSE intrinsics to convert 4 values at a time
#define convert_item32_1_to_fc32_1_nswap_guts(_al_)                                \
    for (; i + 7 < nsamps; i += 8) {                                               \
        /* load from input */                                                      \
        __m256i tmpi =                                                             \
            _mm256_loadu_si256(reinterpret_cast<const __m256i*>(input + i)); \
                                                                                   \
        /* unpack + swap 16-bit pairs */                                           \
        tmpi = _mm256_shufflelo_epi16(tmpi, _MM_SHUFFLE(2, 3, 0, 1));              \
        tmpi = _mm256_shufflehi_epi16(tmpi, _MM_SHUFFLE(2, 3, 0, 1));              \
                                                                                   \
        __m256i tmpilo =                                                           \
            _mm256_unpacklo_epi16(zeroi, tmpi); /* value in upper 16 bits */       \
        __m256i tmpihi = _mm256_unpackhi_epi16(zeroi, tmpi);                       \
                                                                                   \
        __m256i shuffled_lo =                                                      \
            _mm256_permute2x128_si256(tmpilo, tmpihi, 0x20); /* lower 128-bit  */  \
        __m256i shuffled_hi =                                                      \
            _mm256_permute2x128_si256(tmpilo, tmpihi, 0x31); /* upper 128-bit  */  \
                                                                                   \
        /* convert and scale */                                                    \
        __m256 tmplo = _mm256_mul_ps(_mm256_cvtepi32_ps(shuffled_lo), scalar);     \
        __m256 tmphi = _mm256_mul_ps(_mm256_cvtepi32_ps(shuffled_hi), scalar);     \
                                                                                   \
        /* store to output */                                                      \
        _mm256_storeu_ps(reinterpret_cast<float*>(output + i + 0), tmplo);   \
        _mm256_storeu_ps(reinterpret_cast<float*>(output + i + 4), tmphi);   \
    }

    size_t i = 0;

    // need to dispatch according to alignment for fastest conversion
    switch (size_t(output) & 0xf) {
        case 0x0:
            // the data is 16-byte aligned, so do the fast processing of the bulk of the
            // samples
            convert_item32_1_to_fc32_1_nswap_guts(_) break;
        case 0x8:
            // the first sample is 8-byte aligned - process it to align the remainder of
            // the samples to 16-bytes
            item32_sc16_to_xx<uhd::htowx>(input, output, 1, scale_factor);
            i++;
            // do faster processing of the bulk of the samples now that we are 16-byte
            // aligned
            convert_item32_1_to_fc32_1_nswap_guts(_) break;
        default:
            // we are not 8 or 16-byte aligned, so do fast processing with the unaligned
            // load and store
            convert_item32_1_to_fc32_1_nswap_guts(u_)
    }

    // convert any remaining samples
    item32_sc16_to_xx<uhd::htowx>(input + i, output + i, nsamps - i, scale_factor);
}

DECLARE_CONVERTER(sc16_item32_be, 1, fc32, 1, PRIORITY_SIMD)
{
    const item32_t* input = reinterpret_cast<const item32_t*>(inputs[0]);
    fc32_t* output        = reinterpret_cast<fc32_t*>(outputs[0]);

    const __m256 scalar = _mm256_set1_ps(float(scale_factor) / (1 << 16));
    const __m256i zeroi = _mm256_setzero_si256();

// this macro converts values faster by using SSE intrinsics to convert 4 values at a time
#define convert_item32_1_to_fc32_1_bswap_guts(_al_)                                     \
    for (; i + 7 < nsamps; i += 8) {                                                    \
        /* load from input */                                                           \
        __m256i tmpi = _mm256_loadu_si256(reinterpret_cast<const __m256i*>(input + i)); \
                                                                                        \
        /* byteswap + unpack -> byteswap 16 bit words */                                \
        tmpi = _mm256_or_si256(_mm256_srli_epi16(tmpi, 8), _mm256_slli_epi16(tmpi, 8)); \
                                                                                        \
        __m256i tmpilo = _mm256_unpacklo_epi16(zeroi, tmpi);                            \
        __m256i tmpihi = _mm256_unpackhi_epi16(zeroi, tmpi);                            \
                                                                                        \
        __m256i shuffled_lo =                                                           \
            _mm256_permute2x128_si256(tmpilo, tmpihi, 0x20); /* lower 128-bit  */       \
        __m256i shuffled_hi =                                                           \
            _mm256_permute2x128_si256(tmpilo, tmpihi, 0x31); /* upper 128-bit  */       \
                                                                                        \
        /* convert and scale */                                                         \
        __m256 tmplo = _mm256_mul_ps(_mm256_cvtepi32_ps(shuffled_lo), scalar);          \
        __m256 tmphi = _mm256_mul_ps(_mm256_cvtepi32_ps(shuffled_hi), scalar);          \
                                                                                        \
        /* store to output */                                                           \
        _mm256_storeu_ps(reinterpret_cast<float*>(output + i + 0), tmplo);              \
        _mm256_storeu_ps(reinterpret_cast<float*>(output + i + 4), tmphi);              \
    }

    size_t i = 0;

    // need to dispatch according to alignment for fastest conversion
    switch (size_t(output) & 0xf) {
        case 0x0:
            // the data is 16-byte aligned, so do the fast processing of the bulk of the
            // samples
            convert_item32_1_to_fc32_1_bswap_guts(_) break;
        case 0x8:
            // the first sample is 8-byte aligned - process it to align the remainder of
            // the samples to 16-bytes
            item32_sc16_to_xx<uhd::htonx>(input, output, 1, scale_factor);
            i++;
            // do faster processing of the bulk of the samples now that we are 16-byte
            // aligned
            convert_item32_1_to_fc32_1_bswap_guts(_) break;
        default:
            // we are not 8 or 16-byte aligned, so do fast processing with the unaligned
            // load and store
            convert_item32_1_to_fc32_1_bswap_guts(u_)
    }

    // convert any remaining samples
    item32_sc16_to_xx<uhd::htonx>(input + i, output + i, nsamps - i, scale_factor);
}

DECLARE_CONVERTER(sc16_chdr, 1, fc32, 1, PRIORITY_SIMD)
{
    const sc16_t* input = reinterpret_cast<const sc16_t*>(inputs[0]);
    fc32_t* output      = reinterpret_cast<fc32_t*>(outputs[0]);

    const __m256 scalar = _mm256_set1_ps(float(scale_factor) / (1 << 16));
    const __m256i zeroi = _mm256_setzero_si256();

// this macro converts values faster by using SSE intrinsics to convert 4 values at a time
#define convert_item32_1_to_fc32_1_guts(_al_)                                      \
    for (; i + 7 < nsamps; i += 8) {                                               \
        /* load from input */                                                      \
        __m256i tmpi =                                                             \
            _mm256_loadu_si256(reinterpret_cast<const __m256i*>(input + i)); \
                                                                                   \
        /* unpack + swap 16-bit pairs */                                           \
        __m256i tmpilo =                                                           \
            _mm256_unpacklo_epi16(zeroi, tmpi); /* value in upper 16 bits */       \
        __m256i tmpihi = _mm256_unpackhi_epi16(zeroi, tmpi);                       \
                                                                                   \
        __m256i shuffled_lo =                                                      \
            _mm256_permute2x128_si256(tmpilo, tmpihi, 0x20); /* lower 128-bit  */  \
        __m256i shuffled_hi =                                                      \
            _mm256_permute2x128_si256(tmpilo, tmpihi, 0x31); /* upper 128-bit  */  \
                                                                                   \
        /* convert and scale */                                                    \
        __m256 tmplo = _mm256_mul_ps(_mm256_cvtepi32_ps(shuffled_lo), scalar);     \
        __m256 tmphi = _mm256_mul_ps(_mm256_cvtepi32_ps(shuffled_hi), scalar);     \
                                                                                   \
        /* store to output */                                                      \
        _mm256_storeu_ps(reinterpret_cast<float*>(output + i + 0), tmplo);   \
        _mm256_storeu_ps(reinterpret_cast<float*>(output + i + 4), tmphi);   \
    }

    size_t i = 0;

    // need to dispatch according to alignment for fastest conversion
    switch (size_t(output) & 0xf) {
        case 0x0:
            // the data is 16-byte aligned, so do the fast processing of the bulk of the
            // samples
            convert_item32_1_to_fc32_1_guts(_) break;
        case 0x8:
            // the first sample is 8-byte aligned - process it to align the remainder of
            // the samples to 16-bytes
            chdr_sc16_to_xx(input, output, 1, scale_factor);
            i++;
            // do faster processing of the bulk of the samples now that we are 16-byte
            // aligned
            convert_item32_1_to_fc32_1_guts(_) break;
        default:
            // we are not 8 or 16-byte aligned, so do fast processing with the unaligned
            // load and store
            convert_item32_1_to_fc32_1_guts(u_)
    }

    // convert any remaining samples
    chdr_sc16_to_xx(input + i, output + i, nsamps - i, scale_factor);
}
