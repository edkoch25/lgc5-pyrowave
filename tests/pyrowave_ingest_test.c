#include "../src/pyrowave_ingest.h"
#include <assert.h>
#include <stdio.h>
static unsigned calls;
static int push(void *decoder,const void *data,size_t n) {
    assert(decoder==(void *)0x1);
    const uint8_t *p=data;
    assert(n==16 && (p[3]&0x80) && p[10]==2);
    ++calls;
    return 0;
}
static bool ready(void *decoder,bool partial) {
    assert(decoder==(void *)0x1 && !partial);
    return calls==1;
}
int main(void) {
    uint8_t frame[16]={0};
    uint32_t h=0x80000000u|3199u|(1799u<<14);
    for(unsigned i=0;i<4;i++) frame[i]=(uint8_t)(h>>(8*i));
    frame[10]=2;
    lgc5_pw_ingest ctx={.decoder=(void *)0x1,.push=push,.ready=ready};
    assert(lgc5_pw_ingest_frame(&ctx,frame,sizeof(frame),3200,1800));
    assert(ctx.frames==1 && ctx.dropped==0 && calls==1);
    assert(!lgc5_pw_ingest_frame(&ctx,frame,12,3200,1800));
    assert(ctx.dropped==1);
    puts("PyroWave packet ingest: PASS");
    return 0;
}
