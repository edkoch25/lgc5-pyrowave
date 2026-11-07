#include <dlfcn.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>

typedef void (*version_fn)(uint32_t *, uint32_t *, uint32_t *);
typedef int (*create_fn)(void **);
typedef void (*destroy_fn)(void *);
typedef struct {
    void *device;
    int width;
    int height;
    int chroma;
    _Bool fragment_path;
} decoder_info;
typedef int (*decoder_create_fn)(const decoder_info *, void **);
typedef _Bool (*prefers_fragment_fn)(void *);
typedef void (*decoder_destroy_fn)(void *);

int main(int argc, char **argv) {
    const char *library = argc > 1 ? argv[1] : "./lib/libpyrowave-shared.so.1";
    void *handle = dlopen(library, RTLD_NOW | RTLD_LOCAL);
    if (!handle) {
        fprintf(stderr, "PyroWave dlopen failed: %s\n", dlerror());
        return 1;
    }
    version_fn version = (version_fn)dlsym(handle, "pyrowave_get_api_version");
    create_fn create = (create_fn)dlsym(handle, "pyrowave_create_default_device");
    destroy_fn destroy = (destroy_fn)dlsym(handle, "pyrowave_device_destroy");
    if (!version || !create || !destroy) {
        fprintf(stderr, "PyroWave required C API symbols unavailable\n");
        dlclose(handle);
        return 2;
    }
    uint32_t major = 0, minor = 0, patch = 0;
    version(&major, &minor, &patch);
    printf("PyroWave C API: %u.%u.%u\n", major, minor, patch);
    void *device = NULL;
    int result = create(&device);
    printf("Vulkan device creation: result=%d device=%p\n", result, device);
    if (device && argc >= 4) {
        int width = atoi(argv[2]), height = atoi(argv[3]);
        decoder_create_fn decoder_create = (decoder_create_fn)dlsym(handle, "pyrowave_decoder_create");
        decoder_destroy_fn decoder_destroy = (decoder_destroy_fn)dlsym(handle, "pyrowave_decoder_destroy");
        prefers_fragment_fn prefers_fragment = (prefers_fragment_fn)dlsym(handle, "pyrowave_decoder_device_prefers_fragment_path");
        if (!decoder_create || !decoder_destroy || !prefers_fragment ||
            width < 128 || height < 128 || (width & 1) || (height & 1)) {
            fprintf(stderr, "Invalid dimensions or decoder symbols missing\\n");
            destroy(device);
            dlclose(handle);
            return 4;
        }
        decoder_info info = {device, width, height, 0, prefers_fragment(device)};
        void *decoder = NULL;
        int decoder_result = decoder_create(&info, &decoder);
        printf("PyroWave decoder creation: result=%d decoder=%p fragment_path=%d resolution=%dx%d\\n",
               decoder_result, decoder, info.fragment_path, width, height);
        if (decoder) decoder_destroy(decoder);
        if (decoder_result != 0 || !decoder) {
            destroy(device);
            dlclose(handle);
            return 5;
        }
    }
    if (device) destroy(device);
    dlclose(handle);
    return result == 0 && device ? 0 : 3;
}
