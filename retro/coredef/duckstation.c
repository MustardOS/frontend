#include "coredef.h"

static const struct coredef_option options[] = {
    {.key = "duckstation_CPU_ExecutionMode", .value = "Recompiler"},
    {.key = "duckstation_CPU_FastmemMode", .value = "MMap"},
    {.key = "duckstation_CPU_RecompilerBlockLinking", .value = "true"},
    {.key = "duckstation_GPU_ResolutionScale", .value = "1"},
    {.key = "duckstation_GPU_MSAA", .value = "1"},
    {.key = "duckstation_GPU_TrueColor", .value = "true"},
    {.key = "duckstation_GPU_ScaledDithering", .value = "false"},
    {.key = "duckstation_GPU_DisableInterlacing", .value = "true"},
    {.key = "duckstation_GPU_TextureFilter", .value = "Nearest"},
    {.key = "duckstation_GPU_DownsampleMode", .value = "Disabled"},
    {.key = "duckstation_GPU_UseSoftwareRendererForReadbacks", .value = "false"},
    {.key = "duckstation_GPU_ChromaSmoothing24Bit", .value = "false"},
    {.key = "duckstation_GPU_PGXPEnable", .value = "false"},
    {.key = "duckstation_GPU_ShaderPrecompile", .value = "Enabled"},
    {.key = "duckstation_Main_RunaheadFrameCount", .value = "0"},
    {.key = "duckstation_Logging_LogLevel", .value = "None"},
    {.key = "duckstation_Audio_FastHook", .value = "true"},
};

COREDEF_CORE(duckstation, "duckstation", options);
