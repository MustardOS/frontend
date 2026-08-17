#include "coredef.h"

static const struct coredef_option options[] = {
    {.key = "swanstation_CPU_ExecutionMode", .value = "Recompiler"},
    {.key = "swanstation_CPU_FastmemMode", .value = "MMap"},
    {.key = "swanstation_CPU_RecompilerBlockLinking", .value = "true"},
    {.key = "swanstation_GPU_ResolutionScale", .value = "1"},
    {.key = "swanstation_GPU_MSAA", .value = "1"},
    {.key = "swanstation_GPU_TrueColor", .value = "true"},
    {.key = "swanstation_GPU_ScaledDithering", .value = "false"},
    {.key = "swanstation_GPU_DisableInterlacing", .value = "true"},
    {.key = "swanstation_GPU_TextureFilter", .value = "Nearest"},
    {.key = "swanstation_GPU_DownsampleMode", .value = "Disabled"},
    {.key = "swanstation_GPU_UseSoftwareRendererForReadbacks", .value = "false"},
    {.key = "swanstation_GPU_ChromaSmoothing24Bit", .value = "false"},
    {.key = "swanstation_GPU_PGXPEnable", .value = "false"},
    {.key = "swanstation_GPU_ShaderPrecompile", .value = "Enabled"},
    {.key = "swanstation_Main_RunaheadFrameCount", .value = "0"},
    {.key = "swanstation_Logging_LogLevel", .value = "None"},
    {.key = "swanstation_Audio_FastHook", .value = "true"},
};

COREDEF_CORE(swanstation, "swanstation", options);
