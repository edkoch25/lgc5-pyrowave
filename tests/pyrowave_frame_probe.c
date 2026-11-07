#define _POSIX_C_SOURCE 200809L
/* Offline PyroWave 8-bit 4:2:0 decoder test.
 * Accepts a complete Vibepollo record-framed frame; never alters TV services.
 * Usage: pyrowave_frame_probe <width> <height> <frame.bin> <libpyrowave.so> [output.yuv] [auto|fragment|compute]
 */
#include <dlfcn.h>
#include "../src/pyrowave_records.h"
#include <stdint.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
typedef struct {void *device; int width,height,chroma; bool fragment_path;} DecoderInfo;
typedef struct {void *data[3]; size_t stride[3], plane_size[3]; int width,height,format;} CpuBuffer;
int main(int argc, char **argv) {
    if (argc < 5 || argc > 7) {fprintf(stderr,"usage: %s width height frame.bin libpyrowave.so [output.yuv] [auto|fragment|compute]\n",argv[0]);return 2;}
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
    if(!lib){fprintf(stderr,"dlopen: %s\n",dlerror());free(data);return 3;}
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
        fprintf(stderr,"missing PyroWave API\n");dlclose(lib);free(data);return 3;
    }
    /* The webOS Mali ICD does not expose the standard Vulkan loader symbol.
     * Initialize the same explicit loader path used by the test application. */
    void *mali=dlopen("/usr/lib/libmali.so",RTLD_NOW|RTLD_LOCAL);
    int (*init_loader)(void*)=dlsym(lib,"pyrowave_lgc5_init_vulkan_loader");
    if(mali && init_loader) {
        void *icd=dlsym(mali,"vk_icdGetInstanceProcAddr");
        if(!icd || !init_loader(icd)) {
            fprintf(stderr,"Mali Vulkan ICD initialization failed\\n");
            dlclose(lib);free(data);return 4;
        }
    }
    /* Override precision without changing the live test app. */
    const char *precision=getenv("LGC5_PW_PROBE_PRECISION");
    if(!precision || !*precision) precision="2";
    if(strcmp(precision,"1") && strcmp(precision,"2")){
        fprintf(stderr,"LGC5_PW_PROBE_PRECISION must be 1 or 2\\n");
        if(mali)dlclose(mali);dlclose(lib);free(data);return 2;
    }
    setenv("PYROWAVE_PRECISION",precision,1);
    const char *packet_mode=getenv("LGC5_PW_PROBE_PACKET_MODE");
    bool combined=packet_mode && strcmp(packet_mode,"combined")==0;
    if(packet_mode && *packet_mode && !combined && strcmp(packet_mode,"records")){
        fprintf(stderr,"LGC5_PW_PROBE_PACKET_MODE must be records or combined\\n");
        if(mali)dlclose(mali);dlclose(lib);free(data);return 2;
    }
    fprintf(stderr,"Probe settings: precision=%s packet_mode=%s\\n",precision,combined?"combined":"records");
    void *device=NULL,*decoder=NULL;
    int rc=make_device(&device);
    if(rc||!device){fprintf(stderr,"device rc=%d\n",rc);dlclose(lib);free(data);return 4;}
    bool preferred_fragment=fragment(device);
    bool selected_fragment=preferred_fragment;
    if(argc==7){
        if(strcmp(argv[6],"fragment")==0) selected_fragment=true;
        else if(strcmp(argv[6],"compute")==0) selected_fragment=false;
        else if(strcmp(argv[6],"auto")!=0){fprintf(stderr,"invalid decoder path: %s\n",argv[6]);drop_device(device);dlclose(lib);free(data);return 2;}
    }
    fprintf(stderr,"Decoder path: preferred=%s selected=%s\n",preferred_fragment?"fragment":"compute",selected_fragment?"fragment":"compute");
    DecoderInfo info={device,w,h,0,selected_fragment};
    rc=make_decoder(&info,&decoder);
    if(rc||!decoder){fprintf(stderr,"decoder rc=%d\n",rc);drop_device(device);dlclose(lib);free(data);return 5;}
    clear(decoder);
    lgc5_pw_records records;
    bool valid=lgc5_pw_records_init(&records,data,(size_t)n,(unsigned)w,(unsigned)h);
    const uint8_t *record=NULL;
    size_t record_len=0;
    int state=0;
    unsigned packets=0;
    unsigned char *joined=combined?malloc((size_t)n):NULL;
    size_t joined_size=0;
    if(combined && !joined)valid=false;
    while(valid && (state=lgc5_pw_records_next(&records,&record,&record_len))==1) {
        if(combined){
            if(record_len>(size_t)n-joined_size){valid=false;break;}
            memcpy(joined+joined_size,record,record_len);
            joined_size+=record_len;
        }else{
            rc=push(decoder,record,record_len);
            if(rc){fprintf(stderr,"push rc=%d packet=%u\\n",rc,packets);valid=false;break;}
            packets++;
        }
    }
    if(state!=0)valid=false;
    if(valid && combined){
        rc=push(decoder,joined,joined_size);
        if(rc){fprintf(stderr,"combined push rc=%d size=%zu\\n",rc,joined_size);valid=false;}
        else packets=1;
    }
    free(joined);
    if(valid && ready(decoder,false)){
        size_t y=(size_t)w*h,c=y/4;
        unsigned char *planes=calloc(1,y+2*c);
        if(planes){
            CpuBuffer output={{planes,planes+y,planes+y+c},{(size_t)w,(size_t)w/2,(size_t)w/2},{y,c,c},w,h,1};
            rc=decode(decoder,&output);
            printf("PyroWave replay: rc=%d packets=%u blocks=%u %dx%d fragment=%d\\n",
                   rc,packets,records.blocks,w,h,info.fragment_path);
            if(rc==0 && argc>=6){
                FILE *out=fopen(argv[5],"wb");
                if(!out){perror("output");rc=-4;}
                else {
                    size_t wrote=fwrite(planes,1,y+2*c,out);
                    if(fclose(out)!=0 || wrote!=y+2*c){fprintf(stderr,"output write failed\\n");rc=-5;}
                    else printf("Saved decoded YUV420P: %s (%zu bytes)\\n",argv[5],wrote);
                }
            }
            free(planes);
        }else rc=-3;
    }else{
        fprintf(stderr,"Invalid or incomplete PyroWave record frame (blocks=%u packets=%u)\\n",
                records.blocks,packets);rc=-2;
    }
    drop_decoder(decoder);drop_device(device);if(mali)dlclose(mali);dlclose(lib);free(data);
    return rc==0?0:6;
}
