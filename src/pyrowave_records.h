/* Minimal PyroWave record iterator for complete Moonlight frames. */
#ifndef LGC5_PYROWAVE_RECORDS_H
#define LGC5_PYROWAVE_RECORDS_H
#include <stddef.h>
#include <stdint.h>
#include <stdbool.h>
#include <string.h>
typedef struct {
    const uint8_t *frame;
    size_t size, offset;
    uint32_t sequence, total_blocks;
    unsigned width, height, blocks;
    bool have_sequence, invalid;
} lgc5_pw_records;
static inline uint32_t lgc5_pw_le32(const uint8_t *p) {
    return (uint32_t)p[0] | ((uint32_t)p[1]<<8) |
           ((uint32_t)p[2]<<16) | ((uint32_t)p[3]<<24);
}
static inline bool lgc5_pw_records_init(lgc5_pw_records *r, const void *data,
                                         size_t size, unsigned width, unsigned height) {
    if (!r || !data || size < 16 || size > 32u*1024u*1024u ||
        !width || !height || width > 16384 || height > 16384) return false;
    memset(r,0,sizeof(*r));
    r->frame=(const uint8_t *)data;
    r->size=size;
    r->width=width;
    r->height=height;
    return true;
}
/* Returns 1 for sequence/block record, 0 for complete frame, -1 if malformed. */
static inline int lgc5_pw_records_next(lgc5_pw_records *r,
                                        const uint8_t **record, size_t *length) {
    if (!r || !record || !length || r->invalid) return -1;
    while (r->offset<r->size) {
        size_t remaining=r->size-r->offset;
        if (remaining<8) goto invalid;
        const uint8_t *p=r->frame+r->offset;
        uint32_t a=lgc5_pw_le32(p), b=lgc5_pw_le32(p+4);
        if (a==0xffffffffu) {
            size_t bytes=8u+(size_t)b*4u;
            if (!r->have_sequence || bytes>remaining) goto invalid;
            for (size_t i=0;i<(size_t)b;++i)
                if (lgc5_pw_le32(p+8+i*4)!=0) goto invalid;
            r->offset+=bytes;
            continue;
        }
        if (a&0x80000000u) {
            if (r->offset!=0 || r->have_sequence ||
                ((a&0x3fffu)+1u)!=r->width ||
                (((a>>14)&0x3fffu)+1u)!=r->height ||
                ((b>>24)&3u)!=0 || ((b>>26)&1u)!=0) goto invalid;
            r->sequence=(a>>28)&7u;
            r->total_blocks=b&0x00ffffffu;
            r->have_sequence=true;
            *record=p; *length=8;
            r->offset+=8;
            return 1;
        }
        if (!r->have_sequence || ((a>>28)&7u)!=r->sequence) goto invalid;
        size_t bytes=(size_t)((a>>16)&0xfffu)*4u;
        if (bytes<8 || bytes>remaining) goto invalid;
        *record=p; *length=bytes;
        r->offset+=bytes;
        ++r->blocks;
        return 1;
    }
    if (r->offset==r->size && r->have_sequence &&
        r->blocks==r->total_blocks) return 0;
invalid:
    r->invalid=true;
    return -1;
}
#endif
