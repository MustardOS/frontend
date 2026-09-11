#include <pthread.h>
#include <stdint.h>
#include <string.h>
#include <SDL2/SDL.h>
#include "interframe_blend.h"

// This reminds of working on the OG RG35XX RA code for NEON specific performance bumps!
#if (defined(__ARM_NEON) || defined(__ARM_NEON__)) && !defined(INTERFRAME_BLEND_DISABLE_NEON)
#include <arm_neon.h>
#define INTERFRAME_BLEND_NEON 1
#endif

#define COLOUR_DIFFERENCE_THRESHOLD 64
#define FLICKER_HOLD_FRAMES          3

static unsigned channel_difference(const unsigned a, const unsigned b) {
    return a > b ? a - b : b - a;
}

static int colour_contrast_rgb888(
    const unsigned ar, const unsigned ag, const unsigned ab, const unsigned br, const unsigned bg, const unsigned bb
) {
    return channel_difference(ar, br) >= COLOUR_DIFFERENCE_THRESHOLD
           || channel_difference(ag, bg) >= COLOUR_DIFFERENCE_THRESHOLD
           || channel_difference(ab, bb) >= COLOUR_DIFFERENCE_THRESHOLD;
}

static int colour_contrast_xrgb8888(const uint32_t a, const uint32_t b) {
    return colour_contrast_rgb888(
        (a >> 16) & 0xff, (a >> 8) & 0xff, a & 0xff, (b >> 16) & 0xff, (b >> 8) & 0xff, b & 0xff
    );
}

static int colour_contrast_rgb565(const uint16_t a, const uint16_t b) {
    return channel_difference((a >> 11) & 0x1f, (b >> 11) & 0x1f) >= COLOUR_DIFFERENCE_THRESHOLD / 8
           || channel_difference((a >> 5) & 0x3f, (b >> 5) & 0x3f) >= COLOUR_DIFFERENCE_THRESHOLD / 4
           || channel_difference(a & 0x1f, b & 0x1f) >= COLOUR_DIFFERENCE_THRESHOLD / 8;
}

static int colour_contrast_xrgb1555(const uint16_t a, const uint16_t b) {
    return channel_difference((a >> 10) & 0x1f, (b >> 10) & 0x1f) >= COLOUR_DIFFERENCE_THRESHOLD / 8
           || channel_difference((a >> 5) & 0x1f, (b >> 5) & 0x1f) >= COLOUR_DIFFERENCE_THRESHOLD / 8
           || channel_difference(a & 0x1f, b & 0x1f) >= COLOUR_DIFFERENCE_THRESHOLD / 8;
}

static uint32_t average_xrgb8888(const uint32_t a, const uint32_t b) {
    // Average colour bytes independently and preserve the unused X byte.
    return (a & 0xff000000U) | (((a & b) + (((a ^ b) & 0x00fefefeU) >> 1)) & 0x00ffffffU);
}

static uint16_t average_rgb565(const uint16_t a, const uint16_t b) {
    return (uint16_t) ((a & b) + (((a ^ b) & 0xf7deU) >> 1));
}

static uint16_t average_xrgb1555(const uint16_t a, const uint16_t b) {
    return (uint16_t) ((a & 0x8000U) | (((a & b) + (((a ^ b) & 0x7bdeU) >> 1)) & 0x7fffU));
}

static int row_has_persistence(const uint8_t *persistence, const unsigned width) {
    unsigned x = 0;
    for (; x + sizeof(uint64_t) <= width; x += sizeof(uint64_t)) {
        uint64_t values;
        memcpy(&values, &persistence[x], sizeof(values));
        if (values) return 1;
    }
    for (; x < width; x++)
        if (persistence[x]) return 1;
    return 0;
}

#ifdef INTERFRAME_BLEND_NEON
static int neon_mask_any_u8(const uint8x8_t mask) {
    return vget_lane_u64(vreinterpret_u64_u8(mask), 0) != 0;
}

static int neon_mask_any_u16(const uint16x8_t mask) {
    const uint64x2_t packed = vreinterpretq_u64_u16(mask);
    return (vgetq_lane_u64(packed, 0) | vgetq_lane_u64(packed, 1)) != 0;
}

static uint8x8_t neon_colour_contrast_xrgb8888(const uint8x8x4_t a, const uint8x8x4_t b) {
    uint8x8_t contrast = vabd_u8(a.val[0], b.val[0]);
    contrast = vmax_u8(contrast, vabd_u8(a.val[1], b.val[1]));
    contrast = vmax_u8(contrast, vabd_u8(a.val[2], b.val[2]));
    return vcge_u8(contrast, vdup_n_u8(COLOUR_DIFFERENCE_THRESHOLD));
}

static uint16x8_t neon_colour_contrast_rgb565(const uint16x8_t a, const uint16x8_t b) {
    const uint16x8_t rdiff = vabdq_u16(vshrq_n_u16(a, 11), vshrq_n_u16(b, 11));
    const uint16x8_t gdiff = vabdq_u16(
        vandq_u16(vshrq_n_u16(a, 5), vdupq_n_u16(0x3f)),
        vandq_u16(vshrq_n_u16(b, 5), vdupq_n_u16(0x3f))
    );
    const uint16x8_t bdiff = vabdq_u16(vandq_u16(a, vdupq_n_u16(0x1f)), vandq_u16(b, vdupq_n_u16(0x1f)));
    return vorrq_u16(
        vcgeq_u16(rdiff, vdupq_n_u16(COLOUR_DIFFERENCE_THRESHOLD / 8)),
        vorrq_u16(
            vcgeq_u16(gdiff, vdupq_n_u16(COLOUR_DIFFERENCE_THRESHOLD / 4)),
            vcgeq_u16(bdiff, vdupq_n_u16(COLOUR_DIFFERENCE_THRESHOLD / 8))
        )
    );
}

static uint16x8_t neon_colour_contrast_xrgb1555(const uint16x8_t a, const uint16x8_t b) {
    const uint16x8_t rdiff = vabdq_u16(
        vandq_u16(vshrq_n_u16(a, 10), vdupq_n_u16(0x1f)),
        vandq_u16(vshrq_n_u16(b, 10), vdupq_n_u16(0x1f))
    );
    const uint16x8_t gdiff = vabdq_u16(
        vandq_u16(vshrq_n_u16(a, 5), vdupq_n_u16(0x1f)),
        vandq_u16(vshrq_n_u16(b, 5), vdupq_n_u16(0x1f))
    );
    const uint16x8_t bdiff = vabdq_u16(vandq_u16(a, vdupq_n_u16(0x1f)), vandq_u16(b, vdupq_n_u16(0x1f)));
    const uint16x8_t threshold = vdupq_n_u16(COLOUR_DIFFERENCE_THRESHOLD / 8);
    return vorrq_u16(
        vcgeq_u16(rdiff, threshold), vorrq_u16(vcgeq_u16(gdiff, threshold), vcgeq_u16(bdiff, threshold))
    );
}
#endif

static void blend_xrgb8888_rows(
    void *current, const void *previous, const void *two_back, const void *three_back, const void *four_back,
    const unsigned width, const unsigned start_y, const unsigned end_y, const size_t pitch, uint8_t *persistence,
    const size_t persistence_pitch
) {
    for (unsigned y = start_y; y < end_y; y++) {
        uint32_t *dst = (uint32_t *) ((uint8_t *) current + (size_t) y * pitch);
        const uint32_t *prev = (const uint32_t *) ((const uint8_t *) previous + (size_t) y * pitch);
        const uint32_t *prev2 = (const uint32_t *) ((const uint8_t *) two_back + (size_t) y * pitch);
        const uint32_t *prev3 =
            three_back ? (const uint32_t *) ((const uint8_t *) three_back + (size_t) y * pitch) : NULL;
        const uint32_t *prev4 =
            four_back ? (const uint32_t *) ((const uint8_t *) four_back + (size_t) y * pitch) : NULL;
        uint8_t *hold = persistence + (size_t) y * persistence_pitch;
        if (memcmp(dst, prev, (size_t) width * sizeof(*dst)) == 0 && !row_has_persistence(hold, width)) continue;

        unsigned x = 0;
#ifdef INTERFRAME_BLEND_NEON
        for (; x + 8 <= width; x += 8) {
            uint8x8x4_t curr = vld4_u8((const uint8_t *) &dst[x]);
            const uint8x8x4_t prior = vld4_u8((const uint8_t *) &prev[x]);
            const uint8x8x4_t prior2 = vld4_u8((const uint8_t *) &prev2[x]);
            uint8x8_t changed = vmvn_u8(vceq_u8(curr.val[0], prior.val[0]));
            changed = vorr_u8(changed, vmvn_u8(vceq_u8(curr.val[1], prior.val[1])));
            changed = vorr_u8(changed, vmvn_u8(vceq_u8(curr.val[2], prior.val[2])));
            const uint8x8_t old_hold = vld1_u8(&hold[x]);
            const uint8x8_t held = vcgt_u8(old_hold, vdup_n_u8(0));
            const uint8x8_t untracked_changed = vand_u8(changed, vmvn_u8(held));
            if (!neon_mask_any_u8(untracked_changed) && !neon_mask_any_u8(held)) continue;
            uint8x8_t repeats = vceq_u8(curr.val[0], prior2.val[0]);
            repeats = vand_u8(repeats, vceq_u8(curr.val[1], prior2.val[1]));
            repeats = vand_u8(repeats, vceq_u8(curr.val[2], prior2.val[2]));
            uint8x8_t exact_repeat = vand_u8(repeats, untracked_changed);
            if (prev3 && prev4 && neon_mask_any_u8(untracked_changed)) {
                const uint8x8x4_t prior3 = vld4_u8((const uint8_t *) &prev3[x]);
                const uint8x8x4_t prior4 = vld4_u8((const uint8_t *) &prev4[x]);
                uint8x8_t cadence2 = vceq_u8(curr.val[0], prior3.val[0]);
                cadence2 = vand_u8(cadence2, vceq_u8(curr.val[1], prior3.val[1]));
                cadence2 = vand_u8(cadence2, vceq_u8(curr.val[2], prior3.val[2]));
                cadence2 = vand_u8(cadence2, vceq_u8(prior3.val[0], prior4.val[0]));
                cadence2 = vand_u8(cadence2, vceq_u8(prior3.val[1], prior4.val[1]));
                cadence2 = vand_u8(cadence2, vceq_u8(prior3.val[2], prior4.val[2]));
                cadence2 = vand_u8(cadence2, vceq_u8(prior.val[0], prior2.val[0]));
                cadence2 = vand_u8(cadence2, vceq_u8(prior.val[1], prior2.val[1]));
                cadence2 = vand_u8(cadence2, vceq_u8(prior.val[2], prior2.val[2]));
                exact_repeat = vorr_u8(exact_repeat, vand_u8(cadence2, untracked_changed));
            }
            uint8x8_t confirmed = vdup_n_u8(0);
            if (neon_mask_any_u8(exact_repeat)) {
                const uint8x8_t prior_contrast = neon_colour_contrast_xrgb8888(curr, prior);
                confirmed = vand_u8(exact_repeat, prior_contrast);
            }
            const uint8x8_t blend = vorr_u8(confirmed, held);
            const uint8x8_t next_hold =
                vbsl_u8(confirmed, vdup_n_u8(FLICKER_HOLD_FRAMES), vqsub_u8(old_hold, vdup_n_u8(1)));
            if (neon_mask_any_u8(blend)) {
                const uint8x8_t use_prior = vorr_u8(confirmed, vand_u8(held, changed));
                const uint8x8_t reference0 = vbsl_u8(use_prior, prior.val[0], prior2.val[0]);
                const uint8x8_t reference1 = vbsl_u8(use_prior, prior.val[1], prior2.val[1]);
                const uint8x8_t reference2 = vbsl_u8(use_prior, prior.val[2], prior2.val[2]);
                curr.val[0] = vbsl_u8(blend, vhadd_u8(curr.val[0], reference0), curr.val[0]);
                curr.val[1] = vbsl_u8(blend, vhadd_u8(curr.val[1], reference1), curr.val[1]);
                curr.val[2] = vbsl_u8(blend, vhadd_u8(curr.val[2], reference2), curr.val[2]);
                vst4_u8((uint8_t *) &dst[x], curr);
            }
            vst1_u8(&hold[x], next_hold);
        }
#endif
        for (; x < width; x++) {
            if (hold[x]) {
                if (((dst[x] ^ prev[x]) & 0x00ffffffU) != 0)
                    dst[x] = average_xrgb8888(dst[x], prev[x]);
                else
                    dst[x] = average_xrgb8888(dst[x], prev2[x]);
                hold[x]--;
                continue;
            }
            const int changed = ((dst[x] ^ prev[x]) & 0x00ffffffU) != 0;
            const int cadence1 = ((dst[x] ^ prev2[x]) & 0x00ffffffU) == 0;
            const int cadence2 = prev3 && prev4 && ((dst[x] ^ prev3[x]) & 0x00ffffffU) == 0
                                 && ((prev3[x] ^ prev4[x]) & 0x00ffffffU) == 0
                                 && ((prev[x] ^ prev2[x]) & 0x00ffffffU) == 0;
            const int exact_repeat = changed && (cadence1 || cadence2);
            const int confirmed = exact_repeat && colour_contrast_xrgb8888(dst[x], prev[x]);
            if (confirmed) {
                dst[x] = average_xrgb8888(dst[x], prev[x]);
                hold[x] = FLICKER_HOLD_FRAMES;
            }
        }
    }
}

static void blend_rgb565_rows(
    void *current, const void *previous, const void *two_back, const void *three_back, const void *four_back,
    const unsigned width, const unsigned start_y, const unsigned end_y, const size_t pitch, uint8_t *persistence,
    const size_t persistence_pitch
) {
    for (unsigned y = start_y; y < end_y; y++) {
        uint16_t *dst = (uint16_t *) ((uint8_t *) current + (size_t) y * pitch);
        const uint16_t *prev = (const uint16_t *) ((const uint8_t *) previous + (size_t) y * pitch);
        const uint16_t *prev2 = (const uint16_t *) ((const uint8_t *) two_back + (size_t) y * pitch);
        const uint16_t *prev3 =
            three_back ? (const uint16_t *) ((const uint8_t *) three_back + (size_t) y * pitch) : NULL;
        const uint16_t *prev4 =
            four_back ? (const uint16_t *) ((const uint8_t *) four_back + (size_t) y * pitch) : NULL;
        uint8_t *hold = persistence + (size_t) y * persistence_pitch;
        if (memcmp(dst, prev, (size_t) width * sizeof(*dst)) == 0 && !row_has_persistence(hold, width)) continue;

        unsigned x = 0;
#ifdef INTERFRAME_BLEND_NEON
        for (; x + 8 <= width; x += 8) {
            const uint16x8_t curr = vld1q_u16(&dst[x]);
            const uint16x8_t prior = vld1q_u16(&prev[x]);
            const uint16x8_t prior2 = vld1q_u16(&prev2[x]);
            const uint16x8_t changed = vmvnq_u16(vceqq_u16(curr, prior));
            const uint8x8_t old_hold = vld1_u8(&hold[x]);
            const uint16x8_t held = vcgtq_u16(vmovl_u8(old_hold), vdupq_n_u16(0));
            const uint16x8_t untracked_changed = vandq_u16(changed, vmvnq_u16(held));
            if (!neon_mask_any_u16(untracked_changed) && !neon_mask_any_u16(held)) continue;
            uint16x8_t exact_repeat = vandq_u16(vceqq_u16(curr, prior2), untracked_changed);
            if (prev3 && prev4 && neon_mask_any_u16(untracked_changed)) {
                const uint16x8_t prior3 = vld1q_u16(&prev3[x]);
                const uint16x8_t prior4 = vld1q_u16(&prev4[x]);
                const uint16x8_t cadence2 =
                    vandq_u16(vceqq_u16(curr, prior3), vandq_u16(vceqq_u16(prior3, prior4), vceqq_u16(prior, prior2)));
                exact_repeat = vorrq_u16(exact_repeat, vandq_u16(cadence2, untracked_changed));
            }
            uint16x8_t confirmed = vdupq_n_u16(0);
            if (neon_mask_any_u16(exact_repeat)) {
                const uint16x8_t prior_contrast = neon_colour_contrast_rgb565(curr, prior);
                confirmed = vandq_u16(exact_repeat, prior_contrast);
            }
            const uint16x8_t blend = vorrq_u16(confirmed, held);
            const uint8x8_t confirmed_bytes = vmovn_u16(confirmed);
            const uint8x8_t next_hold =
                vbsl_u8(confirmed_bytes, vdup_n_u8(FLICKER_HOLD_FRAMES), vqsub_u8(old_hold, vdup_n_u8(1)));
            if (neon_mask_any_u16(blend)) {
                const uint16x8_t use_prior = vorrq_u16(confirmed, vandq_u16(held, changed));
                const uint16x8_t reference = vbslq_u16(use_prior, prior, prior2);
                const uint16x8_t average = vaddq_u16(
                    vandq_u16(curr, reference),
                    vshrq_n_u16(vandq_u16(veorq_u16(curr, reference), vdupq_n_u16(0xf7de)), 1)
                );
                vst1q_u16(&dst[x], vbslq_u16(blend, average, curr));
            }
            vst1_u8(&hold[x], next_hold);
        }
#endif
        for (; x < width; x++) {
            if (hold[x]) {
                if (dst[x] != prev[x])
                    dst[x] = average_rgb565(dst[x], prev[x]);
                else
                    dst[x] = average_rgb565(dst[x], prev2[x]);
                hold[x]--;
                continue;
            }
            const int cadence1 = dst[x] == prev2[x];
            const int cadence2 = prev3 && prev4 && dst[x] == prev3[x] && prev3[x] == prev4[x] && prev[x] == prev2[x];
            const int exact_repeat = dst[x] != prev[x] && (cadence1 || cadence2);
            const int confirmed = exact_repeat && colour_contrast_rgb565(dst[x], prev[x]);
            if (confirmed) {
                dst[x] = average_rgb565(dst[x], prev[x]);
                hold[x] = FLICKER_HOLD_FRAMES;
            }
        }
    }
}

static void blend_xrgb1555_rows(
    void *current, const void *previous, const void *two_back, const void *three_back, const void *four_back,
    const unsigned width, const unsigned start_y, const unsigned end_y, const size_t pitch, uint8_t *persistence,
    const size_t persistence_pitch
) {
    for (unsigned y = start_y; y < end_y; y++) {
        uint16_t *dst = (uint16_t *) ((uint8_t *) current + (size_t) y * pitch);
        const uint16_t *prev = (const uint16_t *) ((const uint8_t *) previous + (size_t) y * pitch);
        const uint16_t *prev2 = (const uint16_t *) ((const uint8_t *) two_back + (size_t) y * pitch);
        const uint16_t *prev3 =
            three_back ? (const uint16_t *) ((const uint8_t *) three_back + (size_t) y * pitch) : NULL;
        const uint16_t *prev4 =
            four_back ? (const uint16_t *) ((const uint8_t *) four_back + (size_t) y * pitch) : NULL;
        uint8_t *hold = persistence + (size_t) y * persistence_pitch;
        if (memcmp(dst, prev, (size_t) width * sizeof(*dst)) == 0 && !row_has_persistence(hold, width)) continue;

        unsigned x = 0;
#ifdef INTERFRAME_BLEND_NEON
        const uint16x8_t colour_mask = vdupq_n_u16(0x7fff);
        for (; x + 8 <= width; x += 8) {
            const uint16x8_t curr = vld1q_u16(&dst[x]);
            const uint16x8_t prior = vld1q_u16(&prev[x]);
            const uint16x8_t prior2 = vld1q_u16(&prev2[x]);
            const uint16x8_t curr_colour = vandq_u16(curr, colour_mask);
            const uint16x8_t prior_colour = vandq_u16(prior, colour_mask);
            const uint16x8_t prior2_colour = vandq_u16(prior2, colour_mask);
            const uint16x8_t changed = vmvnq_u16(vceqq_u16(curr_colour, prior_colour));
            const uint8x8_t old_hold = vld1_u8(&hold[x]);
            const uint16x8_t held = vcgtq_u16(vmovl_u8(old_hold), vdupq_n_u16(0));
            const uint16x8_t untracked_changed = vandq_u16(changed, vmvnq_u16(held));
            if (!neon_mask_any_u16(untracked_changed) && !neon_mask_any_u16(held)) continue;
            uint16x8_t exact_repeat = vandq_u16(vceqq_u16(curr_colour, prior2_colour), untracked_changed);
            if (prev3 && prev4 && neon_mask_any_u16(untracked_changed)) {
                const uint16x8_t prior3_colour = vandq_u16(vld1q_u16(&prev3[x]), colour_mask);
                const uint16x8_t prior4_colour = vandq_u16(vld1q_u16(&prev4[x]), colour_mask);
                const uint16x8_t cadence2 = vandq_u16(
                    vceqq_u16(curr_colour, prior3_colour),
                    vandq_u16(vceqq_u16(prior3_colour, prior4_colour), vceqq_u16(prior_colour, prior2_colour))
                );
                exact_repeat = vorrq_u16(exact_repeat, vandq_u16(cadence2, untracked_changed));
            }
            uint16x8_t confirmed = vdupq_n_u16(0);
            if (neon_mask_any_u16(exact_repeat)) {
                const uint16x8_t prior_contrast = neon_colour_contrast_xrgb1555(curr, prior);
                confirmed = vandq_u16(exact_repeat, prior_contrast);
            }
            const uint16x8_t blend = vorrq_u16(confirmed, held);
            const uint8x8_t confirmed_bytes = vmovn_u16(confirmed);
            const uint8x8_t next_hold =
                vbsl_u8(confirmed_bytes, vdup_n_u8(FLICKER_HOLD_FRAMES), vqsub_u8(old_hold, vdup_n_u8(1)));
            if (neon_mask_any_u16(blend)) {
                const uint16x8_t use_prior = vorrq_u16(confirmed, vandq_u16(held, changed));
                const uint16x8_t reference = vbslq_u16(use_prior, prior, prior2);
                uint16x8_t average = vaddq_u16(
                    vandq_u16(curr, reference),
                    vshrq_n_u16(vandq_u16(veorq_u16(curr, reference), vdupq_n_u16(0x7bde)), 1)
                );
                average = vorrq_u16(vandq_u16(average, colour_mask), vandq_u16(curr, vdupq_n_u16(0x8000)));
                vst1q_u16(&dst[x], vbslq_u16(blend, average, curr));
            }
            vst1_u8(&hold[x], next_hold);
        }
#endif
        for (; x < width; x++) {
            if (hold[x]) {
                if (((dst[x] ^ prev[x]) & 0x7fffU) != 0)
                    dst[x] = average_xrgb1555(dst[x], prev[x]);
                else
                    dst[x] = average_xrgb1555(dst[x], prev2[x]);
                hold[x]--;
                continue;
            }
            const int changed = ((dst[x] ^ prev[x]) & 0x7fffU) != 0;
            const int cadence1 = ((dst[x] ^ prev2[x]) & 0x7fffU) == 0;
            const int cadence2 = prev3 && prev4 && ((dst[x] ^ prev3[x]) & 0x7fffU) == 0
                                 && ((prev3[x] ^ prev4[x]) & 0x7fffU) == 0 && ((prev[x] ^ prev2[x]) & 0x7fffU) == 0;
            const int exact_repeat = changed && (cadence1 || cadence2);
            const int confirmed = exact_repeat && colour_contrast_xrgb1555(dst[x], prev[x]);
            if (confirmed) {
                dst[x] = average_xrgb1555(dst[x], prev[x]);
                hold[x] = FLICKER_HOLD_FRAMES;
            }
        }
    }
}

static void blend_rows(
    void *current, const void *previous, const void *two_back, const void *three_back, const void *four_back,
    const unsigned width, const unsigned start_y, const unsigned end_y, const size_t pitch,
    const enum retro_pixel_format format, uint8_t *persistence, const size_t persistence_pitch
) {
    switch (format) {
        case RETRO_PIXEL_FORMAT_XRGB8888:
            blend_xrgb8888_rows(
                current, previous, two_back, three_back, four_back, width, start_y, end_y, pitch, persistence,
                persistence_pitch
            );
            break;
        case RETRO_PIXEL_FORMAT_RGB565:
            blend_rgb565_rows(
                current, previous, two_back, three_back, four_back, width, start_y, end_y, pitch, persistence,
                persistence_pitch
            );
            break;
        case RETRO_PIXEL_FORMAT_0RGB1555:
        default:
            blend_xrgb1555_rows(
                current, previous, two_back, three_back, four_back, width, start_y, end_y, pitch, persistence,
                persistence_pitch
            );
            break;
    }
}

enum { initial_thread_threshold_pixels = 1280 * 720, max_filter_threads = 4, max_filter_workers = 3 };

struct filter_job {
    void *current;
    const void *previous;
    const void *two_back;
    const void *three_back;
    const void *four_back;
    unsigned width;
    unsigned height;
    size_t pitch;
    uint8_t *persistence;
    size_t persistence_pitch;
    enum retro_pixel_format format;
    unsigned threads;
};

static pthread_mutex_t worker_mutex = PTHREAD_MUTEX_INITIALIZER;
static pthread_cond_t worker_start = PTHREAD_COND_INITIALIZER;
static pthread_cond_t worker_done = PTHREAD_COND_INITIALIZER;
static pthread_t worker_threads[max_filter_workers];
static unsigned worker_indices[max_filter_workers];
static uint64_t worker_generation = 0;
static unsigned workers_completed = 0;
static unsigned workers_created = 0;
static int workers_running = 0;
static int workers_failed = 0;
static int workers_stop = 0;
static struct filter_job active_job = {0};
static size_t adaptive_thread_threshold = initial_thread_threshold_pixels;
static double last_filter_ms = 0.0;
static unsigned last_filter_threads = 1;

static void *filter_worker(void *opaque) {
    const unsigned slice = *(const unsigned *) opaque;
    uint64_t seen_generation = 0;

    pthread_mutex_lock(&worker_mutex);
    for (;;) {
        while (!workers_stop && seen_generation == worker_generation)
            pthread_cond_wait(&worker_start, &worker_mutex);
        if (workers_stop) break;

        seen_generation = worker_generation;
        const struct filter_job job = active_job;
        pthread_mutex_unlock(&worker_mutex);

        if (slice < job.threads) {
            const unsigned start_y = (unsigned) (((uint64_t) job.height * slice) / job.threads);
            const unsigned end_y = (unsigned) (((uint64_t) job.height * (slice + 1)) / job.threads);
            blend_rows(
                job.current, job.previous, job.two_back, job.three_back, job.four_back, job.width, start_y, end_y,
                job.pitch, job.format, job.persistence, job.persistence_pitch
            );
        }

        pthread_mutex_lock(&worker_mutex);
        workers_completed++;
        if (workers_completed == workers_created) pthread_cond_signal(&worker_done);
    }
    pthread_mutex_unlock(&worker_mutex);
    return NULL;
}

static int start_workers(void) {
    if (workers_running) return 1;
    if (workers_failed) return 0;

    workers_stop = 0;
    workers_created = 0;
    worker_generation = 0;
    unsigned wanted_workers = SDL_GetCPUCount() > 1 ? (unsigned) SDL_GetCPUCount() - 1 : 0;
    if (wanted_workers > max_filter_workers) wanted_workers = max_filter_workers;
    if (!wanted_workers) return 0;

    for (unsigned i = 0; i < wanted_workers; i++) {
        worker_indices[i] = i + 1;
        if (pthread_create(&worker_threads[i], NULL, filter_worker, &worker_indices[i]) != 0) {
            pthread_mutex_lock(&worker_mutex);
            workers_stop = 1;
            pthread_cond_broadcast(&worker_start);
            pthread_mutex_unlock(&worker_mutex);
            for (unsigned joined = 0; joined < workers_created; joined++)
                pthread_join(worker_threads[joined], NULL);
            workers_created = 0;
            workers_stop = 0;
            workers_failed = 1;
            return 0;
        }
        workers_created++;
    }

    workers_running = 1;
    return 1;
}

void interframe_blend_shutdown(void) {
    adaptive_thread_threshold = initial_thread_threshold_pixels;
    last_filter_ms = 0.0;
    last_filter_threads = 1;

    if (!workers_running) {
        workers_failed = 0;
        return;
    }

    pthread_mutex_lock(&worker_mutex);
    workers_stop = 1;
    pthread_cond_broadcast(&worker_start);
    pthread_mutex_unlock(&worker_mutex);
    for (unsigned i = 0; i < workers_created; i++)
        pthread_join(worker_threads[i], NULL);

    workers_created = 0;
    workers_running = 0;
    workers_failed = 0;
    workers_stop = 0;
    worker_generation = 0;
}

double interframe_blend_last_ms(void) {
    return last_filter_ms;
}

unsigned interframe_blend_thread_count(void) {
    return last_filter_threads;
}

size_t interframe_blend_thread_threshold_pixels(void) {
    return adaptive_thread_threshold;
}

void interframe_blend_detected(
    void *current, const void *previous, const void *two_back, const void *three_back, const void *four_back,
    const unsigned width, const unsigned height, const size_t pitch, const enum retro_pixel_format format,
    uint8_t *persistence, const size_t persistence_pitch
) {
    if (!current || !previous || !two_back || !persistence || width == 0 || height == 0 || persistence_pitch < width)
        return;

    const uint64_t started = SDL_GetPerformanceCounter();
    const size_t pixels = (size_t) width * height;
    const int threaded = pixels >= adaptive_thread_threshold && start_workers();
    if (!threaded) {
        blend_rows(
            current, previous, two_back, three_back, four_back, width, 0, height, pitch, format, persistence,
            persistence_pitch
        );
        last_filter_ms =
            (double) (SDL_GetPerformanceCounter() - started) * 1000.0 / (double) SDL_GetPerformanceFrequency();
        last_filter_threads = 1;
        if (last_filter_ms > 1.25 && pixels >= 320u * 240u) {
            adaptive_thread_threshold = pixels;
        } else if (last_filter_ms < 0.45 && pixels >= adaptive_thread_threshold / 2) {
            adaptive_thread_threshold = pixels + pixels / 2;
        }
        return;
    }

    unsigned active_threads = workers_created + 1;
    if (active_threads > 2 && pixels < adaptive_thread_threshold * 2) active_threads = 2;

    pthread_mutex_lock(&worker_mutex);
    active_job = (struct filter_job) {
        .current = current,
        .previous = previous,
        .two_back = two_back,
        .three_back = three_back,
        .four_back = four_back,
        .width = width,
        .height = height,
        .pitch = pitch,
        .format = format,
        .persistence = persistence,
        .persistence_pitch = persistence_pitch,
        .threads = active_threads,
    };
    workers_completed = 0;
    worker_generation++;
    pthread_cond_broadcast(&worker_start);
    pthread_mutex_unlock(&worker_mutex);

    blend_rows(
        current, previous, two_back, three_back, four_back, width, 0, (unsigned) ((uint64_t) height / active_threads),
        pitch, format, persistence, persistence_pitch
    );

    pthread_mutex_lock(&worker_mutex);
    while (workers_completed != workers_created)
        pthread_cond_wait(&worker_done, &worker_mutex);
    pthread_mutex_unlock(&worker_mutex);
    last_filter_ms = (double) (SDL_GetPerformanceCounter() - started) * 1000.0 / (double) SDL_GetPerformanceFrequency();
    last_filter_threads = active_threads;
}
