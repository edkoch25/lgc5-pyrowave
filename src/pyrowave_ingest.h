/* PyroWave packet ingest. No display backend is implied by successful decode readiness. */
#ifndef LGC5_PYROWAVE_INGEST_H
#define LGC5_PYROWAVE_INGEST_H
#include "pyrowave_records.h"
#include <stdlib.h>
typedef int (*lgc5_pw_push_fn)(void *, const void *, size_t);
typedef bool (*lgc5_pw_ready_fn)(void *, bool);
typedef struct {
    void *decoder;
    lgc5_pw_push_fn push;
    lgc5_pw_ready_fn ready;
    unsigned frames, dropped;
} lgc5_pw_ingest;
static inline bool lgc5_pw_ingest_frame(lgc5_pw_ingest *ctx, const void *frame,
                                        size_t size, unsigned width, unsigned height) {
    if (!ctx || !ctx->decoder || !ctx->push || !ctx->ready) return false;
    lgc5_pw_records records;
    if (!lgc5_pw_records_init(&records,frame,size,width,height)) goto fail;
    const uint8_t *block;
    size_t length;
    int state;
    while ((state=lgc5_pw_records_next(&records,&block,&length))==1) {
        int rc;
        if (records.blocks==1) {
            /* The first packet carries the 8-byte sequence header. */
            uint8_t *first=malloc(length+8);
            if (!first) goto fail;
            memcpy(first,frame,8);
            memcpy(first+8,block,length);
            rc=ctx->push(ctx->decoder,first,length+8);
            free(first);
        } else {
            rc=ctx->push(ctx->decoder,block,length);
        }
        if (rc!=0) goto fail;
    }
    if (state!=0 || !ctx->ready(ctx->decoder,false)) goto fail;
    ++ctx->frames;
    return true;
fail:
    ++ctx->dropped;
    return false;
}
#endif
