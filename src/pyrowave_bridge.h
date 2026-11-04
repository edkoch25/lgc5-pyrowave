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
    void *library, *device, *decoder;
    int (*create_device)(void **);
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
    memset(b,0,sizeof(*b));
}
static inline bool lgc5_pw_bridge_open(lgc5_pw_bridge *b, unsigned width, unsigned height) {
    if (!b || !width || !height || (width&1u) || (height&1u)) return false;
    memset(b,0,sizeof(*b));
    const char *paths[] = {
        "lib/pyrowave/libpyrowave-shared.so.0",
        "lib/pyrowave/libpyrowave-shared.so.0.6.0",
        "/media/developer/apps/usr/palm/applications/com.pyrowave.lgc5.test/lib/pyrowave/libpyrowave-shared.so.0",
        "/media/developer/apps/usr/palm/applications/com.pyrowave.lgc5.test/lib/pyrowave/libpyrowave-shared.so.0.6.0"
    };
    for (unsigned i=0;i<sizeof(paths)/sizeof(paths[0]);++i) {
        b->library=dlopen(paths[i],RTLD_NOW|RTLD_LOCAL);
        if (b->library) break;
    }
    if (!b->library) { lgc5_pw_diag("dlopen failed: %s",dlerror()); goto fail; }
    lgc5_pw_diag("decoder library loaded");
    *(void **)(&b->api_version)=dlsym(b->library,"pyrowave_get_api_version");
    *(void **)(&b->create_device)=dlsym(b->library,"pyrowave_create_default_device");
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
    lgc5_pw_diag("Vulkan device init begin");
    if (b->create_device(&b->device)!=0 || !b->device) { lgc5_pw_diag("Vulkan device init failed"); goto fail; }
    lgc5_pw_diag("Vulkan device ready");
    lgc5_pw_decoder_info info={b->device,(int)width,(int)height,0,b->prefers_fragment(b->device)};
    if (b->create_decoder(&info,&b->decoder)!=0 || !b->decoder) { lgc5_pw_diag("decoder create failed"); goto fail; }
    lgc5_pw_diag("decoder ready %ux%u",width,height);
    b->ingress.decoder=b->decoder;
    b->width=width; b->height=height;
    b->clear_decoder(b->decoder);
    return true;
fail:
    lgc5_pw_bridge_close(b);
    return false;
}
static inline bool lgc5_pw_bridge_submit(lgc5_pw_bridge *b, const void *frame, size_t length) {
    if (!b || !b->decoder) return false;
    if (!lgc5_pw_ingest_frame(&b->ingress,frame,length,b->width,b->height)) {
        b->clear_decoder(b->decoder);
        return false;
    }
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
        if (b->decode_cpu(b->decoder,&output)==0)
            presented=b->present_cpu_frame(b->present_userdata,&output);
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
