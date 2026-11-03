/* Offline PyroWave 8-bit 4:2:0 decoder test.
 * Accepts a complete Vibepollo record-framed frame; never alters TV services.
 * Usage: pyrowave_frame_probe <width> <height> <frame.bin> <libpyrowave.so>
 */
#include <dlfcn.h>
#include <stdint.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
typedef struct {void *device; int width,height,chroma; bool fragment_path;} DecoderInfo;
typedef struct {void *data[3]; size_t stride[3], plane_size[3]; int width,height,format;} CpuBuffer;
static uint32_t le32(const unsigned char *p) {
    return (uint32_t)p[0] | ((uint32_t)p[1]<<8) | ((uint32_t)p[2]<<16) | ((uint32_t)p[3]<<24);
}
int main(int argc, char **argv) {
    if (argc != 5) {fprintf(stderr,"usage: %s width height frame.bin libpyrowave.so\\n",argv[0]);return 2;}
    int w=atoi(argv[1]),h=atoi(argv[2]);
    if(w<128||h<128||w>4096||h>4096||(w&1)||(h&1)) return 2;
    FILE *fp=fopen(argv[3],"rb");
    if(!fp){perror("frame");return 2;}
    if(fseek(fp,0,SEEK_END)!=0){fclose(fp);return 2;}
    long n=ftell(fp);
    if(n<8||n>32*1024*1024||fseek(fp,0,SEEK_SET)!=0){fclose(fp);return 2;}
    unsigned char *data=malloc((size_t)n);
    if(!data){fclose(fp);return 2;}
    if(fread(data,1,(size_t)n,fp)!=(size_t)n){free(data);fclose(fp);return 2;}
    fclose(fp);
    void *lib=dlopen(argv[4],RTLD_NOW|RTLD_LOCAL);
    if(!lib){fprintf(stderr,"dlopen: %s\\n",dlerror());free(data);return 3;}
    int (*make_device)(void**)=dlsym(lib,"pyrowave_create_default_device");
    void (*drop_device)(void*)=dlsym(lib,"pyrowave_device_destroy");
    int (*make_decoder)(const DecoderInfo*,void**)=dlsym(lib,"pyrowave_decoder_create");
    void (*drop_decoder)(void*)=dlsym(lib,"pyrowave_decoder_destroy");
    void (*clear)(void*)=dlsym(lib,"pyrowave_decoder_clear");
    int (*push)(void*,const void*,size_t)=dlsym(lib,"pyrowave_decoder_push_packet");
    bool (*ready)(void*,bool)=dlsym(lib,"pyrowave_decoder_decode_is_ready");
    int (*decode)(void*,const CpuBuffer*)=dlsym(lib,"pyrowave_decoder_decode_cpu_buffer_synchronous");
    bool (*fragment)(void*)=dlsym(lib,"pyrowave_decoder_device_prefers_fragment_path");
    if(!make_device||!drop_device||!make_decoder||!drop_decoder||!clear||!push||!ready||!decode||!fragment){
        fprintf(stderr,"missing PyroWave API\\n");dlclose(lib);free(data);return 3;
    }
    void *device=NULL,*decoder=NULL;
    int rc=make_device(&device);
    if(rc||!device){fprintf(stderr,"device rc=%d\\n",rc);dlclose(lib);free(data);return 4;}
    DecoderInfo info={device,w,h,0,fragment(device)};
    rc=make_decoder(&info,&decoder);
    if(rc||!decoder){fprintf(stderr,"decoder rc=%d\\n",rc);drop_device(device);dlclose(lib);free(data);return 5;}
    clear(decoder);
    size_t at=0;
    unsigned blocks=0;
    bool valid=true;
    /* The first 8 bytes must be the extended sequence header. */
    uint32_t head=le32(data),seq=le32(data+4);
    if(!(head&0x80000000u)||((head&0x3fff)+1)!=(uint32_t)w||
       (((head>>14)&0x3fff)+1)!=(uint32_t)h||((seq>>24)&3)!=0||((seq>>26)&1)!=0)
        valid=false;
    if(valid && push(decoder,data,8)!=0) valid=false;\n    while(valid&&at<(size_t)n) {
        if((size_t)n-at<8){valid=false;break;}
        uint32_t a=le32(data+at),b=le32(data+at+4);
        if(a==0xffffffffu){
            size_t pad=(size_t)b*4+8;
            if(pad>(size_t)n-at){valid=false;break;}
            at+=pad;
            continue;
        }
        if(a&0x80000000u){
            if(at!=0){valid=false;break;}
            at+=8;continue;
        }
        size_t bytes=(size_t)((a>>16)&0xfffu)*4;
        if(bytes<8||bytes>(size_t)n-at||((a>>28)&7)!=((head>>28)&7)){valid=false;break;}
        rc=push(decoder,data+at,bytes);
        if(rc){fprintf(stderr,"push rc=%d block=%u\\n",rc,blocks);valid=false;break;}
        blocks++;at+=bytes;
    }
    if(valid && blocks>0 && ready(decoder,false)){
        size_t y=(size_t)w*h,c=y/4;
        unsigned char *planes=malloc(y+2*c);
        if(planes){
            CpuBuffer output={{planes,planes+y,planes+y+c},{(size_t)w,(size_t)w/2,(size_t)w/2},{y,c,c},w,h,1};
            rc=decode(decoder,&output);
            printf("PyroWave CPU-output decode: rc=%d blocks=%u %dx%d fragment=%d\\n",rc,blocks,w,h,info.fragment_path);
            free(planes);
        }else rc=-3;
    }else{
        fprintf(stderr,"Invalid or incomplete PyroWave record frame (blocks=%u)\\n",blocks);rc=-2;
    }
    drop_decoder(decoder);drop_device(device);dlclose(lib);free(data);
    return rc==0?0:6;
}
