#pragma once

#include "graphics/host_gpu/vulkanCommon.h"

namespace Libs::Graphics {

struct GpuCrashMarkerInfo {
	uint64_t submit  = 0;
	uint64_t shader0 = 0;
	uint64_t shader1 = 0;
	uint64_t hash0   = 0;
	uint64_t hash1   = 0;
	uint32_t kind    = 0;
	uint32_t x       = 0;
	uint32_t y       = 0;
	uint32_t z       = 0;
};

bool        GpuCrashMarkersRequested();
void        RegisterGpuFaultExtraInfo(std::string (*provider)(GraphicContext&));
void        InitializeGpuCrashMarkers(GraphicContext& graphics);
void        DestroyGpuCrashMarkers(GraphicContext& graphics);
void        ArmGpuCrashMarkers();
uint32_t    BeginGpuCrashMarker(GraphicContext& graphics, vk::CommandBuffer command,
                                const GpuCrashMarkerInfo& info);
void        EndGpuCrashMarker(GraphicContext& graphics, vk::CommandBuffer command, uint32_t token);
std::string DescribeGpuCrashMarkers(GraphicContext& graphics);

} // namespace Libs::Graphics
