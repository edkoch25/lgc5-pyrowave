#include "../src/pyrowave_records.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>
static void put32(uint8_t *p,uint32_t v) {
    p[0]=(uint8_t)v; p[1]=(uint8_t)(v>>8); p[2]=(uint8_t)(v>>16); p[3]=(uint8_t)(v>>24);
}
static void base_frame(uint8_t *f) {
    memset(f,0,32);
    put32(f,0x80000000u|3199u|(1799u<<14));
    put32(f+4,1u);
    put32(f+8,2u<<16);
}
int main(void) {
    uint8_t f[32]; const uint8_t *p; size_t n; lgc5_pw_records r;
    base_frame(f);
    assert(lgc5_pw_records_init(&r,f,16,3200,1800));
    assert(lgc5_pw_records_next(&r,&p,&n)==1 && p==f && n==8);
    assert(lgc5_pw_records_next(&r,&p,&n)==1 && p==f+8 && n==8);
    assert(lgc5_pw_records_next(&r,&p,&n)==0);
    assert(lgc5_pw_records_init(&r,f,16,3840,2160));
    assert(lgc5_pw_records_next(&r,&p,&n)==-1);
    base_frame(f); put32(f+8,4u<<16);
    assert(lgc5_pw_records_init(&r,f,16,3200,1800));
    assert(lgc5_pw_records_next(&r,&p,&n)==1);
    assert(lgc5_pw_records_next(&r,&p,&n)==-1);
    base_frame(f); put32(f+8,1u<<16);
    assert(lgc5_pw_records_init(&r,f,16,3200,1800));
    assert(lgc5_pw_records_next(&r,&p,&n)==1);
    assert(lgc5_pw_records_next(&r,&p,&n)==-1);
    base_frame(f); put32(f+8,(1u<<28)|(2u<<16));
    assert(lgc5_pw_records_init(&r,f,16,3200,1800));
    assert(lgc5_pw_records_next(&r,&p,&n)==1);
    assert(lgc5_pw_records_next(&r,&p,&n)==-1);
    base_frame(f); memcpy(f+8,f,8);
    assert(lgc5_pw_records_init(&r,f,24,3200,1800));
    assert(lgc5_pw_records_next(&r,&p,&n)==1);
    assert(lgc5_pw_records_next(&r,&p,&n)==-1);
    base_frame(f); put32(f,2u<<16); put32(f+4,0);
    put32(f+8,0x80000000u|3199u|(1799u<<14)); put32(f+12,1);
    assert(lgc5_pw_records_init(&r,f,16,3200,1800));
    assert(lgc5_pw_records_next(&r,&p,&n)==-1);
    base_frame(f); put32(f+4,2u);
    assert(lgc5_pw_records_init(&r,f,16,3200,1800));
    assert(lgc5_pw_records_next(&r,&p,&n)==1);
    assert(lgc5_pw_records_next(&r,&p,&n)==1);
    assert(lgc5_pw_records_next(&r,&p,&n)==-1);
    base_frame(f); put32(f+8,0xffffffffu); put32(f+12,1); put32(f+16,0);
    put32(f+20,2u<<16);
    assert(lgc5_pw_records_init(&r,f,28,3200,1800));
    assert(lgc5_pw_records_next(&r,&p,&n)==1);
    assert(lgc5_pw_records_next(&r,&p,&n)==1 && p==f+20 && n==8);
    assert(lgc5_pw_records_next(&r,&p,&n)==0);
    puts("PyroWave record parser: PASS");
    return 0;
}
