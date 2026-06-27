#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include <math.h>
#include <stddef.h>
#include <stdint.h>
#include <pthread.h>
#include <dlfcn.h>
#include <sys/stat.h>
#include <linux/limits.h>
#include <SDL2/SDL.h>
#include <SDL2/SDL_image.h>
#include <vulkan/vulkan.h>
#include "../../common/log.h"
#include "../../common/inotify.h"
#include "../common/common.h"
#include "../common/alpha.h"
#include "../common/anchor.h"
#include "../common/colour.h"
#include "../common/filter.h"
#include "../common/rotate.h"
#include "../common/scale.h"
#include "../common/stretch.h"
#include "../overlay/base.h"
#include "../overlay/battery.h"
#include "../overlay/bright.h"
#include "../overlay/volume.h"
#include "../overlay/notif.h"
#include "../hook.h"

#define V_MAX_DEVICES     4
#define V_MAX_SWAPCHAINS  8
#define V_MAX_SWAPCHAIN_I 8
#define V_MAX_TRACKED_TEX 128
#define V_INJECT_FAIL_MAX 3

#define V_STAGING_SIZE (4 * 1024 * 1024)

typedef struct {
    float colour[4];
} v_push_overlay_t;

typedef struct {
    float x, y;
    float u, v;
} v_quad_vertex_t;

#define V_QUAD_VERTS_PER_DRAW 4u

#define V_QUAD_MAX_DRAWS 64u

#define V_QUAD_BUFFER_SIZE ((VkDeviceSize) (V_QUAD_MAX_DRAWS * V_QUAD_VERTS_PER_DRAW * sizeof(v_quad_vertex_t)))

typedef struct {
    float brightness;
    float contrast;
    float saturation;
    float cosH;
    float sinH;
    float gamma;
    int filter_enabled;
    int _pad;
    float filter[12];
} v_push_content_t;

typedef struct {
    float texel[2];
    float strength;
    float _pad;
} v_push_smooth_t;

typedef struct {
    float resolution[2];
    float native_resolution[2];
    float time;
    int frame;
    int _pad[2];
} v_push_user_shader_t;

static struct {
    int ready;
    int tried;
    void *handle;

    PFN_vkGetPhysicalDeviceMemoryProperties GetPhysicalDeviceMemoryProperties;
    PFN_vkGetPhysicalDeviceQueueFamilyProperties GetPhysicalDeviceQueueFamilyProperties;

    PFN_vkGetDeviceQueue GetDeviceQueue;
    PFN_vkDeviceWaitIdle DeviceWaitIdle;
    PFN_vkQueueSubmit QueueSubmit;
    PFN_vkQueueWaitIdle QueueWaitIdle;

    PFN_vkCreateCommandPool CreateCommandPool;
    PFN_vkDestroyCommandPool DestroyCommandPool;
    PFN_vkAllocateCommandBuffers AllocateCommandBuffers;
    PFN_vkFreeCommandBuffers FreeCommandBuffers;
    PFN_vkBeginCommandBuffer BeginCommandBuffer;
    PFN_vkEndCommandBuffer EndCommandBuffer;
    PFN_vkResetCommandBuffer ResetCommandBuffer;
    PFN_vkCmdPipelineBarrier CmdPipelineBarrier;
    PFN_vkCmdBeginRenderPass CmdBeginRenderPass;
    PFN_vkCmdEndRenderPass CmdEndRenderPass;
    PFN_vkCmdBindPipeline CmdBindPipeline;
    PFN_vkCmdBindDescriptorSets CmdBindDescriptorSets;
    PFN_vkCmdBindVertexBuffers CmdBindVertexBuffers;
    PFN_vkCmdSetViewport CmdSetViewport;
    PFN_vkCmdSetScissor CmdSetScissor;
    PFN_vkCmdPushConstants CmdPushConstants;
    PFN_vkCmdDraw CmdDraw;
    PFN_vkCmdCopyImage CmdCopyImage;
    PFN_vkCmdCopyBufferToImage CmdCopyBufferToImage;

    PFN_vkCreateRenderPass CreateRenderPass;
    PFN_vkDestroyRenderPass DestroyRenderPass;
    PFN_vkCreateFramebuffer CreateFramebuffer;
    PFN_vkDestroyFramebuffer DestroyFramebuffer;

    PFN_vkCreateShaderModule CreateShaderModule;
    PFN_vkDestroyShaderModule DestroyShaderModule;
    PFN_vkCreatePipelineLayout CreatePipelineLayout;
    PFN_vkDestroyPipelineLayout DestroyPipelineLayout;
    PFN_vkCreateGraphicsPipelines CreateGraphicsPipelines;
    PFN_vkDestroyPipeline DestroyPipeline;
    PFN_vkCreatePipelineCache CreatePipelineCache;
    PFN_vkDestroyPipelineCache DestroyPipelineCache;

    PFN_vkCreateDescriptorSetLayout CreateDescriptorSetLayout;
    PFN_vkDestroyDescriptorSetLayout DestroyDescriptorSetLayout;
    PFN_vkCreateDescriptorPool CreateDescriptorPool;
    PFN_vkDestroyDescriptorPool DestroyDescriptorPool;
    PFN_vkAllocateDescriptorSets AllocateDescriptorSets;
    PFN_vkFreeDescriptorSets FreeDescriptorSets;
    PFN_vkUpdateDescriptorSets UpdateDescriptorSets;

    PFN_vkCreateImage CreateImage;
    PFN_vkDestroyImage DestroyImage;
    PFN_vkCreateImageView CreateImageView;
    PFN_vkDestroyImageView DestroyImageView;
    PFN_vkCreateSampler CreateSampler;
    PFN_vkDestroySampler DestroySampler;
    PFN_vkGetImageMemoryRequirements GetImageMemoryRequirements;
    PFN_vkBindImageMemory BindImageMemory;

    PFN_vkCreateBuffer CreateBuffer;
    PFN_vkDestroyBuffer DestroyBuffer;
    PFN_vkGetBufferMemoryRequirements GetBufferMemoryRequirements;
    PFN_vkBindBufferMemory BindBufferMemory;
    PFN_vkMapMemory MapMemory;
    PFN_vkUnmapMemory UnmapMemory;
    PFN_vkFlushMappedMemoryRanges FlushMappedMemoryRanges;

    PFN_vkAllocateMemory AllocateMemory;
    PFN_vkFreeMemory FreeMemory;

    PFN_vkCreateFence CreateFence;
    PFN_vkDestroyFence DestroyFence;
    PFN_vkWaitForFences WaitForFences;
    PFN_vkResetFences ResetFences;

    PFN_vkCreateSemaphore CreateSemaphore;
    PFN_vkDestroySemaphore DestroySemaphore;

    PFN_vkGetSwapchainImagesKHR GetSwapchainImagesKHR;
} vk_dl;

static pthread_once_t vk_dl_once = PTHREAD_ONCE_INIT;

#define V_LOAD(name)                                                                                                   \
    do {                                                                                                               \
        vk_dl.name = (PFN_vk##name) dlsym(vk_dl.handle, "vk" #name);                                                   \
        if (!vk_dl.name) {                                                                                             \
            LOG_INFO("stage", "[vk] missing proc: vk" #name);                                                          \
            return;                                                                                                    \
        }                                                                                                              \
    } while (0)

static void vk_dl_init_once(void) {
    vk_dl.tried = 1;

    vk_dl.handle = dlopen("libvulkan.so.1", RTLD_LAZY | RTLD_LOCAL);
    if (!vk_dl.handle) {
        LOG_INFO("stage", "[vk] libvulkan.so.1 not present - Vulkan path disabled");
        return;
    }

    V_LOAD(GetPhysicalDeviceMemoryProperties);
    V_LOAD(GetPhysicalDeviceQueueFamilyProperties);

    V_LOAD(GetDeviceQueue);
    V_LOAD(DeviceWaitIdle);
    V_LOAD(QueueSubmit);
    V_LOAD(QueueWaitIdle);

    V_LOAD(CreateCommandPool);
    V_LOAD(DestroyCommandPool);
    V_LOAD(AllocateCommandBuffers);
    V_LOAD(FreeCommandBuffers);
    V_LOAD(BeginCommandBuffer);
    V_LOAD(EndCommandBuffer);
    V_LOAD(ResetCommandBuffer);
    V_LOAD(CmdPipelineBarrier);
    V_LOAD(CmdBeginRenderPass);
    V_LOAD(CmdEndRenderPass);
    V_LOAD(CmdBindPipeline);
    V_LOAD(CmdBindDescriptorSets);
    V_LOAD(CmdBindVertexBuffers);
    V_LOAD(CmdSetViewport);
    V_LOAD(CmdSetScissor);
    V_LOAD(CmdPushConstants);
    V_LOAD(CmdDraw);
    V_LOAD(CmdCopyImage);
    V_LOAD(CmdCopyBufferToImage);

    V_LOAD(CreateRenderPass);
    V_LOAD(DestroyRenderPass);
    V_LOAD(CreateFramebuffer);
    V_LOAD(DestroyFramebuffer);

    V_LOAD(CreateShaderModule);
    V_LOAD(DestroyShaderModule);
    V_LOAD(CreatePipelineLayout);
    V_LOAD(DestroyPipelineLayout);
    V_LOAD(CreateGraphicsPipelines);
    V_LOAD(DestroyPipeline);
    V_LOAD(CreatePipelineCache);
    V_LOAD(DestroyPipelineCache);

    V_LOAD(CreateDescriptorSetLayout);
    V_LOAD(DestroyDescriptorSetLayout);
    V_LOAD(CreateDescriptorPool);
    V_LOAD(DestroyDescriptorPool);
    V_LOAD(AllocateDescriptorSets);
    V_LOAD(FreeDescriptorSets);
    V_LOAD(UpdateDescriptorSets);

    V_LOAD(CreateImage);
    V_LOAD(DestroyImage);
    V_LOAD(CreateImageView);
    V_LOAD(DestroyImageView);
    V_LOAD(CreateSampler);
    V_LOAD(DestroySampler);
    V_LOAD(GetImageMemoryRequirements);
    V_LOAD(BindImageMemory);

    V_LOAD(CreateBuffer);
    V_LOAD(DestroyBuffer);
    V_LOAD(GetBufferMemoryRequirements);
    V_LOAD(BindBufferMemory);
    V_LOAD(MapMemory);
    V_LOAD(UnmapMemory);
    V_LOAD(FlushMappedMemoryRanges);

    V_LOAD(AllocateMemory);
    V_LOAD(FreeMemory);

    V_LOAD(CreateFence);
    V_LOAD(DestroyFence);
    V_LOAD(WaitForFences);
    V_LOAD(ResetFences);

    V_LOAD(CreateSemaphore);
    V_LOAD(DestroySemaphore);

    V_LOAD(GetSwapchainImagesKHR);

    vk_dl.ready = 1;
    LOG_INFO("stage", "[vk] Vulkan dispatch resolved");
}

#undef V_LOAD

static int vk_dl_init(void) {
    pthread_once(&vk_dl_once, vk_dl_init_once);
    return vk_dl.ready;
}

static const uint32_t spv_vs_quad[] = {
    0x07230203u, 0x00010000u, 0x00000000u, 0x00000023u, 0x00000000u, 0x00020011u, 0x00000001u, 0x0003000Eu, 0x00000000u,
    0x00000001u, 0x000A000Fu, 0x00000000u, 0x00000003u, 0x6E69616Du, 0x00000000u, 0x00000014u, 0x0000000Cu, 0x0000000Du,
    0x0000000Fu, 0x00000011u, 0x00040005u, 0x00000003u, 0x6E69616Du, 0x00000000u, 0x00040005u, 0x0000000Cu, 0x6F705F61u,
    0x00000073u, 0x00040005u, 0x0000000Du, 0x76755F61u, 0x00000000u, 0x00040005u, 0x0000000Fu, 0x76755F76u, 0x00000000u,
    0x00040005u, 0x00000011u, 0x6F635F76u, 0x0000006Cu, 0x00060005u, 0x00000012u, 0x505F6C67u, 0x65567265u, 0x78657472u,
    0x00000000u, 0x00060006u, 0x00000012u, 0x00000000u, 0x505F6C67u, 0x7469736Fu, 0x006E6F69u, 0x00030005u, 0x00000015u,
    0x00004350u, 0x00050006u, 0x00000015u, 0x00000000u, 0x6F6C6F63u, 0x00007275u, 0x00040047u, 0x0000000Cu, 0x0000001Eu,
    0x00000000u, 0x00040047u, 0x0000000Du, 0x0000001Eu, 0x00000001u, 0x00040047u, 0x0000000Fu, 0x0000001Eu, 0x00000000u,
    0x00040047u, 0x00000011u, 0x0000001Eu, 0x00000001u, 0x00050048u, 0x00000012u, 0x00000000u, 0x0000000Bu, 0x00000000u,
    0x00030047u, 0x00000012u, 0x00000002u, 0x00050048u, 0x00000015u, 0x00000000u, 0x00000023u, 0x00000000u, 0x00030047u,
    0x00000015u, 0x00000002u, 0x00020013u, 0x00000001u, 0x00030021u, 0x00000002u, 0x00000001u, 0x00040015u, 0x00000004u,
    0x00000020u, 0x00000001u, 0x00030016u, 0x00000005u, 0x00000020u, 0x00040017u, 0x00000006u, 0x00000005u, 0x00000002u,
    0x00040017u, 0x00000007u, 0x00000005u, 0x00000004u, 0x0004002Bu, 0x00000004u, 0x00000008u, 0x00000000u, 0x0004002Bu,
    0x00000005u, 0x00000009u, 0x00000000u, 0x0004002Bu, 0x00000005u, 0x0000000Au, 0x3F800000u, 0x00040020u, 0x0000000Bu,
    0x00000001u, 0x00000006u, 0x0004003Bu, 0x0000000Bu, 0x0000000Cu, 0x00000001u, 0x0004003Bu, 0x0000000Bu, 0x0000000Du,
    0x00000001u, 0x00040020u, 0x0000000Eu, 0x00000003u, 0x00000006u, 0x00040020u, 0x00000010u, 0x00000003u, 0x00000007u,
    0x0004003Bu, 0x0000000Eu, 0x0000000Fu, 0x00000003u, 0x0004003Bu, 0x00000010u, 0x00000011u, 0x00000003u, 0x0003001Eu,
    0x00000012u, 0x00000007u, 0x00040020u, 0x00000013u, 0x00000003u, 0x00000012u, 0x0004003Bu, 0x00000013u, 0x00000014u,
    0x00000003u, 0x0003001Eu, 0x00000015u, 0x00000007u, 0x00040020u, 0x00000016u, 0x00000009u, 0x00000015u, 0x00040020u,
    0x00000018u, 0x00000009u, 0x00000007u, 0x0004003Bu, 0x00000016u, 0x00000017u, 0x00000009u, 0x00050036u, 0x00000001u,
    0x00000003u, 0x00000000u, 0x00000002u, 0x000200F8u, 0x00000019u, 0x0004003Du, 0x00000006u, 0x0000001Au, 0x0000000Cu,
    0x0004003Du, 0x00000006u, 0x0000001Bu, 0x0000000Du, 0x00050041u, 0x00000018u, 0x0000001Cu, 0x00000017u, 0x00000008u,
    0x0004003Du, 0x00000007u, 0x0000001Du, 0x0000001Cu, 0x00050051u, 0x00000005u, 0x0000001Eu, 0x0000001Au, 0x00000000u,
    0x00050051u, 0x00000005u, 0x0000001Fu, 0x0000001Au, 0x00000001u, 0x0004007Fu, 0x00000005u, 0x00000020u, 0x0000001Fu,
    0x00070050u, 0x00000007u, 0x00000021u, 0x0000001Eu, 0x00000020u, 0x00000009u, 0x0000000Au, 0x00050041u, 0x00000010u,
    0x00000022u, 0x00000014u, 0x00000008u, 0x0003003Eu, 0x00000022u, 0x00000021u, 0x0003003Eu, 0x0000000Fu, 0x0000001Bu,
    0x0003003Eu, 0x00000011u, 0x0000001Du, 0x000100FDu, 0x00010038u
};
static const size_t spv_vs_quad_size = sizeof(spv_vs_quad);

static const uint32_t spv_fs_overlay[] = {
    0x07230203u, 0x00010000u, 0x00000000u, 0x00000017u, 0x00000000u, 0x00020011u, 0x00000001u, 0x0003000Eu, 0x00000000u,
    0x00000001u, 0x0008000Fu, 0x00000004u, 0x00000003u, 0x6E69616Du, 0x00000000u, 0x00000010u, 0x0000000Cu, 0x0000000Eu,
    0x00030010u, 0x00000003u, 0x00000007u, 0x00040005u, 0x00000003u, 0x6E69616Du, 0x00000000u, 0x00040005u, 0x0000000Au,
    0x65745F75u, 0x00000078u, 0x00040005u, 0x0000000Cu, 0x76755F76u, 0x00000000u, 0x00040005u, 0x0000000Eu, 0x6F635F76u,
    0x0000006Cu, 0x00040005u, 0x00000010u, 0x6F635F6Fu, 0x0000006Cu, 0x00040047u, 0x0000000Au, 0x00000022u, 0x00000000u,
    0x00040047u, 0x0000000Au, 0x00000021u, 0x00000000u, 0x00040047u, 0x0000000Cu, 0x0000001Eu, 0x00000000u, 0x00040047u,
    0x0000000Eu, 0x0000001Eu, 0x00000001u, 0x00040047u, 0x00000010u, 0x0000001Eu, 0x00000000u, 0x00020013u, 0x00000001u,
    0x00030021u, 0x00000002u, 0x00000001u, 0x00030016u, 0x00000004u, 0x00000020u, 0x00040017u, 0x00000005u, 0x00000004u,
    0x00000002u, 0x00040017u, 0x00000006u, 0x00000004u, 0x00000004u, 0x00090019u, 0x00000007u, 0x00000004u, 0x00000001u,
    0x00000000u, 0x00000000u, 0x00000000u, 0x00000001u, 0x00000000u, 0x0003001Bu, 0x00000008u, 0x00000007u, 0x00040020u,
    0x00000009u, 0x00000000u, 0x00000008u, 0x0004003Bu, 0x00000009u, 0x0000000Au, 0x00000000u, 0x00040020u, 0x0000000Bu,
    0x00000001u, 0x00000005u, 0x00040020u, 0x0000000Du, 0x00000001u, 0x00000006u, 0x00040020u, 0x0000000Fu, 0x00000003u,
    0x00000006u, 0x0004003Bu, 0x0000000Bu, 0x0000000Cu, 0x00000001u, 0x0004003Bu, 0x0000000Du, 0x0000000Eu, 0x00000001u,
    0x0004003Bu, 0x0000000Fu, 0x00000010u, 0x00000003u, 0x00050036u, 0x00000001u, 0x00000003u, 0x00000000u, 0x00000002u,
    0x000200F8u, 0x00000011u, 0x0004003Du, 0x00000008u, 0x00000012u, 0x0000000Au, 0x0004003Du, 0x00000005u, 0x00000013u,
    0x0000000Cu, 0x00050057u, 0x00000006u, 0x00000014u, 0x00000012u, 0x00000013u, 0x0004003Du, 0x00000006u, 0x00000015u,
    0x0000000Eu, 0x00050085u, 0x00000006u, 0x00000016u, 0x00000014u, 0x00000015u, 0x0003003Eu, 0x00000010u, 0x00000016u,
    0x000100FDu, 0x00010038u
};
static const size_t spv_fs_overlay_size = sizeof(spv_fs_overlay);

static const uint32_t spv_fs_solid[] = {
    0x07230203u, 0x00010000u, 0x00000000u, 0x0000000Cu, 0x00000000u, 0x00020011u, 0x00000001u, 0x0003000Eu, 0x00000000u,
    0x00000001u, 0x0007000Fu, 0x00000004u, 0x00000003u, 0x6E69616Du, 0x00000000u, 0x00000009u, 0x00000007u, 0x00030010u,
    0x00000003u, 0x00000007u, 0x00040005u, 0x00000003u, 0x6E69616Du, 0x00000000u, 0x00040005u, 0x00000007u, 0x6F635F76u,
    0x0000006Cu, 0x00040005u, 0x00000009u, 0x6F635F6Fu, 0x0000006Cu, 0x00040047u, 0x00000007u, 0x0000001Eu, 0x00000001u,
    0x00040047u, 0x00000009u, 0x0000001Eu, 0x00000000u, 0x00020013u, 0x00000001u, 0x00030021u, 0x00000002u, 0x00000001u,
    0x00030016u, 0x00000004u, 0x00000020u, 0x00040017u, 0x00000005u, 0x00000004u, 0x00000004u, 0x00040020u, 0x00000006u,
    0x00000001u, 0x00000005u, 0x00040020u, 0x00000008u, 0x00000003u, 0x00000005u, 0x0004003Bu, 0x00000006u, 0x00000007u,
    0x00000001u, 0x0004003Bu, 0x00000008u, 0x00000009u, 0x00000003u, 0x00050036u, 0x00000001u, 0x00000003u, 0x00000000u,
    0x00000002u, 0x000200F8u, 0x0000000Au, 0x0004003Du, 0x00000005u, 0x0000000Bu, 0x00000007u, 0x0003003Eu, 0x00000009u,
    0x0000000Bu, 0x000100FDu, 0x00010038u
};
static const size_t spv_fs_solid_size = sizeof(spv_fs_solid);

static const uint32_t spv_fs_content[] = {
    0x07230203u, 0x00010000u, 0x00000000u, 0x000000CAu, 0x00000000u, 0x00020011u, 0x00000001u, 0x0006000Bu, 0x00000001u,
    0x4C534C47u, 0x6474732Eu, 0x3035342Eu, 0x00000000u, 0x0003000Eu, 0x00000000u, 0x00000001u, 0x0007000Fu, 0x00000004u,
    0x00000004u, 0x6E69616Du, 0x00000000u, 0x00000036u, 0x00000038u, 0x00030010u, 0x00000004u, 0x00000007u, 0x00040005u,
    0x00000004u, 0x6E69616Du, 0x00000000u, 0x00040005u, 0x00000033u, 0x65745F75u, 0x00000078u, 0x00040005u, 0x00000036u,
    0x76755F76u, 0x00000000u, 0x00040005u, 0x00000038u, 0x6F635F6Fu, 0x0000006Cu, 0x00030005u, 0x00000039u, 0x00004350u,
    0x00060006u, 0x00000039u, 0x00000000u, 0x67697262u, 0x656E7468u, 0x00007373u, 0x00060006u, 0x00000039u, 0x00000001u,
    0x746E6F63u, 0x74736172u, 0x00000000u, 0x00060006u, 0x00000039u, 0x00000002u, 0x75746173u, 0x69746172u, 0x00006E6Fu,
    0x00050006u, 0x00000039u, 0x00000003u, 0x48736F63u, 0x00000000u, 0x00050006u, 0x00000039u, 0x00000004u, 0x486E6973u,
    0x00000000u, 0x00050006u, 0x00000039u, 0x00000005u, 0x6D6D6167u, 0x00000061u, 0x00070006u, 0x00000039u, 0x00000006u,
    0x746C6966u, 0x655F7265u, 0x6C62616Eu, 0x00006465u, 0x00050006u, 0x00000039u, 0x00000007u, 0x6461705Fu, 0x00000000u,
    0x00050006u, 0x00000039u, 0x00000008u, 0x746C6966u, 0x00007265u, 0x00040047u, 0x00000033u, 0x00000022u, 0x00000000u,
    0x00040047u, 0x00000033u, 0x00000021u, 0x00000000u, 0x00040047u, 0x00000036u, 0x0000001Eu, 0x00000000u, 0x00040047u,
    0x00000038u, 0x0000001Eu, 0x00000000u, 0x00040047u, 0x00000030u, 0x00000006u, 0x00000004u, 0x00050048u, 0x00000039u,
    0x00000000u, 0x00000023u, 0x00000000u, 0x00050048u, 0x00000039u, 0x00000001u, 0x00000023u, 0x00000004u, 0x00050048u,
    0x00000039u, 0x00000002u, 0x00000023u, 0x00000008u, 0x00050048u, 0x00000039u, 0x00000003u, 0x00000023u, 0x0000000Cu,
    0x00050048u, 0x00000039u, 0x00000004u, 0x00000023u, 0x00000010u, 0x00050048u, 0x00000039u, 0x00000005u, 0x00000023u,
    0x00000014u, 0x00050048u, 0x00000039u, 0x00000006u, 0x00000023u, 0x00000018u, 0x00050048u, 0x00000039u, 0x00000007u,
    0x00000023u, 0x0000001Cu, 0x00050048u, 0x00000039u, 0x00000008u, 0x00000023u, 0x00000020u, 0x00030047u, 0x00000039u,
    0x00000002u, 0x00020013u, 0x00000002u, 0x00030021u, 0x00000003u, 0x00000002u, 0x00040015u, 0x00000005u, 0x00000020u,
    0x00000001u, 0x00040015u, 0x00000006u, 0x00000020u, 0x00000000u, 0x00030016u, 0x00000007u, 0x00000020u, 0x00040017u,
    0x00000008u, 0x00000007u, 0x00000002u, 0x00040017u, 0x00000009u, 0x00000007u, 0x00000003u, 0x00040017u, 0x0000000Au,
    0x00000007u, 0x00000004u, 0x0004002Bu, 0x00000005u, 0x0000000Bu, 0x00000000u, 0x0004002Bu, 0x00000005u, 0x0000000Cu,
    0x00000001u, 0x0004002Bu, 0x00000005u, 0x0000000Du, 0x00000002u, 0x0004002Bu, 0x00000005u, 0x0000000Eu, 0x00000003u,
    0x0004002Bu, 0x00000005u, 0x0000000Fu, 0x00000004u, 0x0004002Bu, 0x00000005u, 0x00000010u, 0x00000005u, 0x0004002Bu,
    0x00000005u, 0x00000011u, 0x00000006u, 0x0004002Bu, 0x00000005u, 0x00000012u, 0x00000007u, 0x0004002Bu, 0x00000005u,
    0x00000013u, 0x00000008u, 0x0004002Bu, 0x00000005u, 0x00000014u, 0x00000009u, 0x0004002Bu, 0x00000005u, 0x00000015u,
    0x0000000Au, 0x0004002Bu, 0x00000005u, 0x00000016u, 0x0000000Bu, 0x0004002Bu, 0x00000005u, 0x00000017u, 0x0000000Cu,
    0x0004002Bu, 0x00000006u, 0x00000018u, 0x0000000Cu, 0x0004002Bu, 0x00000007u, 0x00000019u, 0x00000000u, 0x0004002Bu,
    0x00000007u, 0x0000001Au, 0x3F000000u, 0x0004002Bu, 0x00000007u, 0x0000001Bu, 0x3F800000u, 0x0004002Bu, 0x00000007u,
    0x0000001Cu, 0x3E59B3D0u, 0x0004002Bu, 0x00000007u, 0x0000001Du, 0x3F371759u, 0x0004002Bu, 0x00000007u, 0x0000001Eu,
    0x3D93DD98u, 0x0004002Bu, 0x00000007u, 0x0000001Fu, 0x3E991687u, 0x0004002Bu, 0x00000007u, 0x00000020u, 0x3F3374BCu,
    0x0004002Bu, 0x00000007u, 0x00000021u, 0x3E2C0831u, 0x0004002Bu, 0x00000007u, 0x00000022u, 0x3F1645A2u, 0x0004002Bu,
    0x00000007u, 0x00000023u, 0x3EA8F5C3u, 0x0004002Bu, 0x00000007u, 0x00000024u, 0x3DE978D5u, 0x0004002Bu, 0x00000007u,
    0x00000025u, 0x3EFE76C9u, 0x0004002Bu, 0x00000007u, 0x00000026u, 0x3EA7EF9Eu, 0x0004002Bu, 0x00000007u, 0x00000027u,
    0x3ED374BCu, 0x0004002Bu, 0x00000007u, 0x00000028u, 0x3D0F5C29u, 0x0004002Bu, 0x00000007u, 0x00000029u, 0x3E958106u,
    0x0004002Bu, 0x00000007u, 0x0000002Au, 0x3E99999Au, 0x0004002Bu, 0x00000007u, 0x0000002Bu, 0x3FA00000u, 0x0004002Bu,
    0x00000007u, 0x0000002Cu, 0x3F16872Bu, 0x0004002Bu, 0x00000007u, 0x0000002Du, 0x3F866666u, 0x0004002Bu, 0x00000007u,
    0x0000002Eu, 0x3F62D0E5u, 0x0004002Bu, 0x00000007u, 0x0000002Fu, 0x3E4FDF3Bu, 0x0004001Cu, 0x00000030u, 0x00000007u,
    0x00000018u, 0x00090019u, 0x00000034u, 0x00000007u, 0x00000001u, 0x00000000u, 0x00000000u, 0x00000000u, 0x00000001u,
    0x00000000u, 0x0003001Bu, 0x00000031u, 0x00000034u, 0x00040020u, 0x00000032u, 0x00000000u, 0x00000031u, 0x0004003Bu,
    0x00000032u, 0x00000033u, 0x00000000u, 0x00040020u, 0x00000035u, 0x00000001u, 0x00000008u, 0x0004003Bu, 0x00000035u,
    0x00000036u, 0x00000001u, 0x00040020u, 0x00000037u, 0x00000003u, 0x0000000Au, 0x0004003Bu, 0x00000037u, 0x00000038u,
    0x00000003u, 0x000B001Eu, 0x00000039u, 0x00000007u, 0x00000007u, 0x00000007u, 0x00000007u, 0x00000007u, 0x00000007u,
    0x00000005u, 0x00000005u, 0x00000030u, 0x00040020u, 0x0000003Au, 0x00000009u, 0x00000039u, 0x00040020u, 0x0000003Cu,
    0x00000009u, 0x00000007u, 0x00040020u, 0x0000003Du, 0x00000009u, 0x00000005u, 0x0004003Bu, 0x0000003Au, 0x0000003Bu,
    0x00000009u, 0x00050036u, 0x00000002u, 0x00000004u, 0x00000000u, 0x00000003u, 0x000200F8u, 0x0000003Eu, 0x0004003Du,
    0x00000008u, 0x0000003Fu, 0x00000036u, 0x0004003Du, 0x00000031u, 0x00000040u, 0x00000033u, 0x00050057u, 0x0000000Au,
    0x00000041u, 0x00000040u, 0x0000003Fu, 0x00050051u, 0x00000007u, 0x00000042u, 0x00000041u, 0x00000000u, 0x00050051u,
    0x00000007u, 0x00000043u, 0x00000041u, 0x00000001u, 0x00050051u, 0x00000007u, 0x00000044u, 0x00000041u, 0x00000002u,
    0x00050051u, 0x00000007u, 0x00000045u, 0x00000041u, 0x00000003u, 0x00050041u, 0x0000003Cu, 0x00000046u, 0x0000003Bu,
    0x0000000Bu, 0x0004003Du, 0x00000007u, 0x00000047u, 0x00000046u, 0x00050041u, 0x0000003Cu, 0x00000048u, 0x0000003Bu,
    0x0000000Cu, 0x0004003Du, 0x00000007u, 0x00000049u, 0x00000048u, 0x00050041u, 0x0000003Cu, 0x0000004Au, 0x0000003Bu,
    0x0000000Du, 0x0004003Du, 0x00000007u, 0x0000004Bu, 0x0000004Au, 0x00050041u, 0x0000003Cu, 0x0000004Cu, 0x0000003Bu,
    0x0000000Eu, 0x0004003Du, 0x00000007u, 0x0000004Du, 0x0000004Cu, 0x00050041u, 0x0000003Cu, 0x0000004Eu, 0x0000003Bu,
    0x0000000Fu, 0x0004003Du, 0x00000007u, 0x0000004Fu, 0x0000004Eu, 0x00050041u, 0x0000003Cu, 0x00000050u, 0x0000003Bu,
    0x00000010u, 0x0004003Du, 0x00000007u, 0x00000051u, 0x00000050u, 0x00050081u, 0x00000007u, 0x00000052u, 0x00000042u,
    0x00000047u, 0x00050081u, 0x00000007u, 0x00000053u, 0x00000043u, 0x00000047u, 0x00050081u, 0x00000007u, 0x00000054u,
    0x00000044u, 0x00000047u, 0x00050083u, 0x00000007u, 0x00000055u, 0x00000052u, 0x0000001Au, 0x00050085u, 0x00000007u,
    0x00000056u, 0x00000055u, 0x00000049u, 0x00050081u, 0x00000007u, 0x00000057u, 0x00000056u, 0x0000001Au, 0x00050083u,
    0x00000007u, 0x00000058u, 0x00000053u, 0x0000001Au, 0x00050085u, 0x00000007u, 0x00000059u, 0x00000058u, 0x00000049u,
    0x00050081u, 0x00000007u, 0x0000005Au, 0x00000059u, 0x0000001Au, 0x00050083u, 0x00000007u, 0x0000005Bu, 0x00000054u,
    0x0000001Au, 0x00050085u, 0x00000007u, 0x0000005Cu, 0x0000005Bu, 0x00000049u, 0x00050081u, 0x00000007u, 0x0000005Du,
    0x0000005Cu, 0x0000001Au, 0x00050085u, 0x00000007u, 0x0000005Eu, 0x00000057u, 0x0000001Cu, 0x00050085u, 0x00000007u,
    0x0000005Fu, 0x0000005Au, 0x0000001Du, 0x00050081u, 0x00000007u, 0x00000060u, 0x0000005Eu, 0x0000005Fu, 0x00050085u,
    0x00000007u, 0x00000061u, 0x0000005Du, 0x0000001Eu, 0x00050081u, 0x00000007u, 0x00000062u, 0x00000060u, 0x00000061u,
    0x00050083u, 0x00000007u, 0x00000063u, 0x00000057u, 0x00000062u, 0x00050085u, 0x00000007u, 0x00000064u, 0x00000063u,
    0x0000004Bu, 0x00050081u, 0x00000007u, 0x00000065u, 0x00000062u, 0x00000064u, 0x00050083u, 0x00000007u, 0x00000066u,
    0x0000005Au, 0x00000062u, 0x00050085u, 0x00000007u, 0x00000067u, 0x00000066u, 0x0000004Bu, 0x00050081u, 0x00000007u,
    0x00000068u, 0x00000062u, 0x00000067u, 0x00050083u, 0x00000007u, 0x00000069u, 0x0000005Du, 0x00000062u, 0x00050085u,
    0x00000007u, 0x0000006Au, 0x00000069u, 0x0000004Bu, 0x00050081u, 0x00000007u, 0x0000006Bu, 0x00000062u, 0x0000006Au,
    0x00050085u, 0x00000007u, 0x0000006Cu, 0x00000020u, 0x0000004Du, 0x00050081u, 0x00000007u, 0x0000006Du, 0x0000001Fu,
    0x0000006Cu, 0x00050085u, 0x00000007u, 0x0000006Eu, 0x00000021u, 0x0000004Fu, 0x00050081u, 0x00000007u, 0x0000006Fu,
    0x0000006Du, 0x0000006Eu, 0x00050085u, 0x00000007u, 0x00000070u, 0x00000022u, 0x0000004Du, 0x00050083u, 0x00000007u,
    0x00000071u, 0x00000022u, 0x00000070u, 0x00050085u, 0x00000007u, 0x00000072u, 0x00000023u, 0x0000004Fu, 0x00050081u,
    0x00000007u, 0x00000073u, 0x00000071u, 0x00000072u, 0x00050085u, 0x00000007u, 0x00000074u, 0x00000024u, 0x0000004Du,
    0x00050083u, 0x00000007u, 0x00000075u, 0x00000024u, 0x00000074u, 0x00050085u, 0x00000007u, 0x00000076u, 0x00000025u,
    0x0000004Fu, 0x00050083u, 0x00000007u, 0x00000077u, 0x00000075u, 0x00000076u, 0x00050085u, 0x00000007u, 0x00000078u,
    0x0000001Fu, 0x0000004Du, 0x00050083u, 0x00000007u, 0x00000079u, 0x0000001Fu, 0x00000078u, 0x00050085u, 0x00000007u,
    0x0000007Au, 0x00000026u, 0x0000004Fu, 0x00050083u, 0x00000007u, 0x0000007Bu, 0x00000079u, 0x0000007Au, 0x00050085u,
    0x00000007u, 0x0000007Cu, 0x00000027u, 0x0000004Du, 0x00050081u, 0x00000007u, 0x0000007Du, 0x00000022u, 0x0000007Cu,
    0x00050085u, 0x00000007u, 0x0000007Eu, 0x00000028u, 0x0000004Fu, 0x00050081u, 0x00000007u, 0x0000007Fu, 0x0000007Du,
    0x0000007Eu, 0x00050085u, 0x00000007u, 0x00000080u, 0x00000024u, 0x0000004Du, 0x00050083u, 0x00000007u, 0x00000081u,
    0x00000024u, 0x00000080u, 0x00050085u, 0x00000007u, 0x00000082u, 0x00000029u, 0x0000004Fu, 0x00050081u, 0x00000007u,
    0x00000083u, 0x00000081u, 0x00000082u, 0x00050085u, 0x00000007u, 0x00000084u, 0x0000002Au, 0x0000004Du, 0x00050083u,
    0x00000007u, 0x00000085u, 0x0000001Fu, 0x00000084u, 0x00050085u, 0x00000007u, 0x00000086u, 0x0000002Bu, 0x0000004Fu,
    0x00050081u, 0x00000007u, 0x00000087u, 0x00000085u, 0x00000086u, 0x00050085u, 0x00000007u, 0x00000088u, 0x0000002Cu,
    0x0000004Du, 0x00050083u, 0x00000007u, 0x00000089u, 0x00000022u, 0x00000088u, 0x00050085u, 0x00000007u, 0x0000008Au,
    0x0000002Du, 0x0000004Fu, 0x00050083u, 0x00000007u, 0x0000008Bu, 0x00000089u, 0x0000008Au, 0x00050085u, 0x00000007u,
    0x0000008Cu, 0x0000002Eu, 0x0000004Du, 0x00050081u, 0x00000007u, 0x0000008Du, 0x00000024u, 0x0000008Cu, 0x00050085u,
    0x00000007u, 0x0000008Eu, 0x0000002Fu, 0x0000004Fu, 0x00050083u, 0x00000007u, 0x0000008Fu, 0x0000008Du, 0x0000008Eu,
    0x00050085u, 0x00000007u, 0x00000090u, 0x0000006Fu, 0x00000065u, 0x00050085u, 0x00000007u, 0x00000091u, 0x0000007Bu,
    0x00000068u, 0x00050081u, 0x00000007u, 0x00000092u, 0x00000090u, 0x00000091u, 0x00050085u, 0x00000007u, 0x00000093u,
    0x00000087u, 0x0000006Bu, 0x00050081u, 0x00000007u, 0x00000094u, 0x00000092u, 0x00000093u, 0x00050085u, 0x00000007u,
    0x00000095u, 0x00000073u, 0x00000065u, 0x00050085u, 0x00000007u, 0x00000096u, 0x0000007Fu, 0x00000068u, 0x00050081u,
    0x00000007u, 0x00000097u, 0x00000095u, 0x00000096u, 0x00050085u, 0x00000007u, 0x00000098u, 0x0000008Bu, 0x0000006Bu,
    0x00050081u, 0x00000007u, 0x00000099u, 0x00000097u, 0x00000098u, 0x00050085u, 0x00000007u, 0x0000009Au, 0x00000077u,
    0x00000065u, 0x00050085u, 0x00000007u, 0x0000009Bu, 0x00000083u, 0x00000068u, 0x00050081u, 0x00000007u, 0x0000009Cu,
    0x0000009Au, 0x0000009Bu, 0x00050085u, 0x00000007u, 0x0000009Du, 0x0000008Fu, 0x0000006Bu, 0x00050081u, 0x00000007u,
    0x0000009Eu, 0x0000009Cu, 0x0000009Du, 0x00050088u, 0x00000007u, 0x0000009Fu, 0x0000001Bu, 0x00000051u, 0x00060050u,
    0x00000009u, 0x000000A0u, 0x00000019u, 0x00000019u, 0x00000019u, 0x00060050u, 0x00000009u, 0x000000A1u, 0x00000094u,
    0x00000099u, 0x0000009Eu, 0x0007000Cu, 0x00000009u, 0x000000A2u, 0x00000001u, 0x00000028u, 0x000000A1u, 0x000000A0u,
    0x00060050u, 0x00000009u, 0x000000A3u, 0x0000009Fu, 0x0000009Fu, 0x0000009Fu, 0x0007000Cu, 0x00000009u, 0x000000A4u,
    0x00000001u, 0x0000001Au, 0x000000A2u, 0x000000A3u, 0x00050051u, 0x00000007u, 0x000000A5u, 0x000000A4u, 0x00000000u,
    0x00050051u, 0x00000007u, 0x000000A6u, 0x000000A4u, 0x00000001u, 0x00050051u, 0x00000007u, 0x000000A7u, 0x000000A4u,
    0x00000002u, 0x00060041u, 0x0000003Cu, 0x000000A8u, 0x0000003Bu, 0x00000013u, 0x0000000Bu, 0x0004003Du, 0x00000007u,
    0x000000A9u, 0x000000A8u, 0x00060041u, 0x0000003Cu, 0x000000AAu, 0x0000003Bu, 0x00000013u, 0x0000000Cu, 0x0004003Du,
    0x00000007u, 0x000000ABu, 0x000000AAu, 0x00060041u, 0x0000003Cu, 0x000000ACu, 0x0000003Bu, 0x00000013u, 0x0000000Du,
    0x0004003Du, 0x00000007u, 0x000000ADu, 0x000000ACu, 0x00060041u, 0x0000003Cu, 0x000000AEu, 0x0000003Bu, 0x00000013u,
    0x0000000Fu, 0x0004003Du, 0x00000007u, 0x000000AFu, 0x000000AEu, 0x00060041u, 0x0000003Cu, 0x000000B0u, 0x0000003Bu,
    0x00000013u, 0x00000010u, 0x0004003Du, 0x00000007u, 0x000000B1u, 0x000000B0u, 0x00060041u, 0x0000003Cu, 0x000000B2u,
    0x0000003Bu, 0x00000013u, 0x00000011u, 0x0004003Du, 0x00000007u, 0x000000B3u, 0x000000B2u, 0x00060041u, 0x0000003Cu,
    0x000000B4u, 0x0000003Bu, 0x00000013u, 0x00000013u, 0x0004003Du, 0x00000007u, 0x000000B5u, 0x000000B4u, 0x00060041u,
    0x0000003Cu, 0x000000B6u, 0x0000003Bu, 0x00000013u, 0x00000014u, 0x0004003Du, 0x00000007u, 0x000000B7u, 0x000000B6u,
    0x00060041u, 0x0000003Cu, 0x000000B8u, 0x0000003Bu, 0x00000013u, 0x00000015u, 0x0004003Du, 0x00000007u, 0x000000B9u,
    0x000000B8u, 0x00050085u, 0x00000007u, 0x000000BAu, 0x000000A9u, 0x000000A5u, 0x00050085u, 0x00000007u, 0x000000BBu,
    0x000000AFu, 0x000000A6u, 0x00050081u, 0x00000007u, 0x000000BCu, 0x000000BAu, 0x000000BBu, 0x00050085u, 0x00000007u,
    0x000000BDu, 0x000000B5u, 0x000000A7u, 0x00050081u, 0x00000007u, 0x000000BEu, 0x000000BCu, 0x000000BDu, 0x00050085u,
    0x00000007u, 0x000000BFu, 0x000000ABu, 0x000000A5u, 0x00050085u, 0x00000007u, 0x000000C0u, 0x000000B1u, 0x000000A6u,
    0x00050081u, 0x00000007u, 0x000000C1u, 0x000000BFu, 0x000000C0u, 0x00050085u, 0x00000007u, 0x000000C2u, 0x000000B7u,
    0x000000A7u, 0x00050081u, 0x00000007u, 0x000000C3u, 0x000000C1u, 0x000000C2u, 0x00050085u, 0x00000007u, 0x000000C4u,
    0x000000ADu, 0x000000A5u, 0x00050085u, 0x00000007u, 0x000000C5u, 0x000000B3u, 0x000000A6u, 0x00050081u, 0x00000007u,
    0x000000C6u, 0x000000C4u, 0x000000C5u, 0x00050085u, 0x00000007u, 0x000000C7u, 0x000000B9u, 0x000000A7u, 0x00050081u,
    0x00000007u, 0x000000C8u, 0x000000C6u, 0x000000C7u, 0x00070050u, 0x0000000Au, 0x000000C9u, 0x000000BEu, 0x000000C3u,
    0x000000C8u, 0x00000045u, 0x0003003Eu, 0x00000038u, 0x000000C9u, 0x000100FDu, 0x00010038u
};
static const size_t spv_fs_content_size = sizeof(spv_fs_content);

static const uint32_t spv_fs_smooth[] = {0};

static const size_t spv_fs_smooth_size = sizeof(spv_fs_smooth);

enum {
    V_LAYER_NONE = 0,
    V_LAYER_BASE,
    V_LAYER_BATTERY,
    V_LAYER_BRIGHT,
    V_LAYER_VOLUME,
    V_LAYER_NOTIF,
};

static inline uint64_t v_tex_key(uint16_t layer, uint64_t payload) {
    return ((uint64_t) layer << 48) | (payload & 0xFFFFFFFFFFFFULL);
}

typedef struct {
    VkImage image;
    VkImageView view;
    VkDeviceMemory memory;
    VkDescriptorSet desc;
    uint32_t width;
    uint32_t height;
    uint64_t key;
} v_tex_t;

typedef struct {
    VkDevice device;
    VkPhysicalDevice phys;
    uint32_t graphics_qf;
    VkQueue graphics_queue;

    VkCommandPool cmd_pool;
    VkCommandBuffer xfer_cmdbuf;
    VkFence xfer_fence;
    VkBuffer staging_buf;
    VkDeviceMemory staging_mem;
    void *staging_map;
    int staging_host_coherent;

    VkDescriptorSetLayout set_layout;
    VkDescriptorPool desc_pool;
    VkSampler sampler;
    VkPipelineCache pipeline_cache;

    VkShaderModule vs_quad;
    VkShaderModule fs_overlay;
    VkShaderModule fs_solid;
    VkShaderModule fs_content;
    VkShaderModule fs_smooth;

    VkPipelineLayout layout_overlay;
    VkPipelineLayout layout_content;
    VkPipelineLayout layout_smooth;
    VkPipelineLayout layout_user;

    VkRenderPass rp_offscreen;

    struct {
        VkFormat format;
        VkRenderPass rp;
    } rp_present[V_MAX_SWAPCHAINS];
    int rp_present_count;

    VkPipeline pipe_overlay_offscreen;
    VkPipeline pipe_content_offscreen;
    VkPipeline pipe_smooth_offscreen;

    struct {
        VkFormat format;
        VkPipeline pipe_overlay;
        VkPipeline pipe_solid;
        VkPipeline pipe_content;
        VkPipeline pipe_user_shader;
    } pipe_present[V_MAX_SWAPCHAINS];
    int pipe_present_count;

    VkShaderModule fs_user_module;
    char user_shader_name[128];
    time_t user_shader_mtime;

    VkPhysicalDeviceMemoryProperties mem_props;

    v_tex_t textures[V_MAX_TRACKED_TEX];

    int ready;
    int init_failed;
} v_device_t;

static v_device_t v_devices[V_MAX_DEVICES];
static pthread_mutex_t v_devices_lock = PTHREAD_MUTEX_INITIALIZER;

static v_device_t *v_device_find(VkDevice d) {
    for (int i = 0; i < V_MAX_DEVICES; i++) {
        if (v_devices[i].device == d) return &v_devices[i];
    }
    return NULL;
}

static v_device_t *v_device_alloc(void) {
    for (int i = 0; i < V_MAX_DEVICES; i++) {
        if (v_devices[i].device == VK_NULL_HANDLE) return &v_devices[i];
    }
    return NULL;
}

typedef struct {
    uint64_t base_key, battery_key, bright_key, volume_key;
    int base_anchor, base_scale;
    int battery_anchor, battery_scale, battery_step;
    int bright_anchor, bright_scale, bright_step, bright_visible;
    int volume_anchor, volume_scale, volume_step, volume_visible;
    float base_alpha, battery_alpha, bright_alpha, volume_alpha;
    int fb_w, fb_h, rot;
} v_batch_state_t;

typedef struct {
    VkImage image;
    VkImageView view;
    VkFramebuffer framebuffer;
    VkCommandBuffer cmdbuf;
    VkFence fence;
    VkSemaphore signal;

    VkBuffer quad_buf;
    VkDeviceMemory quad_mem;
    void *quad_map;
    int quad_host_coherent;
    uint32_t quad_used;

    VkImage content_image;
    VkDeviceMemory content_mem;
    VkImageView content_view;
    VkFramebuffer content_fb;
    VkDescriptorSet content_sample_desc;

    VkImage overlay_image;
    VkDeviceMemory overlay_mem;
    VkImageView overlay_view;
    VkFramebuffer overlay_fb;
    VkDescriptorSet overlay_sample_desc;

    int valid;
} v_image_state_t;

typedef struct {
    VkSwapchainKHR swapchain;
    v_device_t *dev;

    VkFormat format;
    VkExtent2D extent;
    VkImageUsageFlags usage;

    uint32_t image_count;
    v_image_state_t images[V_MAX_SWAPCHAIN_I];

    int inject_fail;
    int ready;
} v_swapchain_t;

static v_swapchain_t v_swapchains[V_MAX_SWAPCHAINS];
static pthread_mutex_t v_swapchains_lock = PTHREAD_MUTEX_INITIALIZER;

static v_swapchain_t *v_swapchain_find(VkSwapchainKHR s) {
    for (int i = 0; i < V_MAX_SWAPCHAINS; i++) {
        if (v_swapchains[i].swapchain == s) return &v_swapchains[i];
    }
    return NULL;
}

static v_swapchain_t *v_swapchain_alloc(void) {
    for (int i = 0; i < V_MAX_SWAPCHAINS; i++) {
        if (v_swapchains[i].swapchain == VK_NULL_HANDLE) return &v_swapchains[i];
    }
    return NULL;
}

static uint32_t v_find_memory_type(
    v_device_t *dev, uint32_t type_bits, VkMemoryPropertyFlags required, VkMemoryPropertyFlags *out_actual
) {
    for (uint32_t i = 0; i < dev->mem_props.memoryTypeCount; i++) {
        if (!(type_bits & (1u << i))) continue;
        VkMemoryPropertyFlags f = dev->mem_props.memoryTypes[i].propertyFlags;
        if ((f & required) == required) {
            if (out_actual) *out_actual = f;
            return i;
        }
    }
    return UINT32_MAX;
}

static int v_alloc_image(
    v_device_t *dev, uint32_t w, uint32_t h, VkFormat fmt, VkImageUsageFlags usage, VkImage *out_img,
    VkDeviceMemory *out_mem
) {
    VkImageCreateInfo ci = {VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO};
    ci.imageType = VK_IMAGE_TYPE_2D;
    ci.format = fmt;
    ci.extent.width = w;
    ci.extent.height = h;
    ci.extent.depth = 1;
    ci.mipLevels = 1;
    ci.arrayLayers = 1;
    ci.samples = VK_SAMPLE_COUNT_1_BIT;
    ci.tiling = VK_IMAGE_TILING_OPTIMAL;
    ci.usage = usage;
    ci.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
    ci.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;

    if (vk_dl.CreateImage(dev->device, &ci, NULL, out_img) != VK_SUCCESS) return 0;

    VkMemoryRequirements req;
    vk_dl.GetImageMemoryRequirements(dev->device, *out_img, &req);

    uint32_t mt = v_find_memory_type(dev, req.memoryTypeBits, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT, NULL);
    if (mt == UINT32_MAX) {
        vk_dl.DestroyImage(dev->device, *out_img, NULL);
        *out_img = VK_NULL_HANDLE;
        return 0;
    }

    VkMemoryAllocateInfo ai = {VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO};
    ai.allocationSize = req.size;
    ai.memoryTypeIndex = mt;

    if (vk_dl.AllocateMemory(dev->device, &ai, NULL, out_mem) != VK_SUCCESS) {
        vk_dl.DestroyImage(dev->device, *out_img, NULL);
        *out_img = VK_NULL_HANDLE;
        return 0;
    }

    if (vk_dl.BindImageMemory(dev->device, *out_img, *out_mem, 0) != VK_SUCCESS) {
        vk_dl.FreeMemory(dev->device, *out_mem, NULL);
        vk_dl.DestroyImage(dev->device, *out_img, NULL);
        *out_img = VK_NULL_HANDLE;
        *out_mem = VK_NULL_HANDLE;
        return 0;
    }

    return 1;
}

static VkImageView v_make_view(v_device_t *dev, VkImage img, VkFormat fmt) {
    VkImageViewCreateInfo ci = {VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO};
    ci.image = img;
    ci.viewType = VK_IMAGE_VIEW_TYPE_2D;
    ci.format = fmt;
    ci.components.r = VK_COMPONENT_SWIZZLE_IDENTITY;
    ci.components.g = VK_COMPONENT_SWIZZLE_IDENTITY;
    ci.components.b = VK_COMPONENT_SWIZZLE_IDENTITY;
    ci.components.a = VK_COMPONENT_SWIZZLE_IDENTITY;
    ci.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
    ci.subresourceRange.levelCount = 1;
    ci.subresourceRange.layerCount = 1;

    VkImageView view = VK_NULL_HANDLE;
    if (vk_dl.CreateImageView(dev->device, &ci, NULL, &view) != VK_SUCCESS) return VK_NULL_HANDLE;
    return view;
}

static int v_alloc_staging(v_device_t *dev) {
    VkBufferCreateInfo bci = {VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO};
    bci.size = V_STAGING_SIZE;
    bci.usage = VK_BUFFER_USAGE_TRANSFER_SRC_BIT;
    bci.sharingMode = VK_SHARING_MODE_EXCLUSIVE;

    if (vk_dl.CreateBuffer(dev->device, &bci, NULL, &dev->staging_buf) != VK_SUCCESS) return 0;

    VkMemoryRequirements req;
    vk_dl.GetBufferMemoryRequirements(dev->device, dev->staging_buf, &req);

    VkMemoryPropertyFlags actual = 0;
    uint32_t mt = v_find_memory_type(dev, req.memoryTypeBits, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT, &actual);

    if (mt == UINT32_MAX) {
        vk_dl.DestroyBuffer(dev->device, dev->staging_buf, NULL);
        dev->staging_buf = VK_NULL_HANDLE;
        return 0;
    }

    dev->staging_host_coherent = (actual & VK_MEMORY_PROPERTY_HOST_COHERENT_BIT) ? 1 : 0;

    VkMemoryAllocateInfo ai = {VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO};
    ai.allocationSize = req.size;
    ai.memoryTypeIndex = mt;
    if (vk_dl.AllocateMemory(dev->device, &ai, NULL, &dev->staging_mem) != VK_SUCCESS) goto fail;
    if (vk_dl.BindBufferMemory(dev->device, dev->staging_buf, dev->staging_mem, 0) != VK_SUCCESS) goto fail;
    if (vk_dl.MapMemory(dev->device, dev->staging_mem, 0, VK_WHOLE_SIZE, 0, &dev->staging_map) != VK_SUCCESS) goto fail;

    VkCommandBufferAllocateInfo cbai = {VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO};
    cbai.commandPool = dev->cmd_pool;
    cbai.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
    cbai.commandBufferCount = 1;
    if (vk_dl.AllocateCommandBuffers(dev->device, &cbai, &dev->xfer_cmdbuf) != VK_SUCCESS) goto fail;

    VkFenceCreateInfo fci = {VK_STRUCTURE_TYPE_FENCE_CREATE_INFO};
    if (vk_dl.CreateFence(dev->device, &fci, NULL, &dev->xfer_fence) != VK_SUCCESS) goto fail;

    return 1;

fail:
    if (dev->staging_map) {
        vk_dl.UnmapMemory(dev->device, dev->staging_mem);
        dev->staging_map = NULL;
    }
    if (dev->staging_mem) {
        vk_dl.FreeMemory(dev->device, dev->staging_mem, NULL);
        dev->staging_mem = VK_NULL_HANDLE;
    }
    if (dev->staging_buf) {
        vk_dl.DestroyBuffer(dev->device, dev->staging_buf, NULL);
        dev->staging_buf = VK_NULL_HANDLE;
    }
    return 0;
}

static void v_free_staging(v_device_t *dev) {
    if (dev->xfer_fence) vk_dl.DestroyFence(dev->device, dev->xfer_fence, NULL);
    if (dev->xfer_cmdbuf) vk_dl.FreeCommandBuffers(dev->device, dev->cmd_pool, 1, &dev->xfer_cmdbuf);
    if (dev->staging_map) vk_dl.UnmapMemory(dev->device, dev->staging_mem);
    if (dev->staging_mem) vk_dl.FreeMemory(dev->device, dev->staging_mem, NULL);
    if (dev->staging_buf) vk_dl.DestroyBuffer(dev->device, dev->staging_buf, NULL);

    dev->xfer_fence = VK_NULL_HANDLE;
    dev->xfer_cmdbuf = VK_NULL_HANDLE;
    dev->staging_map = NULL;
    dev->staging_mem = VK_NULL_HANDLE;
    dev->staging_buf = VK_NULL_HANDLE;
}

static int v_alloc_host_buffer(
    v_device_t *dev, VkDeviceSize size, VkBufferUsageFlags usage, VkBuffer *out_buf, VkDeviceMemory *out_mem,
    void **out_map, int *out_host_coherent
) {
    if (!dev || size == 0 || !out_buf || !out_mem || !out_map || !out_host_coherent) return 0;

    *out_buf = VK_NULL_HANDLE;
    *out_mem = VK_NULL_HANDLE;
    *out_map = NULL;
    *out_host_coherent = 0;

    VkBufferCreateInfo bci = {VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO};
    bci.size = size;
    bci.usage = usage;
    bci.sharingMode = VK_SHARING_MODE_EXCLUSIVE;

    if (vk_dl.CreateBuffer(dev->device, &bci, NULL, out_buf) != VK_SUCCESS) return 0;

    VkMemoryRequirements req;
    vk_dl.GetBufferMemoryRequirements(dev->device, *out_buf, &req);

    VkMemoryPropertyFlags actual = 0;
    uint32_t mt = v_find_memory_type(dev, req.memoryTypeBits, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT, &actual);
    if (mt == UINT32_MAX) goto fail;

    *out_host_coherent = (actual & VK_MEMORY_PROPERTY_HOST_COHERENT_BIT) ? 1 : 0;

    VkMemoryAllocateInfo ai = {VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO};
    ai.allocationSize = req.size;
    ai.memoryTypeIndex = mt;

    if (vk_dl.AllocateMemory(dev->device, &ai, NULL, out_mem) != VK_SUCCESS) goto fail;
    if (vk_dl.BindBufferMemory(dev->device, *out_buf, *out_mem, 0) != VK_SUCCESS) goto fail;
    if (vk_dl.MapMemory(dev->device, *out_mem, 0, VK_WHOLE_SIZE, 0, out_map) != VK_SUCCESS) goto fail;

    return 1;

fail:
    if (*out_map) {
        vk_dl.UnmapMemory(dev->device, *out_mem);
        *out_map = NULL;
    }
    if (*out_mem) {
        vk_dl.FreeMemory(dev->device, *out_mem, NULL);
        *out_mem = VK_NULL_HANDLE;
    }
    if (*out_buf) {
        vk_dl.DestroyBuffer(dev->device, *out_buf, NULL);
        *out_buf = VK_NULL_HANDLE;
    }
    *out_host_coherent = 0;
    return 0;
}

static int v_alloc_quad_buffer(v_device_t *dev, v_image_state_t *st) {
    return v_alloc_host_buffer(
        dev, V_QUAD_BUFFER_SIZE, VK_BUFFER_USAGE_VERTEX_BUFFER_BIT, &st->quad_buf, &st->quad_mem, &st->quad_map,
        &st->quad_host_coherent
    );
}

static void v_free_quad_buffer(v_device_t *dev, v_image_state_t *st) {
    if (st->quad_map) vk_dl.UnmapMemory(dev->device, st->quad_mem);
    if (st->quad_mem) vk_dl.FreeMemory(dev->device, st->quad_mem, NULL);
    if (st->quad_buf) vk_dl.DestroyBuffer(dev->device, st->quad_buf, NULL);

    st->quad_map = NULL;
    st->quad_mem = VK_NULL_HANDLE;
    st->quad_buf = VK_NULL_HANDLE;
    st->quad_host_coherent = 0;
    st->quad_used = 0;
}

static void v_flush_quad_buffer(v_device_t *dev, v_image_state_t *st) {
    if (!st->quad_map || st->quad_host_coherent || st->quad_used == 0) return;

    VkMappedMemoryRange r = {VK_STRUCTURE_TYPE_MAPPED_MEMORY_RANGE};
    r.memory = st->quad_mem;
    r.offset = 0;
    r.size = VK_WHOLE_SIZE;
    vk_dl.FlushMappedMemoryRanges(dev->device, 1, &r);
}

static int v_upload_rgba(v_device_t *dev, const void *pixels, uint32_t w, uint32_t h, VkImage dst_image) {
    const size_t bytes = (size_t) w * (size_t) h * 4u;
    if (bytes == 0 || bytes > V_STAGING_SIZE) {
        LOG_WARN("stage", "[vk] upload size %zu out of range", bytes);
        return 0;
    }

    memcpy(dev->staging_map, pixels, bytes);
    if (!dev->staging_host_coherent) {
        VkMappedMemoryRange r = {VK_STRUCTURE_TYPE_MAPPED_MEMORY_RANGE};
        r.memory = dev->staging_mem;
        r.offset = 0;
        r.size = VK_WHOLE_SIZE;
        vk_dl.FlushMappedMemoryRanges(dev->device, 1, &r);
    }

    vk_dl.ResetFences(dev->device, 1, &dev->xfer_fence);
    vk_dl.ResetCommandBuffer(dev->xfer_cmdbuf, 0);

    VkCommandBufferBeginInfo bi = {VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO};
    bi.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;
    if (vk_dl.BeginCommandBuffer(dev->xfer_cmdbuf, &bi) != VK_SUCCESS) return 0;

    VkImageMemoryBarrier to_dst = {VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER};
    to_dst.srcAccessMask = 0;
    to_dst.dstAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
    to_dst.oldLayout = VK_IMAGE_LAYOUT_UNDEFINED;
    to_dst.newLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
    to_dst.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    to_dst.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    to_dst.image = dst_image;
    to_dst.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
    to_dst.subresourceRange.levelCount = 1;
    to_dst.subresourceRange.layerCount = 1;
    vk_dl.CmdPipelineBarrier(
        dev->xfer_cmdbuf, VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT, VK_PIPELINE_STAGE_TRANSFER_BIT, 0, 0, NULL, 0, NULL, 1,
        &to_dst
    );

    VkBufferImageCopy region = {0};
    region.imageSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
    region.imageSubresource.layerCount = 1;
    region.imageExtent.width = w;
    region.imageExtent.height = h;
    region.imageExtent.depth = 1;
    vk_dl.CmdCopyBufferToImage(
        dev->xfer_cmdbuf, dev->staging_buf, dst_image, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, 1, &region
    );

    VkImageMemoryBarrier to_read = {VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER};
    to_read.srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
    to_read.dstAccessMask = VK_ACCESS_SHADER_READ_BIT;
    to_read.oldLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
    to_read.newLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
    to_read.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    to_read.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    to_read.image = dst_image;
    to_read.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
    to_read.subresourceRange.levelCount = 1;
    to_read.subresourceRange.layerCount = 1;
    vk_dl.CmdPipelineBarrier(
        dev->xfer_cmdbuf, VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT, 0, 0, NULL, 0, NULL, 1,
        &to_read
    );

    if (vk_dl.EndCommandBuffer(dev->xfer_cmdbuf) != VK_SUCCESS) return 0;

    VkSubmitInfo si = {VK_STRUCTURE_TYPE_SUBMIT_INFO};
    si.commandBufferCount = 1;
    si.pCommandBuffers = &dev->xfer_cmdbuf;
    if (vk_dl.QueueSubmit(dev->graphics_queue, 1, &si, dev->xfer_fence) != VK_SUCCESS) return 0;
    vk_dl.WaitForFences(dev->device, 1, &dev->xfer_fence, VK_TRUE, UINT64_MAX);
    return 1;
}

static v_tex_t *v_tex_find(v_device_t *dev, uint64_t key) {
    for (int i = 0; i < V_MAX_TRACKED_TEX; i++) {
        if (dev->textures[i].key == key && dev->textures[i].image != VK_NULL_HANDLE) {
            return &dev->textures[i];
        }
    }
    return NULL;
}

static v_tex_t *v_tex_alloc(v_device_t *dev) {
    for (int i = 0; i < V_MAX_TRACKED_TEX; i++) {
        if (dev->textures[i].image == VK_NULL_HANDLE) return &dev->textures[i];
    }
    return NULL;
}

static void v_tex_destroy(v_device_t *dev, v_tex_t *t) {
    if (!t || t->image == VK_NULL_HANDLE) return;
    if (t->desc) vk_dl.FreeDescriptorSets(dev->device, dev->desc_pool, 1, &t->desc);
    if (t->view) vk_dl.DestroyImageView(dev->device, t->view, NULL);
    vk_dl.DestroyImage(dev->device, t->image, NULL);
    if (t->memory) vk_dl.FreeMemory(dev->device, t->memory, NULL);
    memset(t, 0, sizeof(*t));
}

static v_tex_t *v_tex_get_or_upload_rgba(v_device_t *dev, uint64_t key, const void *pixels, uint32_t w, uint32_t h);

static v_tex_t *v_tex_get_or_upload_rgba_pitched(
    v_device_t *dev, uint64_t key, const uint8_t *pixels, uint32_t w, uint32_t h, int pitch
) {
    if (!pixels || w == 0 || h == 0 || pitch <= 0) return NULL;

    const size_t row_bytes = (size_t) w * 4u;
    if ((size_t) pitch == row_bytes) {
        return v_tex_get_or_upload_rgba(dev, key, pixels, w, h);
    }

    const size_t bytes = row_bytes * (size_t) h;
    uint8_t *packed = malloc(bytes);
    if (!packed) return NULL;

    for (uint32_t y = 0; y < h; y++) {
        memcpy(packed + (size_t) y * row_bytes, pixels + (size_t) y * (size_t) pitch, row_bytes);
    }

    v_tex_t *out = v_tex_get_or_upload_rgba(dev, key, packed, w, h);
    free(packed);
    return out;
}

static v_tex_t *v_tex_get_or_upload_rgba(v_device_t *dev, uint64_t key, const void *pixels, uint32_t w, uint32_t h) {
    v_tex_t *t = v_tex_find(dev, key);
    if (t) return t;

    t = v_tex_alloc(dev);
    if (!t) {
        LOG_WARN("stage", "[vk] texture cache full");
        return NULL;
    }

    if (!v_alloc_image(
            dev, w, h, VK_FORMAT_R8G8B8A8_UNORM, VK_IMAGE_USAGE_SAMPLED_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT,
            &t->image, &t->memory
        )) {
        return NULL;
    }
    t->view = v_make_view(dev, t->image, VK_FORMAT_R8G8B8A8_UNORM);
    if (t->view == VK_NULL_HANDLE) {
        v_tex_destroy(dev, t);
        return NULL;
    }

    if (!v_upload_rgba(dev, pixels, w, h, t->image)) {
        v_tex_destroy(dev, t);
        return NULL;
    }

    VkDescriptorSetAllocateInfo dsai = {VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO};
    dsai.descriptorPool = dev->desc_pool;
    dsai.descriptorSetCount = 1;
    dsai.pSetLayouts = &dev->set_layout;
    if (vk_dl.AllocateDescriptorSets(dev->device, &dsai, &t->desc) != VK_SUCCESS) {
        v_tex_destroy(dev, t);
        return NULL;
    }

    VkDescriptorImageInfo dii = {dev->sampler, t->view, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL};
    VkWriteDescriptorSet wr = {VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET};
    wr.dstSet = t->desc;
    wr.descriptorCount = 1;
    wr.descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
    wr.pImageInfo = &dii;
    vk_dl.UpdateDescriptorSets(dev->device, 1, &wr, 0, NULL);

    t->width = w;
    t->height = h;
    t->key = key;
    return t;
}

static v_tex_t *v_tex_get_or_upload_png(v_device_t *dev, uint64_t key, const char *path) {
    v_tex_t *t = v_tex_find(dev, key);
    if (t) return t;

    SDL_Surface *raw = IMG_Load(path);
    if (!raw) {
        LOG_WARN("stage", "[vk] PNG load failed: %s", path);
        return NULL;
    }
    SDL_Surface *rgba = raw;
    if (raw->format->format != SDL_PIXELFORMAT_RGBA32) {
        rgba = SDL_ConvertSurfaceFormat(raw, SDL_PIXELFORMAT_RGBA32, 0);
        SDL_FreeSurface(raw);
        if (!rgba) return NULL;
    }

    SDL_LockSurface(rgba);
    v_tex_t *out = v_tex_get_or_upload_rgba_pitched(
        dev, key, (const uint8_t *) rgba->pixels, (uint32_t) rgba->w, (uint32_t) rgba->h, rgba->pitch
    );
    SDL_UnlockSurface(rgba);
    SDL_FreeSurface(rgba);
    return out;
}

static v_tex_t *v_tex_get_or_upload_surface(v_device_t *dev, uint64_t key, SDL_Surface *surf) {
    if (!surf) return NULL;
    SDL_Surface *rgba = surf;
    if (surf->format->format != SDL_PIXELFORMAT_RGBA32) {
        rgba = SDL_ConvertSurfaceFormat(surf, SDL_PIXELFORMAT_RGBA32, 0);
        if (!rgba) return NULL;
    }
    SDL_LockSurface(rgba);
    v_tex_t *out = v_tex_get_or_upload_rgba_pitched(
        dev, key, (const uint8_t *) rgba->pixels, (uint32_t) rgba->w, (uint32_t) rgba->h, rgba->pitch
    );
    SDL_UnlockSurface(rgba);
    if (rgba != surf) SDL_FreeSurface(rgba);
    return out;
}

static void v_tex_evict(v_device_t *dev, uint64_t key) __attribute__((unused));

static void v_tex_evict(v_device_t *dev, uint64_t key) {
    v_tex_t *t = v_tex_find(dev, key);
    if (!t) return;
    vk_dl.DeviceWaitIdle(dev->device);
    v_tex_destroy(dev, t);
}

static void v_tex_evict_layer(v_device_t *dev, uint16_t layer) __attribute__((unused));

static void v_tex_evict_layer(v_device_t *dev, uint16_t layer) {
    int touched = 0;
    for (int i = 0; i < V_MAX_TRACKED_TEX; i++) {
        v_tex_t *t = &dev->textures[i];
        if (t->image == VK_NULL_HANDLE) continue;
        if ((uint16_t) (t->key >> 48) != layer) continue;
        if (!touched) {
            vk_dl.DeviceWaitIdle(dev->device);
            touched = 1;
        }
        v_tex_destroy(dev, t);
    }
}

static VkShaderModule v_make_shader(v_device_t *dev, const uint32_t *code, size_t size) {
    if (!code || size == 0 || (size == sizeof(uint32_t) && code[0] == 0)) {
        return VK_NULL_HANDLE;
    }
    VkShaderModuleCreateInfo ci = {VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO};
    ci.codeSize = size;
    ci.pCode = code;
    VkShaderModule mod = VK_NULL_HANDLE;
    if (vk_dl.CreateShaderModule(dev->device, &ci, NULL, &mod) != VK_SUCCESS) {
        LOG_WARN("stage", "[vk] vkCreateShaderModule failed");
        return VK_NULL_HANDLE;
    }
    return mod;
}

static VkRenderPass v_make_renderpass(
    v_device_t *dev, VkFormat fmt, VkImageLayout initial, VkImageLayout final, VkAttachmentLoadOp load_op
) {
    VkAttachmentDescription att = {0};
    att.format = fmt;
    att.samples = VK_SAMPLE_COUNT_1_BIT;
    att.loadOp = load_op;
    att.storeOp = VK_ATTACHMENT_STORE_OP_STORE;
    att.stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
    att.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
    att.initialLayout = initial;
    att.finalLayout = final;

    VkAttachmentReference ref = {0, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL};

    VkSubpassDescription sp = {0};
    sp.pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS;
    sp.colorAttachmentCount = 1;
    sp.pColorAttachments = &ref;

    VkSubpassDependency deps[2] = {0};
    deps[0].srcSubpass = VK_SUBPASS_EXTERNAL;
    deps[0].dstSubpass = 0;
    deps[0].srcStageMask = VK_PIPELINE_STAGE_ALL_COMMANDS_BIT;
    deps[0].dstStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
    deps[0].srcAccessMask = VK_ACCESS_MEMORY_READ_BIT | VK_ACCESS_MEMORY_WRITE_BIT;
    deps[0].dstAccessMask = (load_op == VK_ATTACHMENT_LOAD_OP_LOAD)
                                ? (VK_ACCESS_COLOR_ATTACHMENT_READ_BIT | VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT)
                                : VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;
    deps[1].srcSubpass = 0;
    deps[1].dstSubpass = VK_SUBPASS_EXTERNAL;
    deps[1].srcStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
    deps[1].dstStageMask = VK_PIPELINE_STAGE_ALL_COMMANDS_BIT;
    deps[1].srcAccessMask = VK_ACCESS_COLOR_ATTACHMENT_READ_BIT | VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;
    deps[1].dstAccessMask = 0;

    VkRenderPassCreateInfo ci = {VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO};
    ci.attachmentCount = 1;
    ci.pAttachments = &att;
    ci.subpassCount = 1;
    ci.pSubpasses = &sp;
    ci.dependencyCount = 2;
    ci.pDependencies = deps;

    VkRenderPass rp = VK_NULL_HANDLE;
    if (vk_dl.CreateRenderPass(dev->device, &ci, NULL, &rp) != VK_SUCCESS) return VK_NULL_HANDLE;
    return rp;
}

static VkRenderPass v_get_present_renderpass(v_device_t *dev, VkFormat fmt) {
    for (int i = 0; i < dev->rp_present_count; i++) {
        if (dev->rp_present[i].format == fmt) return dev->rp_present[i].rp;
    }
    if (dev->rp_present_count >= (int) (sizeof(dev->rp_present) / sizeof(dev->rp_present[0]))) {
        return VK_NULL_HANDLE;
    }

    VkRenderPass rp = v_make_renderpass(
        dev, fmt, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,
        VK_ATTACHMENT_LOAD_OP_LOAD
    );
    if (rp == VK_NULL_HANDLE) return VK_NULL_HANDLE;

    dev->rp_present[dev->rp_present_count].format = fmt;
    dev->rp_present[dev->rp_present_count].rp = rp;
    dev->rp_present_count++;
    return rp;
}

static VkPipeline
v_make_pipeline(v_device_t *dev, VkShaderModule fs, VkPipelineLayout layout, VkRenderPass rp, int blend_enable) {
    if (dev->vs_quad == VK_NULL_HANDLE || fs == VK_NULL_HANDLE) return VK_NULL_HANDLE;

    VkPipelineShaderStageCreateInfo stages[2] = {0};
    stages[0].sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
    stages[0].stage = VK_SHADER_STAGE_VERTEX_BIT;
    stages[0].module = dev->vs_quad;
    stages[0].pName = "main";
    stages[1].sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
    stages[1].stage = VK_SHADER_STAGE_FRAGMENT_BIT;
    stages[1].module = fs;
    stages[1].pName = "main";

    VkVertexInputBindingDescription bind = {0};
    bind.binding = 0;
    bind.stride = sizeof(v_quad_vertex_t);
    bind.inputRate = VK_VERTEX_INPUT_RATE_VERTEX;

    VkVertexInputAttributeDescription attr[2] = {0};
    attr[0].location = 0;
    attr[0].binding = 0;
    attr[0].format = VK_FORMAT_R32G32_SFLOAT;
    attr[0].offset = offsetof(v_quad_vertex_t, x);
    attr[1].location = 1;
    attr[1].binding = 0;
    attr[1].format = VK_FORMAT_R32G32_SFLOAT;
    attr[1].offset = offsetof(v_quad_vertex_t, u);

    VkPipelineVertexInputStateCreateInfo vi = {VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO};
    vi.vertexBindingDescriptionCount = 1;
    vi.pVertexBindingDescriptions = &bind;
    vi.vertexAttributeDescriptionCount = 2;
    vi.pVertexAttributeDescriptions = attr;

    VkPipelineInputAssemblyStateCreateInfo ia = {VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO};
    ia.topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_STRIP;

    VkPipelineViewportStateCreateInfo vp = {VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO};
    vp.viewportCount = 1;
    vp.scissorCount = 1;

    VkPipelineRasterizationStateCreateInfo rs = {VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO};
    rs.polygonMode = VK_POLYGON_MODE_FILL;
    rs.cullMode = VK_CULL_MODE_NONE;
    rs.frontFace = VK_FRONT_FACE_COUNTER_CLOCKWISE;
    rs.lineWidth = 1.0f;

    VkPipelineMultisampleStateCreateInfo ms = {VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO};
    ms.rasterizationSamples = VK_SAMPLE_COUNT_1_BIT;

    VkPipelineColorBlendAttachmentState att = {0};
    att.colorWriteMask =
        VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT | VK_COLOR_COMPONENT_B_BIT | VK_COLOR_COMPONENT_A_BIT;
    if (blend_enable) {
        att.blendEnable = VK_TRUE;
        att.srcColorBlendFactor = VK_BLEND_FACTOR_SRC_ALPHA;
        att.dstColorBlendFactor = VK_BLEND_FACTOR_ONE_MINUS_SRC_ALPHA;
        att.colorBlendOp = VK_BLEND_OP_ADD;
        att.srcAlphaBlendFactor = VK_BLEND_FACTOR_ONE;
        att.dstAlphaBlendFactor = VK_BLEND_FACTOR_ONE_MINUS_SRC_ALPHA;
        att.alphaBlendOp = VK_BLEND_OP_ADD;
    }

    VkPipelineColorBlendStateCreateInfo cb = {VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO};
    cb.attachmentCount = 1;
    cb.pAttachments = &att;

    VkDynamicState dyn[] = {VK_DYNAMIC_STATE_VIEWPORT, VK_DYNAMIC_STATE_SCISSOR};
    VkPipelineDynamicStateCreateInfo ds = {VK_STRUCTURE_TYPE_PIPELINE_DYNAMIC_STATE_CREATE_INFO};
    ds.dynamicStateCount = sizeof(dyn) / sizeof(dyn[0]);
    ds.pDynamicStates = dyn;

    VkGraphicsPipelineCreateInfo ci = {VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO};
    ci.stageCount = 2;
    ci.pStages = stages;
    ci.pVertexInputState = &vi;
    ci.pInputAssemblyState = &ia;
    ci.pViewportState = &vp;
    ci.pRasterizationState = &rs;
    ci.pMultisampleState = &ms;
    ci.pColorBlendState = &cb;
    ci.pDynamicState = &ds;
    ci.layout = layout;
    ci.renderPass = rp;
    ci.subpass = 0;

    VkPipeline pipe = VK_NULL_HANDLE;
    if (vk_dl.CreateGraphicsPipelines(dev->device, dev->pipeline_cache, 1, &ci, NULL, &pipe) != VK_SUCCESS) {
        LOG_WARN("stage", "[vk] vkCreateGraphicsPipelines failed");
        return VK_NULL_HANDLE;
    }
    return pipe;
}

static VkPipelineLayout v_make_layout(v_device_t *dev, VkDescriptorSetLayout set_layout, uint32_t pc_size) {
    VkPushConstantRange pcr = {0};
    pcr.stageFlags = VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT;
    pcr.offset = 0;
    pcr.size = pc_size;

    VkPipelineLayoutCreateInfo ci = {VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO};
    ci.setLayoutCount = 1;
    ci.pSetLayouts = &set_layout;
    ci.pushConstantRangeCount = (pc_size > 0) ? 1 : 0;
    ci.pPushConstantRanges = (pc_size > 0) ? &pcr : NULL;

    VkPipelineLayout layout = VK_NULL_HANDLE;
    if (vk_dl.CreatePipelineLayout(dev->device, &ci, NULL, &layout) != VK_SUCCESS) return VK_NULL_HANDLE;
    return layout;
}

static VkPipelineLayout v_make_user_layout(v_device_t *dev, VkDescriptorSetLayout set_layout) {
    VkPushConstantRange pcr[2] = {0};
    pcr[0].stageFlags = VK_SHADER_STAGE_VERTEX_BIT;
    pcr[0].offset = 0;
    pcr[0].size = sizeof(v_push_overlay_t);
    pcr[1].stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT;
    pcr[1].offset = 0;
    pcr[1].size = sizeof(v_push_user_shader_t);

    VkPipelineLayoutCreateInfo ci = {VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO};
    ci.setLayoutCount = 1;
    ci.pSetLayouts = &set_layout;
    ci.pushConstantRangeCount = 2;
    ci.pPushConstantRanges = pcr;

    VkPipelineLayout layout = VK_NULL_HANDLE;
    if (vk_dl.CreatePipelineLayout(dev->device, &ci, NULL, &layout) != VK_SUCCESS) return VK_NULL_HANDLE;
    return layout;
}

static VkPipeline v_get_present_pipeline_overlay(v_device_t *dev, VkFormat fmt) {
    for (int i = 0; i < dev->pipe_present_count; i++) {
        if (dev->pipe_present[i].format == fmt) return dev->pipe_present[i].pipe_overlay;
    }
    if (dev->pipe_present_count >= (int) (sizeof(dev->pipe_present) / sizeof(dev->pipe_present[0]))) {
        return VK_NULL_HANDLE;
    }
    VkRenderPass rp = v_get_present_renderpass(dev, fmt);
    if (rp == VK_NULL_HANDLE) return VK_NULL_HANDLE;
    VkPipeline pipe = v_make_pipeline(dev, dev->fs_overlay, dev->layout_overlay, rp, 1);
    dev->pipe_present[dev->pipe_present_count].format = fmt;
    dev->pipe_present[dev->pipe_present_count].pipe_overlay = pipe;
    dev->pipe_present[dev->pipe_present_count].pipe_solid = VK_NULL_HANDLE;
    dev->pipe_present[dev->pipe_present_count].pipe_content = VK_NULL_HANDLE;
    dev->pipe_present[dev->pipe_present_count].pipe_user_shader = VK_NULL_HANDLE;
    dev->pipe_present_count++;
    return pipe;
}

static VkPipeline v_get_present_pipeline_content(v_device_t *dev, VkFormat fmt) {
    if (dev->fs_content == VK_NULL_HANDLE) return VK_NULL_HANDLE;

    for (int i = 0; i < dev->pipe_present_count; i++) {
        if (dev->pipe_present[i].format == fmt) {
            if (dev->pipe_present[i].pipe_content != VK_NULL_HANDLE) return dev->pipe_present[i].pipe_content;

            VkRenderPass rp = v_get_present_renderpass(dev, fmt);
            if (rp == VK_NULL_HANDLE) return VK_NULL_HANDLE;

            VkPipeline pipe = v_make_pipeline(dev, dev->fs_content, dev->layout_content, rp, 0);
            dev->pipe_present[i].pipe_content = pipe;
            return pipe;
        }
    }

    (void) v_get_present_pipeline_overlay(dev, fmt);
    for (int i = 0; i < dev->pipe_present_count; i++) {
        if (dev->pipe_present[i].format == fmt) {
            VkRenderPass rp = v_get_present_renderpass(dev, fmt);
            if (rp == VK_NULL_HANDLE) return VK_NULL_HANDLE;

            VkPipeline pipe = v_make_pipeline(dev, dev->fs_content, dev->layout_content, rp, 0);
            dev->pipe_present[i].pipe_content = pipe;
            return pipe;
        }
    }

    return VK_NULL_HANDLE;
}

static VkPipeline v_get_present_pipeline_user(v_device_t *dev, VkFormat fmt) {
    if (dev->fs_user_module == VK_NULL_HANDLE) return VK_NULL_HANDLE;

    for (int i = 0; i < dev->pipe_present_count; i++) {
        if (dev->pipe_present[i].format == fmt) {
            if (dev->pipe_present[i].pipe_user_shader != VK_NULL_HANDLE) return dev->pipe_present[i].pipe_user_shader;

            VkRenderPass rp = v_get_present_renderpass(dev, fmt);
            if (rp == VK_NULL_HANDLE) return VK_NULL_HANDLE;

            VkPipeline pipe = v_make_pipeline(dev, dev->fs_user_module, dev->layout_user, rp, 0);
            dev->pipe_present[i].pipe_user_shader = pipe;
            return pipe;
        }
    }

    (void) v_get_present_pipeline_overlay(dev, fmt);
    for (int i = 0; i < dev->pipe_present_count; i++) {
        if (dev->pipe_present[i].format == fmt) {
            VkRenderPass rp = v_get_present_renderpass(dev, fmt);
            if (rp == VK_NULL_HANDLE) return VK_NULL_HANDLE;

            VkPipeline pipe = v_make_pipeline(dev, dev->fs_user_module, dev->layout_user, rp, 0);
            dev->pipe_present[i].pipe_user_shader = pipe;
            return pipe;
        }
    }

    return VK_NULL_HANDLE;
}

static VkPipeline v_get_present_pipeline_solid(v_device_t *dev, VkFormat fmt) {
    if (dev->fs_solid == VK_NULL_HANDLE) return VK_NULL_HANDLE;

    for (int i = 0; i < dev->pipe_present_count; i++) {
        if (dev->pipe_present[i].format == fmt) {
            if (dev->pipe_present[i].pipe_solid != VK_NULL_HANDLE) return dev->pipe_present[i].pipe_solid;

            VkRenderPass rp = v_get_present_renderpass(dev, fmt);
            if (rp == VK_NULL_HANDLE) return VK_NULL_HANDLE;

            VkPipeline pipe = v_make_pipeline(dev, dev->fs_solid, dev->layout_overlay, rp, 1);
            dev->pipe_present[i].pipe_solid = pipe;
            return pipe;
        }
    }

    (void) v_get_present_pipeline_overlay(dev, fmt);
    for (int i = 0; i < dev->pipe_present_count; i++) {
        if (dev->pipe_present[i].format == fmt) {
            VkRenderPass rp = v_get_present_renderpass(dev, fmt);
            if (rp == VK_NULL_HANDLE) return VK_NULL_HANDLE;

            VkPipeline pipe = v_make_pipeline(dev, dev->fs_solid, dev->layout_overlay, rp, 1);
            dev->pipe_present[i].pipe_solid = pipe;
            return pipe;
        }
    }

    return VK_NULL_HANDLE;
}

static void v_user_shader_clear(v_device_t *dev) {
    if (!dev || dev->device == VK_NULL_HANDLE) return;

    vk_dl.DeviceWaitIdle(dev->device);

    for (int i = 0; i < dev->pipe_present_count; i++) {
        if (dev->pipe_present[i].pipe_user_shader) {
            vk_dl.DestroyPipeline(dev->device, dev->pipe_present[i].pipe_user_shader, NULL);
            dev->pipe_present[i].pipe_user_shader = VK_NULL_HANDLE;
        }
    }

    if (dev->fs_user_module) {
        vk_dl.DestroyShaderModule(dev->device, dev->fs_user_module, NULL);
        dev->fs_user_module = VK_NULL_HANDLE;
    }

    dev->user_shader_name[0] = '\0';
    dev->user_shader_mtime = 0;
}

static int v_read_shader_name(char *out, size_t out_sz) {
    if (!out || out_sz == 0) return 0;
    out[0] = '\0';

    if (!read_line_from_file(OVERLAY_RUNNER "shader", 1, out, out_sz)) return 0;
    if (!out[0] || strcmp(out, "none") == 0) return 0;

    return 1;
}

static void v_user_shader_sync(v_device_t *dev) {
    static int reload_tick = 0;
    if (dev->fs_user_module != VK_NULL_HANDLE) {
        if (++reload_tick < 60) return;
        reload_tick = 0;
    }

    char name[64];
    if (!v_read_shader_name(name, sizeof(name))) {
        if (dev->fs_user_module || dev->user_shader_name[0]) v_user_shader_clear(dev);
        return;
    }

    char spv_path[PATH_MAX];
    snprintf(spv_path, sizeof(spv_path), "%s/shader/%s.frag.spv", INTERNAL_SHARE, name);

    struct stat st;
    if (stat(spv_path, &st) != 0) {
        static char missing_name[64];
        if (strcmp(missing_name, name) != 0) {
            snprintf(missing_name, sizeof(missing_name), "%s", name);
            LOG_WARN("stage", "[vk] Vulkan shader SPIR-V missing: %s", spv_path);
        }
        if (dev->fs_user_module || strcmp(dev->user_shader_name, name) == 0) v_user_shader_clear(dev);
        return;
    }

    if (dev->fs_user_module && strcmp(dev->user_shader_name, name) == 0 && dev->user_shader_mtime == st.st_mtime) {
        return;
    }

    v_user_shader_clear(dev);

    FILE *f = fopen(spv_path, "rb");
    if (!f) return;

    if (fseek(f, 0, SEEK_END) != 0) {
        fclose(f);
        return;
    }

    long n = ftell(f);
    if (n <= 0 || n > 256 * 1024 || (n & 3)) {
        fclose(f);
        LOG_WARN("stage", "[vk] bad Vulkan shader SPIR-V size: %s", spv_path);
        return;
    }

    if (fseek(f, 0, SEEK_SET) != 0) {
        fclose(f);
        return;
    }

    uint32_t *buf = malloc((size_t) n);
    if (!buf) {
        fclose(f);
        return;
    }

    size_t got = fread(buf, 1, (size_t) n, f);
    fclose(f);

    if (got != (size_t) n) {
        free(buf);
        return;
    }

    dev->fs_user_module = v_make_shader(dev, buf, (size_t) n);
    free(buf);

    if (!dev->fs_user_module) {
        LOG_WARN("stage", "[vk] failed to create Vulkan shader module: %s", spv_path);
        return;
    }

    snprintf(dev->user_shader_name, sizeof(dev->user_shader_name), "%s", name);
    dev->user_shader_mtime = st.st_mtime;
    LOG_INFO("stage", "[vk] Vulkan shader loaded: %s", spv_path);
}

static void v_device_destroy(v_device_t *dev) {
    if (!dev || dev->device == VK_NULL_HANDLE) return;
    vk_dl.DeviceWaitIdle(dev->device);

    for (int i = 0; i < V_MAX_TRACKED_TEX; i++)
        v_tex_destroy(dev, &dev->textures[i]);

    if (dev->fs_user_module) vk_dl.DestroyShaderModule(dev->device, dev->fs_user_module, NULL);

    for (int i = 0; i < dev->pipe_present_count; i++) {
        if (dev->pipe_present[i].pipe_overlay)
            vk_dl.DestroyPipeline(dev->device, dev->pipe_present[i].pipe_overlay, NULL);
        if (dev->pipe_present[i].pipe_solid) vk_dl.DestroyPipeline(dev->device, dev->pipe_present[i].pipe_solid, NULL);
        if (dev->pipe_present[i].pipe_content)
            vk_dl.DestroyPipeline(dev->device, dev->pipe_present[i].pipe_content, NULL);
        if (dev->pipe_present[i].pipe_user_shader)
            vk_dl.DestroyPipeline(dev->device, dev->pipe_present[i].pipe_user_shader, NULL);
    }

    if (dev->pipe_overlay_offscreen) vk_dl.DestroyPipeline(dev->device, dev->pipe_overlay_offscreen, NULL);
    if (dev->pipe_content_offscreen) vk_dl.DestroyPipeline(dev->device, dev->pipe_content_offscreen, NULL);
    if (dev->pipe_smooth_offscreen) vk_dl.DestroyPipeline(dev->device, dev->pipe_smooth_offscreen, NULL);

    if (dev->layout_overlay) vk_dl.DestroyPipelineLayout(dev->device, dev->layout_overlay, NULL);
    if (dev->layout_content) vk_dl.DestroyPipelineLayout(dev->device, dev->layout_content, NULL);
    if (dev->layout_smooth) vk_dl.DestroyPipelineLayout(dev->device, dev->layout_smooth, NULL);
    if (dev->layout_user) vk_dl.DestroyPipelineLayout(dev->device, dev->layout_user, NULL);

    if (dev->vs_quad) vk_dl.DestroyShaderModule(dev->device, dev->vs_quad, NULL);
    if (dev->fs_overlay) vk_dl.DestroyShaderModule(dev->device, dev->fs_overlay, NULL);
    if (dev->fs_solid) vk_dl.DestroyShaderModule(dev->device, dev->fs_solid, NULL);
    if (dev->fs_content) vk_dl.DestroyShaderModule(dev->device, dev->fs_content, NULL);
    if (dev->fs_smooth) vk_dl.DestroyShaderModule(dev->device, dev->fs_smooth, NULL);

    if (dev->rp_offscreen) vk_dl.DestroyRenderPass(dev->device, dev->rp_offscreen, NULL);
    for (int i = 0; i < dev->rp_present_count; i++) {
        if (dev->rp_present[i].rp) vk_dl.DestroyRenderPass(dev->device, dev->rp_present[i].rp, NULL);
    }

    if (dev->pipeline_cache) vk_dl.DestroyPipelineCache(dev->device, dev->pipeline_cache, NULL);
    if (dev->sampler) vk_dl.DestroySampler(dev->device, dev->sampler, NULL);
    if (dev->desc_pool) vk_dl.DestroyDescriptorPool(dev->device, dev->desc_pool, NULL);
    if (dev->set_layout) vk_dl.DestroyDescriptorSetLayout(dev->device, dev->set_layout, NULL);

    v_free_staging(dev);
    if (dev->cmd_pool) vk_dl.DestroyCommandPool(dev->device, dev->cmd_pool, NULL);

    memset(dev, 0, sizeof(*dev));
}

static uint32_t v_pick_graphics_qf(VkPhysicalDevice phys, const VkDeviceCreateInfo *ci) {
    uint32_t n = 0;
    vk_dl.GetPhysicalDeviceQueueFamilyProperties(phys, &n, NULL);
    if (n == 0) return UINT32_MAX;

    VkQueueFamilyProperties props[16];
    if (n > 16) n = 16;
    vk_dl.GetPhysicalDeviceQueueFamilyProperties(phys, &n, props);

    if (ci) {
        for (uint32_t i = 0; i < ci->queueCreateInfoCount; i++) {
            uint32_t qf = ci->pQueueCreateInfos[i].queueFamilyIndex;
            if (qf < n && (props[qf].queueFlags & VK_QUEUE_GRAPHICS_BIT)) return qf;
        }
    }

    for (uint32_t i = 0; i < n; i++) {
        if (props[i].queueFlags & VK_QUEUE_GRAPHICS_BIT) return i;
    }
    return UINT32_MAX;
}

static int v_device_init(v_device_t *dev) {
    if (dev->ready) return 1;
    if (dev->init_failed) return 0;

    vk_dl.GetPhysicalDeviceMemoryProperties(dev->phys, &dev->mem_props);
    vk_dl.GetDeviceQueue(dev->device, dev->graphics_qf, 0, &dev->graphics_queue);

    VkCommandPoolCreateInfo cpci = {VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO};
    cpci.flags = VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT;
    cpci.queueFamilyIndex = dev->graphics_qf;
    if (vk_dl.CreateCommandPool(dev->device, &cpci, NULL, &dev->cmd_pool) != VK_SUCCESS) goto fail;

    if (!v_alloc_staging(dev)) goto fail;

    VkDescriptorSetLayoutBinding b = {0};
    b.binding = 0;
    b.descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
    b.descriptorCount = 1;
    b.stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT;

    VkDescriptorSetLayoutCreateInfo dslci = {VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO};
    dslci.bindingCount = 1;
    dslci.pBindings = &b;
    if (vk_dl.CreateDescriptorSetLayout(dev->device, &dslci, NULL, &dev->set_layout) != VK_SUCCESS) goto fail;

    VkDescriptorPoolSize ps = {VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, V_MAX_TRACKED_TEX + 64};
    VkDescriptorPoolCreateInfo dpci = {VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO};
    dpci.flags = VK_DESCRIPTOR_POOL_CREATE_FREE_DESCRIPTOR_SET_BIT;
    dpci.maxSets = V_MAX_TRACKED_TEX + 64;
    dpci.poolSizeCount = 1;
    dpci.pPoolSizes = &ps;
    if (vk_dl.CreateDescriptorPool(dev->device, &dpci, NULL, &dev->desc_pool) != VK_SUCCESS) goto fail;

    VkSamplerCreateInfo sci = {VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO};
    sci.magFilter = VK_FILTER_LINEAR;
    sci.minFilter = VK_FILTER_LINEAR;
    sci.mipmapMode = VK_SAMPLER_MIPMAP_MODE_LINEAR;
    sci.addressModeU = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
    sci.addressModeV = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
    sci.addressModeW = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
    sci.maxLod = 1.0f;
    if (vk_dl.CreateSampler(dev->device, &sci, NULL, &dev->sampler) != VK_SUCCESS) goto fail;

    VkPipelineCacheCreateInfo pcci = {VK_STRUCTURE_TYPE_PIPELINE_CACHE_CREATE_INFO};
    vk_dl.CreatePipelineCache(dev->device, &pcci, NULL, &dev->pipeline_cache);

    dev->vs_quad = v_make_shader(dev, spv_vs_quad, spv_vs_quad_size);
    dev->fs_overlay = v_make_shader(dev, spv_fs_overlay, spv_fs_overlay_size);
    dev->fs_solid = v_make_shader(dev, spv_fs_solid, spv_fs_solid_size);
    dev->fs_content = v_make_shader(dev, spv_fs_content, spv_fs_content_size);
    dev->fs_smooth = v_make_shader(dev, spv_fs_smooth, spv_fs_smooth_size);

    if (dev->vs_quad == VK_NULL_HANDLE || dev->fs_overlay == VK_NULL_HANDLE) {
        LOG_WARN("stage", "[vk] required overlay shaders unavailable - Vulkan indicators disabled");
        goto fail;
    }

    dev->layout_overlay = v_make_layout(dev, dev->set_layout, sizeof(v_push_overlay_t));
    dev->layout_content = v_make_layout(dev, dev->set_layout, sizeof(v_push_content_t));
    dev->layout_smooth = v_make_layout(dev, dev->set_layout, sizeof(v_push_smooth_t));
    dev->layout_user = v_make_user_layout(dev, dev->set_layout);

    dev->rp_offscreen = v_make_renderpass(
        dev, VK_FORMAT_R8G8B8A8_UNORM, VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
        VK_ATTACHMENT_LOAD_OP_CLEAR
    );

    if (dev->rp_offscreen == VK_NULL_HANDLE) goto fail;

    dev->pipe_overlay_offscreen = v_make_pipeline(dev, dev->fs_overlay, dev->layout_overlay, dev->rp_offscreen, 1);
    dev->pipe_content_offscreen = v_make_pipeline(dev, dev->fs_content, dev->layout_content, dev->rp_offscreen, 0);
    dev->pipe_smooth_offscreen = v_make_pipeline(dev, dev->fs_smooth, dev->layout_smooth, dev->rp_offscreen, 0);

    if (dev->pipe_overlay_offscreen == VK_NULL_HANDLE) {
        LOG_WARN("stage", "[vk] required overlay pipeline unavailable - Vulkan indicators disabled");
        goto fail;
    }

    dev->ready = 1;
    LOG_INFO("stage", "[vk] device ready (qf=%u)", dev->graphics_qf);
    return 1;

fail:
    LOG_WARN("stage", "[vk] device init failed");
    dev->init_failed = 1;
    v_device_destroy(dev);
    return 0;
}

static void v_image_state_destroy(v_device_t *dev, v_image_state_t *st) {
    if (!dev || !st) return;

    if (st->framebuffer) vk_dl.DestroyFramebuffer(dev->device, st->framebuffer, NULL);
    if (st->view) vk_dl.DestroyImageView(dev->device, st->view, NULL);

    if (st->content_fb) vk_dl.DestroyFramebuffer(dev->device, st->content_fb, NULL);
    if (st->content_view) vk_dl.DestroyImageView(dev->device, st->content_view, NULL);
    if (st->content_image) vk_dl.DestroyImage(dev->device, st->content_image, NULL);
    if (st->content_mem) vk_dl.FreeMemory(dev->device, st->content_mem, NULL);

    if (st->overlay_fb) vk_dl.DestroyFramebuffer(dev->device, st->overlay_fb, NULL);
    if (st->overlay_view) vk_dl.DestroyImageView(dev->device, st->overlay_view, NULL);
    if (st->overlay_image) vk_dl.DestroyImage(dev->device, st->overlay_image, NULL);
    if (st->overlay_mem) vk_dl.FreeMemory(dev->device, st->overlay_mem, NULL);

    if (st->cmdbuf) vk_dl.FreeCommandBuffers(dev->device, dev->cmd_pool, 1, &st->cmdbuf);
    if (st->fence) vk_dl.DestroyFence(dev->device, st->fence, NULL);
    if (st->signal) vk_dl.DestroySemaphore(dev->device, st->signal, NULL);

    v_free_quad_buffer(dev, st);

    if (st->content_sample_desc) vk_dl.FreeDescriptorSets(dev->device, dev->desc_pool, 1, &st->content_sample_desc);
    if (st->overlay_sample_desc) vk_dl.FreeDescriptorSets(dev->device, dev->desc_pool, 1, &st->overlay_sample_desc);

    memset(st, 0, sizeof(*st));
}

static void v_swapchain_destroy(v_swapchain_t *sc) {
    if (!sc->ready && sc->swapchain == VK_NULL_HANDLE) return;
    if (sc->dev) vk_dl.DeviceWaitIdle(sc->dev->device);

    for (uint32_t i = 0; i < sc->image_count; i++) {
        v_image_state_destroy(sc->dev, &sc->images[i]);
    }
    memset(sc, 0, sizeof(*sc));
}

static int v_image_state_build(v_device_t *dev, v_swapchain_t *sc, uint32_t idx, VkImage swap_image) {
    v_image_state_t *st = &sc->images[idx];
    st->image = swap_image;

    st->view = v_make_view(dev, swap_image, sc->format);
    if (st->view == VK_NULL_HANDLE) return 0;

    VkRenderPass rp = v_get_present_renderpass(dev, sc->format);
    if (rp == VK_NULL_HANDLE) return 0;

    VkFramebufferCreateInfo fbci = {VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO};
    fbci.renderPass = rp;
    fbci.attachmentCount = 1;
    fbci.pAttachments = &st->view;
    fbci.width = sc->extent.width;
    fbci.height = sc->extent.height;
    fbci.layers = 1;
    if (vk_dl.CreateFramebuffer(dev->device, &fbci, NULL, &st->framebuffer) != VK_SUCCESS) return 0;

    VkCommandBufferAllocateInfo cbai = {VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO};
    cbai.commandPool = dev->cmd_pool;
    cbai.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
    cbai.commandBufferCount = 1;
    if (vk_dl.AllocateCommandBuffers(dev->device, &cbai, &st->cmdbuf) != VK_SUCCESS) return 0;

    VkFenceCreateInfo fci = {VK_STRUCTURE_TYPE_FENCE_CREATE_INFO};
    fci.flags = VK_FENCE_CREATE_SIGNALED_BIT;
    if (vk_dl.CreateFence(dev->device, &fci, NULL, &st->fence) != VK_SUCCESS) return 0;

    VkSemaphoreCreateInfo sci = {VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO};
    if (vk_dl.CreateSemaphore(dev->device, &sci, NULL, &st->signal) != VK_SUCCESS) return 0;

    if (!v_alloc_quad_buffer(dev, st)) {
        LOG_WARN("stage", "[vk] failed to allocate per-image quad vertex buffer");
        return 0;
    }

    if (!v_alloc_image(
            dev, sc->extent.width, sc->extent.height, sc->format,
            VK_IMAGE_USAGE_SAMPLED_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT, &st->content_image, &st->content_mem
        )) {
        return 0;
    }
    st->content_view = v_make_view(dev, st->content_image, sc->format);
    if (st->content_view == VK_NULL_HANDLE) return 0;

    VkFramebufferCreateInfo cfbci = {VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO};
    cfbci.renderPass = dev->rp_offscreen;
    cfbci.width = sc->extent.width;
    cfbci.height = sc->extent.height;
    cfbci.layers = 1;

    if (!v_alloc_image(
            dev, sc->extent.width, sc->extent.height, VK_FORMAT_R8G8B8A8_UNORM,
            VK_IMAGE_USAGE_SAMPLED_BIT | VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT, &st->overlay_image, &st->overlay_mem
        )) {
        return 0;
    }
    st->overlay_view = v_make_view(dev, st->overlay_image, VK_FORMAT_R8G8B8A8_UNORM);
    if (st->overlay_view == VK_NULL_HANDLE) return 0;

    VkFramebufferCreateInfo ofbci = cfbci;
    ofbci.attachmentCount = 1;
    ofbci.pAttachments = &st->overlay_view;
    if (vk_dl.CreateFramebuffer(dev->device, &ofbci, NULL, &st->overlay_fb) != VK_SUCCESS) return 0;

    VkDescriptorSetAllocateInfo dsai = {VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO};
    dsai.descriptorPool = dev->desc_pool;
    dsai.descriptorSetCount = 1;
    dsai.pSetLayouts = &dev->set_layout;

    if (vk_dl.AllocateDescriptorSets(dev->device, &dsai, &st->content_sample_desc) != VK_SUCCESS) return 0;
    if (vk_dl.AllocateDescriptorSets(dev->device, &dsai, &st->overlay_sample_desc) != VK_SUCCESS) return 0;

    VkDescriptorImageInfo content_dii = {dev->sampler, st->content_view, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL};
    VkDescriptorImageInfo overlay_dii = {dev->sampler, st->overlay_view, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL};

    VkWriteDescriptorSet writes[2] = {0};
    writes[0].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
    writes[0].dstSet = st->content_sample_desc;
    writes[0].descriptorCount = 1;
    writes[0].descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
    writes[0].pImageInfo = &content_dii;
    writes[1].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
    writes[1].dstSet = st->overlay_sample_desc;
    writes[1].descriptorCount = 1;
    writes[1].descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
    writes[1].pImageInfo = &overlay_dii;
    vk_dl.UpdateDescriptorSets(dev->device, 2, writes, 0, NULL);

    st->valid = 1;
    return 1;
}

static int v_swapchain_register(
    v_device_t *dev, VkSwapchainKHR handle, VkFormat format, VkExtent2D extent, VkImageUsageFlags usage
) {
    pthread_mutex_lock(&v_swapchains_lock);
    v_swapchain_t *sc = v_swapchain_find(handle);
    if (sc)
        v_swapchain_destroy(sc);
    else
        sc = v_swapchain_alloc();
    if (!sc) {
        pthread_mutex_unlock(&v_swapchains_lock);
        LOG_WARN("stage", "[vk] swapchain table full");
        return 0;
    }
    sc->swapchain = handle;
    sc->dev = dev;
    sc->format = format;
    sc->extent = extent;
    sc->usage = usage;
    pthread_mutex_unlock(&v_swapchains_lock);

    uint32_t n = 0;
    if (vk_dl.GetSwapchainImagesKHR(dev->device, handle, &n, NULL) != VK_SUCCESS) {
        v_swapchain_destroy(sc);
        return 0;
    }
    if (n == 0 || n > V_MAX_SWAPCHAIN_I) {
        v_swapchain_destroy(sc);
        return 0;
    }
    VkImage imgs[V_MAX_SWAPCHAIN_I];
    if (vk_dl.GetSwapchainImagesKHR(dev->device, handle, &n, imgs) != VK_SUCCESS) {
        v_swapchain_destroy(sc);
        return 0;
    }
    sc->image_count = n;
    for (uint32_t i = 0; i < n; i++) {
        if (!v_image_state_build(dev, sc, i, imgs[i])) {
            v_swapchain_destroy(sc);
            return 0;
        }
    }
    sc->ready = 1;
    return 1;
}

static void v_barrier_image(
    VkCommandBuffer cb, VkImage img, VkAccessFlags src_acc, VkAccessFlags dst_acc, VkImageLayout old_layout,
    VkImageLayout new_layout, VkPipelineStageFlags src_stage, VkPipelineStageFlags dst_stage
) {
    VkImageMemoryBarrier b = {VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER};
    b.srcAccessMask = src_acc;
    b.dstAccessMask = dst_acc;

    b.oldLayout = old_layout;
    b.newLayout = new_layout;

    b.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    b.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    b.image = img;

    b.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;

    b.subresourceRange.levelCount = 1;
    b.subresourceRange.layerCount = 1;

    vk_dl.CmdPipelineBarrier(cb, src_stage, dst_stage, 0, 0, NULL, 0, NULL, 1, &b);
}

static void v_compute_quad(
    int tex_w, int tex_h, int fb_w, int fb_h, int anchor, int scale, int rot, float *out_x0, float *out_y0,
    float *out_x1, float *out_y1
) {
    int draw_w_i = tex_w, draw_h_i = tex_h;
    stretch_draw_size(tex_w, tex_h, fb_w, fb_h, scale, rot, &draw_w_i, &draw_h_i);

    float draw_w = (float) draw_w_i;
    float draw_h = (float) draw_h_i;
    if (draw_w < 1.0f) draw_w = 1.0f;
    if (draw_h < 1.0f) draw_h = 1.0f;
    if (fb_w < 1) fb_w = 1;
    if (fb_h < 1) fb_h = 1;

    const float w_ndc = draw_w * (2.0f / (float) fb_w);
    const float h_ndc = draw_h * (2.0f / (float) fb_h);

    float x0, x1, y0, y1;
    switch (get_anchor_rotate(anchor, rot)) {
        case ANCHOR_TOP_LEFT:
            x0 = -1.0f;
            x1 = x0 + w_ndc;
            y0 = 1.0f;
            y1 = y0 - h_ndc;
            break;
        case ANCHOR_TOP_MIDDLE:
            x0 = -w_ndc * 0.5f;
            x1 = w_ndc * 0.5f;
            y0 = 1.0f;
            y1 = y0 - h_ndc;
            break;
        case ANCHOR_TOP_RIGHT:
            x1 = 1.0f;
            x0 = x1 - w_ndc;
            y0 = 1.0f;
            y1 = y0 - h_ndc;
            break;
        case ANCHOR_CENTRE_LEFT:
            x0 = -1.0f;
            x1 = x0 + w_ndc;
            y0 = h_ndc * 0.5f;
            y1 = -h_ndc * 0.5f;
            break;
        case ANCHOR_CENTRE_MIDDLE:
            x0 = -w_ndc * 0.5f;
            x1 = w_ndc * 0.5f;
            y0 = h_ndc * 0.5f;
            y1 = -h_ndc * 0.5f;
            break;
        case ANCHOR_CENTRE_RIGHT:
            x1 = 1.0f;
            x0 = x1 - w_ndc;
            y0 = h_ndc * 0.5f;
            y1 = -h_ndc * 0.5f;
            break;
        case ANCHOR_BOTTOM_LEFT:
            x0 = -1.0f;
            x1 = x0 + w_ndc;
            y1 = -1.0f;
            y0 = y1 + h_ndc;
            break;
        case ANCHOR_BOTTOM_MIDDLE:
            x0 = -w_ndc * 0.5f;
            x1 = w_ndc * 0.5f;
            y1 = -1.0f;
            y0 = y1 + h_ndc;
            break;
        case ANCHOR_BOTTOM_RIGHT:
            x1 = 1.0f;
            x0 = x1 - w_ndc;
            y1 = -1.0f;
            y0 = y1 + h_ndc;
            break;
        default:
            x0 = -w_ndc * 0.5f;
            x1 = w_ndc * 0.5f;
            y0 = h_ndc * 0.5f;
            y1 = -h_ndc * 0.5f;
            break;
    }

    *out_x0 = x0;
    *out_y0 = y0;
    *out_x1 = x1;
    *out_y1 = y1;
}

static int v_resolve_sprite_path(const char *type, const char *file, const char *dim, char *out, size_t out_sz) {
    (void) out_sz;
    out[0] = 0;
    if (!ovl_go_cache.valid) return 0;
    return load_stage_image(type, ovl_go_cache.core, ovl_go_cache.system, file, dim, out);
}

static void v_format_lookup_dim(int fb_w, int fb_h, int rot, char *out, size_t out_sz) {
    if (!out || out_sz == 0) return;

    if (fb_w < 1 || fb_h < 1) {
        out[0] = '\0';
        return;
    }

    int lookup_w = fb_w;
    int lookup_h = fb_h;

    if (rot == rotate_90 || rot == rotate_270) {
        lookup_w = fb_h;
        lookup_h = fb_w;
    }

    snprintf(out, out_sz, "%dx%d/", lookup_w, lookup_h);
}

static int v_parse_rotation_env(const char *name, int *out) {
    const char *env = getenv(name);
    if (!env || !*env || !out) return 0;

    char *end = NULL;
    long v = strtol(env, &end, 10);
    if (end == env) return 0;

    while (*end == ' ' || *end == '\t' || *end == '\n' || *end == '\r')
        end++;
    if (*end != '\0') return 0;

    *out = rotate_normalise((int) v);
    return 1;
}

static int v_env_disabled(const char *name) {
    const char *env = getenv(name);
    if (!env || !*env) return 0;

    return env[0] == '0' || env[0] == 'n' || env[0] == 'N' || env[0] == 'f' || env[0] == 'F';
}

static int v_effective_rotation(int fb_w, int fb_h, int configured_rot) {
    int env_rot = rotate_0;

    if (v_parse_rotation_env("STAGE_VK_ROTATE", &env_rot)) {
        return env_rot;
    }

    if (configured_rot != rotate_0) return configured_rot;
    if (v_env_disabled("STAGE_VK_INFER_ROTATE")) return configured_rot;

    if (fb_w > 0 && fb_h > fb_w) {
        int inferred = rotate_270;
        int override = rotate_0;
        if (v_parse_rotation_env("STAGE_VK_PORTRAIT_ROTATE", &override)) inferred = override;
        return inferred;
    }

    return configured_rot;
}

static inline uint64_t v_sprite_key(uint16_t layer, uint64_t payload, int rot) {
    return v_tex_key(layer, (((uint64_t) (uint16_t) rot) << 32) | (payload & 0xFFFFFFFFULL));
}

static int v_read_overlay_loader(void) {
    struct stat st;
    if (stat(OVERLAY_LOADER, &st) != 0) {
        ovl_go_cache.valid = 0;
        return 0;
    }
    if (ovl_go_cache.valid && ovl_go_cache.mtime == st.st_mtime) return 1;

    ovl_go_cache.valid = 0;
    ovl_go_cache.mtime = st.st_mtime;
    if (!read_line_from_file(OVERLAY_LOADER, 1, ovl_go_cache.content, sizeof(ovl_go_cache.content))
        || !read_line_from_file(OVERLAY_LOADER, 2, ovl_go_cache.system, sizeof(ovl_go_cache.system))
        || !read_line_from_file(OVERLAY_LOADER, 3, ovl_go_cache.core, sizeof(ovl_go_cache.core))) {
        return 0;
    }

    ovl_go_cache.valid = 1;
    return 1;
}

static v_batch_state_t v_snapshot_batch_state(int fb_w, int fb_h, int rot, int base_disabled) {
    v_batch_state_t s;
    memset(&s, 0, sizeof(s));
    s.fb_w = fb_w;
    s.fb_h = fb_h;
    s.rot = rot;

    if (!base_disabled) {
        s.base_key = v_sprite_key(V_LAYER_BASE, 0, rot);
        s.base_anchor = get_anchor_cached(&overlay_anchor_cache);
        s.base_scale = get_scale_cached(&overlay_scale_cache);
        s.base_alpha = get_alpha_cached(&overlay_alpha_cache);
    }

    if (battery_last_step >= 0 && battery_last_step < INDICATOR_STEPS) {
        s.battery_step = battery_last_step;
        s.battery_key = v_sprite_key(V_LAYER_BATTERY, (uint64_t) battery_last_step, rot);
        s.battery_anchor = get_anchor_cached(&battery_anchor_cache);
        s.battery_scale = get_scale_cached(&battery_scale_cache);
        s.battery_alpha = get_alpha_cached(&battery_alpha_cache);
    }

    s.bright_visible = bright_is_visible() ? 1 : 0;
    if (s.bright_visible && bright_last_step >= 0 && bright_last_step < INDICATOR_STEPS) {
        s.bright_step = bright_last_step;
        s.bright_key = v_sprite_key(V_LAYER_BRIGHT, (uint64_t) bright_last_step, rot);
        s.bright_anchor = get_anchor_cached(&bright_anchor_cache);
        s.bright_scale = get_scale_cached(&bright_scale_cache);
        s.bright_alpha = get_alpha_cached(&bright_alpha_cache);
    }

    s.volume_visible = volume_is_visible() ? 1 : 0;
    if (s.volume_visible && volume_last_step >= 0 && volume_last_step < INDICATOR_STEPS) {
        s.volume_step = volume_last_step;
        s.volume_key = v_sprite_key(V_LAYER_VOLUME, (uint64_t) volume_last_step, rot);
        s.volume_anchor = get_anchor_cached(&volume_anchor_cache);
        s.volume_scale = get_scale_cached(&volume_scale_cache);
        s.volume_alpha = get_alpha_cached(&volume_alpha_cache);
    }

    return s;
}

static int v_ensure_base_sprite(v_device_t *dev, int fb_w, int fb_h, int rot, v_tex_t **out) {
    *out = NULL;
    if (!v_read_overlay_loader()) return 0;

    char dim[32];
    v_format_lookup_dim(fb_w, fb_h, rot, dim, sizeof(dim));

    char path[PATH_MAX];
    if (!v_resolve_sprite_path("base", ovl_go_cache.content, dim, path, sizeof(path))) {
        base_overlay_disabled_cached = 1;
        return 0;
    }

    *out = v_tex_get_or_upload_png(dev, v_sprite_key(V_LAYER_BASE, 0, rot), path);
    return *out != NULL;
}

static int v_ensure_indicator_sprite(
    v_device_t *dev, int fb_w, int fb_h, int rot, uint16_t layer, const char *type, int step, v_tex_t **out
) {
    *out = NULL;
    if (step < 0 || step >= INDICATOR_STEPS) return 0;
    if (!v_read_overlay_loader()) return 0;

    char dim[32];
    v_format_lookup_dim(fb_w, fb_h, rot, dim, sizeof(dim));

    char name[64];
    snprintf(name, sizeof(name), "%s_%d", type, step);

    char path[PATH_MAX];
    if (!v_resolve_sprite_path(type, name, dim, path, sizeof(path))) {
        return 0;
    }

    *out = v_tex_get_or_upload_png(dev, v_sprite_key(layer, (uint64_t) step, rot), path);
    return *out != NULL;
}

static int v_ensure_notif_texture(
    v_device_t *dev, int fb_w, int fb_h, v_tex_t **out_tex, int *out_w, int *out_h, int *out_bx, int *out_by
) {
    *out_tex = NULL;
    *out_w = *out_h = 0;
    *out_bx = *out_by = 0;
    if (!notif_is_visible()) return 0;

    if (!gl_notif_prepare(fb_w, fb_h)) return 0;

    const uint64_t key = v_tex_key(V_LAYER_NOTIF, 0);
    v_tex_t *t = v_tex_find(dev, key);
    if (!t) {
        const uint8_t transparent[4] = {0, 0, 0, 0};
        t = v_tex_get_or_upload_rgba(dev, key, transparent, 1, 1);
        if (!t) return 0;
    }

    *out_tex = t;
    *out_w = (int) t->width;
    *out_h = (int) t->height;
    *out_bx = 0;
    *out_by = 0;
    return 1;
}

static inline void v_set_uv(float uv[4][2], int rot) {
    const float base[4][2] = {
        {0.0f, 0.0f},
        {0.0f, 1.0f},
        {1.0f, 0.0f},
        {1.0f, 1.0f},
    };

    static const int map_90[4] = {1, 3, 0, 2};
    static const int map_180[4] = {3, 2, 1, 0};
    static const int map_270[4] = {2, 0, 3, 1};

    const int *map = NULL;
    if (rot == rotate_90)
        map = map_90;
    else if (rot == rotate_180)
        map = map_180;
    else if (rot == rotate_270)
        map = map_270;

    for (int i = 0; i < 4; i++) {
        const int src = map ? map[i] : i;
        uv[i][0] = base[src][0];
        uv[i][1] = base[src][1];
    }
}

static inline int v_write_quad_uv(
    v_image_state_t *st, float x0, float y0, float x1, float y1, const float uv[4][2], VkDeviceSize *out_offset
) {
    if (!st || !st->quad_map || !st->quad_buf || !uv || !out_offset) return 0;

    if (st->quad_used >= V_QUAD_MAX_DRAWS) {
        static int logged_full = 0;
        if (!logged_full) {
            LOG_WARN("stage", "[vk] quad vertex buffer full - skipping further overlay draws");
            logged_full = 1;
        }
        return 0;
    }

    v_quad_vertex_t *v = (v_quad_vertex_t *) st->quad_map;
    v += st->quad_used * V_QUAD_VERTS_PER_DRAW;

    v[0].x = x0;
    v[0].y = y0;
    v[0].u = uv[0][0];
    v[0].v = uv[0][1];
    v[1].x = x0;
    v[1].y = y1;
    v[1].u = uv[1][0];
    v[1].v = uv[1][1];
    v[2].x = x1;
    v[2].y = y0;
    v[2].u = uv[2][0];
    v[2].v = uv[2][1];
    v[3].x = x1;
    v[3].y = y1;
    v[3].u = uv[3][0];
    v[3].v = uv[3][1];

    *out_offset = (VkDeviceSize) st->quad_used * V_QUAD_VERTS_PER_DRAW * sizeof(v_quad_vertex_t);
    st->quad_used++;
    return 1;
}

static inline int
v_write_quad(v_image_state_t *st, float x0, float y0, float x1, float y1, int rot, VkDeviceSize *out_offset) {
    float uv[4][2];
    v_set_uv(uv, rot);
    return v_write_quad_uv(st, x0, y0, x1, y1, uv, out_offset);
}

static inline void v_push_colour(VkCommandBuffer cb, VkPipelineLayout layout, float r, float g, float b, float a) {
    v_push_overlay_t pc;
    pc.colour[0] = r;
    pc.colour[1] = g;
    pc.colour[2] = b;
    pc.colour[3] = a;

    vk_dl.CmdPushConstants(cb, layout, VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT, 0, sizeof(pc), &pc);
}

static inline void v_bind_quad_and_draw(v_image_state_t *st, VkDeviceSize offset) {
    vk_dl.CmdBindVertexBuffers(st->cmdbuf, 0, 1, &st->quad_buf, &offset);
    vk_dl.CmdDraw(st->cmdbuf, V_QUAD_VERTS_PER_DRAW, 1, 0, 0);
}

static inline void v_draw_sprite(
    v_image_state_t *st, VkPipelineLayout layout, VkDescriptorSet desc, float x0, float y0, float x1, float y1, int rot,
    float alpha
) {
    VkDeviceSize offset = 0;
    if (!v_write_quad(st, x0, y0, x1, y1, rot, &offset)) return;

    vk_dl.CmdBindDescriptorSets(st->cmdbuf, VK_PIPELINE_BIND_POINT_GRAPHICS, layout, 0, 1, &desc, 0, NULL);
    v_push_colour(st->cmdbuf, layout, 1.0f, 1.0f, 1.0f, alpha);
    v_bind_quad_and_draw(st, offset);
}

static inline void v_draw_solid_rect(
    v_image_state_t *st, VkPipelineLayout layout, float x0, float y0, float x1, float y1, float r, float g, float b,
    float a
) {
    VkDeviceSize offset = 0;
    if (!v_write_quad(st, x0, y0, x1, y1, rotate_0, &offset)) return;

    v_push_colour(st->cmdbuf, layout, r, g, b, a);
    v_bind_quad_and_draw(st, offset);
}

static int v_colour_pass_needed(void) {
    const struct colour_state *a = colour_adjust_get();
    const colour_filter_matrix_t *f = colour_filter_get();

    if (a->brightness == 0.0f && a->contrast == 1.0f && a->saturation == 1.0f && a->hueshift == 0.0f && a->gamma == 1.0f
        && !f->enabled) {
        return 0;
    }

    return 1;
}

static void v_fill_content_push(v_push_content_t *pc) {
    const struct colour_state *a = colour_adjust_get();
    const colour_filter_matrix_t *f = colour_filter_get();

    memset(pc, 0, sizeof(*pc));
    pc->brightness = a->brightness;
    pc->contrast = a->contrast;
    pc->saturation = a->saturation;
    pc->cosH = cosf(a->hueshift);
    pc->sinH = sinf(a->hueshift);
    pc->gamma = a->gamma;
    pc->filter_enabled = f->enabled ? 1 : 0;

    for (int i = 0; i < 12; i++)
        pc->filter[i] = 0.0f;

    if (f->enabled) {
        for (int col = 0; col < 3; col++) {
            for (int row = 0; row < 3; row++) {
                pc->filter[col * 4 + row] = f->matrix[row * 3 + col];
            }
        }
    } else {
        pc->filter[0] = 1.0f;
        pc->filter[5] = 1.0f;
        pc->filter[10] = 1.0f;
    }
}

static void v_draw_content_fullscreen(v_device_t *dev, v_image_state_t *st) {
    static const float content_uv[4][2] = {
        {0.0f, 1.0f},
        {0.0f, 0.0f},
        {1.0f, 1.0f},
        {1.0f, 0.0f},
    };

    VkDeviceSize offset = 0;
    if (!v_write_quad_uv(st, -1.0f, -1.0f, 1.0f, 1.0f, content_uv, &offset)) return;

    v_push_content_t pc;
    v_fill_content_push(&pc);

    vk_dl.CmdBindDescriptorSets(
        st->cmdbuf, VK_PIPELINE_BIND_POINT_GRAPHICS, dev->layout_content, 0, 1, &st->content_sample_desc, 0, NULL
    );
    vk_dl.CmdPushConstants(
        st->cmdbuf, dev->layout_content, VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT, 0, sizeof(pc), &pc
    );

    v_bind_quad_and_draw(st, offset);
}

static int v_user_shader_active(v_device_t *dev) {
    return dev && dev->fs_user_module != VK_NULL_HANDLE;
}

static void v_get_shader_native_resolution(int pass_w, int pass_h, float *out_w, float *out_h) {
    int div_w;
    int div_h;
    int div;
    int native_w;
    int native_h;

    if (pass_w < 1) pass_w = 1;
    if (pass_h < 1) pass_h = 1;

    div_w = pass_w / 640;
    div_h = pass_h / 480;
    div = (div_w < div_h) ? div_w : div_h;

    if (div < 1) div = 1;

    native_w = (pass_w + (div / 2)) / div;
    native_h = (pass_h + (div / 2)) / div;

    if (native_w < 640) native_w = 640;
    if (native_h < 480) native_h = 480;

    *out_w = (float) native_w;
    *out_h = (float) native_h;
}

static void v_fill_user_push(v_push_user_shader_t *pc, int fb_w, int fb_h, int frame) {
    memset(pc, 0, sizeof(*pc));

    pc->resolution[0] = (float) fb_w;
    pc->resolution[1] = (float) fb_h;

    v_get_shader_native_resolution(fb_w, fb_h, &pc->native_resolution[0], &pc->native_resolution[1]);

    pc->time = (float) frame;
    pc->frame = frame;
}

static void v_draw_user_shader_fullscreen(v_device_t *dev, v_image_state_t *st, int fb_w, int fb_h, int frame) {
    static const float content_uv[4][2] = {
        {0.0f, 1.0f},
        {0.0f, 0.0f},
        {1.0f, 1.0f},
        {1.0f, 0.0f},
    };

    VkDeviceSize offset = 0;
    if (!v_write_quad_uv(st, -1.0f, -1.0f, 1.0f, 1.0f, content_uv, &offset)) return;

    v_push_overlay_t vpc;
    vpc.colour[0] = 1.0f;
    vpc.colour[1] = 1.0f;
    vpc.colour[2] = 1.0f;
    vpc.colour[3] = 1.0f;

    v_push_user_shader_t fpc;
    v_fill_user_push(&fpc, fb_w, fb_h, frame);

    vk_dl.CmdBindDescriptorSets(
        st->cmdbuf, VK_PIPELINE_BIND_POINT_GRAPHICS, dev->layout_user, 0, 1, &st->content_sample_desc, 0, NULL
    );

    vk_dl.CmdPushConstants(st->cmdbuf, dev->layout_user, VK_SHADER_STAGE_VERTEX_BIT, 0, sizeof(vpc), &vpc);
    vk_dl.CmdPushConstants(st->cmdbuf, dev->layout_user, VK_SHADER_STAGE_FRAGMENT_BIT, 0, sizeof(fpc), &fpc);

    v_bind_quad_and_draw(st, offset);
}

static void v_copy_swap_to_content(
    VkCommandBuffer cb, v_image_state_t *st, VkImage swap_image, int fb_w, int fb_h, VkImageLayout swap_src_layout,
    VkAccessFlags swap_src_access, VkPipelineStageFlags swap_src_stage, VkImageLayout content_src_layout,
    VkAccessFlags content_src_access, VkPipelineStageFlags content_src_stage
) {
    v_barrier_image(
        cb, swap_image, swap_src_access, VK_ACCESS_TRANSFER_READ_BIT, swap_src_layout,
        VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL, swap_src_stage, VK_PIPELINE_STAGE_TRANSFER_BIT
    );
    v_barrier_image(
        cb, st->content_image, content_src_access, VK_ACCESS_TRANSFER_WRITE_BIT, content_src_layout,
        VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, content_src_stage, VK_PIPELINE_STAGE_TRANSFER_BIT
    );

    VkImageCopy copy = {0};

    copy.srcSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
    copy.srcSubresource.layerCount = 1;

    copy.dstSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
    copy.dstSubresource.layerCount = 1;

    copy.extent.width = (uint32_t) fb_w;
    copy.extent.height = (uint32_t) fb_h;
    copy.extent.depth = 1;

    vk_dl.CmdCopyImage(
        cb, swap_image, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL, st->content_image, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
        1, &copy
    );

    v_barrier_image(
        cb, st->content_image, VK_ACCESS_TRANSFER_WRITE_BIT, VK_ACCESS_SHADER_READ_BIT,
        VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL, VK_PIPELINE_STAGE_TRANSFER_BIT,
        VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT
    );

    v_barrier_image(
        cb, swap_image, VK_ACCESS_TRANSFER_READ_BIT,
        VK_ACCESS_COLOR_ATTACHMENT_READ_BIT | VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT,
        VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL, VK_PIPELINE_STAGE_TRANSFER_BIT,
        VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT
    );
}

static int v_record_cmdbuf(
    v_device_t *dev, v_swapchain_t *sc, v_image_state_t *st, VkImage swap_image, int fb_w, int fb_h, int rot,
    int shader_frame_count
) {
    if (!(sc->usage & VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT)) {
        static int logged_no_colour = 0;
        if (!logged_no_colour) {
            LOG_WARN(
                "stage", "[vk] swapchain usage=0x%x lacks COLOR_ATTACHMENT - cannot draw indicators into swapchain",
                (unsigned) sc->usage
            );
            logged_no_colour = 1;
        }
        return 0;
    }

    VkCommandBufferBeginInfo bi = {VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO};
    bi.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;
    if (vk_dl.BeginCommandBuffer(st->cmdbuf, &bi) != VK_SUCCESS) return 0;

    st->quad_used = 0;

    VkViewport vp = {0.0f, 0.0f, (float) fb_w, (float) fb_h, 0.0f, 1.0f};
    VkRect2D scissor = {{0, 0}, {(uint32_t) fb_w, (uint32_t) fb_h}};

    const int base_disabled = ino_proc ? base_overlay_disabled_cached : (access(BASE_OVERLAY_NOP, F_OK) == 0);
    v_batch_state_t batch = v_snapshot_batch_state(fb_w, fb_h, rot, base_disabled);

    v_tex_t *tex_base = NULL;
    v_tex_t *tex_battery = NULL;
    v_tex_t *tex_bright = NULL;
    v_tex_t *tex_volume = NULL;

    if (!base_disabled) v_ensure_base_sprite(dev, fb_w, fb_h, rot, &tex_base);
    if (batch.battery_key)
        v_ensure_indicator_sprite(dev, fb_w, fb_h, rot, V_LAYER_BATTERY, "battery", batch.battery_step, &tex_battery);
    if (batch.bright_key)
        v_ensure_indicator_sprite(dev, fb_w, fb_h, rot, V_LAYER_BRIGHT, "bright", batch.bright_step, &tex_bright);
    if (batch.volume_key)
        v_ensure_indicator_sprite(dev, fb_w, fb_h, rot, V_LAYER_VOLUME, "volume", batch.volume_step, &tex_volume);

    VkRenderPass present_rp = v_get_present_renderpass(dev, sc->format);
    VkPipeline pipe_overlay_present = v_get_present_pipeline_overlay(dev, sc->format);
    VkPipeline pipe_content_present = VK_NULL_HANDLE;
    VkPipeline pipe_user_present = VK_NULL_HANDLE;
    int colour_active = v_colour_pass_needed();
    int shader_active = v_user_shader_active(dev);
    int post_active = colour_active || shader_active;

    if (post_active && !(sc->usage & VK_IMAGE_USAGE_TRANSFER_SRC_BIT)) {
        static int logged_no_transfer_src = 0;
        if (!logged_no_transfer_src) {
            LOG_WARN(
                "stage", "[vk] swapchain usage=0x%x lacks TRANSFER_SRC - shader/colour pass disabled",
                (unsigned) sc->usage
            );
            logged_no_transfer_src = 1;
        }

        colour_active = 0;
        shader_active = 0;
        post_active = 0;
    }

    if (colour_active) {
        pipe_content_present = v_get_present_pipeline_content(dev, sc->format);
        if (pipe_content_present == VK_NULL_HANDLE) {
            static int logged_no_content_pipe = 0;
            if (!logged_no_content_pipe) {
                LOG_WARN("stage", "[vk] present content pipeline unavailable - colour/filter pass disabled");
                logged_no_content_pipe = 1;
            }
            colour_active = 0;
        }
    }

    if (shader_active) {
        pipe_user_present = v_get_present_pipeline_user(dev, sc->format);
        if (pipe_user_present == VK_NULL_HANDLE) {
            static int logged_no_user_pipe = 0;
            if (!logged_no_user_pipe) {
                LOG_WARN("stage", "[vk] present user shader pipeline unavailable - shader pass disabled");
                logged_no_user_pipe = 1;
            }
            shader_active = 0;
        }
    }

    post_active = colour_active || shader_active;

    if (present_rp == VK_NULL_HANDLE || pipe_overlay_present == VK_NULL_HANDLE) {
        static int logged_no_pipe = 0;
        if (!logged_no_pipe) {
            LOG_WARN(
                "stage", "[vk] present overlay pipeline unavailable (rp=%p pipe=%p)", (void *) present_rp,
                (void *) pipe_overlay_present
            );
            logged_no_pipe = 1;
        }

        vk_dl.EndCommandBuffer(st->cmdbuf);
        return 0;
    }

    if (post_active) {
        v_copy_swap_to_content(
            st->cmdbuf, st, swap_image, fb_w, fb_h, VK_IMAGE_LAYOUT_PRESENT_SRC_KHR,
            VK_ACCESS_MEMORY_READ_BIT | VK_ACCESS_MEMORY_WRITE_BIT, VK_PIPELINE_STAGE_ALL_COMMANDS_BIT,
            VK_IMAGE_LAYOUT_UNDEFINED, 0, VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT
        );
    } else {
        v_barrier_image(
            st->cmdbuf, swap_image, VK_ACCESS_MEMORY_READ_BIT | VK_ACCESS_MEMORY_WRITE_BIT,
            VK_ACCESS_COLOR_ATTACHMENT_READ_BIT | VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT, VK_IMAGE_LAYOUT_PRESENT_SRC_KHR,
            VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL, VK_PIPELINE_STAGE_ALL_COMMANDS_BIT,
            VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT
        );
    }

    VkRenderPassBeginInfo rpb = {VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO};
    rpb.renderPass = present_rp;
    rpb.framebuffer = st->framebuffer;
    rpb.renderArea.extent.width = (uint32_t) fb_w;
    rpb.renderArea.extent.height = (uint32_t) fb_h;

    vk_dl.CmdBeginRenderPass(st->cmdbuf, &rpb, VK_SUBPASS_CONTENTS_INLINE);
    vk_dl.CmdSetViewport(st->cmdbuf, 0, 1, &vp);
    vk_dl.CmdSetScissor(st->cmdbuf, 0, 1, &scissor);

    if (colour_active) {
        vk_dl.CmdBindPipeline(st->cmdbuf, VK_PIPELINE_BIND_POINT_GRAPHICS, pipe_content_present);
        v_draw_content_fullscreen(dev, st);
    }

    if (shader_active) {
        if (colour_active) {
            vk_dl.CmdEndRenderPass(st->cmdbuf);
            v_copy_swap_to_content(
                st->cmdbuf, st, swap_image, fb_w, fb_h, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,
                VK_ACCESS_COLOR_ATTACHMENT_READ_BIT | VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT,
                VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
                VK_ACCESS_SHADER_READ_BIT, VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT
            );
            vk_dl.CmdBeginRenderPass(st->cmdbuf, &rpb, VK_SUBPASS_CONTENTS_INLINE);
            vk_dl.CmdSetViewport(st->cmdbuf, 0, 1, &vp);
            vk_dl.CmdSetScissor(st->cmdbuf, 0, 1, &scissor);
        }

        vk_dl.CmdBindPipeline(st->cmdbuf, VK_PIPELINE_BIND_POINT_GRAPHICS, pipe_user_present);
        v_draw_user_shader_fullscreen(dev, st, fb_w, fb_h, shader_frame_count);
    }

    vk_dl.CmdBindPipeline(st->cmdbuf, VK_PIPELINE_BIND_POINT_GRAPHICS, pipe_overlay_present);
    float x0, y0, x1, y1;
    if (tex_base) {
        v_compute_quad(
            (int) tex_base->width, (int) tex_base->height, fb_w, fb_h, batch.base_anchor, batch.base_scale, rot, &x0,
            &y0, &x1, &y1
        );
        v_draw_sprite(st, dev->layout_overlay, tex_base->desc, x0, y0, x1, y1, rot, batch.base_alpha);
    }
    if (tex_battery) {
        v_compute_quad(
            (int) tex_battery->width, (int) tex_battery->height, fb_w, fb_h, batch.battery_anchor, batch.battery_scale,
            rot, &x0, &y0, &x1, &y1
        );
        v_draw_sprite(st, dev->layout_overlay, tex_battery->desc, x0, y0, x1, y1, rot, batch.battery_alpha);
    }
    if (tex_bright) {
        v_compute_quad(
            (int) tex_bright->width, (int) tex_bright->height, fb_w, fb_h, batch.bright_anchor, batch.bright_scale, rot,
            &x0, &y0, &x1, &y1
        );
        v_draw_sprite(st, dev->layout_overlay, tex_bright->desc, x0, y0, x1, y1, rot, batch.bright_alpha);
    }
    if (tex_volume) {
        v_compute_quad(
            (int) tex_volume->width, (int) tex_volume->height, fb_w, fb_h, batch.volume_anchor, batch.volume_scale, rot,
            &x0, &y0, &x1, &y1
        );
        v_draw_sprite(st, dev->layout_overlay, tex_volume->desc, x0, y0, x1, y1, rot, batch.volume_alpha);
    }

    if (notif_is_visible()) {
        v_tex_t *ntex = NULL;
        int nw = 0, nh = 0, bx = 0, by = 0;
        if (v_ensure_notif_texture(dev, fb_w, fb_h, &ntex, &nw, &nh, &bx, &by) && ntex) {
            if (gl_notif_needs_dim()) {
                VkPipeline dim_pipe = v_get_present_pipeline_solid(dev, sc->format);
                if (dim_pipe != VK_NULL_HANDLE) {
                    vk_dl.CmdBindPipeline(st->cmdbuf, VK_PIPELINE_BIND_POINT_GRAPHICS, dim_pipe);
                    v_draw_solid_rect(
                        st, dev->layout_overlay, -1.0f, -1.0f, 1.0f, 1.0f, notif_cfg.dim_colour.r / 255.0f,
                        notif_cfg.dim_colour.g / 255.0f, notif_cfg.dim_colour.b / 255.0f, notif_cfg.dim_alpha / 255.0f
                    );
                    vk_dl.CmdBindPipeline(st->cmdbuf, VK_PIPELINE_BIND_POINT_GRAPHICS, pipe_overlay_present);
                }
            }

            x0 = (float) bx / (float) fb_w * 2.0f - 1.0f;
            x1 = (float) (bx + nw) / (float) fb_w * 2.0f - 1.0f;
            y0 = 1.0f - (float) by / (float) fb_h * 2.0f;
            y1 = 1.0f - (float) (by + nh) / (float) fb_h * 2.0f;

            v_draw_sprite(st, dev->layout_overlay, ntex->desc, x0, y0, x1, y1, rotate_0, 1.0f);
        }
    }

    v_flush_quad_buffer(dev, st);
    vk_dl.CmdEndRenderPass(st->cmdbuf);

    v_barrier_image(
        st->cmdbuf, swap_image, VK_ACCESS_COLOR_ATTACHMENT_READ_BIT | VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT, 0,
        VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL, VK_IMAGE_LAYOUT_PRESENT_SRC_KHR,
        VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT, VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT
    );

    if (vk_dl.EndCommandBuffer(st->cmdbuf) != VK_SUCCESS) return 0;

    return 1;
}

VkResult vkCreateDevice(
    VkPhysicalDevice phys, const VkDeviceCreateInfo *ci, const VkAllocationCallbacks *alloc, VkDevice *out_device
) {
    resolve_vulkan_symbols();
    if (!real_vk_create_device) {
        LOG_WARN("stage", "[vk] real_vkCreateDevice is NULL");
        return VK_ERROR_INITIALIZATION_FAILED;
    }

    VkResult r = real_vk_create_device(phys, ci, alloc, out_device);
    if (r != VK_SUCCESS || !out_device || *out_device == VK_NULL_HANDLE) {
        return r;
    }

    if (!vk_dl_init()) {
        LOG_WARN("stage", "[vk] vk_dl_init failed - loader unavailable");
        return r;
    }

    uint32_t qf = v_pick_graphics_qf(phys, ci);
    if (qf == UINT32_MAX) {
        LOG_INFO("stage", "[vk] no graphics queue family on this device - Vulkan overlay inactive");
        return r;
    }

    pthread_mutex_lock(&v_devices_lock);
    v_device_t *dev = v_device_find(*out_device);
    if (!dev) dev = v_device_alloc();

    if (!dev) {
        pthread_mutex_unlock(&v_devices_lock);
        LOG_WARN("stage", "[vk] device table full");
        return r;
    }

    dev->device = *out_device;
    dev->phys = phys;
    dev->graphics_qf = qf;

    pthread_mutex_unlock(&v_devices_lock);

    v_device_init(dev);
    return r;
}

void vkDestroyDevice(VkDevice device, const VkAllocationCallbacks *alloc) {
    resolve_vulkan_symbols();
    if (vk_dl.ready && device != VK_NULL_HANDLE) {
        pthread_mutex_lock(&v_swapchains_lock);
        for (int i = 0; i < V_MAX_SWAPCHAINS; i++) {
            if (v_swapchains[i].dev && v_swapchains[i].dev->device == device) {
                v_swapchain_destroy(&v_swapchains[i]);
            }
        }

        pthread_mutex_unlock(&v_swapchains_lock);

        pthread_mutex_lock(&v_devices_lock);
        v_device_t *dev = v_device_find(device);

        if (dev) v_device_destroy(dev);

        pthread_mutex_unlock(&v_devices_lock);
    }

    if (real_vk_destroy_device) real_vk_destroy_device(device, alloc);
}

VkResult vkCreateSwapchainKHR(
    VkDevice device, const VkSwapchainCreateInfoKHR *ci, const VkAllocationCallbacks *alloc, VkSwapchainKHR *out_sc
) {
    resolve_vulkan_symbols();
    if (!real_vk_create_swapchain_khr) return VK_ERROR_INITIALIZATION_FAILED;

    if (vk_dl.ready && ci && ci->oldSwapchain != VK_NULL_HANDLE) {
        pthread_mutex_lock(&v_swapchains_lock);
        v_swapchain_t *old = v_swapchain_find(ci->oldSwapchain);
        if (old) v_swapchain_destroy(old);
        pthread_mutex_unlock(&v_swapchains_lock);
    }

    VkResult r = real_vk_create_swapchain_khr(device, ci, alloc, out_sc);
    if (r != VK_SUCCESS || !out_sc || *out_sc == VK_NULL_HANDLE) return r;
    if (!vk_dl.ready) return r;

    pthread_mutex_lock(&v_devices_lock);
    v_device_t *dev = v_device_find(device);
    pthread_mutex_unlock(&v_devices_lock);

    if (!dev || !dev->ready) {
        LOG_INFO("stage", "[vk] swapchain created on untracked device - overlay disabled for this swapchain");
        return r;
    }

    v_swapchain_register(dev, *out_sc, ci->imageFormat, ci->imageExtent, ci->imageUsage);
    return r;
}

void vkDestroySwapchainKHR(VkDevice device, VkSwapchainKHR swapchain, const VkAllocationCallbacks *alloc) {
    resolve_vulkan_symbols();

    if (vk_dl.ready && swapchain != VK_NULL_HANDLE) {
        pthread_mutex_lock(&v_swapchains_lock);
        v_swapchain_t *sc = v_swapchain_find(swapchain);

        if (sc) v_swapchain_destroy(sc);

        pthread_mutex_unlock(&v_swapchains_lock);
    }

    if (real_vk_destroy_swapchain_khr) real_vk_destroy_swapchain_khr(device, swapchain, alloc);
    (void) device;
}

static int v_shader_frame = 0;

VkResult vkQueuePresentKHR(VkQueue queue, const VkPresentInfoKHR *info) {
    resolve_vulkan_symbols();
    if (!real_vk_queue_present_khr) return VK_ERROR_INITIALIZATION_FAILED;

    if (is_overlay_disabled()) return real_vk_queue_present_khr(queue, info);
    if (!info || info->swapchainCount == 0) return real_vk_queue_present_khr(queue, info);
    if (!vk_dl.ready) return real_vk_queue_present_khr(queue, info);

    base_inotify_check();
    if (ino_proc) inotify_check(ino_proc);

    battery_overlay_update();
    bright_overlay_update();
    volume_overlay_update();
    notif_update();

    v_device_t *dev = NULL;
    pthread_mutex_lock(&v_devices_lock);
    for (int i = 0; i < V_MAX_DEVICES; i++) {
        if (v_devices[i].ready && v_devices[i].graphics_queue == queue) {
            dev = &v_devices[i];
            break;
        }
    }

    pthread_mutex_unlock(&v_devices_lock);

    if (!dev) return real_vk_queue_present_khr(queue, info);

    v_user_shader_sync(dev);

    VkSemaphore signal_semaphores[V_MAX_SWAPCHAINS];
    int prepared = 1;

    for (uint32_t i = 0; i < info->swapchainCount; i++) {
        VkSwapchainKHR sc_h = info->pSwapchains[i];
        pthread_mutex_lock(&v_swapchains_lock);
        v_swapchain_t *sc = v_swapchain_find(sc_h);
        pthread_mutex_unlock(&v_swapchains_lock);

        if (!sc || !sc->ready) {
            prepared = 0;
            break;
        }

        if (info->pImageIndices[i] >= sc->image_count) {
            prepared = 0;
            break;
        }

        v_image_state_t *st = &sc->images[info->pImageIndices[i]];

        vk_dl.WaitForFences(dev->device, 1, &st->fence, VK_TRUE, UINT64_MAX);
        vk_dl.ResetFences(dev->device, 1, &st->fence);
        vk_dl.ResetCommandBuffer(st->cmdbuf, 0);

        const int configured_rot = rotate_read_cached();
        const int rot = v_effective_rotation((int) sc->extent.width, (int) sc->extent.height, configured_rot);

        if (!v_record_cmdbuf(
                dev, sc, st, st->image, (int) sc->extent.width, (int) sc->extent.height, rot, v_shader_frame
            )) {
            prepared = 0;
            sc->inject_fail++;
            if (sc->inject_fail >= V_INJECT_FAIL_MAX) {
                LOG_WARN("stage", "[vk] cmdbuf recording failed %d times; dropping swapchain", sc->inject_fail);
                pthread_mutex_lock(&v_swapchains_lock);
                v_swapchain_destroy(sc);
                pthread_mutex_unlock(&v_swapchains_lock);
            }
            break;
        }
        sc->inject_fail = 0;

        VkPipelineStageFlags wait_stages[8];
        for (int s = 0; s < 8; s++)
            wait_stages[s] = VK_PIPELINE_STAGE_ALL_COMMANDS_BIT;

        VkSubmitInfo si = {VK_STRUCTURE_TYPE_SUBMIT_INFO};

        if (i == 0) {
            si.waitSemaphoreCount = info->waitSemaphoreCount;
            si.pWaitSemaphores = info->pWaitSemaphores;
            si.pWaitDstStageMask = wait_stages;
        }

        si.commandBufferCount = 1;
        si.pCommandBuffers = &st->cmdbuf;
        si.signalSemaphoreCount = 1;
        si.pSignalSemaphores = &st->signal;

        VkResult sr = vk_dl.QueueSubmit(dev->graphics_queue, 1, &si, st->fence);
        if (sr != VK_SUCCESS) {
            LOG_WARN("stage", "[vk] QueueSubmit for overlay injection failed: %d", (int) sr);
            prepared = 0;
            break;
        }

        signal_semaphores[i] = st->signal;
    }

    if (!prepared) return real_vk_queue_present_khr(queue, info);

    VkPresentInfoKHR pi = *info;
    pi.waitSemaphoreCount = info->swapchainCount;
    pi.pWaitSemaphores = signal_semaphores;

    VkResult ret = real_vk_queue_present_khr(queue, &pi);
    v_shader_frame = (v_shader_frame + 1) & 0xFFFF;
    return ret;
}

__attribute__((destructor)) static void vk_dtor(void) {
    if (vk_dl.ready) {
        pthread_mutex_lock(&v_swapchains_lock);
        for (int i = 0; i < V_MAX_SWAPCHAINS; i++) {
            if (v_swapchains[i].swapchain) v_swapchain_destroy(&v_swapchains[i]);
        }
        pthread_mutex_unlock(&v_swapchains_lock);

        pthread_mutex_lock(&v_devices_lock);
        for (int i = 0; i < V_MAX_DEVICES; i++) {
            if (v_devices[i].device) v_device_destroy(&v_devices[i]);
        }
        pthread_mutex_unlock(&v_devices_lock);
    }

    if (vk_dl.handle) {
        dlclose(vk_dl.handle);
        vk_dl.handle = NULL;
    }
}

VKAPI_ATTR PFN_vkVoidFunction VKAPI_CALL vkGetDeviceProcAddr(VkDevice device, const char *name) {

    if (name) {
        if (strcmp(name, "vkGetDeviceProcAddr") == 0) return (PFN_vkVoidFunction) vkGetDeviceProcAddr;
        if (strcmp(name, "vkDestroyDevice") == 0) return (PFN_vkVoidFunction) vkDestroyDevice;
        if (strcmp(name, "vkCreateSwapchainKHR") == 0) return (PFN_vkVoidFunction) vkCreateSwapchainKHR;
        if (strcmp(name, "vkDestroySwapchainKHR") == 0) return (PFN_vkVoidFunction) vkDestroySwapchainKHR;
        if (strcmp(name, "vkQueuePresentKHR") == 0) return (PFN_vkVoidFunction) vkQueuePresentKHR;
    }

    resolve_vulkan_symbols();
    if (!real_vk_get_device_proc_addr) return NULL;

    return real_vk_get_device_proc_addr(device, name);
}

VKAPI_ATTR PFN_vkVoidFunction VKAPI_CALL vkGetInstanceProcAddr(VkInstance instance, const char *name) {

    if (name) {
        if (strcmp(name, "vkGetInstanceProcAddr") == 0) return (PFN_vkVoidFunction) vkGetInstanceProcAddr;
        if (strcmp(name, "vkGetDeviceProcAddr") == 0) return (PFN_vkVoidFunction) vkGetDeviceProcAddr;
        if (strcmp(name, "vkCreateDevice") == 0) return (PFN_vkVoidFunction) vkCreateDevice;
        if (strcmp(name, "vkDestroyDevice") == 0) return (PFN_vkVoidFunction) vkDestroyDevice;
        if (strcmp(name, "vkCreateSwapchainKHR") == 0) return (PFN_vkVoidFunction) vkCreateSwapchainKHR;
        if (strcmp(name, "vkDestroySwapchainKHR") == 0) return (PFN_vkVoidFunction) vkDestroySwapchainKHR;
        if (strcmp(name, "vkQueuePresentKHR") == 0) return (PFN_vkVoidFunction) vkQueuePresentKHR;
    }

    resolve_vulkan_symbols();
    if (!real_vk_get_instance_proc_addr) return NULL;

    return real_vk_get_instance_proc_addr(instance, name);
}
