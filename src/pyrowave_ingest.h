/* Complete-frame PyroWave record ingestion. */
#ifndef LGC5_PYROWAVE_INGEST_H
#define LGC5_PYROWAVE_INGEST_H
#include "pyrowave_records.h"
typedef int (*lgc5_pw_push_fn)(void *,const void *,size_t);
typedef bool (*lgc5_pw_ready_fn)(void *,bool);
typedef void (*lgc5_pw_clear_fn)(void *);
typedef struct {
    void *decoder;
    lgc5_pw_push_fn push;
    lgc5_pw_ready_fn ready;
    lgc5_pw_clear_fn clear;
    unsigned frames,dropped;
    int last_stage,last_result;
    unsigned last_blocks;
} lgc5_pw_ingest;
static inline bool lgc5_pw_ingest_frame(lgc5_pw_ingest *ctx,const void *frame,
                                        size_t size,unsigned width,unsigned height) {
    if (!ctx || !ctx->decoder || !ctx->push || !ctx->ready || !ctx->clear)
        return false;
    ctx->clear(ctx->decoder);
    ctx->last_stage=1; ctx->last_result=0; ctx->last_blocks=0;
    lgc5_pw_records records;
    if (!lgc5_pw_records_init(&records,frame,size,width,height)) {
        ctx->last_stage=2; goto fail;
    }
    const uint8_t *record;
    size_t length;
    int state;
    while ((state=lgc5_pw_records_next(&records,&record,&length))==1) {
        int rc=ctx->push(ctx->decoder,record,length);
        if (rc!=0) { ctx->last_stage=4; ctx->last_result=rc; goto fail; }
    }
    ctx->last_blocks=records.blocks;
    if (state!=0) { ctx->last_stage=5; ctx->last_result=state; goto fail; }
    if (!ctx->ready(ctx->decoder,false)) { ctx->last_stage=6; goto fail; }
    ++ctx->frames;
    ctx->last_stage=0;
    return true;
fail:
    ++ctx->dropped;
    return false;
}
#endif
