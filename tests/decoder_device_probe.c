#include <dlfcn.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>

typedef void (*version_fn)(uint32_t *, uint32_t *, uint32_t *);
typedef int (*create_fn)(void **);
typedef void (*destroy_fn)(void *);

int main(int argc, char **argv) {
    const char *library = argc > 1 ? argv[1] : "./libpyrowave-shared.so.0";
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
    if (device) destroy(device);
    dlclose(handle);
    return result == 0 && device ? 0 : 3;
}
