/* Aurora PyroWave decoder bridge. The presentation backend is not yet wired.
 * This bridge deliberately never advertises a playable renderer by itself. */
#ifndef LGC5_PYROWAVE_BRIDGE_H
#define LGC5_PYROWAVE_BRIDGE_H
#include "pyrowave_ingest.h"
#include "pyrowave_diag.h"
#include <dlfcn.h>
#include <stdio.h>
#include <stdint.h>
#include <stdbool.h>
#include <string.h>
#include <stdlib.h>
typedef struct {
    void *device;
    int width, height, chroma;
    bool fragment_path;
} lgc5_pw_decoder_info;
/* Mirrors pyrowave_cpu_buffer (0.6 ABI), with 8-bit planar YUV420. */
typedef struct lgc5_pw_cpu_buffer {
    void *data[3];
    size_t row_stride_in_bytes[3];
    size_t plane_size_in_bytes[3];
    int width, height, format;
} lgc5_pw_cpu_buffer;
typedef struct {
    void *library, *cpp_runtime, *device, *decoder;
    int (*create_device)(void **);
    int (*init_vulkan_loader)(void *);
    void (*destroy_device)(void *);
    bool (*prefers_fragment)(void *);
    int (*create_decoder)(const lgc5_pw_decoder_info *, void **);
    void (*destroy_decoder)(void *);
    void (*clear_decoder)(void *);
    lgc5_pw_ingest ingress;
    unsigned width, height;
    void (*api_version)(uint32_t *, uint32_t *, uint32_t *);
    int (*decode_cpu)(void *, const struct lgc5_pw_cpu_buffer *);
    /* Presenter must consume a ready frame before it is acknowledged. */
    bool (*present_ready_frame)(void *userdata, void *decoder);
    void *present_userdata;
    /* Optional bring-up path: consumes decoded YUV420 before returning. */
    bool (*present_cpu_frame)(void *, const lgc5_pw_cpu_buffer *);
    unsigned char *cpu_planes;
    size_t cpu_bytes;
} lgc5_pw_bridge;
static inline void lgc5_pw_bridge_close(lgc5_pw_bridge *b) {
    free(b->cpu_planes);
    if (b->decoder && b->destroy_decoder) b->destroy_decoder(b->decoder);
    if (b->device && b->destroy_device) b->destroy_device(b->device);
    if (b->library) dlclose(b->library);
    if (b->cpp_runtime) dlclose(b->cpp_runtime);
    memset(b,0,sizeof(*b));
}
static inline bool lgc5_pw_bridge_open(lgc5_pw_bridge *b, unsigned width, unsigned height) {
    if (!b || !width || !height || (width&1u) || (height&1u)) return false;
    memset(b,0,sizeof(*b));
    /* Load the bundled C++ runtime first: the TV system copy may be older.
     * Keep its handle alive until after the decoder is unloaded. */
    const char *cpp_path="/media/developer/apps/usr/palm/applications/com.pyrowave.lgc5.test/lib/pyrowave/libstdc++.so.6";
    dlerror();
    b->cpp_runtime=dlopen(cpp_path,RTLD_NOW|RTLD_GLOBAL);
    if (b->cpp_runtime) {
        lgc5_pw_diag("C++ runtime loaded: %s",cpp_path);
    } else {
        const char *err=dlerror();
        lgc5_pw_diag("C++ runtime load failed: %s",err ? err : "unknown");
    }
    const char *paths[] = {
        "/media/developer/apps/usr/palm/applications/com.pyrowave.lgc5.test/lib/pyrowave/libpyrowave-shared.so.0",
        "/media/developer/apps/usr/palm/applications/com.pyrowave.lgc5.test/lib/pyrowave/libpyrowave-shared.so.0.6.0",
        "lib/pyrowave/libpyrowave-shared.so.0",
        "lib/pyrowave/libpyrowave-shared.so.0.6.0"
    };
    for (unsigned i=0;i<sizeof(paths)/sizeof(paths[0]);++i) {
        dlerror();
        b->library=dlopen(paths[i],RTLD_NOW|RTLD_LOCAL);
        if (b->library) {
            lgc5_pw_diag("decoder dlopen succeeded: %s",paths[i]);
            break;
        }
        const char *err=dlerror();
        lgc5_pw_diag("decoder dlopen attempt %u (%s) failed: %s",
                     i,paths[i],err ? err : "unknown");
    }
    if (!b->library) {
        lgc5_pw_diag("decoder load exhausted all paths");
        goto fail;
    }
    *(void **)(&b->api_version)=dlsym(b->library,"pyrowave_get_api_version");
    *(void **)(&b->create_device)=dlsym(b->library,"pyrowave_create_default_device");
    *(void **)(&b->init_vulkan_loader)=dlsym(b->library,"pyrowave_lgc5_init_vulkan_loader");
    *(void **)(&b->destroy_device)=dlsym(b->library,"pyrowave_device_destroy");
    *(void **)(&b->prefers_fragment)=dlsym(b->library,"pyrowave_decoder_device_prefers_fragment_path");
    *(void **)(&b->create_decoder)=dlsym(b->library,"pyrowave_decoder_create");
    *(void **)(&b->destroy_decoder)=dlsym(b->library,"pyrowave_decoder_destroy");
    *(void **)(&b->clear_decoder)=dlsym(b->library,"pyrowave_decoder_clear");
    *(void **)(&b->ingress.push)=dlsym(b->library,"pyrowave_decoder_push_packet");
    *(void **)(&b->ingress.ready)=dlsym(b->library,"pyrowave_decoder_decode_is_ready");
    *(void **)(&b->decode_cpu)=dlsym(b->library,"pyrowave_decoder_decode_cpu_buffer_synchronous");
    lgc5_pw_diag("decoder symbols: %s",(b->api_version && b->create_device && b->create_decoder && b->decode_cpu) ? "core present" : "core missing");
    if (!b->api_version || !b->create_device || !b->destroy_device || !b->prefers_fragment ||
        !b->create_decoder || !b->destroy_decoder || !b->clear_decoder ||
        !b->ingress.push || !b->ingress.ready || !b->decode_cpu) goto fail;
    unsigned major=0,minor=0,patch=0;
    b->api_version(&major,&minor,&patch);
    lgc5_pw_diag("decoder ABI %u.%u.%u",major,minor,patch);
    if (major!=0 || minor!=6) goto fail;
    /* Granite's supported external loader entrypoint avoids libvulkan.so discovery. */
    void *mali_loader=dlopen("/usr/lib/libmali.so",RTLD_NOW|RTLD_LOCAL);
    if (!mali_loader) {
        const char *err=dlerror();
        lgc5_pw_diag("Mali external loader open failed: %s",err ? err : "unknown");
        goto fail;
    }
    void *icd_proc=dlsym(mali_loader,"vk_icdGetInstanceProcAddr");
    lgc5_pw_diag("Mali external loader entrypoint=%d bridge_api=%d",!!icd_proc,!!b->init_vulkan_loader);
    if (!icd_proc || !b->init_vulkan_loader || !b->init_vulkan_loader(icd_proc)) {
        lgc5_pw_diag("Mali external loader initialization failed");
        dlclose(mali_loader);
        goto fail;
    }
    lgc5_pw_diag("Mali external loader initialized");
    /* Keep the Mali driver loaded for the lifetime of Granite's global function pointers. */
    /* In-process Mali ICD probe: distinguishes driver/loader failure from Granite setup.
     * Uses the same direct ICD entrypoint validated by the standalone TV probe. */
    {
        void *mali=dlopen("/usr/lib/libmali.so",RTLD_NOW|RTLD_LOCAL);
        if (!mali) {
            const char *err=dlerror();
            lgc5_pw_diag("Mali ICD dlopen failed: %s",err ? err : "unknown");
        } else {
            typedef void *(*lgc5_vk_gipa)(void *,const char *);
            typedef int32_t (*lgc5_vk_create)(const void *,const void *,void **);
            typedef int32_t (*lgc5_vk_enumerate)(void *,uint32_t *,void *);
            typedef void (*lgc5_vk_destroy)(void *,const void *);
            typedef int32_t (*lgc5_vk_negotiate)(uint32_t *);
            lgc5_vk_gipa get=(lgc5_vk_gipa)dlsym(mali,"vkGetInstanceProcAddr");
            lgc5_vk_gipa icd=(lgc5_vk_gipa)dlsym(mali,"vk_icdGetInstanceProcAddr");
            lgc5_vk_negotiate negotiate=(lgc5_vk_negotiate)dlsym(mali,"vk_icdNegotiateLoaderICDInterfaceVersion");
            lgc5_pw_diag("Mali ICD symbols standard=%d icd=%d negotiate=%d",!!get,!!icd,!!negotiate);
            if (negotiate) {
                uint32_t v=5;
                int32_t rc=negotiate(&v);
                lgc5_pw_diag("Mali ICD negotiation result=%d version=%u",(int)rc,v);
            }
            if (!get) get=icd;
            if (get) {
                /* VkApplicationInfo and VkInstanceCreateInfo ABI-compatible minimal layouts. */
                struct lgc5_vk_app { uint32_t type; const void *next; const char *name; uint32_t version; const char *engine; uint32_t engine_version; uint32_t api; };
                struct lgc5_vk_info { uint32_t type; const void *next; uint32_t flags; const void *app; uint32_t layers; const char *const *layer_names; uint32_t extensions; const char *const *extension_names; };
                struct lgc5_vk_app app={0,NULL,"lgc5-pyrowave",0,NULL,0,(1u<<22)|(2u<<12)};
                struct lgc5_vk_info info={1,NULL,0,&app,0,NULL,0,NULL};
                lgc5_vk_create create=(lgc5_vk_create)get(NULL,"vkCreateInstance");
                lgc5_pw_diag("Mali ICD vkCreateInstance symbol=%d",!!create);
                if (create) {
                    void *instance=NULL;
                    int32_t rc=create(&info,NULL,&instance);
                    lgc5_pw_diag("Mali ICD vkCreateInstance result=%d instance=%d",(int)rc,!!instance);
                    if (rc==0 && instance) {
                        lgc5_vk_enumerate enumerate=(lgc5_vk_enumerate)get(instance,"vkEnumeratePhysicalDevices");
                        lgc5_vk_destroy destroy=(lgc5_vk_destroy)get(instance,"vkDestroyInstance");
                        if (enumerate) {
                            uint32_t count=0;
                            rc=enumerate(instance,&count,NULL);
                            lgc5_pw_diag("Mali ICD physical devices result=%d count=%u",(int)rc,count);
                        } else lgc5_pw_diag("Mali ICD enumerate symbol missing");
                        if (destroy) destroy(instance,NULL);
                    }
                }
            }
            dlclose(mali);
        }
    }
    lgc5_pw_diag("Vulkan device init begin");
    int device_result=b->create_device(&b->device);
    lgc5_pw_diag("Vulkan device init result=%d device=%d",device_result,!!b->device);
    if (device_result!=0 || !b->device) { lgc5_pw_diag("Vulkan device init failed"); goto fail; }
    lgc5_pw_diag("Vulkan device ready");
    lgc5_pw_decoder_info info={b->device,(int)width,(int)height,0,b->prefers_fragment(b->device)};
    if (b->create_decoder(&info,&b->decoder)!=0 || !b->decoder) { lgc5_pw_diag("decoder create failed"); goto fail; }
    lgc5_pw_diag("decoder ready %ux%u",width,height);
    b->ingress.decoder=b->decoder;
    b->ingress.clear=b->clear_decoder;
    b->width=width; b->height=height;
    lgc5_pw_diag("decoder clear begin");
    b->clear_decoder(b->decoder);
    lgc5_pw_diag("decoder clear complete");
    return true;
fail:
    lgc5_pw_bridge_close(b);
    return false;
}
static inline bool lgc5_pw_bridge_submit(lgc5_pw_bridge *b, const void *frame, size_t length) {
    if (!b || !b->decoder) return false;
    static unsigned long pw_submits=0, pw_decoded=0, pw_presented=0;
    ++pw_submits;
    if (!lgc5_pw_ingest_frame(&b->ingress,frame,length,b->width,b->height)) {
        if (pw_submits<=3 || pw_submits%120==0)
            lgc5_pw_diag("ingest failed count=%lu stage=%d rc=%d blocks=%u accepted=%u dropped=%u",
                pw_submits,b->ingress.last_stage,b->ingress.last_result,b->ingress.last_blocks,b->ingress.frames,b->ingress.dropped);
        b->clear_decoder(b->decoder);
        return false;
    }
    if (pw_submits<=3 || pw_submits%120==0)
        lgc5_pw_diag("ingest ready count=%lu blocks=%u accepted=%u dropped=%u",
            pw_submits,b->ingress.last_blocks,b->ingress.frames,b->ingress.dropped);
    bool presented=false;
    if (b->present_ready_frame) {
        presented=b->present_ready_frame(b->present_userdata,b->decoder);
    } else if (b->present_cpu_frame) {
        const size_t y=(size_t)b->width*b->height;
        if (y > SIZE_MAX / 3) goto not_presented;
        const size_t bytes=y+y/2;
        if (!b->cpu_planes) {
            b->cpu_planes=malloc(bytes);
            if (!b->cpu_planes) goto not_presented;
            b->cpu_bytes=bytes;
        }
        lgc5_pw_cpu_buffer output={0};
        output.data[0]=b->cpu_planes;
        output.data[1]=b->cpu_planes+y;
        output.data[2]=b->cpu_planes+y+y/4;
        output.row_stride_in_bytes[0]=b->width;
        output.row_stride_in_bytes[1]=b->width/2;
        output.row_stride_in_bytes[2]=b->width/2;
        output.plane_size_in_bytes[0]=y;
        output.plane_size_in_bytes[1]=y/4;
        output.plane_size_in_bytes[2]=y/4;
        output.width=(int)b->width;
        output.height=(int)b->height;
        output.format=1; /* PYROWAVE_CPU_BUFFER_FORMAT_YUV420P */
        int decode_rc=b->decode_cpu(b->decoder,&output);
        if (decode_rc==0) {
            /* Capture one untouched decoded frame before SDL sees it.
             * The file is raw, tightly packed YUV420P: Y, U, then V. */
            static bool capture_attempted=false;
            if (!capture_attempted && pw_submits>=120) {
                capture_attempted=true;
                const char *path="/media/developer/apps/usr/palm/applications/com.pyrowave.lgc5.test/cache/pyrowave-frame-1920x1080.yuv";
                FILE *capture=fopen(path,"wb");
                if (capture) {
                    size_t written=fwrite(b->cpu_planes,1,bytes,capture);
                    int close_rc=fclose(capture);
                    lgc5_pw_diag("YUV capture path=%s bytes=%zu expected=%zu close_rc=%d",
                                 path,written,bytes,close_rc);
                } else {
                    lgc5_pw_diag("YUV capture failed: %s",path);
                }
            }
            /* Verify that GPU readback actually changes after decoding.
             * The 8-bit Y/U/V samples are read before SDL can modify them. */
            if (pw_submits<=3 || pw_submits%120==0) {
                const unsigned char *base=b->cpu_planes;
                size_t plane_offset[3]={0,y,y+y/4};
                size_t plane_len[3]={y,y/4,y/4};
                for (unsigned k=0;k<3;++k) {
                    unsigned long hash=2166136261ul;
                    unsigned minv=255,maxv=0;
                    unsigned long sum=0;
                    for (unsigned n=0;n<256;++n) {
                        unsigned char v=base[plane_offset[k]+(plane_len[k]-1)*n/255];
                        if(v<minv)minv=v;
                        if(v>maxv)maxv=v;
                        sum+=v; hash=(hash ^ v)*16777619ul;
                    }
                    lgc5_pw_diag("readback plane=%c frame=%lu min=%u max=%u mean=%lu hash=%08lx",
                        k==0?'Y':k==1?'U':'V',pw_submits,minv,maxv,sum/256,hash & 0xfffffffful);
                }
            }
            ++pw_decoded;
            presented=b->present_cpu_frame(b->present_userdata,&output);
        }
        if (presented) ++pw_presented;
        if (pw_submits<=3 || pw_submits%120==0)
            lgc5_pw_diag("cpu decode count=%lu rc=%d decoded=%lu presented=%lu",
                pw_submits,decode_rc,pw_decoded,pw_presented);
    }
not_presented:
    if (!presented) {
        /* A decoded-ready frame is not a displayed frame. */
        b->clear_decoder(b->decoder);
        return false;
    }
    return true;
}
#endif
