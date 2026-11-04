/* Aurora PyroWave decoder bridge. The presentation backend is not yet wired.
 * This bridge deliberately never advertises a playable renderer by itself. */
#ifndef LGC5_PYROWAVE_BRIDGE_H
#define LGC5_PYROWAVE_BRIDGE_H
#include "pyrowave_ingest.h"
#include <dlfcn.h>
#include <stdio.h>
typedef struct {
    void *device;
    int width, height, chroma;
    bool fragment_path;
} lgc5_pw_decoder_info;
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
    void (*api_version)(unsigned *, unsigned *, unsigned *);
    /* Presenter must consume a ready frame before it is acknowledged. */
    bool (*present_ready_frame)(void *userdata, void *decoder);
    void *present_userdata;
} lgc5_pw_bridge;
static inline void lgc5_pw_bridge_close(lgc5_pw_bridge *b) {
    if (b->decoder && b->destroy_decoder) b->destroy_decoder(b->decoder);
    if (b->device && b->destroy_device) b->destroy_device(b->device);
    if (b->library) dlclose(b->library);
    memset(b,0,sizeof(*b));
}
static inline bool lgc5_pw_bridge_open(lgc5_pw_bridge *b, unsigned width, unsigned height) {
    if (!b || !width || !height || (width&1u) || (height&1u)) return false;
    memset(b,0,sizeof(*b));
    const char *paths[] = {
        "lib/pyrowave/libpyrowave-shared.so.0.6.0",
        "/media/developer/apps/usr/palm/applications/com.pyrowave.lgc5.test/lib/pyrowave/libpyrowave-shared.so.0.6.0"
    };
    for (unsigned i=0;i<sizeof(paths)/sizeof(paths[0]);++i) {
        b->library=dlopen(paths[i],RTLD_NOW|RTLD_LOCAL);
        if (b->library) break;
    }
    if (!b->library) goto fail;
    *(void **)(&b->api_version)=dlsym(b->library,"pyrowave_get_api_version");
    *(void **)(&b->create_device)=dlsym(b->library,"pyrowave_create_default_device");
    *(void **)(&b->destroy_device)=dlsym(b->library,"pyrowave_device_destroy");
    *(void **)(&b->prefers_fragment)=dlsym(b->library,"pyrowave_decoder_device_prefers_fragment_path");
    *(void **)(&b->create_decoder)=dlsym(b->library,"pyrowave_decoder_create");
    *(void **)(&b->destroy_decoder)=dlsym(b->library,"pyrowave_decoder_destroy");
    *(void **)(&b->clear_decoder)=dlsym(b->library,"pyrowave_decoder_clear");
    *(void **)(&b->ingress.push)=dlsym(b->library,"pyrowave_decoder_push_packet");
    *(void **)(&b->ingress.ready)=dlsym(b->library,"pyrowave_decoder_decode_is_ready");
    if (!b->api_version || !b->create_device || !b->destroy_device || !b->prefers_fragment ||
        !b->create_decoder || !b->destroy_decoder || !b->clear_decoder ||
        !b->ingress.push || !b->ingress.ready) goto fail;
    unsigned major=0,minor=0,patch=0;
    b->api_version(&major,&minor,&patch);
    if (major!=0 || minor!=6) goto fail;
    if (b->create_device(&b->device)!=0 || !b->device) goto fail;
    lgc5_pw_decoder_info info={b->device,(int)width,(int)height,0,b->prefers_fragment(b->device)};
    if (b->create_decoder(&info,&b->decoder)!=0 || !b->decoder) goto fail;
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
    if (!b->present_ready_frame ||
        !b->present_ready_frame(b->present_userdata,b->decoder)) {
        /* A decoded-ready frame is not a displayed frame. */
        b->clear_decoder(b->decoder);
        return false;
    }
    return true;
}
#endif
