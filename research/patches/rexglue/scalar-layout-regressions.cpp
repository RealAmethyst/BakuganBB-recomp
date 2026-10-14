// Original synthetic fixture: no Vulkan loader, physical device or guest input.
// Compile only this fixture with -fno-access-control to inject the instance table.
#include <rex/ui/vulkan/device.h>
#include <rex/logging.h>
#include <spdlog/sinks/ostream_sink.h>
#include <cstring>
#include <iostream>
#include <sstream>
#include <string>
#include <vector>

using rex::ui::vulkan::VulkanDevice;
using rex::ui::vulkan::VulkanInstance;

struct Case {
  const char* name;
  unsigned minor;
  bool properties2;
  bool extension;
  bool feature;
  bool gpu;
  bool expected_accept;
  bool independent_blend = true;
  bool swapchain = false;
  bool swapchain_extension = false;
};
const Case* current;
unsigned create_calls;
unsigned query_core, query_ext, enable_core, enable_ext;
bool scalar_enabled, extension_enabled;
bool invalid_chain;

void VKAPI_CALL Properties(VkPhysicalDevice, VkPhysicalDeviceProperties* p) {
  *p = {};
  p->apiVersion = VK_MAKE_API_VERSION(0, 1, current->minor, 0);
  std::strcpy(p->deviceName, current->name);
}
void VKAPI_CALL Features(VkPhysicalDevice, VkPhysicalDeviceFeatures* f) {
  *f = {};
  f->independentBlend = current->independent_blend;
  f->fragmentStoresAndAtomics = VK_TRUE;
  f->vertexPipelineStoresAndAtomics = VK_TRUE;
  f->geometryShader = VK_TRUE;
  f->fillModeNonSolid = VK_TRUE;
}
VkResult VKAPI_CALL Extensions(VkPhysicalDevice, const char*, uint32_t* count,
                              VkExtensionProperties* properties) {
  std::vector<const char*> names;
  if (current->extension) names.push_back(VK_EXT_SCALAR_BLOCK_LAYOUT_EXTENSION_NAME);
  if (current->swapchain_extension) names.push_back(VK_KHR_SWAPCHAIN_EXTENSION_NAME);
  if (properties) {
    for (size_t i = 0; i < names.size(); ++i) {
      properties[i] = {};
      std::strcpy(properties[i].extensionName, names[i]);
      properties[i].specVersion = 1;
    }
  }
  *count = names.size();
  return VK_SUCCESS;
}
void VKAPI_CALL Properties2(VkPhysicalDevice, VkPhysicalDeviceProperties2*) {}
void VKAPI_CALL Features2(VkPhysicalDevice, VkPhysicalDeviceFeatures2* f) {
  for (auto* p = static_cast<VkBaseOutStructure*>(f->pNext); p; p = p->pNext) {
    if (p->sType == VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_1_2_FEATURES) {
      ++query_core;
      reinterpret_cast<VkPhysicalDeviceVulkan12Features*>(p)->scalarBlockLayout = current->feature;
    }
    if (p->sType == VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_SCALAR_BLOCK_LAYOUT_FEATURES_EXT) {
      ++query_ext;
      reinterpret_cast<VkPhysicalDeviceScalarBlockLayoutFeaturesEXT*>(p)->scalarBlockLayout =
          current->feature;
    }
  }
  invalid_chain |= query_core && query_ext;
}
void VKAPI_CALL Queues(VkPhysicalDevice, uint32_t* count, VkQueueFamilyProperties* q) {
  *count = 1;
  if (q) {
    *q = {};
    q->queueFlags = VK_QUEUE_GRAPHICS_BIT | VK_QUEUE_COMPUTE_BIT;
    q->queueCount = 1;
  }
}
VkResult VKAPI_CALL CreateDevice(VkPhysicalDevice, const VkDeviceCreateInfo* info,
                               const VkAllocationCallbacks*, VkDevice* device) {
  ++create_calls;
  for (uint32_t i = 0; i < info->enabledExtensionCount; ++i) {
    extension_enabled |= std::strcmp(info->ppEnabledExtensionNames[i],
                                    VK_EXT_SCALAR_BLOCK_LAYOUT_EXTENSION_NAME) == 0;
  }
  for (auto* p = static_cast<const VkBaseInStructure*>(info->pNext); p; p = p->pNext) {
    if (p->sType == VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_1_2_FEATURES) {
      ++enable_core;
      scalar_enabled |= reinterpret_cast<const VkPhysicalDeviceVulkan12Features*>(p)->scalarBlockLayout;
    }
    if (p->sType == VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_SCALAR_BLOCK_LAYOUT_FEATURES_EXT) {
      ++enable_ext;
      scalar_enabled |= reinterpret_cast<const VkPhysicalDeviceScalarBlockLayoutFeaturesEXT*>(p)->scalarBlockLayout;
    }
  }
  invalid_chain |= enable_core && enable_ext;
  *device = reinterpret_cast<VkDevice>(uintptr_t{2});
  return VK_SUCCESS;
}
void VKAPI_CALL DestroyDevice(VkDevice, const VkAllocationCallbacks*) {}
void VKAPI_CALL GetQueue(VkDevice, uint32_t, uint32_t, VkQueue* q) {
  *q = reinterpret_cast<VkQueue>(uintptr_t{3});
}
void VKAPI_CALL MemoryProperties(VkPhysicalDevice, VkPhysicalDeviceMemoryProperties* p) { *p = {}; }
void VKAPI_CALL UncalledFunction() { std::abort(); }
PFN_vkVoidFunction VKAPI_CALL GetProc(VkDevice, const char* name) {
  if (std::strcmp(name, "vkGetDeviceQueue") == 0) return reinterpret_cast<PFN_vkVoidFunction>(GetQueue);
  return UncalledFunction;
}

int main() {
  std::ostringstream logs;
  rex::LogConfig config;
  config.extra_sinks.push_back(std::make_shared<spdlog::sinks::ostream_sink_mt>(logs));
  rex::InitLogging(config);
  const Case cases[] = {
      {"core12-supported", 2, true, false, true, true, true},
      {"core12-unsupported", 2, true, false, false, true, false},
      {"core13-supported", 3, true, false, true, true, true},
      {"core13-unsupported", 3, true, false, false, true, false},
      {"core12-advertises-extension", 2, true, true, true, true, true},
      {"ext11-supported", 1, true, true, true, true, true},
      {"ext11-feature-false", 1, true, true, false, true, false},
      {"ext11-missing-extension", 1, true, false, true, true, false},
      {"ext10-supported", 0, true, true, true, true, true},
      {"ext10-feature-false", 0, true, true, false, true, false},
      {"ext10-no-properties2", 0, false, true, true, true, false},
      {"ext10-no-extensions", 0, false, false, false, true, false},
      {"ui10-no-scalar", 0, false, false, false, false, true},
      {"ui11-no-scalar", 1, true, false, false, false, true},
      {"ui12-no-scalar", 2, true, false, false, false, true},
      {"ui12-scalar-not-enabled", 2, true, true, true, false, true},
      {"missing-independent-blend", 2, true, false, true, true, false, false},
      {"missing-swapchain", 2, true, false, true, true, false, true, true, false},
      {"ext11-with-swapchain", 1, true, true, true, true, true, true, true, true},
  };
  unsigned failures = 0;
  for (const auto& c : cases) {
    current = &c;
    create_calls = query_core = query_ext = enable_core = enable_ext = 0;
    scalar_enabled = extension_enabled = invalid_chain = false;
    logs.str("");
    VulkanInstance instance;
    instance.api_version_ = VK_MAKE_API_VERSION(0, 1, c.minor, 0);
    instance.extensions_.ext_1_1_KHR_get_physical_device_properties2 = c.properties2;
    auto& f = instance.functions_;
    f.vkGetPhysicalDeviceProperties = Properties;
    f.vkGetPhysicalDeviceFeatures = Features;
    f.vkEnumerateDeviceExtensionProperties = Extensions;
    f.vkGetPhysicalDeviceProperties2 = Properties2;
    f.vkGetPhysicalDeviceFeatures2 = Features2;
    f.vkGetPhysicalDeviceQueueFamilyProperties = Queues;
    f.vkCreateDevice = CreateDevice;
    f.vkDestroyDevice = DestroyDevice;
    f.vkGetDeviceProcAddr = GetProc;
    f.vkGetPhysicalDeviceMemoryProperties = MemoryProperties;
    auto result = VulkanDevice::CreateIfSupported(
        &instance, reinterpret_cast<VkPhysicalDevice>(uintptr_t{1}), c.gpu, c.swapchain);
    bool pass = bool(result) == c.expected_accept && !invalid_chain;
    if (c.expected_accept) {
      pass &= create_calls == 1;
      pass &= scalar_enabled == c.gpu;
      pass &= result && result->properties().scalarBlockLayout == c.gpu;
      pass &= extension_enabled == (c.gpu && c.minor < 2);
      if (c.gpu) {
        pass &= c.minor >= 2 ? query_core == 1 && enable_core == 1 && query_ext == 0 && enable_ext == 0
                           : query_ext == 1 && enable_ext == 1 && query_core == 0 && enable_core == 0;
      }
    } else {
      pass &= create_calls == 0;
      if (c.independent_blend && !c.swapchain) {
        pass &= logs.str().find("doesn't support scalarBlockLayout") != std::string::npos;
        pass &= logs.str().find(c.name) != std::string::npos;
      }
    }
    std::cout << (pass ? "PASS " : "FAIL ") << c.name << " accepted=" << bool(result)
              << " create=" << create_calls << " query-core=" << query_core
              << " query-ext=" << query_ext << " enabled=" << scalar_enabled
              << " extension=" << extension_enabled << '\n';
    if (!pass) ++failures;
  }
  rex::ShutdownLogging();
  std::cout << "RESULT cases=" << std::size(cases) << " failures=" << failures << '\n';
  return failures ? 1 : 0;
}
