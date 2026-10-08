#include "graphics/host_gpu/graphicContext.h"
#include <atomic>
#include <chrono>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <stdexcept>
#include <thread>

using namespace Libs::Graphics;
static std::atomic<int> calls;
static int mode;
static VKAPI_ATTR VkResult VKAPI_CALL Fault(VkDevice,
                                            VkDeviceFaultCountsEXT *counts,
                                            VkDeviceFaultInfoEXT *info) {
  ++calls;
  if (mode == 2)
    return VK_ERROR_UNKNOWN;
  if (!info) {
    counts->addressInfoCount = mode == 1 ? 0 : 1;
    counts->vendorInfoCount = mode == 1 ? 0 : 1;
    counts->vendorBinarySize = mode == 1 ? 0 : 4;
    return VK_SUCCESS;
  }
  std::strcpy(info->description, mode == 1
                                     ? "Synthetic timeout without addresses"
                                     : "Synthetic invalid GPU read");
  if (mode == 0) {
    info->pAddressInfos[0] = {VK_DEVICE_FAULT_ADDRESS_TYPE_READ_INVALID_EXT,
                              0xdecafbad, 4};
    std::strcpy(info->pVendorInfos[0].description, "Synthetic vendor detail");
    info->pVendorInfos[0].vendorFaultCode = 42;
    info->pVendorInfos[0].vendorFaultData = 99;
    std::memcpy(info->pVendorBinaryData, "TEST", 4);
  }
  return VK_SUCCESS;
}
static void Require(bool condition, const char *message) {
  if (!condition)
    throw std::runtime_error(message);
}
int main(int argc, char **argv) {
  try {
    Require(argc == 2, "test directory required");
    const auto root =
        std::filesystem::absolute(argv[1]) /
        std::to_string(
            std::chrono::steady_clock::now().time_since_epoch().count());
    VULKAN_HPP_DEFAULT_DISPATCHER.vkGetDeviceFaultInfoEXT = Fault;
    for (int test = 0; test < 4; ++test) {
      auto folder = root / ("case-" + std::to_string(test));
      std::filesystem::create_directories(folder);
      std::filesystem::current_path(folder);
      GraphicContext graphics;
      graphics.device = vk::Device(reinterpret_cast<VkDevice>(uintptr_t{1}));
      graphics.device_fault_enabled = test != 3;
      graphics.device_fault_binary_enabled = test == 0;
      mode = test;
      calls = 0;
      std::vector<std::thread> threads;
      for (int index = 0; index < 8; ++index)
        threads.emplace_back([&] { ReportDeviceFault(graphics); });
      for (auto &thread : threads)
        thread.join();
      Require(calls == (test < 2    ? 2
                        : test == 2 ? 1
                                    : 0),
              "duplicate or missing fault query");
      int text_count = 0, binary_count = 0;
      for (const auto &file :
           std::filesystem::directory_iterator("_GpuCrashDumps")) {
        std::ifstream input(file.path(), std::ios::binary);
        std::string content{std::istreambuf_iterator<char>(input), {}};
        if (file.path().extension() == ".txt") {
          ++text_count;
          Require(content.find(test == 0   ? "decafbad"
                               : test == 1 ? "timeout without addresses"
                               : test == 2
                                   ? "ErrorUnknown"
                                   : "unavailable") != std::string::npos,
                  "fault details missing from saved report");
        } else {
          ++binary_count;
          Require(content == "TEST", "binary dump differs from driver data");
        }
      }
      Require(text_count == 1 && binary_count == (test == 0 ? 1 : 0),
              "wrong saved report count");
      std::cout << "Fault report case " << test << " passed\n";
    }
    return 0;
  } catch (const std::exception &error) {
    std::cerr << error.what() << '\n';
    return 1;
  }
}
