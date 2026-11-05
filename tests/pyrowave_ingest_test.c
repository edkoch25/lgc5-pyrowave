#include "../src/pyrowave_ingest.h"
#include <assert.h>
#include <stdio.h>
static unsigned pushes,clears;
static int push(void *decoder,const void *data,size_t n) {
    const uint8_t *p=(const uint8_t *)data;
    assert(decoder==(void *)0x1);
    if (pushes==0) assert(n==8 && (p[3]&0x80));
    else assert(n==8 && p[2]==2);
    ++pushes;
    return 0;
}
static bool ready(void *decoder,bool partial) {
    assert(decoder==(void *)0x1 && !partial);
    return pushes==2;
}
static void clear(void *decoder) {
    assert(decoder==(void *)0x1);
    ++clears; pushes=0;
}
int main(void) {
    uint8_t frame[16]={0};
    uint32_t h=0x80000000u|3199u|(1799u<<14);
    for(unsigned i=0;i<4;i++) frame[i]=(uint8_t)(h>>(8*i));
    frame[4]=1; frame[10]=2;
    lgc5_pw_ingest ctx={.decoder=(void *)0x1,.push=push,.ready=ready,.clear=clear};
    assert(lgc5_pw_ingest_frame(&ctx,frame,sizeof(frame),3200,1800));
    assert(ctx.frames==1 && ctx.dropped==0 && pushes==2 && clears==1);
    assert(!lgc5_pw_ingest_frame(&ctx,frame,12,3200,1800));
    assert(ctx.dropped==1 && clears==2);
    puts("PyroWave frame ingest: PASS");
    return 0;
}
