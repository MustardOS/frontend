#pragma once

#include <SDL2/SDL.h>
#include <EGL/egl.h>
#include <vulkan/vulkan.h>

__attribute__((visibility("hidden"))) extern void (*real_sdl_render_present)(SDL_Renderer *);

__attribute__((visibility("hidden"))) extern EGLBoolean (*real_egl_swap_buffers)(EGLDisplay, EGLSurface);

__attribute__((visibility("hidden"))) extern void (*real_sdl_gl_swap_window)(SDL_Window *);

__attribute__((visibility("hidden"))) extern PFN_vkGetInstanceProcAddr real_vk_get_instance_proc_addr;

__attribute__((visibility("hidden"))) extern PFN_vkGetDeviceProcAddr real_vk_get_device_proc_addr;

__attribute__((visibility("hidden"))) extern PFN_vkQueuePresentKHR real_vk_queue_present_khr;

__attribute__((visibility("hidden"))) extern PFN_vkCreateDevice real_vk_create_device;

__attribute__((visibility("hidden"))) extern PFN_vkDestroyDevice real_vk_destroy_device;

__attribute__((visibility("hidden"))) extern PFN_vkCreateSwapchainKHR real_vk_create_swapchain_khr;

__attribute__((visibility("hidden"))) extern PFN_vkDestroySwapchainKHR real_vk_destroy_swapchain_khr;

__attribute__((visibility("hidden"))) int resolve_vulkan_symbols(void);
