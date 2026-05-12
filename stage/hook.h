#pragma once

#include <SDL2/SDL.h>
#include <EGL/egl.h>
#include <vulkan/vulkan.h>

__attribute__((visibility("hidden")))
extern void (*real_SDL_RenderPresent)(SDL_Renderer *);

__attribute__((visibility("hidden")))
extern EGLBoolean (*real_eglSwapBuffers)(EGLDisplay, EGLSurface);

__attribute__((visibility("hidden")))
extern void (*real_SDL_GL_SwapWindow)(SDL_Window *);

__attribute__((visibility("hidden")))
extern PFN_vkGetInstanceProcAddr real_vkGetInstanceProcAddr;

__attribute__((visibility("hidden")))
extern PFN_vkGetDeviceProcAddr real_vkGetDeviceProcAddr;

__attribute__((visibility("hidden")))
extern PFN_vkQueuePresentKHR real_vkQueuePresentKHR;

__attribute__((visibility("hidden")))
extern PFN_vkCreateDevice real_vkCreateDevice;

__attribute__((visibility("hidden")))
extern PFN_vkDestroyDevice real_vkDestroyDevice;

__attribute__((visibility("hidden")))
extern PFN_vkCreateSwapchainKHR real_vkCreateSwapchainKHR;

__attribute__((visibility("hidden")))
extern PFN_vkDestroySwapchainKHR real_vkDestroySwapchainKHR;

__attribute__((visibility("hidden")))
int resolve_vulkan_symbols(void);
