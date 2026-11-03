#include <dlfcn.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>

typedef int32_t VkResult;
typedef uint32_t VkFlags;
typedef struct {
    uint32_t sType;
    const void *pNext;
    VkFlags flags;
    const void *pApplicationInfo;
    uint32_t enabledLayerCount;
    const char *const *ppEnabledLayerNames;
    uint32_t enabledExtensionCount;
    const char *const *ppEnabledExtensionNames;
} VkInstanceCreateInfo;
typedef struct {
    uint32_t sType;
    const void *pNext;
    const char *pApplicationName;
    uint32_t applicationVersion;
    const char *pEngineName;
    uint32_t engineVersion;
    uint32_t apiVersion;
} VkApplicationInfo;
typedef void *(*PFN_vkGetInstanceProcAddr)(void *, const char *);
typedef VkResult (*PFN_vkEnumerateInstanceVersion)(uint32_t *);
typedef VkResult (*PFN_vkCreateInstance)(const VkInstanceCreateInfo *, const void *, void **);
typedef VkResult (*PFN_vkEnumeratePhysicalDevices)(void *, uint32_t *, void *);
typedef void (*PFN_vkDestroyInstance)(void *, const void *);

int main(int argc, char **argv) {
    const char *path = argc > 1 ? argv[1] : "/usr/lib/libmali.so";
    void *lib = dlopen(path, RTLD_NOW | RTLD_LOCAL);
    if (!lib) { fprintf(stderr, "dlopen(%s): %s\n", path, dlerror()); return 1; }
    PFN_vkGetInstanceProcAddr get = (PFN_vkGetInstanceProcAddr)dlsym(lib, "vkGetInstanceProcAddr");
    printf("driver=%s vkGetInstanceProcAddr=%s\n", path, get ? "YES" : "NO");
    if (!get) { dlclose(lib); return 2; }
    PFN_vkEnumerateInstanceVersion version = (PFN_vkEnumerateInstanceVersion)get(NULL, "vkEnumerateInstanceVersion");
    uint32_t api = 0;
    if (version) printf("vkEnumerateInstanceVersion result=%d version=%u.%u.%u\n", version(&api), api >> 22, (api >> 12) & 1023, api & 4095);
    PFN_vkCreateInstance create = (PFN_vkCreateInstance)get(NULL, "vkCreateInstance");
    printf("vkCreateInstance=%s\n", create ? "YES" : "NO");
    if (!create) { dlclose(lib); return 3; }
    VkApplicationInfo app = {0};
    app.sType = 0;
    app.pApplicationName = "lgc5-vulkan-probe";
    app.apiVersion = (1u << 22) | (2u << 12);
    VkInstanceCreateInfo ci = {0};
    ci.sType = 1;
    ci.pApplicationInfo = &app;
    void *instance = NULL;
    VkResult rc = create(&ci, NULL, &instance);
    printf("vkCreateInstance result=%d instance=%p\n", rc, instance);
    if (rc != 0 || !instance) { dlclose(lib); return 4; }
    PFN_vkEnumeratePhysicalDevices enumerate = (PFN_vkEnumeratePhysicalDevices)get(instance, "vkEnumeratePhysicalDevices");
    PFN_vkDestroyInstance destroy = (PFN_vkDestroyInstance)get(instance, "vkDestroyInstance");
    uint32_t count = 0;
    if (enumerate) {
        rc = enumerate(instance, &count, NULL);
        printf("vkEnumeratePhysicalDevices result=%d count=%u\n", rc, count);
    } else printf("vkEnumeratePhysicalDevices=NO\n");
    if (destroy) destroy(instance, NULL);
    dlclose(lib);
    return enumerate && rc == 0 && count > 0 ? 0 : 5;
}
