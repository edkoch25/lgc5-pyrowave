/* Minimal PyroWave frame-record iterator for Moonlight reassembled frames.
 * Pure C, no allocation or platform dependencies. */
#ifndef LGC5_PYROWAVE_RECORDS_H
#define LGC5_PYROWAVE_RECORDS_H
#include <stddef.h>
#include <stdint.h>
#include <stdbool.h>
#include <string.h>
typedef struct {
    const uint8_t *frame;
    size_t size, offset;
    uint32_t sequence_header;
    uint32_t total_blocks;
    unsigned width, height;
    unsigned blocks, sequence_headers;
    bool have_sequence, invalid;
} lgc5_pw_records;
static inline uint32_t lgc5_pw_le32(const uint8_t *p) {
    return (uint32_t)p[0] | ((uint32_t)p[1]<<8) | ((uint32_t)p[2]<<16) | ((uint32_t)p[3]<<24);
}
static inline bool lgc5_pw_records_init(lgc5_pw_records *r, const void *data,
                                         size_t size, unsigned width, unsigned height) {
    if (!r || !data || size < 16 || size > 32u*1024u*1024u ||
        !width || !height || width > 16384 || height > 16384) return false;
    memset(r, 0, sizeof(*r));
    r->frame = (const uint8_t *)data;
    r->size = size;
    r->offset = 0;
    r->width = width;
    r->height = height;
    return true;
}
/* Returns 1 for a complete bitstream record (sequence header or block),
 * 0 for end, -1 for malformed input. Pass each returned record unchanged
 * to pyrowave_decoder_push_packet() in the order received. */
static inline int lgc5_pw_records_next(lgc5_pw_records *r,
                                        const uint8_t **block, size_t *length) {
    if (!r || !block || !length || r->invalid) return -1;
    while (r->offset < r->size) {
        size_t remaining = r->size - r->offset;
        if (remaining < 8) break;
        const uint8_t *p = r->frame + r->offset;
        uint32_t a = lgc5_pw_le32(p), b = lgc5_pw_le32(p+4);
        if (a == 0xffffffffu) {
            size_t words = (size_t)b;
            if (words > (remaining-8)/4) break;
            for (size_t i=0; i<words; ++i)
                if (lgc5_pw_le32(p+8+i*4) != 0) goto invalid;
            r->offset += 8 + words*4;
            continue;
        }
        if (a & 0x80000000u) {
            uint32_t flags = b;
            if (((a & 0x3fffu) + 1) != r->width ||
                (((a >> 14) & 0x3fffu) + 1) != r->height ||
                ((flags >> 24) & 3u) != 0 || ((flags >> 26) & 1u)) goto invalid;
            if (r->have_sequence &&
                ((a >> 28) & 7u) != ((r->sequence_header >> 28) & 7u)) goto invalid;
            r->sequence_header = a;
            r->total_blocks = flags & 0x00ffffffu;
            r->have_sequence = true;
            ++r->sequence_headers;
            *block = p; *length = 8;
            r->offset += 8;
            return 1;
        }
        size_t bytes = (size_t)((a >> 16) & 0xfffu) * 4;
        if (bytes < 8 || bytes > remaining) goto invalid;
        if (r->have_sequence &&
            ((a >> 28) & 7u) != ((r->sequence_header >> 28) & 7u)) goto invalid;
        *block = p; *length = bytes;
        r->offset += bytes;
        ++r->blocks;
        return 1;
    }
    if (r->offset == r->size && r->blocks) return 0;
invalid:
    r->invalid = true;
    return -1;
}
#endif
