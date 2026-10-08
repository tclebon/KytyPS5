#include "graphics/host_gpu/vulkanCommon.h"

#include "common/assert.h"
#include "common/logging/log.h"
#include "graphics/guest_gpu/gpu_defs.h"
#include "graphics/host_gpu/graphicContext.h"

#include <algorithm>
#include <array>
#include <chrono>
#include <filesystem>
#include <fstream>

VULKAN_HPP_DEFAULT_DISPATCH_LOADER_DYNAMIC_STORAGE

namespace Libs::Graphics {
namespace {

struct FormatMapping {
	Prospero::BufferFormat guest;
	vk::Format             host;
};

constexpr FormatMapping kFormatMappings[] = {
    {Prospero::BufferFormat::k8UNorm, vk::Format::eR8Unorm},
    {Prospero::BufferFormat::k8SNorm, vk::Format::eR8Snorm},
    {Prospero::BufferFormat::k8UScaled, vk::Format::eR8Uscaled},
    {Prospero::BufferFormat::k8SScaled, vk::Format::eR8Sscaled},
    {Prospero::BufferFormat::k8UInt, vk::Format::eR8Uint},
    {Prospero::BufferFormat::k8SInt, vk::Format::eR8Sint},
    {Prospero::BufferFormat::k16UNorm, vk::Format::eR16Unorm},
    {Prospero::BufferFormat::k16SNorm, vk::Format::eR16Snorm},
    {Prospero::BufferFormat::k16UScaled, vk::Format::eR16Uscaled},
    {Prospero::BufferFormat::k16SScaled, vk::Format::eR16Sscaled},
    {Prospero::BufferFormat::k16UInt, vk::Format::eR16Uint},
    {Prospero::BufferFormat::k16SInt, vk::Format::eR16Sint},
    {Prospero::BufferFormat::k16Float, vk::Format::eR16Sfloat},
    {Prospero::BufferFormat::k8_8UNorm, vk::Format::eR8G8Unorm},
    {Prospero::BufferFormat::k8_8SNorm, vk::Format::eR8G8Snorm},
    {Prospero::BufferFormat::k8_8UScaled, vk::Format::eR8G8Uscaled},
    {Prospero::BufferFormat::k8_8SScaled, vk::Format::eR8G8Sscaled},
    {Prospero::BufferFormat::k8_8UInt, vk::Format::eR8G8Uint},
    {Prospero::BufferFormat::k8_8SInt, vk::Format::eR8G8Sint},
    {Prospero::BufferFormat::k32UInt, vk::Format::eR32Uint},
    {Prospero::BufferFormat::k32SInt, vk::Format::eR32Sint},
    {Prospero::BufferFormat::k32Float, vk::Format::eR32Sfloat},
    {Prospero::BufferFormat::k16_16UNorm, vk::Format::eR16G16Unorm},
    {Prospero::BufferFormat::k16_16SNorm, vk::Format::eR16G16Snorm},
    {Prospero::BufferFormat::k16_16UScaled, vk::Format::eR16G16Uscaled},
    {Prospero::BufferFormat::k16_16SScaled, vk::Format::eR16G16Sscaled},
    {Prospero::BufferFormat::k16_16UInt, vk::Format::eR16G16Uint},
    {Prospero::BufferFormat::k16_16SInt, vk::Format::eR16G16Sint},
    {Prospero::BufferFormat::k16_16Float, vk::Format::eR16G16Sfloat},
    {Prospero::BufferFormat::k11_11_10Float, vk::Format::eB10G11R11UfloatPack32},
    {Prospero::BufferFormat::k10_10_10_2UNorm, vk::Format::eA2B10G10R10UnormPack32},
    {Prospero::BufferFormat::k10_10_10_2SNorm, vk::Format::eA2B10G10R10SnormPack32},
    {Prospero::BufferFormat::k10_10_10_2UScaled, vk::Format::eA2B10G10R10UscaledPack32},
    {Prospero::BufferFormat::k10_10_10_2UInt, vk::Format::eA2B10G10R10UintPack32},
    {Prospero::BufferFormat::k8_8_8_8UNorm, vk::Format::eR8G8B8A8Unorm},
    {Prospero::BufferFormat::k8_8_8_8SNorm, vk::Format::eR8G8B8A8Snorm},
    {Prospero::BufferFormat::k8_8_8_8UScaled, vk::Format::eR8G8B8A8Uscaled},
    {Prospero::BufferFormat::k8_8_8_8SScaled, vk::Format::eR8G8B8A8Sscaled},
    {Prospero::BufferFormat::k8_8_8_8UInt, vk::Format::eR8G8B8A8Uint},
    {Prospero::BufferFormat::k8_8_8_8SInt, vk::Format::eR8G8B8A8Sint},
    {Prospero::BufferFormat::k32_32UInt, vk::Format::eR32G32Uint},
    {Prospero::BufferFormat::k32_32SInt, vk::Format::eR32G32Sint},
    {Prospero::BufferFormat::k32_32Float, vk::Format::eR32G32Sfloat},
    {Prospero::BufferFormat::k16_16_16_16UNorm, vk::Format::eR16G16B16A16Unorm},
    {Prospero::BufferFormat::k16_16_16_16SNorm, vk::Format::eR16G16B16A16Snorm},
    {Prospero::BufferFormat::k16_16_16_16UScaled, vk::Format::eR16G16B16A16Uscaled},
    {Prospero::BufferFormat::k16_16_16_16SScaled, vk::Format::eR16G16B16A16Sscaled},
    {Prospero::BufferFormat::k16_16_16_16UInt, vk::Format::eR16G16B16A16Uint},
    {Prospero::BufferFormat::k16_16_16_16SInt, vk::Format::eR16G16B16A16Sint},
    {Prospero::BufferFormat::k16_16_16_16Float, vk::Format::eR16G16B16A16Sfloat},
    {Prospero::BufferFormat::k32_32_32UInt, vk::Format::eR32G32B32Uint},
    {Prospero::BufferFormat::k32_32_32SInt, vk::Format::eR32G32B32Sint},
    {Prospero::BufferFormat::k32_32_32Float, vk::Format::eR32G32B32Sfloat},
    {Prospero::BufferFormat::k32_32_32_32UInt, vk::Format::eR32G32B32A32Uint},
    {Prospero::BufferFormat::k32_32_32_32SInt, vk::Format::eR32G32B32A32Sint},
    {Prospero::BufferFormat::k32_32_32_32Float, vk::Format::eR32G32B32A32Sfloat},
    // Narrow-channel sRGB formats are optional in Vulkan. Keep a same-width fallback until
    // sampler-aware sRGB emulation is available.
    {Prospero::BufferFormat::k8Srgb, vk::Format::eR8Unorm},
    {Prospero::BufferFormat::k8_8Srgb, vk::Format::eR8G8Unorm},
    {Prospero::BufferFormat::k8_8_8_8Srgb, vk::Format::eR8G8B8A8Srgb},
    {Prospero::BufferFormat::k9_9_9_5Float, vk::Format::eE5B9G9R9UfloatPack32},
    {Prospero::BufferFormat::k5_6_5UNorm, vk::Format::eB5G6R5UnormPack16},
    {Prospero::BufferFormat::k5_5_5_1UNorm, vk::Format::eA1R5G5B5UnormPack16},
    {Prospero::BufferFormat::k1_5_5_5UNorm, vk::Format::eR5G5B5A1UnormPack16},
    {Prospero::BufferFormat::k4_4_4_4UNorm, vk::Format::eR4G4B4A4UnormPack16},
    {Prospero::BufferFormat::kBc1UNorm, vk::Format::eBc1RgbaUnormBlock},
    {Prospero::BufferFormat::kBc1Srgb, vk::Format::eBc1RgbaSrgbBlock},
    {Prospero::BufferFormat::kBc2UNorm, vk::Format::eBc2UnormBlock},
    {Prospero::BufferFormat::kBc2Srgb, vk::Format::eBc2SrgbBlock},
    {Prospero::BufferFormat::kBc3UNorm, vk::Format::eBc3UnormBlock},
    {Prospero::BufferFormat::kBc3Srgb, vk::Format::eBc3SrgbBlock},
    {Prospero::BufferFormat::kBc4UNorm, vk::Format::eBc4UnormBlock},
    {Prospero::BufferFormat::kBc4SNorm, vk::Format::eBc4SnormBlock},
    {Prospero::BufferFormat::kBc5UNorm, vk::Format::eBc5UnormBlock},
    {Prospero::BufferFormat::kBc5SNorm, vk::Format::eBc5SnormBlock},
    {Prospero::BufferFormat::kBc6UFloat, vk::Format::eBc6HUfloatBlock},
    {Prospero::BufferFormat::kBc6SFloat, vk::Format::eBc6HSfloatBlock},
    {Prospero::BufferFormat::kBc7UNorm, vk::Format::eBc7UnormBlock},
    {Prospero::BufferFormat::kBc7Srgb, vk::Format::eBc7SrgbBlock},
};

constexpr auto MakeFormatLookup() {
	constexpr auto kMaxFormat = static_cast<size_t>(Prospero::BufferFormat::kBc7Srgb);
	std::array<vk::Format, kMaxFormat + 1> lookup {};
	lookup.fill(vk::Format::eUndefined);
	for (const auto& mapping: kFormatMappings) {
		lookup[static_cast<size_t>(mapping.guest)] = mapping.host;
	}
	return lookup;
}

constexpr auto kFormatLookup = MakeFormatLookup();

} // namespace

void ReportDeviceFault(GraphicContext& graphics) {
	// Other failing threads must wait for the first report before terminating.
	std::lock_guard lock(graphics.device_fault_report_mutex);
	if (graphics.device_fault_reported) {
		return;
	}
	graphics.device_fault_reported = true;
	try {
		std::string report = fmt::format("GPU device-loss report\nGPU: {}\nDriver: 0x{:08x}\n",
		                                 graphics.physical_device_properties.deviceName.data(),
		                                 graphics.physical_device_properties.driverVersion);
		std::vector<uint8_t> binary;
		if (!graphics.device_fault_enabled ||
		    VULKAN_HPP_DEFAULT_DISPATCHER.vkGetDeviceFaultInfoEXT == nullptr) {
			report += "Native fault reporting is unavailable on this device.\n";
		} else {
			vk::DeviceFaultCountsEXT counts {};
			const auto count_result = graphics.device.getFaultInfoEXT(&counts, nullptr);
			report += fmt::format("Fault count query: {}\n", vk::to_string(count_result));
			if (count_result == vk::Result::eSuccess) {
				report += fmt::format(
				    "Available addresses: {}, vendor records: {}, binary bytes: {}\n",
				    counts.addressInfoCount, counts.vendorInfoCount, counts.vendorBinarySize);
				// Bound allocations even when the driver supplies an unexpectedly large dump.
				counts.addressInfoCount = std::min(counts.addressInfoCount, 4096u);
				counts.vendorInfoCount  = std::min(counts.vendorInfoCount, 4096u);
				counts.vendorBinarySize =
				    std::min(counts.vendorBinarySize, vk::DeviceSize {64u * 1024u * 1024u});
				std::vector<vk::DeviceFaultAddressInfoEXT> addresses(counts.addressInfoCount);
				std::vector<vk::DeviceFaultVendorInfoEXT>  vendors(counts.vendorInfoCount);
				binary.resize(static_cast<size_t>(counts.vendorBinarySize));
				vk::DeviceFaultInfoEXT info {};
				info.pAddressInfos     = addresses.empty() ? nullptr : addresses.data();
				info.pVendorInfos      = vendors.empty() ? nullptr : vendors.data();
				info.pVendorBinaryData = binary.empty() ? nullptr : binary.data();
				const auto result      = graphics.device.getFaultInfoEXT(&counts, &info);
				report += fmt::format("Fault detail query: {}\n", vk::to_string(result));
				if (result == vk::Result::eSuccess || result == vk::Result::eIncomplete) {
					const auto description = [](const auto& text) {
						return std::string_view(
						    text.data(), std::find(text.begin(), text.end(), '\0') - text.begin());
					};
					report += fmt::format("Description: {}\n", description(info.description));
					for (size_t index = 0;
					     index < std::min<size_t>(addresses.size(), counts.addressInfoCount);
					     ++index) {
						const auto& address = addresses[index];
						report +=
						    fmt::format("Address {}: type={} address=0x{:016x} precision={}\n",
						                index, vk::to_string(address.addressType),
						                address.reportedAddress, address.addressPrecision);
					}
					for (size_t index = 0;
					     index < std::min<size_t>(vendors.size(), counts.vendorInfoCount);
					     ++index) {
						const auto& vendor = vendors[index];
						report += fmt::format("Vendor {}: {} code=0x{:016x} data=0x{:016x}\n",
						                      index, description(vendor.description),
						                      vendor.vendorFaultCode, vendor.vendorFaultData);
					}
					binary.resize(std::min<size_t>(binary.size(), counts.vendorBinarySize));
				} else {
					binary.clear();
				}
			}
		}
		Log::WriteToConsoleAndLog(report);
		std::error_code             error;
		const std::filesystem::path directory = "_GpuCrashDumps";
		std::filesystem::create_directories(directory, error);
		if (error) {
			Log::WriteToConsoleAndLog(
			    fmt::format("Could not create GPU crash directory: {}\n", error.message()));
			return;
		}
		const auto    timestamp = std::chrono::duration_cast<std::chrono::milliseconds>(
		                              std::chrono::system_clock::now().time_since_epoch())
		                              .count();
		const auto    prefix    = directory / fmt::format("gpu-fault-{}", timestamp);
		std::ofstream text(prefix.string() + ".txt", std::ios::binary);
		text << report;
		text.close();
		if (!text) {
			Log::WriteToConsoleAndLog("Could not save GPU fault text report.\n");
		}
		if (!binary.empty()) {
			std::ofstream dump(prefix.string() + ".bin", std::ios::binary);
			dump.write(reinterpret_cast<const char*>(binary.data()),
			           static_cast<std::streamsize>(binary.size()));
			dump.close();
			if (!dump) {
				Log::WriteToConsoleAndLog("Could not save GPU fault binary dump.\n");
			}
		}
		Log::WriteToConsoleAndLog(
		    fmt::format("GPU fault reports: {}\n", std::filesystem::absolute(prefix).string()));
	} catch (const std::exception& error) {
		Log::WriteToConsoleAndLog(fmt::format("GPU fault reporting failed: {}\n", error.what()));
	}
}

vk::Format VulkanFormat(Prospero::BufferFormat guest_format) {
	const auto index = static_cast<size_t>(guest_format);
	return index < kFormatLookup.size() ? kFormatLookup[index] : vk::Format::eUndefined;
}

void RequireVulkanSuccess(vk::Result result, const char* operation) {
	if (result != vk::Result::eSuccess) {
		EXIT("%s failed: %s (%d)\n", operation, vk::to_string(result).c_str(),
		     static_cast<int>(result));
	}
}

vk::ShaderModule CompileSPV(std::span<const uint32_t> code, vk::Device device) {
	vk::ShaderModuleCreateInfo create_info {};
	create_info.codeSize    = code.size_bytes();
	create_info.pCode       = code.data();
	vk::ShaderModule module = nullptr;
	RequireVulkanSuccess(device.createShaderModule(&create_info, nullptr, &module),
	                     "create SPIR-V shader module");
	return module;
}

} // namespace Libs::Graphics
