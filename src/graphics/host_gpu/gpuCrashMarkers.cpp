#include "graphics/host_gpu/gpuCrashMarkers.h"

#include "common/logging/log.h"
#include "graphics/host_gpu/graphicContext.h"

#include <algorithm>
#include <atomic>
#include <cstdlib>
#include <cstring>
#include <map>
#include <memory>
#include <mutex>

namespace Libs::Graphics {
namespace {

constexpr uint32_t MaxMarkers = 262144;
struct MarkerState {
	VkBuffer                        buffer     = VK_NULL_HANDLE;
	VmaAllocation                   allocation = nullptr;
	const volatile uint32_t*        mapped     = nullptr;
	std::vector<GpuCrashMarkerInfo> records;
};
std::mutex                      marker_mutex;
std::map<VkDevice, MarkerState> states;
std::atomic_bool                armed = false;

} // namespace

bool GpuCrashMarkersRequested() {
	const char* setting = std::getenv("KYTY_GPU_MARKERS");
	return setting != nullptr && std::strcmp(setting, "1") == 0;
}

void InitializeGpuCrashMarkers(GraphicContext& graphics) {
	std::lock_guard lock(marker_mutex);
	if (states.contains(graphics.device)) return;
	VkBufferCreateInfo buffer {VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO};
	buffer.size  = MaxMarkers * 2u * sizeof(uint32_t);
	buffer.usage = VK_BUFFER_USAGE_TRANSFER_DST_BIT;
	VmaAllocationCreateInfo allocation {};
	allocation.usage = VMA_MEMORY_USAGE_AUTO_PREFER_HOST;
	allocation.flags =
	    VMA_ALLOCATION_CREATE_MAPPED_BIT | VMA_ALLOCATION_CREATE_HOST_ACCESS_RANDOM_BIT;
	allocation.requiredFlags =
	    VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT;
	MarkerState       state;
	VmaAllocationInfo info {};
	const auto result = vmaCreateBuffer(graphics.allocator, &buffer, &allocation, &state.buffer,
	                                    &state.allocation, &info);
	if (result != VK_SUCCESS) {
		Log::WriteToConsoleAndLog(
		    fmt::format("GPU marker allocation failed: {}\n", static_cast<int>(result)));
		return;
	}
	std::memset(info.pMappedData, 0, static_cast<size_t>(buffer.size));
	state.mapped = static_cast<const volatile uint32_t*>(info.pMappedData);
	state.records.reserve(MaxMarkers);
	states.emplace(graphics.device, std::move(state));
	RegisterGpuFaultExtraInfo(DescribeGpuCrashMarkers);
	Log::WriteToConsoleAndLog(
	    "GPU crash markers: ready; activate at first BDA use, no per-draw waits.\n");
}

void DestroyGpuCrashMarkers(GraphicContext& graphics) {
	std::lock_guard lock(marker_mutex);
	if (auto found = states.find(graphics.device); found != states.end()) {
		vmaDestroyBuffer(graphics.allocator, found->second.buffer, found->second.allocation);
		states.erase(found);
	}
	if (states.empty()) armed.store(false, std::memory_order_relaxed);
}

void ArmGpuCrashMarkers() {
	std::lock_guard lock(marker_mutex);
	if (!states.empty() && !armed.exchange(true, std::memory_order_relaxed))
		Log::WriteToConsoleAndLog("GPU crash markers: recording draws and dispatches.\n");
}

uint32_t BeginGpuCrashMarker(GraphicContext& graphics, vk::CommandBuffer command,
                             const GpuCrashMarkerInfo& info) {
	if (!armed.load(std::memory_order_relaxed)) return 0;
	std::lock_guard lock(marker_mutex);
	auto            found = states.find(graphics.device);
	if (found == states.end()) return 0;
	auto& state = found->second;
	if (state.records.size() >= MaxMarkers) return 0;
	state.records.push_back(info);
	const auto token = static_cast<uint32_t>(state.records.size());
	command.writeBufferMarkerAMD(vk::PipelineStageFlagBits::eTopOfPipe, state.buffer,
	                             (token - 1u) * 2u * sizeof(uint32_t), token);
	return token;
}

void EndGpuCrashMarker(GraphicContext& graphics, vk::CommandBuffer command, uint32_t token) {
	if (token == 0) return;
	std::lock_guard lock(marker_mutex);
	auto            found = states.find(graphics.device);
	if (found == states.end()) return;
	command.writeBufferMarkerAMD(vk::PipelineStageFlagBits::eBottomOfPipe, found->second.buffer,
	                             ((token - 1u) * 2u + 1u) * sizeof(uint32_t), token);
}

std::string DescribeGpuCrashMarkers(GraphicContext& graphics) {
	std::lock_guard lock(marker_mutex);
	auto            found = states.find(graphics.device);
	if (found == states.end()) return "GPU crash markers: not enabled.\n";
	const auto& state      = found->second;
	uint32_t    last_start = 0, last_end = 0;
	for (uint32_t index = 0; index < state.records.size(); ++index) {
		const uint32_t values[] {state.mapped[index * 2u], state.mapped[index * 2u + 1u]};
		if (values[0] == index + 1u) last_start = index + 1u;
		if (values[1] == index + 1u) last_end = index + 1u;
	}
	std::string report =
	    fmt::format("GPU markers: recorded={} highest start={} highest end={} capacity={}\n",
	                state.records.size(), last_start, last_end, MaxMarkers);
	report += "Post-loss marker reads are best effort; intervals also include unmarked helper "
	          "commands.\n";
	const uint32_t first = last_end > 32u ? last_end - 32u : 0u;
	const uint32_t end   = std::min<uint32_t>(state.records.size(), last_end + 128u);
	for (uint32_t index = first; index < end; ++index) {
		const uint32_t values[] {state.mapped[index * 2u], state.mapped[index * 2u + 1u]};
		const auto&    item = state.records[index];
		report +=
		    fmt::format("Marker {}: start={} end={} kind={} submit={} shader={}/{} "
		                "hash={:016x}/{:016x} args={},{},{}\n",
		                index + 1u, values[0], values[1], item.kind, item.submit, item.shader0,
		                item.shader1, item.hash0, item.hash1, item.x, item.y, item.z);
	}
	return report;
}

} // namespace Libs::Graphics
