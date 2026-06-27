#include <dlfcn.h>
#include <pthread.h>
#include "hook.h"
#include "../common/log.h"

#define RENDER_SDL "SDL_RenderPresent"
#define RENDER_EGL "eglSwapBuffers"
#define RENDER_GL  "SDL_GL_SwapWindow"

#define RENDER_VK_GIPA        "vkGetInstanceProcAddr"
#define RENDER_VK_GDPA        "vkGetDeviceProcAddr"
#define RENDER_VK_PRESENT     "vkQueuePresentKHR"
#define RENDER_VK_CREATE_DEV  "vkCreateDevice"
#define RENDER_VK_DESTROY_DEV "vkDestroyDevice"
#define RENDER_VK_CREATE_SC   "vkCreateSwapchainKHR"
#define RENDER_VK_DESTROY_SC  "vkDestroySwapchainKHR"

void (*real_sdl_render_present)(SDL_Renderer *) = NULL;

EGLBoolean (*real_egl_swap_buffers)(EGLDisplay, EGLSurface) = NULL;

void (*real_sdl_gl_swap_window)(SDL_Window *) = NULL;

PFN_vkGetInstanceProcAddr real_vk_get_instance_proc_addr = NULL;

PFN_vkGetDeviceProcAddr real_vk_get_device_proc_addr = NULL;

PFN_vkQueuePresentKHR real_vk_queue_present_khr = NULL;

PFN_vkCreateDevice real_vk_create_device = NULL;

PFN_vkDestroyDevice real_vk_destroy_device = NULL;

PFN_vkCreateSwapchainKHR real_vk_create_swapchain_khr = NULL;

PFN_vkDestroySwapchainKHR real_vk_destroy_swapchain_khr = NULL;

__attribute__((constructor)) static void resolve_symbols(void) {
    real_sdl_render_present = dlsym(RTLD_NEXT, RENDER_SDL);
    real_egl_swap_buffers = dlsym(RTLD_NEXT, RENDER_EGL);
    real_sdl_gl_swap_window = dlsym(RTLD_NEXT, RENDER_GL);

    if (!real_sdl_render_present) LOG_ERROR("stage", "Failed to hook to renderer: " RENDER_SDL);
    if (!real_egl_swap_buffers) LOG_WARN("stage", "Failed to hook to renderer: " RENDER_EGL);
    if (!real_sdl_gl_swap_window) LOG_ERROR("stage", "Failed to hook to renderer: " RENDER_GL);
}

static pthread_once_t vulkan_resolve_once = PTHREAD_ONCE_INIT;
static int vulkan_resolved_ok = 0;

extern VKAPI_ATTR PFN_vkVoidFunction VKAPI_CALL vkGetInstanceProcAddr(VkInstance, const char *);

extern VKAPI_ATTR PFN_vkVoidFunction VKAPI_CALL vkGetDeviceProcAddr(VkDevice, const char *);

extern VkResult vkQueuePresentKHR(VkQueue, const VkPresentInfoKHR *);

extern VkResult vkCreateDevice(VkPhysicalDevice, const VkDeviceCreateInfo *, const VkAllocationCallbacks *, VkDevice *);

extern void vkDestroyDevice(VkDevice, const VkAllocationCallbacks *);

extern VkResult
vkCreateSwapchainKHR(VkDevice, const VkSwapchainCreateInfoKHR *, const VkAllocationCallbacks *, VkSwapchainKHR *);

extern void vkDestroySwapchainKHR(VkDevice, VkSwapchainKHR, const VkAllocationCallbacks *);

static int is_self_pointer(void *p) {
    return p == (void *) vkGetInstanceProcAddr || p == (void *) vkGetDeviceProcAddr || p == (void *) vkQueuePresentKHR
           || p == (void *) vkCreateDevice || p == (void *) vkDestroyDevice || p == (void *) vkCreateSwapchainKHR
           || p == (void *) vkDestroySwapchainKHR;
}

static int try_resolve_via(void *handle, const char *tag) {
    void *p_gipa = dlsym(handle, RENDER_VK_GIPA);
    void *p_gdpa = dlsym(handle, RENDER_VK_GDPA);
    void *p_present = dlsym(handle, RENDER_VK_PRESENT);
    void *p_create_dev = dlsym(handle, RENDER_VK_CREATE_DEV);
    void *p_destroy_dev = dlsym(handle, RENDER_VK_DESTROY_DEV);
    void *p_create_sc = dlsym(handle, RENDER_VK_CREATE_SC);
    void *p_destroy_sc = dlsym(handle, RENDER_VK_DESTROY_SC);

    if (is_self_pointer(p_gipa)) p_gipa = NULL;
    if (is_self_pointer(p_gdpa)) p_gdpa = NULL;
    if (is_self_pointer(p_present)) p_present = NULL;
    if (is_self_pointer(p_create_dev)) p_create_dev = NULL;
    if (is_self_pointer(p_destroy_dev)) p_destroy_dev = NULL;
    if (is_self_pointer(p_create_sc)) p_create_sc = NULL;
    if (is_self_pointer(p_destroy_sc)) p_destroy_sc = NULL;

    int n = !!p_gipa + !!p_gdpa + !!p_present + !!p_create_dev + !!p_destroy_dev + !!p_create_sc + !!p_destroy_sc;
    LOG_INFO("stage", "[vk] resolve via %s: %d/7 real symbols found", tag, n);

    if (n == 0) return 0;

    if (!p_present) return n;

    real_vk_get_instance_proc_addr = (PFN_vkGetInstanceProcAddr) p_gipa;
    real_vk_get_device_proc_addr = (PFN_vkGetDeviceProcAddr) p_gdpa;
    real_vk_queue_present_khr = (PFN_vkQueuePresentKHR) p_present;
    real_vk_create_device = (PFN_vkCreateDevice) p_create_dev;
    real_vk_destroy_device = (PFN_vkDestroyDevice) p_destroy_dev;
    real_vk_create_swapchain_khr = (PFN_vkCreateSwapchainKHR) p_create_sc;
    real_vk_destroy_swapchain_khr = (PFN_vkDestroySwapchainKHR) p_destroy_sc;
    return n;
}

static void resolve_vulkan_once(void) {
    void *handle = dlopen("libvulkan.so.1", RTLD_LAZY | RTLD_LOCAL | RTLD_NOLOAD);

    if (!handle) handle = dlopen("libvulkan.so.1", RTLD_LAZY | RTLD_LOCAL);
    if (!handle) handle = dlopen("libvulkan.so", RTLD_LAZY | RTLD_LOCAL);

    if (handle && try_resolve_via(handle, "dlopen(libvulkan)") > 0 && real_vk_queue_present_khr) {
        LOG_INFO("stage", "[vk] resolved via explicit dlopen of libvulkan");
        vulkan_resolved_ok = 1;
        return;
    }

    if (try_resolve_via(RTLD_NEXT, "RTLD_NEXT") > 0 && real_vk_queue_present_khr) {
        LOG_INFO("stage", "[vk] resolved via RTLD_NEXT");
        vulkan_resolved_ok = 1;
        return;
    }

    void *gipa = dlsym(RTLD_NEXT, RENDER_VK_GIPA);
    void *gdpa = dlsym(RTLD_NEXT, RENDER_VK_GDPA);

    LOG_WARN(
        "stage",
        "[vk] could not resolve real Vulkan symbols. vkGetInstanceProcAddr=%p vkGetDeviceProcAddr=%p "
        "libvulkan_handle=%p",
        gipa, gdpa, handle
    );
}

int resolve_vulkan_symbols(void) {
    pthread_once(&vulkan_resolve_once, resolve_vulkan_once);
    return vulkan_resolved_ok;
}
