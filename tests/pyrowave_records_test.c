#include "../src/pyrowave_records.h"
#include <assert.h>
#include <stdio.h>
int main(void) {
    /* 3200x1800, 8-bit 420, one 8-byte block. */
    uint8_t frame[16] = {0};
    uint32_t header = 0x80000000u | (3199u) | (1799u << 14);
    for (int i=0;i<4;i++) frame[i]=(uint8_t)(header>>(8*i));
    frame[10]=2; /* block length 2 words */
    lgc5_pw_records r;
    assert(lgc5_pw_records_init(&r,frame,sizeof(frame),3200,1800));
    const uint8_t *p; size_t n;
    assert(lgc5_pw_records_next(&r,&p,&n)==1 && p==frame && n==8);
    assert(lgc5_pw_records_next(&r,&p,&n)==1 && p==frame+8 && n==8);
    assert(lgc5_pw_records_next(&r,&p,&n)==0);
    assert(!lgc5_pw_records_init(&r,frame,sizeof(frame),3840,2160));
    frame[10]=4; /* truncated block */
    assert(lgc5_pw_records_init(&r,frame,sizeof(frame),3200,1800));
    assert(lgc5_pw_records_next(&r,&p,&n)==-1);
    puts("PyroWave record iterator: PASS");
    return 0;
}
