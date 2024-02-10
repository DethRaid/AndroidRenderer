
/*
 * Copyright (C) 2021-2023 Valve Corporation
 * Copyright (C) 2021-2023 LunarG, Inc.
 *
 * Licensed under the Apache License, Version 2.0 (the "License")
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 *
 * This file is ***GENERATED***.  Do Not Edit.
 * See scripts/gen_profiles_solution.py for modifications.
 */

#pragma once

#define VPAPI_ATTR inline

#include <vulkan/vulkan.h>

#include <cstddef>
#include <cstdarg>
#include <cstdio>
#include <cstring>
#include <cstdint>
#include <cassert>
#include <cmath>
#include <string>
#include <vector>
#include <algorithm>
#include <memory>
#include <map>

#define VP_HEADER_VERSION_COMPLETE VK_MAKE_API_VERSION(0, 2, 0, VK_HEADER_VERSION)

#define VP_MAX_PROFILE_NAME_SIZE 256U

typedef struct VpProfileProperties {
    char        profileName[VP_MAX_PROFILE_NAME_SIZE];
    uint32_t    specVersion;
} VpProfileProperties;

typedef struct VpBlockProperties {
    VpProfileProperties profiles;
    uint32_t apiVersion;
    char blockName[VP_MAX_PROFILE_NAME_SIZE];
} VpBlockProperties;

typedef enum VpInstanceCreateFlagBits {
    VP_INSTANCE_CREATE_FLAG_BITS_MAX_ENUM = 0x7FFFFFFF
} VpInstanceCreateFlagBits;
typedef VkFlags VpInstanceCreateFlags;

typedef struct VpInstanceCreateInfo {
    const VkInstanceCreateInfo* pCreateInfo;
    VpInstanceCreateFlags       flags;
    uint32_t                    enabledFullProfileCount;
    const VpProfileProperties*  pEnabledFullProfiles;
    uint32_t                    enabledProfileBlockCount;
    const VpBlockProperties*    pEnabledProfileBlocks;
} VpInstanceCreateInfo;

typedef enum VpDeviceCreateFlagBits {
    VP_DEVICE_CREATE_DISABLE_ROBUST_BUFFER_ACCESS_BIT = 0x0000001,
    VP_DEVICE_CREATE_DISABLE_ROBUST_IMAGE_ACCESS_BIT = 0x0000002,
    VP_DEVICE_CREATE_DISABLE_ROBUST_ACCESS =
        VP_DEVICE_CREATE_DISABLE_ROBUST_BUFFER_ACCESS_BIT | VP_DEVICE_CREATE_DISABLE_ROBUST_IMAGE_ACCESS_BIT,

    VP_DEVICE_CREATE_FLAG_BITS_MAX_ENUM = 0x7FFFFFFF
} VpDeviceCreateFlagBits;
typedef VkFlags VpDeviceCreateFlags;

typedef struct VpDeviceCreateInfo {
    const VkDeviceCreateInfo*   pCreateInfo;
    VpDeviceCreateFlags         flags;
    uint32_t                    enabledFullProfileCount;
    const VpProfileProperties*  pEnabledFullProfiles;
    uint32_t                    enabledProfileBlockCount;
    const VpBlockProperties*    pEnabledProfileBlocks;
} VpDeviceCreateInfo;

// Query the list of available profiles in the library
VPAPI_ATTR VkResult vpGetProfiles(uint32_t *pPropertyCount, VpProfileProperties *pProperties);

// List the required profiles of a profile
VPAPI_ATTR VkResult vpGetProfileRequiredProfiles(const VpProfileProperties* pProfile, uint32_t* pPropertyCount, VpProfileProperties* pProperties);

// Query the profile required Vulkan API version
VPAPI_ATTR uint32_t vpGetProfileAPIVersion(const VpProfileProperties* pProfile);

// List the recommended fallback profiles of a profile
VPAPI_ATTR VkResult vpGetProfileFallbacks(const VpProfileProperties *pProfile, uint32_t *pPropertyCount, VpProfileProperties *pProperties);

// Query whether the profile has multiple variants. Profiles with multiple variants can only use vpGetInstanceProfileSupport and vpGetPhysicalDeviceProfileSupport capabilities of the library. Other function will return a VK_ERROR_UNKNOWN error
VPAPI_ATTR VkResult vpHasMultipleVariantsProfile(const VpProfileProperties *pProfile, VkBool32 *pHasMultipleVariants);

// Check whether a profile is supported at the instance level
VPAPI_ATTR VkResult vpGetInstanceProfileSupport(const char *pLayerName, const VpProfileProperties *pProfile, VkBool32 *pSupported);

// Check whether a variant of a profile is supported at the instance level and report this list of blocks used to validate the profiles
VPAPI_ATTR VkResult vpGetInstanceProfileVariantsSupport(const char *pLayerName, const VpProfileProperties *pProfile, VkBool32 *pSupported, uint32_t *pPropertyCount, VpBlockProperties* pProperties);

// Create a VkInstance with the profile instance extensions enabled
VPAPI_ATTR VkResult vpCreateInstance(const VpInstanceCreateInfo *pCreateInfo, const VkAllocationCallbacks *pAllocator, VkInstance *pInstance);

// Check whether a profile is supported by the physical device
VPAPI_ATTR VkResult vpGetPhysicalDeviceProfileSupport(VkInstance instance, VkPhysicalDevice physicalDevice, const VpProfileProperties *pProfile, VkBool32 *pSupported);

// Check whether a variant of a profile is supported by the physical device and report this list of blocks used to validate the profiles
VPAPI_ATTR VkResult vpGetPhysicalDeviceProfileVariantsSupport(VkInstance instance, VkPhysicalDevice physicalDevice, const VpProfileProperties *pProfile, VkBool32 *pSupported, uint32_t *pPropertyCount, VpBlockProperties* pProperties);

// Create a VkDevice with the profile features and device extensions enabled
VPAPI_ATTR VkResult vpCreateDevice(VkPhysicalDevice physicalDevice, const VpDeviceCreateInfo *pCreateInfo, const VkAllocationCallbacks *pAllocator, VkDevice *pDevice);

// Query the list of instance extensions of a profile
VPAPI_ATTR VkResult vpGetProfileInstanceExtensionProperties(const VpProfileProperties *pProfile, const char* pBlockName, uint32_t *pPropertyCount, VkExtensionProperties *pProperties);

// Query the list of device extensions of a profile
VPAPI_ATTR VkResult vpGetProfileDeviceExtensionProperties(const VpProfileProperties *pProfile, const char* pBlockName, uint32_t *pPropertyCount, VkExtensionProperties *pProperties);

// Fill the feature structures with the requirements of a profile
VPAPI_ATTR VkResult vpGetProfileFeatures(const VpProfileProperties *pProfile, const char* pBlockName, void *pNext);

// Query the list of feature structure types specified by the profile
VPAPI_ATTR VkResult vpGetProfileFeatureStructureTypes(const VpProfileProperties *pProfile, const char* pBlockName, uint32_t *pStructureTypeCount, VkStructureType *pStructureTypes);

// Fill the property structures with the requirements of a profile
VPAPI_ATTR VkResult vpGetProfileProperties(const VpProfileProperties *pProfile, const char* pBlockName, void *pNext);

// Query the list of property structure types specified by the profile
VPAPI_ATTR VkResult vpGetProfilePropertyStructureTypes(const VpProfileProperties *pProfile, const char* pBlockName, uint32_t *pStructureTypeCount, VkStructureType *pStructureTypes);

// Query the list of formats with specified requirements by a profile
VPAPI_ATTR VkResult vpGetProfileFormats(const VpProfileProperties *pProfile, const char* pBlockName, uint32_t *pFormatCount, VkFormat *pFormats);

// Query the requirements of a format for a profile
VPAPI_ATTR VkResult vpGetProfileFormatProperties(const VpProfileProperties *pProfile, const char* pBlockName, VkFormat format, void *pNext);

// Query the list of format structure types specified by the profile
VPAPI_ATTR VkResult vpGetProfileFormatStructureTypes(const VpProfileProperties *pProfile, const char* pBlockName, uint32_t *pStructureTypeCount, VkStructureType *pStructureTypes);

namespace detail {


VPAPI_ATTR std::string FormatString(const char* message, ...) {
    std::size_t const STRING_BUFFER(4096);

    assert(message != nullptr);
    assert(strlen(message) >= 1 && strlen(message) < STRING_BUFFER);

    char buffer[STRING_BUFFER];
    va_list list;

    va_start(list, message);
    vsnprintf(buffer, STRING_BUFFER, message, list);
    va_end(list);

    return buffer;
}

VPAPI_ATTR const void* vpGetStructure(const void* pNext, VkStructureType type) {
    const VkBaseOutStructure* p = static_cast<const VkBaseOutStructure*>(pNext);
    while (p != nullptr) {
        if (p->sType == type) return p;
        p = p->pNext;
    }
    return nullptr;
}

VPAPI_ATTR void* vpGetStructure(void* pNext, VkStructureType type) {
    VkBaseOutStructure* p = static_cast<VkBaseOutStructure*>(pNext);
    while (p != nullptr) {
        if (p->sType == type) return p;
        p = p->pNext;
    }
    return nullptr;
}

VPAPI_ATTR VkBaseOutStructure* vpExtractStructure(VkPhysicalDeviceFeatures2KHR* pFeatures, VkStructureType structureType) {
    if (structureType == VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_FEATURES_2_KHR) {
        return nullptr;
    }

    VkBaseOutStructure* current = reinterpret_cast<VkBaseOutStructure*>(pFeatures);
    VkBaseOutStructure* previous = nullptr;
    VkBaseOutStructure* found = nullptr;

    while (current != nullptr) {
        if (structureType == current->sType) {
            found = current;
            if (previous != nullptr) {
                previous->pNext = current->pNext;
            }
            current = nullptr;
        } else {
            previous = current;
            current = current->pNext;
        }
    }

    if (found != nullptr) {
        found->pNext = nullptr;
        return found;
    } else {
        return nullptr;
    }
}

VPAPI_ATTR void GatherStructureTypes(std::vector<VkStructureType>& structureTypes, VkBaseOutStructure* pNext) {
    while (pNext) {
        if (std::find(structureTypes.begin(), structureTypes.end(), pNext->sType) == structureTypes.end()) {
            structureTypes.push_back(pNext->sType);
        }

        pNext = pNext->pNext;
    }
}

VPAPI_ATTR bool isMultiple(double source, double multiple) {
    double mod = std::fmod(source, multiple);
    return std::abs(mod) < 0.0001; 
}

VPAPI_ATTR bool isPowerOfTwo(double source) {
    double mod = std::fmod(source, 1.0);
    if (std::abs(mod) >= 0.0001) return false;

    std::uint64_t value = static_cast<std::uint64_t>(std::abs(source));
    return !(value & (value - static_cast<std::uint64_t>(1)));
}

using PFN_vpStructFiller = void(*)(VkBaseOutStructure* p);
using PFN_vpStructComparator = bool(*)(VkBaseOutStructure* p);
using PFN_vpStructChainerCb =  void(*)(VkBaseOutStructure* p, void* pUser);
using PFN_vpStructChainer = void(*)(VkBaseOutStructure* p, void* pUser, PFN_vpStructChainerCb pfnCb);

struct VpFeatureDesc {
    PFN_vpStructFiller              pfnFiller;
    PFN_vpStructComparator          pfnComparator;
};

struct VpPropertyDesc {
    PFN_vpStructFiller              pfnFiller;
    PFN_vpStructComparator          pfnComparator;
};

struct VpQueueFamilyDesc {
    PFN_vpStructFiller              pfnFiller;
    PFN_vpStructComparator          pfnComparator;
};

struct VpFormatDesc {
    VkFormat                        format;
    PFN_vpStructFiller              pfnFiller;
    PFN_vpStructComparator          pfnComparator;
};

struct VpStructChainerDesc {
    PFN_vpStructChainer             pfnFeature;
    PFN_vpStructChainer             pfnProperty;
    PFN_vpStructChainer             pfnQueueFamily;
    PFN_vpStructChainer             pfnFormat;
};

struct VpVariantDesc {
    char blockName[VP_MAX_PROFILE_NAME_SIZE];

    uint32_t instanceExtensionCount;
    const VkExtensionProperties* pInstanceExtensions;

    uint32_t deviceExtensionCount;
    const VkExtensionProperties* pDeviceExtensions;

    uint32_t featureStructTypeCount;
    const VkStructureType* pFeatureStructTypes;
    VpFeatureDesc feature;

    uint32_t propertyStructTypeCount;
    const VkStructureType* pPropertyStructTypes;
    VpPropertyDesc property;

    uint32_t queueFamilyStructTypeCount;
    const VkStructureType* pQueueFamilyStructTypes;
    uint32_t queueFamilyCount;
    const VpQueueFamilyDesc* pQueueFamilies;

    uint32_t formatStructTypeCount;
    const VkStructureType* pFormatStructTypes;
    uint32_t formatCount;
    const VpFormatDesc* pFormats;

    VpStructChainerDesc chainers;
};

struct VpCapabilitiesDesc {
    uint32_t variantCount;
    const VpVariantDesc* pVariants;
};

struct VpProfileDesc {
    VpProfileProperties             props;
    uint32_t                        minApiVersion;

    const detail::VpVariantDesc*    pMergedCapabilities;
    
    uint32_t                        requiredProfileCount;
    const VpProfileProperties*      pRequiredProfiles;

    uint32_t                        requiredCapabilityCount;
    const VpCapabilitiesDesc*       pRequiredCapabilities;

    uint32_t                        fallbackCount;
    const VpProfileProperties*      pFallbacks;
};

template <typename T>
VPAPI_ATTR bool vpCheckFlags(const T& actual, const uint64_t expected) {
    return (actual & expected) == expected;
}

static const VpProfileDesc profiles[] = {
};
static const uint32_t profileCount = static_cast<uint32_t>(std::size(profiles));


struct FeaturesChain {
    std::map<VkStructureType, std::size_t> structureSize;

    template<typename T>
    constexpr std::size_t size() const {
        return (sizeof(T) - sizeof(VkBaseOutStructure)) / sizeof(VkBool32);
    }

	// Chain with all Vulkan Features structures
    VkPhysicalDeviceDeviceGeneratedCommandsFeaturesNV physicalDeviceDeviceGeneratedCommandsFeaturesNV{ VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_DEVICE_GENERATED_COMMANDS_FEATURES_NV, nullptr };
    VkPhysicalDeviceDeviceGeneratedCommandsComputeFeaturesNV physicalDeviceDeviceGeneratedCommandsComputeFeaturesNV{ VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_DEVICE_GENERATED_COMMANDS_COMPUTE_FEATURES_NV, nullptr };
    VkPhysicalDevicePrivateDataFeatures physicalDevicePrivateDataFeatures{ VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_PRIVATE_DATA_FEATURES, nullptr };
    VkPhysicalDeviceVariablePointersFeatures physicalDeviceVariablePointersFeatures{ VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VARIABLE_POINTERS_FEATURES, nullptr };
    VkPhysicalDeviceMultiviewFeatures physicalDeviceMultiviewFeatures{ VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_MULTIVIEW_FEATURES, nullptr };
    VkPhysicalDevicePresentIdFeaturesKHR physicalDevicePresentIdFeaturesKHR{ VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_PRESENT_ID_FEATURES_KHR, nullptr };
    VkPhysicalDevicePresentWaitFeaturesKHR physicalDevicePresentWaitFeaturesKHR{ VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_PRESENT_WAIT_FEATURES_KHR, nullptr };
    VkPhysicalDevice16BitStorageFeatures physicalDevice16BitStorageFeatures{ VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_16BIT_STORAGE_FEATURES, nullptr };
    VkPhysicalDeviceShaderSubgroupExtendedTypesFeatures physicalDeviceShaderSubgroupExtendedTypesFeatures{ VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_SHADER_SUBGROUP_EXTENDED_TYPES_FEATURES, nullptr };
    VkPhysicalDeviceSamplerYcbcrConversionFeatures physicalDeviceSamplerYcbcrConversionFeatures{ VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_SAMPLER_YCBCR_CONVERSION_FEATURES, nullptr };
    VkPhysicalDeviceProtectedMemoryFeatures physicalDeviceProtectedMemoryFeatures{ VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_PROTECTED_MEMORY_FEATURES, nullptr };
    VkPhysicalDeviceBlendOperationAdvancedFeaturesEXT physicalDeviceBlendOperationAdvancedFeaturesEXT{ VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_BLEND_OPERATION_ADVANCED_FEATURES_EXT, nullptr };
    VkPhysicalDeviceMultiDrawFeaturesEXT physicalDeviceMultiDrawFeaturesEXT{ VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_MULTI_DRAW_FEATURES_EXT, nullptr };
    VkPhysicalDeviceInlineUniformBlockFeatures physicalDeviceInlineUniformBlockFeatures{ VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_INLINE_UNIFORM_BLOCK_FEATURES, nullptr };
    VkPhysicalDeviceMaintenance4Features physicalDeviceMaintenance4Features{ VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_MAINTENANCE_4_FEATURES, nullptr };
    VkPhysicalDeviceMaintenance5FeaturesKHR physicalDeviceMaintenance5FeaturesKHR{ VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_MAINTENANCE_5_FEATURES_KHR, nullptr };
    VkPhysicalDeviceMaintenance6FeaturesKHR physicalDeviceMaintenance6FeaturesKHR{ VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_MAINTENANCE_6_FEATURES_KHR, nullptr };
    VkPhysicalDeviceShaderDrawParametersFeatures physicalDeviceShaderDrawParametersFeatures{ VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_SHADER_DRAW_PARAMETERS_FEATURES, nullptr };
    VkPhysicalDeviceShaderFloat16Int8Features physicalDeviceShaderFloat16Int8Features{ VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_SHADER_FLOAT16_INT8_FEATURES, nullptr };
    VkPhysicalDeviceHostQueryResetFeatures physicalDeviceHostQueryResetFeatures{ VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_HOST_QUERY_RESET_FEATURES, nullptr };
    VkPhysicalDeviceGlobalPriorityQueryFeaturesKHR physicalDeviceGlobalPriorityQueryFeaturesKHR{ VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_GLOBAL_PRIORITY_QUERY_FEATURES_KHR, nullptr };
    VkPhysicalDeviceDeviceMemoryReportFeaturesEXT physicalDeviceDeviceMemoryReportFeaturesEXT{ VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_DEVICE_MEMORY_REPORT_FEATURES_EXT, nullptr };
    VkPhysicalDeviceDescriptorIndexingFeatures physicalDeviceDescriptorIndexingFeatures{ VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_DESCRIPTOR_INDEXING_FEATURES, nullptr };
    VkPhysicalDeviceTimelineSemaphoreFeatures physicalDeviceTimelineSemaphoreFeatures{ VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_TIMELINE_SEMAPHORE_FEATURES, nullptr };
    VkPhysicalDevice8BitStorageFeatures physicalDevice8BitStorageFeatures{ VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_8BIT_STORAGE_FEATURES, nullptr };
    VkPhysicalDeviceConditionalRenderingFeaturesEXT physicalDeviceConditionalRenderingFeaturesEXT{ VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_CONDITIONAL_RENDERING_FEATURES_EXT, nullptr };
    VkPhysicalDeviceVulkanMemoryModelFeatures physicalDeviceVulkanMemoryModelFeatures{ VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_MEMORY_MODEL_FEATURES, nullptr };
    VkPhysicalDeviceShaderAtomicInt64Features physicalDeviceShaderAtomicInt64Features{ VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_SHADER_ATOMIC_INT64_FEATURES, nullptr };
    VkPhysicalDeviceShaderAtomicFloatFeaturesEXT physicalDeviceShaderAtomicFloatFeaturesEXT{ VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_SHADER_ATOMIC_FLOAT_FEATURES_EXT, nullptr };
    VkPhysicalDeviceShaderAtomicFloat2FeaturesEXT physicalDeviceShaderAtomicFloat2FeaturesEXT{ VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_SHADER_ATOMIC_FLOAT_2_FEATURES_EXT, nullptr };
    VkPhysicalDeviceVertexAttributeDivisorFeaturesKHR physicalDeviceVertexAttributeDivisorFeaturesKHR{ VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VERTEX_ATTRIBUTE_DIVISOR_FEATURES_KHR, nullptr };
    VkPhysicalDeviceASTCDecodeFeaturesEXT physicalDeviceASTCDecodeFeaturesEXT{ VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_ASTC_DECODE_FEATURES_EXT, nullptr };
    VkPhysicalDeviceTransformFeedbackFeaturesEXT physicalDeviceTransformFeedbackFeaturesEXT{ VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_TRANSFORM_FEEDBACK_FEATURES_EXT, nullptr };
    VkPhysicalDeviceRepresentativeFragmentTestFeaturesNV physicalDeviceRepresentativeFragmentTestFeaturesNV{ VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_REPRESENTATIVE_FRAGMENT_TEST_FEATURES_NV, nullptr };
    VkPhysicalDeviceExclusiveScissorFeaturesNV physicalDeviceExclusiveScissorFeaturesNV{ VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_EXCLUSIVE_SCISSOR_FEATURES_NV, nullptr };
    VkPhysicalDeviceCornerSampledImageFeaturesNV physicalDeviceCornerSampledImageFeaturesNV{ VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_CORNER_SAMPLED_IMAGE_FEATURES_NV, nullptr };
    VkPhysicalDeviceComputeShaderDerivativesFeaturesNV physicalDeviceComputeShaderDerivativesFeaturesNV{ VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_COMPUTE_SHADER_DERIVATIVES_FEATURES_NV, nullptr };
    VkPhysicalDeviceShaderImageFootprintFeaturesNV physicalDeviceShaderImageFootprintFeaturesNV{ VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_SHADER_IMAGE_FOOTPRINT_FEATURES_NV, nullptr };
    VkPhysicalDeviceDedicatedAllocationImageAliasingFeaturesNV physicalDeviceDedicatedAllocationImageAliasingFeaturesNV{ VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_DEDICATED_ALLOCATION_IMAGE_ALIASING_FEATURES_NV, nullptr };
    VkPhysicalDeviceCopyMemoryIndirectFeaturesNV physicalDeviceCopyMemoryIndirectFeaturesNV{ VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_COPY_MEMORY_INDIRECT_FEATURES_NV, nullptr };
    VkPhysicalDeviceMemoryDecompressionFeaturesNV physicalDeviceMemoryDecompressionFeaturesNV{ VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_MEMORY_DECOMPRESSION_FEATURES_NV, nullptr };
    VkPhysicalDeviceShadingRateImageFeaturesNV physicalDeviceShadingRateImageFeaturesNV{ VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_SHADING_RATE_IMAGE_FEATURES_NV, nullptr };
    VkPhysicalDeviceInvocationMaskFeaturesHUAWEI physicalDeviceInvocationMaskFeaturesHUAWEI{ VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_INVOCATION_MASK_FEATURES_HUAWEI, nullptr };
    VkPhysicalDeviceMeshShaderFeaturesNV physicalDeviceMeshShaderFeaturesNV{ VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_MESH_SHADER_FEATURES_NV, nullptr };
    VkPhysicalDeviceMeshShaderFeaturesEXT physicalDeviceMeshShaderFeaturesEXT{ VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_MESH_SHADER_FEATURES_EXT, nullptr };
    VkPhysicalDeviceAccelerationStructureFeaturesKHR physicalDeviceAccelerationStructureFeaturesKHR{ VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_ACCELERATION_STRUCTURE_FEATURES_KHR, nullptr };
    VkPhysicalDeviceRayTracingPipelineFeaturesKHR physicalDeviceRayTracingPipelineFeaturesKHR{ VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_RAY_TRACING_PIPELINE_FEATURES_KHR, nullptr };
    VkPhysicalDeviceRayQueryFeaturesKHR physicalDeviceRayQueryFeaturesKHR{ VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_RAY_QUERY_FEATURES_KHR, nullptr };
    VkPhysicalDeviceRayTracingMaintenance1FeaturesKHR physicalDeviceRayTracingMaintenance1FeaturesKHR{ VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_RAY_TRACING_MAINTENANCE_1_FEATURES_KHR, nullptr };
    VkPhysicalDeviceFragmentDensityMapFeaturesEXT physicalDeviceFragmentDensityMapFeaturesEXT{ VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_FRAGMENT_DENSITY_MAP_FEATURES_EXT, nullptr };
    VkPhysicalDeviceFragmentDensityMap2FeaturesEXT physicalDeviceFragmentDensityMap2FeaturesEXT{ VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_FRAGMENT_DENSITY_MAP_2_FEATURES_EXT, nullptr };
    VkPhysicalDeviceFragmentDensityMapOffsetFeaturesQCOM physicalDeviceFragmentDensityMapOffsetFeaturesQCOM{ VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_FRAGMENT_DENSITY_MAP_OFFSET_FEATURES_QCOM, nullptr };
    VkPhysicalDeviceScalarBlockLayoutFeatures physicalDeviceScalarBlockLayoutFeatures{ VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_SCALAR_BLOCK_LAYOUT_FEATURES, nullptr };
    VkPhysicalDeviceUniformBufferStandardLayoutFeatures physicalDeviceUniformBufferStandardLayoutFeatures{ VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_UNIFORM_BUFFER_STANDARD_LAYOUT_FEATURES, nullptr };
    VkPhysicalDeviceDepthClipEnableFeaturesEXT physicalDeviceDepthClipEnableFeaturesEXT{ VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_DEPTH_CLIP_ENABLE_FEATURES_EXT, nullptr };
    VkPhysicalDeviceMemoryPriorityFeaturesEXT physicalDeviceMemoryPriorityFeaturesEXT{ VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_MEMORY_PRIORITY_FEATURES_EXT, nullptr };
    VkPhysicalDevicePageableDeviceLocalMemoryFeaturesEXT physicalDevicePageableDeviceLocalMemoryFeaturesEXT{ VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_PAGEABLE_DEVICE_LOCAL_MEMORY_FEATURES_EXT, nullptr };
    VkPhysicalDeviceBufferDeviceAddressFeatures physicalDeviceBufferDeviceAddressFeatures{ VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_BUFFER_DEVICE_ADDRESS_FEATURES, nullptr };
    VkPhysicalDeviceBufferDeviceAddressFeaturesEXT physicalDeviceBufferDeviceAddressFeaturesEXT{ VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_BUFFER_DEVICE_ADDRESS_FEATURES_EXT, nullptr };
    VkPhysicalDeviceImagelessFramebufferFeatures physicalDeviceImagelessFramebufferFeatures{ VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_IMAGELESS_FRAMEBUFFER_FEATURES, nullptr };
    VkPhysicalDeviceTextureCompressionASTCHDRFeatures physicalDeviceTextureCompressionASTCHDRFeatures{ VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_TEXTURE_COMPRESSION_ASTC_HDR_FEATURES, nullptr };
    VkPhysicalDeviceCooperativeMatrixFeaturesNV physicalDeviceCooperativeMatrixFeaturesNV{ VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_COOPERATIVE_MATRIX_FEATURES_NV, nullptr };
    VkPhysicalDeviceYcbcrImageArraysFeaturesEXT physicalDeviceYcbcrImageArraysFeaturesEXT{ VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_YCBCR_IMAGE_ARRAYS_FEATURES_EXT, nullptr };
    VkPhysicalDevicePresentBarrierFeaturesNV physicalDevicePresentBarrierFeaturesNV{ VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_PRESENT_BARRIER_FEATURES_NV, nullptr };
    VkPhysicalDevicePerformanceQueryFeaturesKHR physicalDevicePerformanceQueryFeaturesKHR{ VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_PERFORMANCE_QUERY_FEATURES_KHR, nullptr };
    VkPhysicalDeviceCoverageReductionModeFeaturesNV physicalDeviceCoverageReductionModeFeaturesNV{ VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_COVERAGE_REDUCTION_MODE_FEATURES_NV, nullptr };
    VkPhysicalDeviceShaderIntegerFunctions2FeaturesINTEL physicalDeviceShaderIntegerFunctions2FeaturesINTEL{ VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_SHADER_INTEGER_FUNCTIONS_2_FEATURES_INTEL, nullptr };
    VkPhysicalDeviceShaderClockFeaturesKHR physicalDeviceShaderClockFeaturesKHR{ VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_SHADER_CLOCK_FEATURES_KHR, nullptr };
    VkPhysicalDeviceIndexTypeUint8FeaturesEXT physicalDeviceIndexTypeUint8FeaturesEXT{ VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_INDEX_TYPE_UINT8_FEATURES_EXT, nullptr };
    VkPhysicalDeviceShaderSMBuiltinsFeaturesNV physicalDeviceShaderSMBuiltinsFeaturesNV{ VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_SHADER_SM_BUILTINS_FEATURES_NV, nullptr };
    VkPhysicalDeviceFragmentShaderInterlockFeaturesEXT physicalDeviceFragmentShaderInterlockFeaturesEXT{ VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_FRAGMENT_SHADER_INTERLOCK_FEATURES_EXT, nullptr };
    VkPhysicalDeviceSeparateDepthStencilLayoutsFeatures physicalDeviceSeparateDepthStencilLayoutsFeatures{ VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_SEPARATE_DEPTH_STENCIL_LAYOUTS_FEATURES, nullptr };
    VkPhysicalDevicePrimitiveTopologyListRestartFeaturesEXT physicalDevicePrimitiveTopologyListRestartFeaturesEXT{ VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_PRIMITIVE_TOPOLOGY_LIST_RESTART_FEATURES_EXT, nullptr };
    VkPhysicalDevicePipelineExecutablePropertiesFeaturesKHR physicalDevicePipelineExecutablePropertiesFeaturesKHR{ VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_PIPELINE_EXECUTABLE_PROPERTIES_FEATURES_KHR, nullptr };
    VkPhysicalDeviceShaderDemoteToHelperInvocationFeatures physicalDeviceShaderDemoteToHelperInvocationFeatures{ VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_SHADER_DEMOTE_TO_HELPER_INVOCATION_FEATURES, nullptr };
    VkPhysicalDeviceTexelBufferAlignmentFeaturesEXT physicalDeviceTexelBufferAlignmentFeaturesEXT{ VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_TEXEL_BUFFER_ALIGNMENT_FEATURES_EXT, nullptr };
    VkPhysicalDeviceSubgroupSizeControlFeatures physicalDeviceSubgroupSizeControlFeatures{ VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_SUBGROUP_SIZE_CONTROL_FEATURES, nullptr };
    VkPhysicalDeviceLineRasterizationFeaturesEXT physicalDeviceLineRasterizationFeaturesEXT{ VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_LINE_RASTERIZATION_FEATURES_EXT, nullptr };
    VkPhysicalDevicePipelineCreationCacheControlFeatures physicalDevicePipelineCreationCacheControlFeatures{ VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_PIPELINE_CREATION_CACHE_CONTROL_FEATURES, nullptr };
    VkPhysicalDeviceVulkan11Features physicalDeviceVulkan11Features{ VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_1_1_FEATURES, nullptr };
    VkPhysicalDeviceVulkan12Features physicalDeviceVulkan12Features{ VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_1_2_FEATURES, nullptr };
    VkPhysicalDeviceVulkan13Features physicalDeviceVulkan13Features{ VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_1_3_FEATURES, nullptr };
    VkPhysicalDeviceCoherentMemoryFeaturesAMD physicalDeviceCoherentMemoryFeaturesAMD{ VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_COHERENT_MEMORY_FEATURES_AMD, nullptr };
    VkPhysicalDeviceCustomBorderColorFeaturesEXT physicalDeviceCustomBorderColorFeaturesEXT{ VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_CUSTOM_BORDER_COLOR_FEATURES_EXT, nullptr };
    VkPhysicalDeviceBorderColorSwizzleFeaturesEXT physicalDeviceBorderColorSwizzleFeaturesEXT{ VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_BORDER_COLOR_SWIZZLE_FEATURES_EXT, nullptr };
    VkPhysicalDeviceExtendedDynamicStateFeaturesEXT physicalDeviceExtendedDynamicStateFeaturesEXT{ VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_EXTENDED_DYNAMIC_STATE_FEATURES_EXT, nullptr };
    VkPhysicalDeviceExtendedDynamicState2FeaturesEXT physicalDeviceExtendedDynamicState2FeaturesEXT{ VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_EXTENDED_DYNAMIC_STATE_2_FEATURES_EXT, nullptr };
    VkPhysicalDeviceExtendedDynamicState3FeaturesEXT physicalDeviceExtendedDynamicState3FeaturesEXT{ VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_EXTENDED_DYNAMIC_STATE_3_FEATURES_EXT, nullptr };
    VkPhysicalDeviceDiagnosticsConfigFeaturesNV physicalDeviceDiagnosticsConfigFeaturesNV{ VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_DIAGNOSTICS_CONFIG_FEATURES_NV, nullptr };
    VkPhysicalDeviceZeroInitializeWorkgroupMemoryFeatures physicalDeviceZeroInitializeWorkgroupMemoryFeatures{ VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_ZERO_INITIALIZE_WORKGROUP_MEMORY_FEATURES, nullptr };
    VkPhysicalDeviceShaderSubgroupUniformControlFlowFeaturesKHR physicalDeviceShaderSubgroupUniformControlFlowFeaturesKHR{ VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_SHADER_SUBGROUP_UNIFORM_CONTROL_FLOW_FEATURES_KHR, nullptr };
    VkPhysicalDeviceRobustness2FeaturesEXT physicalDeviceRobustness2FeaturesEXT{ VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_ROBUSTNESS_2_FEATURES_EXT, nullptr };
    VkPhysicalDeviceImageRobustnessFeatures physicalDeviceImageRobustnessFeatures{ VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_IMAGE_ROBUSTNESS_FEATURES, nullptr };
    VkPhysicalDeviceWorkgroupMemoryExplicitLayoutFeaturesKHR physicalDeviceWorkgroupMemoryExplicitLayoutFeaturesKHR{ VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_WORKGROUP_MEMORY_EXPLICIT_LAYOUT_FEATURES_KHR, nullptr };
#ifdef VK_ENABLE_BETA_EXTENSIONS
    VkPhysicalDevicePortabilitySubsetFeaturesKHR physicalDevicePortabilitySubsetFeaturesKHR{ VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_PORTABILITY_SUBSET_FEATURES_KHR, nullptr };
#endif
    VkPhysicalDevice4444FormatsFeaturesEXT physicalDevice4444FormatsFeaturesEXT{ VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_4444_FORMATS_FEATURES_EXT, nullptr };
    VkPhysicalDeviceSubpassShadingFeaturesHUAWEI physicalDeviceSubpassShadingFeaturesHUAWEI{ VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_SUBPASS_SHADING_FEATURES_HUAWEI, nullptr };
    VkPhysicalDeviceClusterCullingShaderFeaturesHUAWEI physicalDeviceClusterCullingShaderFeaturesHUAWEI{ VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_CLUSTER_CULLING_SHADER_FEATURES_HUAWEI, nullptr };
    VkPhysicalDeviceShaderImageAtomicInt64FeaturesEXT physicalDeviceShaderImageAtomicInt64FeaturesEXT{ VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_SHADER_IMAGE_ATOMIC_INT64_FEATURES_EXT, nullptr };
    VkPhysicalDeviceFragmentShadingRateFeaturesKHR physicalDeviceFragmentShadingRateFeaturesKHR{ VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_FRAGMENT_SHADING_RATE_FEATURES_KHR, nullptr };
    VkPhysicalDeviceShaderTerminateInvocationFeatures physicalDeviceShaderTerminateInvocationFeatures{ VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_SHADER_TERMINATE_INVOCATION_FEATURES, nullptr };
    VkPhysicalDeviceFragmentShadingRateEnumsFeaturesNV physicalDeviceFragmentShadingRateEnumsFeaturesNV{ VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_FRAGMENT_SHADING_RATE_ENUMS_FEATURES_NV, nullptr };
    VkPhysicalDeviceImage2DViewOf3DFeaturesEXT physicalDeviceImage2DViewOf3DFeaturesEXT{ VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_IMAGE_2D_VIEW_OF_3D_FEATURES_EXT, nullptr };
    VkPhysicalDeviceImageSlicedViewOf3DFeaturesEXT physicalDeviceImageSlicedViewOf3DFeaturesEXT{ VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_IMAGE_SLICED_VIEW_OF_3D_FEATURES_EXT, nullptr };
    VkPhysicalDeviceAttachmentFeedbackLoopDynamicStateFeaturesEXT physicalDeviceAttachmentFeedbackLoopDynamicStateFeaturesEXT{ VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_ATTACHMENT_FEEDBACK_LOOP_DYNAMIC_STATE_FEATURES_EXT, nullptr };
    VkPhysicalDeviceMutableDescriptorTypeFeaturesEXT physicalDeviceMutableDescriptorTypeFeaturesEXT{ VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_MUTABLE_DESCRIPTOR_TYPE_FEATURES_EXT, nullptr };
    VkPhysicalDeviceDepthClipControlFeaturesEXT physicalDeviceDepthClipControlFeaturesEXT{ VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_DEPTH_CLIP_CONTROL_FEATURES_EXT, nullptr };
    VkPhysicalDeviceVertexInputDynamicStateFeaturesEXT physicalDeviceVertexInputDynamicStateFeaturesEXT{ VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VERTEX_INPUT_DYNAMIC_STATE_FEATURES_EXT, nullptr };
    VkPhysicalDeviceExternalMemoryRDMAFeaturesNV physicalDeviceExternalMemoryRDMAFeaturesNV{ VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_EXTERNAL_MEMORY_RDMA_FEATURES_NV, nullptr };
    VkPhysicalDeviceColorWriteEnableFeaturesEXT physicalDeviceColorWriteEnableFeaturesEXT{ VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_COLOR_WRITE_ENABLE_FEATURES_EXT, nullptr };
    VkPhysicalDeviceSynchronization2Features physicalDeviceSynchronization2Features{ VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_SYNCHRONIZATION_2_FEATURES, nullptr };
    VkPhysicalDeviceHostImageCopyFeaturesEXT physicalDeviceHostImageCopyFeaturesEXT{ VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_HOST_IMAGE_COPY_FEATURES_EXT, nullptr };
    VkPhysicalDevicePrimitivesGeneratedQueryFeaturesEXT physicalDevicePrimitivesGeneratedQueryFeaturesEXT{ VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_PRIMITIVES_GENERATED_QUERY_FEATURES_EXT, nullptr };
    VkPhysicalDeviceLegacyDitheringFeaturesEXT physicalDeviceLegacyDitheringFeaturesEXT{ VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_LEGACY_DITHERING_FEATURES_EXT, nullptr };
    VkPhysicalDeviceMultisampledRenderToSingleSampledFeaturesEXT physicalDeviceMultisampledRenderToSingleSampledFeaturesEXT{ VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_MULTISAMPLED_RENDER_TO_SINGLE_SAMPLED_FEATURES_EXT, nullptr };
    VkPhysicalDevicePipelineProtectedAccessFeaturesEXT physicalDevicePipelineProtectedAccessFeaturesEXT{ VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_PIPELINE_PROTECTED_ACCESS_FEATURES_EXT, nullptr };
    VkPhysicalDeviceVideoMaintenance1FeaturesKHR physicalDeviceVideoMaintenance1FeaturesKHR{ VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VIDEO_MAINTENANCE_1_FEATURES_KHR, nullptr };
    VkPhysicalDeviceInheritedViewportScissorFeaturesNV physicalDeviceInheritedViewportScissorFeaturesNV{ VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_INHERITED_VIEWPORT_SCISSOR_FEATURES_NV, nullptr };
    VkPhysicalDeviceYcbcr2Plane444FormatsFeaturesEXT physicalDeviceYcbcr2Plane444FormatsFeaturesEXT{ VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_YCBCR_2_PLANE_444_FORMATS_FEATURES_EXT, nullptr };
    VkPhysicalDeviceProvokingVertexFeaturesEXT physicalDeviceProvokingVertexFeaturesEXT{ VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_PROVOKING_VERTEX_FEATURES_EXT, nullptr };
    VkPhysicalDeviceDescriptorBufferFeaturesEXT physicalDeviceDescriptorBufferFeaturesEXT{ VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_DESCRIPTOR_BUFFER_FEATURES_EXT, nullptr };
    VkPhysicalDeviceShaderIntegerDotProductFeatures physicalDeviceShaderIntegerDotProductFeatures{ VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_SHADER_INTEGER_DOT_PRODUCT_FEATURES, nullptr };
    VkPhysicalDeviceFragmentShaderBarycentricFeaturesKHR physicalDeviceFragmentShaderBarycentricFeaturesKHR{ VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_FRAGMENT_SHADER_BARYCENTRIC_FEATURES_KHR, nullptr };
    VkPhysicalDeviceRayTracingMotionBlurFeaturesNV physicalDeviceRayTracingMotionBlurFeaturesNV{ VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_RAY_TRACING_MOTION_BLUR_FEATURES_NV, nullptr };
    VkPhysicalDeviceRGBA10X6FormatsFeaturesEXT physicalDeviceRGBA10X6FormatsFeaturesEXT{ VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_RGBA10X6_FORMATS_FEATURES_EXT, nullptr };
    VkPhysicalDeviceDynamicRenderingFeatures physicalDeviceDynamicRenderingFeatures{ VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_DYNAMIC_RENDERING_FEATURES, nullptr };
    VkPhysicalDeviceImageViewMinLodFeaturesEXT physicalDeviceImageViewMinLodFeaturesEXT{ VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_IMAGE_VIEW_MIN_LOD_FEATURES_EXT, nullptr };
    VkPhysicalDeviceRasterizationOrderAttachmentAccessFeaturesEXT physicalDeviceRasterizationOrderAttachmentAccessFeaturesEXT{ VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_RASTERIZATION_ORDER_ATTACHMENT_ACCESS_FEATURES_EXT, nullptr };
    VkPhysicalDeviceLinearColorAttachmentFeaturesNV physicalDeviceLinearColorAttachmentFeaturesNV{ VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_LINEAR_COLOR_ATTACHMENT_FEATURES_NV, nullptr };
    VkPhysicalDeviceGraphicsPipelineLibraryFeaturesEXT physicalDeviceGraphicsPipelineLibraryFeaturesEXT{ VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_GRAPHICS_PIPELINE_LIBRARY_FEATURES_EXT, nullptr };
    VkPhysicalDeviceDescriptorSetHostMappingFeaturesVALVE physicalDeviceDescriptorSetHostMappingFeaturesVALVE{ VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_DESCRIPTOR_SET_HOST_MAPPING_FEATURES_VALVE, nullptr };
    VkPhysicalDeviceNestedCommandBufferFeaturesEXT physicalDeviceNestedCommandBufferFeaturesEXT{ VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_NESTED_COMMAND_BUFFER_FEATURES_EXT, nullptr };
    VkPhysicalDeviceShaderModuleIdentifierFeaturesEXT physicalDeviceShaderModuleIdentifierFeaturesEXT{ VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_SHADER_MODULE_IDENTIFIER_FEATURES_EXT, nullptr };
    VkPhysicalDeviceImageCompressionControlFeaturesEXT physicalDeviceImageCompressionControlFeaturesEXT{ VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_IMAGE_COMPRESSION_CONTROL_FEATURES_EXT, nullptr };
    VkPhysicalDeviceImageCompressionControlSwapchainFeaturesEXT physicalDeviceImageCompressionControlSwapchainFeaturesEXT{ VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_IMAGE_COMPRESSION_CONTROL_SWAPCHAIN_FEATURES_EXT, nullptr };
    VkPhysicalDeviceSubpassMergeFeedbackFeaturesEXT physicalDeviceSubpassMergeFeedbackFeaturesEXT{ VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_SUBPASS_MERGE_FEEDBACK_FEATURES_EXT, nullptr };
    VkPhysicalDeviceOpacityMicromapFeaturesEXT physicalDeviceOpacityMicromapFeaturesEXT{ VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_OPACITY_MICROMAP_FEATURES_EXT, nullptr };
#ifdef VK_ENABLE_BETA_EXTENSIONS
    VkPhysicalDeviceDisplacementMicromapFeaturesNV physicalDeviceDisplacementMicromapFeaturesNV{ VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_DISPLACEMENT_MICROMAP_FEATURES_NV, nullptr };
#endif
    VkPhysicalDevicePipelinePropertiesFeaturesEXT physicalDevicePipelinePropertiesFeaturesEXT{ VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_PIPELINE_PROPERTIES_FEATURES_EXT, nullptr };
    VkPhysicalDeviceShaderEarlyAndLateFragmentTestsFeaturesAMD physicalDeviceShaderEarlyAndLateFragmentTestsFeaturesAMD{ VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_SHADER_EARLY_AND_LATE_FRAGMENT_TESTS_FEATURES_AMD, nullptr };
    VkPhysicalDeviceNonSeamlessCubeMapFeaturesEXT physicalDeviceNonSeamlessCubeMapFeaturesEXT{ VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_NON_SEAMLESS_CUBE_MAP_FEATURES_EXT, nullptr };
    VkPhysicalDevicePipelineRobustnessFeaturesEXT physicalDevicePipelineRobustnessFeaturesEXT{ VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_PIPELINE_ROBUSTNESS_FEATURES_EXT, nullptr };
    VkPhysicalDeviceImageProcessingFeaturesQCOM physicalDeviceImageProcessingFeaturesQCOM{ VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_IMAGE_PROCESSING_FEATURES_QCOM, nullptr };
    VkPhysicalDeviceTilePropertiesFeaturesQCOM physicalDeviceTilePropertiesFeaturesQCOM{ VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_TILE_PROPERTIES_FEATURES_QCOM, nullptr };
    VkPhysicalDeviceAmigoProfilingFeaturesSEC physicalDeviceAmigoProfilingFeaturesSEC{ VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_AMIGO_PROFILING_FEATURES_SEC, nullptr };
    VkPhysicalDeviceAttachmentFeedbackLoopLayoutFeaturesEXT physicalDeviceAttachmentFeedbackLoopLayoutFeaturesEXT{ VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_ATTACHMENT_FEEDBACK_LOOP_LAYOUT_FEATURES_EXT, nullptr };
    VkPhysicalDeviceDepthClampZeroOneFeaturesEXT physicalDeviceDepthClampZeroOneFeaturesEXT{ VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_DEPTH_CLAMP_ZERO_ONE_FEATURES_EXT, nullptr };
    VkPhysicalDeviceAddressBindingReportFeaturesEXT physicalDeviceAddressBindingReportFeaturesEXT{ VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_ADDRESS_BINDING_REPORT_FEATURES_EXT, nullptr };
    VkPhysicalDeviceOpticalFlowFeaturesNV physicalDeviceOpticalFlowFeaturesNV{ VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_OPTICAL_FLOW_FEATURES_NV, nullptr };
    VkPhysicalDeviceFaultFeaturesEXT physicalDeviceFaultFeaturesEXT{ VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_FAULT_FEATURES_EXT, nullptr };
    VkPhysicalDevicePipelineLibraryGroupHandlesFeaturesEXT physicalDevicePipelineLibraryGroupHandlesFeaturesEXT{ VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_PIPELINE_LIBRARY_GROUP_HANDLES_FEATURES_EXT, nullptr };
    VkPhysicalDeviceShaderCoreBuiltinsFeaturesARM physicalDeviceShaderCoreBuiltinsFeaturesARM{ VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_SHADER_CORE_BUILTINS_FEATURES_ARM, nullptr };
    VkPhysicalDeviceFrameBoundaryFeaturesEXT physicalDeviceFrameBoundaryFeaturesEXT{ VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_FRAME_BOUNDARY_FEATURES_EXT, nullptr };
    VkPhysicalDeviceDynamicRenderingUnusedAttachmentsFeaturesEXT physicalDeviceDynamicRenderingUnusedAttachmentsFeaturesEXT{ VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_DYNAMIC_RENDERING_UNUSED_ATTACHMENTS_FEATURES_EXT, nullptr };
    VkPhysicalDeviceSwapchainMaintenance1FeaturesEXT physicalDeviceSwapchainMaintenance1FeaturesEXT{ VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_SWAPCHAIN_MAINTENANCE_1_FEATURES_EXT, nullptr };
    VkPhysicalDeviceDepthBiasControlFeaturesEXT physicalDeviceDepthBiasControlFeaturesEXT{ VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_DEPTH_BIAS_CONTROL_FEATURES_EXT, nullptr };
    VkPhysicalDeviceRayTracingInvocationReorderFeaturesNV physicalDeviceRayTracingInvocationReorderFeaturesNV{ VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_RAY_TRACING_INVOCATION_REORDER_FEATURES_NV, nullptr };
    VkPhysicalDeviceExtendedSparseAddressSpaceFeaturesNV physicalDeviceExtendedSparseAddressSpaceFeaturesNV{ VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_EXTENDED_SPARSE_ADDRESS_SPACE_FEATURES_NV, nullptr };
    VkPhysicalDeviceMultiviewPerViewViewportsFeaturesQCOM physicalDeviceMultiviewPerViewViewportsFeaturesQCOM{ VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_MULTIVIEW_PER_VIEW_VIEWPORTS_FEATURES_QCOM, nullptr };
    VkPhysicalDeviceRayTracingPositionFetchFeaturesKHR physicalDeviceRayTracingPositionFetchFeaturesKHR{ VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_RAY_TRACING_POSITION_FETCH_FEATURES_KHR, nullptr };
    VkPhysicalDeviceMultiviewPerViewRenderAreasFeaturesQCOM physicalDeviceMultiviewPerViewRenderAreasFeaturesQCOM{ VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_MULTIVIEW_PER_VIEW_RENDER_AREAS_FEATURES_QCOM, nullptr };
    VkPhysicalDeviceShaderObjectFeaturesEXT physicalDeviceShaderObjectFeaturesEXT{ VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_SHADER_OBJECT_FEATURES_EXT, nullptr };
    VkPhysicalDeviceShaderTileImageFeaturesEXT physicalDeviceShaderTileImageFeaturesEXT{ VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_SHADER_TILE_IMAGE_FEATURES_EXT, nullptr };
#ifdef VK_USE_PLATFORM_SCREEN_QNX
    VkPhysicalDeviceExternalMemoryScreenBufferFeaturesQNX physicalDeviceExternalMemoryScreenBufferFeaturesQNX{ VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_EXTERNAL_MEMORY_SCREEN_BUFFER_FEATURES_QNX, nullptr };
#endif
    VkPhysicalDeviceCooperativeMatrixFeaturesKHR physicalDeviceCooperativeMatrixFeaturesKHR{ VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_COOPERATIVE_MATRIX_FEATURES_KHR, nullptr };
#ifdef VK_ENABLE_BETA_EXTENSIONS
    VkPhysicalDeviceShaderEnqueueFeaturesAMDX physicalDeviceShaderEnqueueFeaturesAMDX{ VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_SHADER_ENQUEUE_FEATURES_AMDX, nullptr };
#endif
    VkPhysicalDeviceCubicClampFeaturesQCOM physicalDeviceCubicClampFeaturesQCOM{ VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_CUBIC_CLAMP_FEATURES_QCOM, nullptr };
    VkPhysicalDeviceYcbcrDegammaFeaturesQCOM physicalDeviceYcbcrDegammaFeaturesQCOM{ VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_YCBCR_DEGAMMA_FEATURES_QCOM, nullptr };
    VkPhysicalDeviceCubicWeightsFeaturesQCOM physicalDeviceCubicWeightsFeaturesQCOM{ VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_CUBIC_WEIGHTS_FEATURES_QCOM, nullptr };
    VkPhysicalDeviceImageProcessing2FeaturesQCOM physicalDeviceImageProcessing2FeaturesQCOM{ VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_IMAGE_PROCESSING_2_FEATURES_QCOM, nullptr };
    VkPhysicalDeviceDescriptorPoolOverallocationFeaturesNV physicalDeviceDescriptorPoolOverallocationFeaturesNV{ VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_DESCRIPTOR_POOL_OVERALLOCATION_FEATURES_NV, nullptr };
    VkPhysicalDevicePerStageDescriptorSetFeaturesNV physicalDevicePerStageDescriptorSetFeaturesNV{ VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_PER_STAGE_DESCRIPTOR_SET_FEATURES_NV, nullptr };
#ifdef VK_USE_PLATFORM_ANDROID_KHR
    VkPhysicalDeviceExternalFormatResolveFeaturesANDROID physicalDeviceExternalFormatResolveFeaturesANDROID{ VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_EXTERNAL_FORMAT_RESOLVE_FEATURES_ANDROID, nullptr };
#endif
    VkPhysicalDeviceCudaKernelLaunchFeaturesNV physicalDeviceCudaKernelLaunchFeaturesNV{ VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_CUDA_KERNEL_LAUNCH_FEATURES_NV, nullptr };
    VkPhysicalDeviceSchedulingControlsFeaturesARM physicalDeviceSchedulingControlsFeaturesARM{ VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_SCHEDULING_CONTROLS_FEATURES_ARM, nullptr };
    VkPhysicalDeviceRelaxedLineRasterizationFeaturesIMG physicalDeviceRelaxedLineRasterizationFeaturesIMG{ VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_RELAXED_LINE_RASTERIZATION_FEATURES_IMG, nullptr };
    VkPhysicalDeviceRenderPassStripedFeaturesARM physicalDeviceRenderPassStripedFeaturesARM{ VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_RENDER_PASS_STRIPED_FEATURES_ARM, nullptr };
    VkPhysicalDeviceFeatures2KHR physicalDeviceFeatures2KHR{ VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_FEATURES_2_KHR, nullptr };

    FeaturesChain() {
        // Initializing all feature structures, number of Features (VkBool32) per structure.
        this->structureSize.insert({ VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_DEVICE_GENERATED_COMMANDS_FEATURES_NV, size<VkPhysicalDeviceDeviceGeneratedCommandsFeaturesNV>() });
        this->structureSize.insert({ VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_DEVICE_GENERATED_COMMANDS_COMPUTE_FEATURES_NV, size<VkPhysicalDeviceDeviceGeneratedCommandsComputeFeaturesNV>() });
        this->structureSize.insert({ VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_PRIVATE_DATA_FEATURES, size<VkPhysicalDevicePrivateDataFeatures>() });
        this->structureSize.insert({ VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VARIABLE_POINTERS_FEATURES, size<VkPhysicalDeviceVariablePointersFeatures>() });
        this->structureSize.insert({ VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_MULTIVIEW_FEATURES, size<VkPhysicalDeviceMultiviewFeatures>() });
        this->structureSize.insert({ VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_PRESENT_ID_FEATURES_KHR, size<VkPhysicalDevicePresentIdFeaturesKHR>() });
        this->structureSize.insert({ VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_PRESENT_WAIT_FEATURES_KHR, size<VkPhysicalDevicePresentWaitFeaturesKHR>() });
        this->structureSize.insert({ VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_16BIT_STORAGE_FEATURES, size<VkPhysicalDevice16BitStorageFeatures>() });
        this->structureSize.insert({ VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_SHADER_SUBGROUP_EXTENDED_TYPES_FEATURES, size<VkPhysicalDeviceShaderSubgroupExtendedTypesFeatures>() });
        this->structureSize.insert({ VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_SAMPLER_YCBCR_CONVERSION_FEATURES, size<VkPhysicalDeviceSamplerYcbcrConversionFeatures>() });
        this->structureSize.insert({ VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_PROTECTED_MEMORY_FEATURES, size<VkPhysicalDeviceProtectedMemoryFeatures>() });
        this->structureSize.insert({ VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_BLEND_OPERATION_ADVANCED_FEATURES_EXT, size<VkPhysicalDeviceBlendOperationAdvancedFeaturesEXT>() });
        this->structureSize.insert({ VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_MULTI_DRAW_FEATURES_EXT, size<VkPhysicalDeviceMultiDrawFeaturesEXT>() });
        this->structureSize.insert({ VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_INLINE_UNIFORM_BLOCK_FEATURES, size<VkPhysicalDeviceInlineUniformBlockFeatures>() });
        this->structureSize.insert({ VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_MAINTENANCE_4_FEATURES, size<VkPhysicalDeviceMaintenance4Features>() });
        this->structureSize.insert({ VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_MAINTENANCE_5_FEATURES_KHR, size<VkPhysicalDeviceMaintenance5FeaturesKHR>() });
        this->structureSize.insert({ VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_MAINTENANCE_6_FEATURES_KHR, size<VkPhysicalDeviceMaintenance6FeaturesKHR>() });
        this->structureSize.insert({ VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_SHADER_DRAW_PARAMETERS_FEATURES, size<VkPhysicalDeviceShaderDrawParametersFeatures>() });
        this->structureSize.insert({ VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_SHADER_FLOAT16_INT8_FEATURES, size<VkPhysicalDeviceShaderFloat16Int8Features>() });
        this->structureSize.insert({ VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_HOST_QUERY_RESET_FEATURES, size<VkPhysicalDeviceHostQueryResetFeatures>() });
        this->structureSize.insert({ VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_GLOBAL_PRIORITY_QUERY_FEATURES_KHR, size<VkPhysicalDeviceGlobalPriorityQueryFeaturesKHR>() });
        this->structureSize.insert({ VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_DEVICE_MEMORY_REPORT_FEATURES_EXT, size<VkPhysicalDeviceDeviceMemoryReportFeaturesEXT>() });
        this->structureSize.insert({ VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_DESCRIPTOR_INDEXING_FEATURES, size<VkPhysicalDeviceDescriptorIndexingFeatures>() });
        this->structureSize.insert({ VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_TIMELINE_SEMAPHORE_FEATURES, size<VkPhysicalDeviceTimelineSemaphoreFeatures>() });
        this->structureSize.insert({ VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_8BIT_STORAGE_FEATURES, size<VkPhysicalDevice8BitStorageFeatures>() });
        this->structureSize.insert({ VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_CONDITIONAL_RENDERING_FEATURES_EXT, size<VkPhysicalDeviceConditionalRenderingFeaturesEXT>() });
        this->structureSize.insert({ VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_MEMORY_MODEL_FEATURES, size<VkPhysicalDeviceVulkanMemoryModelFeatures>() });
        this->structureSize.insert({ VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_SHADER_ATOMIC_INT64_FEATURES, size<VkPhysicalDeviceShaderAtomicInt64Features>() });
        this->structureSize.insert({ VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_SHADER_ATOMIC_FLOAT_FEATURES_EXT, size<VkPhysicalDeviceShaderAtomicFloatFeaturesEXT>() });
        this->structureSize.insert({ VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_SHADER_ATOMIC_FLOAT_2_FEATURES_EXT, size<VkPhysicalDeviceShaderAtomicFloat2FeaturesEXT>() });
        this->structureSize.insert({ VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VERTEX_ATTRIBUTE_DIVISOR_FEATURES_KHR, size<VkPhysicalDeviceVertexAttributeDivisorFeaturesKHR>() });
        this->structureSize.insert({ VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_ASTC_DECODE_FEATURES_EXT, size<VkPhysicalDeviceASTCDecodeFeaturesEXT>() });
        this->structureSize.insert({ VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_TRANSFORM_FEEDBACK_FEATURES_EXT, size<VkPhysicalDeviceTransformFeedbackFeaturesEXT>() });
        this->structureSize.insert({ VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_REPRESENTATIVE_FRAGMENT_TEST_FEATURES_NV, size<VkPhysicalDeviceRepresentativeFragmentTestFeaturesNV>() });
        this->structureSize.insert({ VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_EXCLUSIVE_SCISSOR_FEATURES_NV, size<VkPhysicalDeviceExclusiveScissorFeaturesNV>() });
        this->structureSize.insert({ VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_CORNER_SAMPLED_IMAGE_FEATURES_NV, size<VkPhysicalDeviceCornerSampledImageFeaturesNV>() });
        this->structureSize.insert({ VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_COMPUTE_SHADER_DERIVATIVES_FEATURES_NV, size<VkPhysicalDeviceComputeShaderDerivativesFeaturesNV>() });
        this->structureSize.insert({ VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_SHADER_IMAGE_FOOTPRINT_FEATURES_NV, size<VkPhysicalDeviceShaderImageFootprintFeaturesNV>() });
        this->structureSize.insert({ VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_DEDICATED_ALLOCATION_IMAGE_ALIASING_FEATURES_NV, size<VkPhysicalDeviceDedicatedAllocationImageAliasingFeaturesNV>() });
        this->structureSize.insert({ VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_COPY_MEMORY_INDIRECT_FEATURES_NV, size<VkPhysicalDeviceCopyMemoryIndirectFeaturesNV>() });
        this->structureSize.insert({ VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_MEMORY_DECOMPRESSION_FEATURES_NV, size<VkPhysicalDeviceMemoryDecompressionFeaturesNV>() });
        this->structureSize.insert({ VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_SHADING_RATE_IMAGE_FEATURES_NV, size<VkPhysicalDeviceShadingRateImageFeaturesNV>() });
        this->structureSize.insert({ VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_INVOCATION_MASK_FEATURES_HUAWEI, size<VkPhysicalDeviceInvocationMaskFeaturesHUAWEI>() });
        this->structureSize.insert({ VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_MESH_SHADER_FEATURES_NV, size<VkPhysicalDeviceMeshShaderFeaturesNV>() });
        this->structureSize.insert({ VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_MESH_SHADER_FEATURES_EXT, size<VkPhysicalDeviceMeshShaderFeaturesEXT>() });
        this->structureSize.insert({ VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_ACCELERATION_STRUCTURE_FEATURES_KHR, size<VkPhysicalDeviceAccelerationStructureFeaturesKHR>() });
        this->structureSize.insert({ VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_RAY_TRACING_PIPELINE_FEATURES_KHR, size<VkPhysicalDeviceRayTracingPipelineFeaturesKHR>() });
        this->structureSize.insert({ VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_RAY_QUERY_FEATURES_KHR, size<VkPhysicalDeviceRayQueryFeaturesKHR>() });
        this->structureSize.insert({ VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_RAY_TRACING_MAINTENANCE_1_FEATURES_KHR, size<VkPhysicalDeviceRayTracingMaintenance1FeaturesKHR>() });
        this->structureSize.insert({ VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_FRAGMENT_DENSITY_MAP_FEATURES_EXT, size<VkPhysicalDeviceFragmentDensityMapFeaturesEXT>() });
        this->structureSize.insert({ VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_FRAGMENT_DENSITY_MAP_2_FEATURES_EXT, size<VkPhysicalDeviceFragmentDensityMap2FeaturesEXT>() });
        this->structureSize.insert({ VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_FRAGMENT_DENSITY_MAP_OFFSET_FEATURES_QCOM, size<VkPhysicalDeviceFragmentDensityMapOffsetFeaturesQCOM>() });
        this->structureSize.insert({ VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_SCALAR_BLOCK_LAYOUT_FEATURES, size<VkPhysicalDeviceScalarBlockLayoutFeatures>() });
        this->structureSize.insert({ VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_UNIFORM_BUFFER_STANDARD_LAYOUT_FEATURES, size<VkPhysicalDeviceUniformBufferStandardLayoutFeatures>() });
        this->structureSize.insert({ VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_DEPTH_CLIP_ENABLE_FEATURES_EXT, size<VkPhysicalDeviceDepthClipEnableFeaturesEXT>() });
        this->structureSize.insert({ VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_MEMORY_PRIORITY_FEATURES_EXT, size<VkPhysicalDeviceMemoryPriorityFeaturesEXT>() });
        this->structureSize.insert({ VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_PAGEABLE_DEVICE_LOCAL_MEMORY_FEATURES_EXT, size<VkPhysicalDevicePageableDeviceLocalMemoryFeaturesEXT>() });
        this->structureSize.insert({ VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_BUFFER_DEVICE_ADDRESS_FEATURES, size<VkPhysicalDeviceBufferDeviceAddressFeatures>() });
        this->structureSize.insert({ VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_BUFFER_DEVICE_ADDRESS_FEATURES_EXT, size<VkPhysicalDeviceBufferDeviceAddressFeaturesEXT>() });
        this->structureSize.insert({ VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_IMAGELESS_FRAMEBUFFER_FEATURES, size<VkPhysicalDeviceImagelessFramebufferFeatures>() });
        this->structureSize.insert({ VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_TEXTURE_COMPRESSION_ASTC_HDR_FEATURES, size<VkPhysicalDeviceTextureCompressionASTCHDRFeatures>() });
        this->structureSize.insert({ VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_COOPERATIVE_MATRIX_FEATURES_NV, size<VkPhysicalDeviceCooperativeMatrixFeaturesNV>() });
        this->structureSize.insert({ VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_YCBCR_IMAGE_ARRAYS_FEATURES_EXT, size<VkPhysicalDeviceYcbcrImageArraysFeaturesEXT>() });
        this->structureSize.insert({ VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_PRESENT_BARRIER_FEATURES_NV, size<VkPhysicalDevicePresentBarrierFeaturesNV>() });
        this->structureSize.insert({ VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_PERFORMANCE_QUERY_FEATURES_KHR, size<VkPhysicalDevicePerformanceQueryFeaturesKHR>() });
        this->structureSize.insert({ VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_COVERAGE_REDUCTION_MODE_FEATURES_NV, size<VkPhysicalDeviceCoverageReductionModeFeaturesNV>() });
        this->structureSize.insert({ VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_SHADER_INTEGER_FUNCTIONS_2_FEATURES_INTEL, size<VkPhysicalDeviceShaderIntegerFunctions2FeaturesINTEL>() });
        this->structureSize.insert({ VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_SHADER_CLOCK_FEATURES_KHR, size<VkPhysicalDeviceShaderClockFeaturesKHR>() });
        this->structureSize.insert({ VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_INDEX_TYPE_UINT8_FEATURES_EXT, size<VkPhysicalDeviceIndexTypeUint8FeaturesEXT>() });
        this->structureSize.insert({ VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_SHADER_SM_BUILTINS_FEATURES_NV, size<VkPhysicalDeviceShaderSMBuiltinsFeaturesNV>() });
        this->structureSize.insert({ VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_FRAGMENT_SHADER_INTERLOCK_FEATURES_EXT, size<VkPhysicalDeviceFragmentShaderInterlockFeaturesEXT>() });
        this->structureSize.insert({ VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_SEPARATE_DEPTH_STENCIL_LAYOUTS_FEATURES, size<VkPhysicalDeviceSeparateDepthStencilLayoutsFeatures>() });
        this->structureSize.insert({ VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_PRIMITIVE_TOPOLOGY_LIST_RESTART_FEATURES_EXT, size<VkPhysicalDevicePrimitiveTopologyListRestartFeaturesEXT>() });
        this->structureSize.insert({ VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_PIPELINE_EXECUTABLE_PROPERTIES_FEATURES_KHR, size<VkPhysicalDevicePipelineExecutablePropertiesFeaturesKHR>() });
        this->structureSize.insert({ VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_SHADER_DEMOTE_TO_HELPER_INVOCATION_FEATURES, size<VkPhysicalDeviceShaderDemoteToHelperInvocationFeatures>() });
        this->structureSize.insert({ VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_TEXEL_BUFFER_ALIGNMENT_FEATURES_EXT, size<VkPhysicalDeviceTexelBufferAlignmentFeaturesEXT>() });
        this->structureSize.insert({ VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_SUBGROUP_SIZE_CONTROL_FEATURES, size<VkPhysicalDeviceSubgroupSizeControlFeatures>() });
        this->structureSize.insert({ VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_LINE_RASTERIZATION_FEATURES_EXT, size<VkPhysicalDeviceLineRasterizationFeaturesEXT>() });
        this->structureSize.insert({ VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_PIPELINE_CREATION_CACHE_CONTROL_FEATURES, size<VkPhysicalDevicePipelineCreationCacheControlFeatures>() });
        this->structureSize.insert({ VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_1_1_FEATURES, size<VkPhysicalDeviceVulkan11Features>() });
        this->structureSize.insert({ VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_1_2_FEATURES, size<VkPhysicalDeviceVulkan12Features>() });
        this->structureSize.insert({ VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_1_3_FEATURES, size<VkPhysicalDeviceVulkan13Features>() });
        this->structureSize.insert({ VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_COHERENT_MEMORY_FEATURES_AMD, size<VkPhysicalDeviceCoherentMemoryFeaturesAMD>() });
        this->structureSize.insert({ VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_CUSTOM_BORDER_COLOR_FEATURES_EXT, size<VkPhysicalDeviceCustomBorderColorFeaturesEXT>() });
        this->structureSize.insert({ VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_BORDER_COLOR_SWIZZLE_FEATURES_EXT, size<VkPhysicalDeviceBorderColorSwizzleFeaturesEXT>() });
        this->structureSize.insert({ VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_EXTENDED_DYNAMIC_STATE_FEATURES_EXT, size<VkPhysicalDeviceExtendedDynamicStateFeaturesEXT>() });
        this->structureSize.insert({ VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_EXTENDED_DYNAMIC_STATE_2_FEATURES_EXT, size<VkPhysicalDeviceExtendedDynamicState2FeaturesEXT>() });
        this->structureSize.insert({ VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_EXTENDED_DYNAMIC_STATE_3_FEATURES_EXT, size<VkPhysicalDeviceExtendedDynamicState3FeaturesEXT>() });
        this->structureSize.insert({ VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_DIAGNOSTICS_CONFIG_FEATURES_NV, size<VkPhysicalDeviceDiagnosticsConfigFeaturesNV>() });
        this->structureSize.insert({ VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_ZERO_INITIALIZE_WORKGROUP_MEMORY_FEATURES, size<VkPhysicalDeviceZeroInitializeWorkgroupMemoryFeatures>() });
        this->structureSize.insert({ VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_SHADER_SUBGROUP_UNIFORM_CONTROL_FLOW_FEATURES_KHR, size<VkPhysicalDeviceShaderSubgroupUniformControlFlowFeaturesKHR>() });
        this->structureSize.insert({ VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_ROBUSTNESS_2_FEATURES_EXT, size<VkPhysicalDeviceRobustness2FeaturesEXT>() });
        this->structureSize.insert({ VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_IMAGE_ROBUSTNESS_FEATURES, size<VkPhysicalDeviceImageRobustnessFeatures>() });
        this->structureSize.insert({ VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_WORKGROUP_MEMORY_EXPLICIT_LAYOUT_FEATURES_KHR, size<VkPhysicalDeviceWorkgroupMemoryExplicitLayoutFeaturesKHR>() });
#ifdef VK_ENABLE_BETA_EXTENSIONS
        this->structureSize.insert({ VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_PORTABILITY_SUBSET_FEATURES_KHR, size<VkPhysicalDevicePortabilitySubsetFeaturesKHR>() });
#endif
        this->structureSize.insert({ VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_4444_FORMATS_FEATURES_EXT, size<VkPhysicalDevice4444FormatsFeaturesEXT>() });
        this->structureSize.insert({ VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_SUBPASS_SHADING_FEATURES_HUAWEI, size<VkPhysicalDeviceSubpassShadingFeaturesHUAWEI>() });
        this->structureSize.insert({ VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_CLUSTER_CULLING_SHADER_FEATURES_HUAWEI, size<VkPhysicalDeviceClusterCullingShaderFeaturesHUAWEI>() });
        this->structureSize.insert({ VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_SHADER_IMAGE_ATOMIC_INT64_FEATURES_EXT, size<VkPhysicalDeviceShaderImageAtomicInt64FeaturesEXT>() });
        this->structureSize.insert({ VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_FRAGMENT_SHADING_RATE_FEATURES_KHR, size<VkPhysicalDeviceFragmentShadingRateFeaturesKHR>() });
        this->structureSize.insert({ VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_SHADER_TERMINATE_INVOCATION_FEATURES, size<VkPhysicalDeviceShaderTerminateInvocationFeatures>() });
        this->structureSize.insert({ VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_FRAGMENT_SHADING_RATE_ENUMS_FEATURES_NV, size<VkPhysicalDeviceFragmentShadingRateEnumsFeaturesNV>() });
        this->structureSize.insert({ VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_IMAGE_2D_VIEW_OF_3D_FEATURES_EXT, size<VkPhysicalDeviceImage2DViewOf3DFeaturesEXT>() });
        this->structureSize.insert({ VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_IMAGE_SLICED_VIEW_OF_3D_FEATURES_EXT, size<VkPhysicalDeviceImageSlicedViewOf3DFeaturesEXT>() });
        this->structureSize.insert({ VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_ATTACHMENT_FEEDBACK_LOOP_DYNAMIC_STATE_FEATURES_EXT, size<VkPhysicalDeviceAttachmentFeedbackLoopDynamicStateFeaturesEXT>() });
        this->structureSize.insert({ VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_MUTABLE_DESCRIPTOR_TYPE_FEATURES_EXT, size<VkPhysicalDeviceMutableDescriptorTypeFeaturesEXT>() });
        this->structureSize.insert({ VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_DEPTH_CLIP_CONTROL_FEATURES_EXT, size<VkPhysicalDeviceDepthClipControlFeaturesEXT>() });
        this->structureSize.insert({ VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VERTEX_INPUT_DYNAMIC_STATE_FEATURES_EXT, size<VkPhysicalDeviceVertexInputDynamicStateFeaturesEXT>() });
        this->structureSize.insert({ VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_EXTERNAL_MEMORY_RDMA_FEATURES_NV, size<VkPhysicalDeviceExternalMemoryRDMAFeaturesNV>() });
        this->structureSize.insert({ VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_COLOR_WRITE_ENABLE_FEATURES_EXT, size<VkPhysicalDeviceColorWriteEnableFeaturesEXT>() });
        this->structureSize.insert({ VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_SYNCHRONIZATION_2_FEATURES, size<VkPhysicalDeviceSynchronization2Features>() });
        this->structureSize.insert({ VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_HOST_IMAGE_COPY_FEATURES_EXT, size<VkPhysicalDeviceHostImageCopyFeaturesEXT>() });
        this->structureSize.insert({ VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_PRIMITIVES_GENERATED_QUERY_FEATURES_EXT, size<VkPhysicalDevicePrimitivesGeneratedQueryFeaturesEXT>() });
        this->structureSize.insert({ VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_LEGACY_DITHERING_FEATURES_EXT, size<VkPhysicalDeviceLegacyDitheringFeaturesEXT>() });
        this->structureSize.insert({ VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_MULTISAMPLED_RENDER_TO_SINGLE_SAMPLED_FEATURES_EXT, size<VkPhysicalDeviceMultisampledRenderToSingleSampledFeaturesEXT>() });
        this->structureSize.insert({ VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_PIPELINE_PROTECTED_ACCESS_FEATURES_EXT, size<VkPhysicalDevicePipelineProtectedAccessFeaturesEXT>() });
        this->structureSize.insert({ VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VIDEO_MAINTENANCE_1_FEATURES_KHR, size<VkPhysicalDeviceVideoMaintenance1FeaturesKHR>() });
        this->structureSize.insert({ VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_INHERITED_VIEWPORT_SCISSOR_FEATURES_NV, size<VkPhysicalDeviceInheritedViewportScissorFeaturesNV>() });
        this->structureSize.insert({ VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_YCBCR_2_PLANE_444_FORMATS_FEATURES_EXT, size<VkPhysicalDeviceYcbcr2Plane444FormatsFeaturesEXT>() });
        this->structureSize.insert({ VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_PROVOKING_VERTEX_FEATURES_EXT, size<VkPhysicalDeviceProvokingVertexFeaturesEXT>() });
        this->structureSize.insert({ VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_DESCRIPTOR_BUFFER_FEATURES_EXT, size<VkPhysicalDeviceDescriptorBufferFeaturesEXT>() });
        this->structureSize.insert({ VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_SHADER_INTEGER_DOT_PRODUCT_FEATURES, size<VkPhysicalDeviceShaderIntegerDotProductFeatures>() });
        this->structureSize.insert({ VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_FRAGMENT_SHADER_BARYCENTRIC_FEATURES_KHR, size<VkPhysicalDeviceFragmentShaderBarycentricFeaturesKHR>() });
        this->structureSize.insert({ VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_RAY_TRACING_MOTION_BLUR_FEATURES_NV, size<VkPhysicalDeviceRayTracingMotionBlurFeaturesNV>() });
        this->structureSize.insert({ VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_RGBA10X6_FORMATS_FEATURES_EXT, size<VkPhysicalDeviceRGBA10X6FormatsFeaturesEXT>() });
        this->structureSize.insert({ VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_DYNAMIC_RENDERING_FEATURES, size<VkPhysicalDeviceDynamicRenderingFeatures>() });
        this->structureSize.insert({ VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_IMAGE_VIEW_MIN_LOD_FEATURES_EXT, size<VkPhysicalDeviceImageViewMinLodFeaturesEXT>() });
        this->structureSize.insert({ VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_RASTERIZATION_ORDER_ATTACHMENT_ACCESS_FEATURES_EXT, size<VkPhysicalDeviceRasterizationOrderAttachmentAccessFeaturesEXT>() });
        this->structureSize.insert({ VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_LINEAR_COLOR_ATTACHMENT_FEATURES_NV, size<VkPhysicalDeviceLinearColorAttachmentFeaturesNV>() });
        this->structureSize.insert({ VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_GRAPHICS_PIPELINE_LIBRARY_FEATURES_EXT, size<VkPhysicalDeviceGraphicsPipelineLibraryFeaturesEXT>() });
        this->structureSize.insert({ VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_DESCRIPTOR_SET_HOST_MAPPING_FEATURES_VALVE, size<VkPhysicalDeviceDescriptorSetHostMappingFeaturesVALVE>() });
        this->structureSize.insert({ VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_NESTED_COMMAND_BUFFER_FEATURES_EXT, size<VkPhysicalDeviceNestedCommandBufferFeaturesEXT>() });
        this->structureSize.insert({ VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_SHADER_MODULE_IDENTIFIER_FEATURES_EXT, size<VkPhysicalDeviceShaderModuleIdentifierFeaturesEXT>() });
        this->structureSize.insert({ VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_IMAGE_COMPRESSION_CONTROL_FEATURES_EXT, size<VkPhysicalDeviceImageCompressionControlFeaturesEXT>() });
        this->structureSize.insert({ VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_IMAGE_COMPRESSION_CONTROL_SWAPCHAIN_FEATURES_EXT, size<VkPhysicalDeviceImageCompressionControlSwapchainFeaturesEXT>() });
        this->structureSize.insert({ VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_SUBPASS_MERGE_FEEDBACK_FEATURES_EXT, size<VkPhysicalDeviceSubpassMergeFeedbackFeaturesEXT>() });
        this->structureSize.insert({ VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_OPACITY_MICROMAP_FEATURES_EXT, size<VkPhysicalDeviceOpacityMicromapFeaturesEXT>() });
#ifdef VK_ENABLE_BETA_EXTENSIONS
        this->structureSize.insert({ VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_DISPLACEMENT_MICROMAP_FEATURES_NV, size<VkPhysicalDeviceDisplacementMicromapFeaturesNV>() });
#endif
        this->structureSize.insert({ VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_PIPELINE_PROPERTIES_FEATURES_EXT, size<VkPhysicalDevicePipelinePropertiesFeaturesEXT>() });
        this->structureSize.insert({ VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_SHADER_EARLY_AND_LATE_FRAGMENT_TESTS_FEATURES_AMD, size<VkPhysicalDeviceShaderEarlyAndLateFragmentTestsFeaturesAMD>() });
        this->structureSize.insert({ VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_NON_SEAMLESS_CUBE_MAP_FEATURES_EXT, size<VkPhysicalDeviceNonSeamlessCubeMapFeaturesEXT>() });
        this->structureSize.insert({ VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_PIPELINE_ROBUSTNESS_FEATURES_EXT, size<VkPhysicalDevicePipelineRobustnessFeaturesEXT>() });
        this->structureSize.insert({ VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_IMAGE_PROCESSING_FEATURES_QCOM, size<VkPhysicalDeviceImageProcessingFeaturesQCOM>() });
        this->structureSize.insert({ VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_TILE_PROPERTIES_FEATURES_QCOM, size<VkPhysicalDeviceTilePropertiesFeaturesQCOM>() });
        this->structureSize.insert({ VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_AMIGO_PROFILING_FEATURES_SEC, size<VkPhysicalDeviceAmigoProfilingFeaturesSEC>() });
        this->structureSize.insert({ VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_ATTACHMENT_FEEDBACK_LOOP_LAYOUT_FEATURES_EXT, size<VkPhysicalDeviceAttachmentFeedbackLoopLayoutFeaturesEXT>() });
        this->structureSize.insert({ VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_DEPTH_CLAMP_ZERO_ONE_FEATURES_EXT, size<VkPhysicalDeviceDepthClampZeroOneFeaturesEXT>() });
        this->structureSize.insert({ VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_ADDRESS_BINDING_REPORT_FEATURES_EXT, size<VkPhysicalDeviceAddressBindingReportFeaturesEXT>() });
        this->structureSize.insert({ VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_OPTICAL_FLOW_FEATURES_NV, size<VkPhysicalDeviceOpticalFlowFeaturesNV>() });
        this->structureSize.insert({ VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_FAULT_FEATURES_EXT, size<VkPhysicalDeviceFaultFeaturesEXT>() });
        this->structureSize.insert({ VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_PIPELINE_LIBRARY_GROUP_HANDLES_FEATURES_EXT, size<VkPhysicalDevicePipelineLibraryGroupHandlesFeaturesEXT>() });
        this->structureSize.insert({ VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_SHADER_CORE_BUILTINS_FEATURES_ARM, size<VkPhysicalDeviceShaderCoreBuiltinsFeaturesARM>() });
        this->structureSize.insert({ VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_FRAME_BOUNDARY_FEATURES_EXT, size<VkPhysicalDeviceFrameBoundaryFeaturesEXT>() });
        this->structureSize.insert({ VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_DYNAMIC_RENDERING_UNUSED_ATTACHMENTS_FEATURES_EXT, size<VkPhysicalDeviceDynamicRenderingUnusedAttachmentsFeaturesEXT>() });
        this->structureSize.insert({ VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_SWAPCHAIN_MAINTENANCE_1_FEATURES_EXT, size<VkPhysicalDeviceSwapchainMaintenance1FeaturesEXT>() });
        this->structureSize.insert({ VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_DEPTH_BIAS_CONTROL_FEATURES_EXT, size<VkPhysicalDeviceDepthBiasControlFeaturesEXT>() });
        this->structureSize.insert({ VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_RAY_TRACING_INVOCATION_REORDER_FEATURES_NV, size<VkPhysicalDeviceRayTracingInvocationReorderFeaturesNV>() });
        this->structureSize.insert({ VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_EXTENDED_SPARSE_ADDRESS_SPACE_FEATURES_NV, size<VkPhysicalDeviceExtendedSparseAddressSpaceFeaturesNV>() });
        this->structureSize.insert({ VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_MULTIVIEW_PER_VIEW_VIEWPORTS_FEATURES_QCOM, size<VkPhysicalDeviceMultiviewPerViewViewportsFeaturesQCOM>() });
        this->structureSize.insert({ VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_RAY_TRACING_POSITION_FETCH_FEATURES_KHR, size<VkPhysicalDeviceRayTracingPositionFetchFeaturesKHR>() });
        this->structureSize.insert({ VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_MULTIVIEW_PER_VIEW_RENDER_AREAS_FEATURES_QCOM, size<VkPhysicalDeviceMultiviewPerViewRenderAreasFeaturesQCOM>() });
        this->structureSize.insert({ VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_SHADER_OBJECT_FEATURES_EXT, size<VkPhysicalDeviceShaderObjectFeaturesEXT>() });
        this->structureSize.insert({ VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_SHADER_TILE_IMAGE_FEATURES_EXT, size<VkPhysicalDeviceShaderTileImageFeaturesEXT>() });
#ifdef VK_USE_PLATFORM_SCREEN_QNX
        this->structureSize.insert({ VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_EXTERNAL_MEMORY_SCREEN_BUFFER_FEATURES_QNX, size<VkPhysicalDeviceExternalMemoryScreenBufferFeaturesQNX>() });
#endif
        this->structureSize.insert({ VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_COOPERATIVE_MATRIX_FEATURES_KHR, size<VkPhysicalDeviceCooperativeMatrixFeaturesKHR>() });
#ifdef VK_ENABLE_BETA_EXTENSIONS
        this->structureSize.insert({ VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_SHADER_ENQUEUE_FEATURES_AMDX, size<VkPhysicalDeviceShaderEnqueueFeaturesAMDX>() });
#endif
        this->structureSize.insert({ VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_CUBIC_CLAMP_FEATURES_QCOM, size<VkPhysicalDeviceCubicClampFeaturesQCOM>() });
        this->structureSize.insert({ VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_YCBCR_DEGAMMA_FEATURES_QCOM, size<VkPhysicalDeviceYcbcrDegammaFeaturesQCOM>() });
        this->structureSize.insert({ VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_CUBIC_WEIGHTS_FEATURES_QCOM, size<VkPhysicalDeviceCubicWeightsFeaturesQCOM>() });
        this->structureSize.insert({ VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_IMAGE_PROCESSING_2_FEATURES_QCOM, size<VkPhysicalDeviceImageProcessing2FeaturesQCOM>() });
        this->structureSize.insert({ VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_DESCRIPTOR_POOL_OVERALLOCATION_FEATURES_NV, size<VkPhysicalDeviceDescriptorPoolOverallocationFeaturesNV>() });
        this->structureSize.insert({ VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_PER_STAGE_DESCRIPTOR_SET_FEATURES_NV, size<VkPhysicalDevicePerStageDescriptorSetFeaturesNV>() });
#ifdef VK_USE_PLATFORM_ANDROID_KHR
        this->structureSize.insert({ VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_EXTERNAL_FORMAT_RESOLVE_FEATURES_ANDROID, size<VkPhysicalDeviceExternalFormatResolveFeaturesANDROID>() });
#endif
        this->structureSize.insert({ VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_CUDA_KERNEL_LAUNCH_FEATURES_NV, size<VkPhysicalDeviceCudaKernelLaunchFeaturesNV>() });
        this->structureSize.insert({ VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_SCHEDULING_CONTROLS_FEATURES_ARM, size<VkPhysicalDeviceSchedulingControlsFeaturesARM>() });
        this->structureSize.insert({ VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_RELAXED_LINE_RASTERIZATION_FEATURES_IMG, size<VkPhysicalDeviceRelaxedLineRasterizationFeaturesIMG>() });
        this->structureSize.insert({ VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_RENDER_PASS_STRIPED_FEATURES_ARM, size<VkPhysicalDeviceRenderPassStripedFeaturesARM>() });
        this->structureSize.insert({ VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_FEATURES_2_KHR, size<VkPhysicalDeviceFeatures2KHR>() });

        //Initializing the full list of available structure features
        void* pNext = nullptr;
        physicalDeviceDeviceGeneratedCommandsFeaturesNV.pNext = pNext;
        pNext = &physicalDeviceDeviceGeneratedCommandsFeaturesNV;
        physicalDeviceDeviceGeneratedCommandsComputeFeaturesNV.pNext = pNext;
        pNext = &physicalDeviceDeviceGeneratedCommandsComputeFeaturesNV;
        physicalDevicePrivateDataFeatures.pNext = pNext;
        pNext = &physicalDevicePrivateDataFeatures;
        physicalDeviceVariablePointersFeatures.pNext = pNext;
        pNext = &physicalDeviceVariablePointersFeatures;
        physicalDeviceMultiviewFeatures.pNext = pNext;
        pNext = &physicalDeviceMultiviewFeatures;
        physicalDevicePresentIdFeaturesKHR.pNext = pNext;
        pNext = &physicalDevicePresentIdFeaturesKHR;
        physicalDevicePresentWaitFeaturesKHR.pNext = pNext;
        pNext = &physicalDevicePresentWaitFeaturesKHR;
        physicalDevice16BitStorageFeatures.pNext = pNext;
        pNext = &physicalDevice16BitStorageFeatures;
        physicalDeviceShaderSubgroupExtendedTypesFeatures.pNext = pNext;
        pNext = &physicalDeviceShaderSubgroupExtendedTypesFeatures;
        physicalDeviceSamplerYcbcrConversionFeatures.pNext = pNext;
        pNext = &physicalDeviceSamplerYcbcrConversionFeatures;
        physicalDeviceProtectedMemoryFeatures.pNext = pNext;
        pNext = &physicalDeviceProtectedMemoryFeatures;
        physicalDeviceBlendOperationAdvancedFeaturesEXT.pNext = pNext;
        pNext = &physicalDeviceBlendOperationAdvancedFeaturesEXT;
        physicalDeviceMultiDrawFeaturesEXT.pNext = pNext;
        pNext = &physicalDeviceMultiDrawFeaturesEXT;
        physicalDeviceInlineUniformBlockFeatures.pNext = pNext;
        pNext = &physicalDeviceInlineUniformBlockFeatures;
        physicalDeviceMaintenance4Features.pNext = pNext;
        pNext = &physicalDeviceMaintenance4Features;
        physicalDeviceMaintenance5FeaturesKHR.pNext = pNext;
        pNext = &physicalDeviceMaintenance5FeaturesKHR;
        physicalDeviceMaintenance6FeaturesKHR.pNext = pNext;
        pNext = &physicalDeviceMaintenance6FeaturesKHR;
        physicalDeviceShaderDrawParametersFeatures.pNext = pNext;
        pNext = &physicalDeviceShaderDrawParametersFeatures;
        physicalDeviceShaderFloat16Int8Features.pNext = pNext;
        pNext = &physicalDeviceShaderFloat16Int8Features;
        physicalDeviceHostQueryResetFeatures.pNext = pNext;
        pNext = &physicalDeviceHostQueryResetFeatures;
        physicalDeviceGlobalPriorityQueryFeaturesKHR.pNext = pNext;
        pNext = &physicalDeviceGlobalPriorityQueryFeaturesKHR;
        physicalDeviceDeviceMemoryReportFeaturesEXT.pNext = pNext;
        pNext = &physicalDeviceDeviceMemoryReportFeaturesEXT;
        physicalDeviceDescriptorIndexingFeatures.pNext = pNext;
        pNext = &physicalDeviceDescriptorIndexingFeatures;
        physicalDeviceTimelineSemaphoreFeatures.pNext = pNext;
        pNext = &physicalDeviceTimelineSemaphoreFeatures;
        physicalDevice8BitStorageFeatures.pNext = pNext;
        pNext = &physicalDevice8BitStorageFeatures;
        physicalDeviceConditionalRenderingFeaturesEXT.pNext = pNext;
        pNext = &physicalDeviceConditionalRenderingFeaturesEXT;
        physicalDeviceVulkanMemoryModelFeatures.pNext = pNext;
        pNext = &physicalDeviceVulkanMemoryModelFeatures;
        physicalDeviceShaderAtomicInt64Features.pNext = pNext;
        pNext = &physicalDeviceShaderAtomicInt64Features;
        physicalDeviceShaderAtomicFloatFeaturesEXT.pNext = pNext;
        pNext = &physicalDeviceShaderAtomicFloatFeaturesEXT;
        physicalDeviceShaderAtomicFloat2FeaturesEXT.pNext = pNext;
        pNext = &physicalDeviceShaderAtomicFloat2FeaturesEXT;
        physicalDeviceVertexAttributeDivisorFeaturesKHR.pNext = pNext;
        pNext = &physicalDeviceVertexAttributeDivisorFeaturesKHR;
        physicalDeviceASTCDecodeFeaturesEXT.pNext = pNext;
        pNext = &physicalDeviceASTCDecodeFeaturesEXT;
        physicalDeviceTransformFeedbackFeaturesEXT.pNext = pNext;
        pNext = &physicalDeviceTransformFeedbackFeaturesEXT;
        physicalDeviceRepresentativeFragmentTestFeaturesNV.pNext = pNext;
        pNext = &physicalDeviceRepresentativeFragmentTestFeaturesNV;
        physicalDeviceExclusiveScissorFeaturesNV.pNext = pNext;
        pNext = &physicalDeviceExclusiveScissorFeaturesNV;
        physicalDeviceCornerSampledImageFeaturesNV.pNext = pNext;
        pNext = &physicalDeviceCornerSampledImageFeaturesNV;
        physicalDeviceComputeShaderDerivativesFeaturesNV.pNext = pNext;
        pNext = &physicalDeviceComputeShaderDerivativesFeaturesNV;
        physicalDeviceShaderImageFootprintFeaturesNV.pNext = pNext;
        pNext = &physicalDeviceShaderImageFootprintFeaturesNV;
        physicalDeviceDedicatedAllocationImageAliasingFeaturesNV.pNext = pNext;
        pNext = &physicalDeviceDedicatedAllocationImageAliasingFeaturesNV;
        physicalDeviceCopyMemoryIndirectFeaturesNV.pNext = pNext;
        pNext = &physicalDeviceCopyMemoryIndirectFeaturesNV;
        physicalDeviceMemoryDecompressionFeaturesNV.pNext = pNext;
        pNext = &physicalDeviceMemoryDecompressionFeaturesNV;
        physicalDeviceShadingRateImageFeaturesNV.pNext = pNext;
        pNext = &physicalDeviceShadingRateImageFeaturesNV;
        physicalDeviceInvocationMaskFeaturesHUAWEI.pNext = pNext;
        pNext = &physicalDeviceInvocationMaskFeaturesHUAWEI;
        physicalDeviceMeshShaderFeaturesNV.pNext = pNext;
        pNext = &physicalDeviceMeshShaderFeaturesNV;
        physicalDeviceMeshShaderFeaturesEXT.pNext = pNext;
        pNext = &physicalDeviceMeshShaderFeaturesEXT;
        physicalDeviceAccelerationStructureFeaturesKHR.pNext = pNext;
        pNext = &physicalDeviceAccelerationStructureFeaturesKHR;
        physicalDeviceRayTracingPipelineFeaturesKHR.pNext = pNext;
        pNext = &physicalDeviceRayTracingPipelineFeaturesKHR;
        physicalDeviceRayQueryFeaturesKHR.pNext = pNext;
        pNext = &physicalDeviceRayQueryFeaturesKHR;
        physicalDeviceRayTracingMaintenance1FeaturesKHR.pNext = pNext;
        pNext = &physicalDeviceRayTracingMaintenance1FeaturesKHR;
        physicalDeviceFragmentDensityMapFeaturesEXT.pNext = pNext;
        pNext = &physicalDeviceFragmentDensityMapFeaturesEXT;
        physicalDeviceFragmentDensityMap2FeaturesEXT.pNext = pNext;
        pNext = &physicalDeviceFragmentDensityMap2FeaturesEXT;
        physicalDeviceFragmentDensityMapOffsetFeaturesQCOM.pNext = pNext;
        pNext = &physicalDeviceFragmentDensityMapOffsetFeaturesQCOM;
        physicalDeviceScalarBlockLayoutFeatures.pNext = pNext;
        pNext = &physicalDeviceScalarBlockLayoutFeatures;
        physicalDeviceUniformBufferStandardLayoutFeatures.pNext = pNext;
        pNext = &physicalDeviceUniformBufferStandardLayoutFeatures;
        physicalDeviceDepthClipEnableFeaturesEXT.pNext = pNext;
        pNext = &physicalDeviceDepthClipEnableFeaturesEXT;
        physicalDeviceMemoryPriorityFeaturesEXT.pNext = pNext;
        pNext = &physicalDeviceMemoryPriorityFeaturesEXT;
        physicalDevicePageableDeviceLocalMemoryFeaturesEXT.pNext = pNext;
        pNext = &physicalDevicePageableDeviceLocalMemoryFeaturesEXT;
        physicalDeviceBufferDeviceAddressFeatures.pNext = pNext;
        pNext = &physicalDeviceBufferDeviceAddressFeatures;
        physicalDeviceBufferDeviceAddressFeaturesEXT.pNext = pNext;
        pNext = &physicalDeviceBufferDeviceAddressFeaturesEXT;
        physicalDeviceImagelessFramebufferFeatures.pNext = pNext;
        pNext = &physicalDeviceImagelessFramebufferFeatures;
        physicalDeviceTextureCompressionASTCHDRFeatures.pNext = pNext;
        pNext = &physicalDeviceTextureCompressionASTCHDRFeatures;
        physicalDeviceCooperativeMatrixFeaturesNV.pNext = pNext;
        pNext = &physicalDeviceCooperativeMatrixFeaturesNV;
        physicalDeviceYcbcrImageArraysFeaturesEXT.pNext = pNext;
        pNext = &physicalDeviceYcbcrImageArraysFeaturesEXT;
        physicalDevicePresentBarrierFeaturesNV.pNext = pNext;
        pNext = &physicalDevicePresentBarrierFeaturesNV;
        physicalDevicePerformanceQueryFeaturesKHR.pNext = pNext;
        pNext = &physicalDevicePerformanceQueryFeaturesKHR;
        physicalDeviceCoverageReductionModeFeaturesNV.pNext = pNext;
        pNext = &physicalDeviceCoverageReductionModeFeaturesNV;
        physicalDeviceShaderIntegerFunctions2FeaturesINTEL.pNext = pNext;
        pNext = &physicalDeviceShaderIntegerFunctions2FeaturesINTEL;
        physicalDeviceShaderClockFeaturesKHR.pNext = pNext;
        pNext = &physicalDeviceShaderClockFeaturesKHR;
        physicalDeviceIndexTypeUint8FeaturesEXT.pNext = pNext;
        pNext = &physicalDeviceIndexTypeUint8FeaturesEXT;
        physicalDeviceShaderSMBuiltinsFeaturesNV.pNext = pNext;
        pNext = &physicalDeviceShaderSMBuiltinsFeaturesNV;
        physicalDeviceFragmentShaderInterlockFeaturesEXT.pNext = pNext;
        pNext = &physicalDeviceFragmentShaderInterlockFeaturesEXT;
        physicalDeviceSeparateDepthStencilLayoutsFeatures.pNext = pNext;
        pNext = &physicalDeviceSeparateDepthStencilLayoutsFeatures;
        physicalDevicePrimitiveTopologyListRestartFeaturesEXT.pNext = pNext;
        pNext = &physicalDevicePrimitiveTopologyListRestartFeaturesEXT;
        physicalDevicePipelineExecutablePropertiesFeaturesKHR.pNext = pNext;
        pNext = &physicalDevicePipelineExecutablePropertiesFeaturesKHR;
        physicalDeviceShaderDemoteToHelperInvocationFeatures.pNext = pNext;
        pNext = &physicalDeviceShaderDemoteToHelperInvocationFeatures;
        physicalDeviceTexelBufferAlignmentFeaturesEXT.pNext = pNext;
        pNext = &physicalDeviceTexelBufferAlignmentFeaturesEXT;
        physicalDeviceSubgroupSizeControlFeatures.pNext = pNext;
        pNext = &physicalDeviceSubgroupSizeControlFeatures;
        physicalDeviceLineRasterizationFeaturesEXT.pNext = pNext;
        pNext = &physicalDeviceLineRasterizationFeaturesEXT;
        physicalDevicePipelineCreationCacheControlFeatures.pNext = pNext;
        pNext = &physicalDevicePipelineCreationCacheControlFeatures;
        physicalDeviceVulkan11Features.pNext = pNext;
        pNext = &physicalDeviceVulkan11Features;
        physicalDeviceVulkan12Features.pNext = pNext;
        pNext = &physicalDeviceVulkan12Features;
        physicalDeviceVulkan13Features.pNext = pNext;
        pNext = &physicalDeviceVulkan13Features;
        physicalDeviceCoherentMemoryFeaturesAMD.pNext = pNext;
        pNext = &physicalDeviceCoherentMemoryFeaturesAMD;
        physicalDeviceCustomBorderColorFeaturesEXT.pNext = pNext;
        pNext = &physicalDeviceCustomBorderColorFeaturesEXT;
        physicalDeviceBorderColorSwizzleFeaturesEXT.pNext = pNext;
        pNext = &physicalDeviceBorderColorSwizzleFeaturesEXT;
        physicalDeviceExtendedDynamicStateFeaturesEXT.pNext = pNext;
        pNext = &physicalDeviceExtendedDynamicStateFeaturesEXT;
        physicalDeviceExtendedDynamicState2FeaturesEXT.pNext = pNext;
        pNext = &physicalDeviceExtendedDynamicState2FeaturesEXT;
        physicalDeviceExtendedDynamicState3FeaturesEXT.pNext = pNext;
        pNext = &physicalDeviceExtendedDynamicState3FeaturesEXT;
        physicalDeviceDiagnosticsConfigFeaturesNV.pNext = pNext;
        pNext = &physicalDeviceDiagnosticsConfigFeaturesNV;
        physicalDeviceZeroInitializeWorkgroupMemoryFeatures.pNext = pNext;
        pNext = &physicalDeviceZeroInitializeWorkgroupMemoryFeatures;
        physicalDeviceShaderSubgroupUniformControlFlowFeaturesKHR.pNext = pNext;
        pNext = &physicalDeviceShaderSubgroupUniformControlFlowFeaturesKHR;
        physicalDeviceRobustness2FeaturesEXT.pNext = pNext;
        pNext = &physicalDeviceRobustness2FeaturesEXT;
        physicalDeviceImageRobustnessFeatures.pNext = pNext;
        pNext = &physicalDeviceImageRobustnessFeatures;
        physicalDeviceWorkgroupMemoryExplicitLayoutFeaturesKHR.pNext = pNext;
        pNext = &physicalDeviceWorkgroupMemoryExplicitLayoutFeaturesKHR;
#ifdef VK_ENABLE_BETA_EXTENSIONS
        physicalDevicePortabilitySubsetFeaturesKHR.pNext = pNext;
        pNext = &physicalDevicePortabilitySubsetFeaturesKHR;
#endif
        physicalDevice4444FormatsFeaturesEXT.pNext = pNext;
        pNext = &physicalDevice4444FormatsFeaturesEXT;
        physicalDeviceSubpassShadingFeaturesHUAWEI.pNext = pNext;
        pNext = &physicalDeviceSubpassShadingFeaturesHUAWEI;
        physicalDeviceClusterCullingShaderFeaturesHUAWEI.pNext = pNext;
        pNext = &physicalDeviceClusterCullingShaderFeaturesHUAWEI;
        physicalDeviceShaderImageAtomicInt64FeaturesEXT.pNext = pNext;
        pNext = &physicalDeviceShaderImageAtomicInt64FeaturesEXT;
        physicalDeviceFragmentShadingRateFeaturesKHR.pNext = pNext;
        pNext = &physicalDeviceFragmentShadingRateFeaturesKHR;
        physicalDeviceShaderTerminateInvocationFeatures.pNext = pNext;
        pNext = &physicalDeviceShaderTerminateInvocationFeatures;
        physicalDeviceFragmentShadingRateEnumsFeaturesNV.pNext = pNext;
        pNext = &physicalDeviceFragmentShadingRateEnumsFeaturesNV;
        physicalDeviceImage2DViewOf3DFeaturesEXT.pNext = pNext;
        pNext = &physicalDeviceImage2DViewOf3DFeaturesEXT;
        physicalDeviceImageSlicedViewOf3DFeaturesEXT.pNext = pNext;
        pNext = &physicalDeviceImageSlicedViewOf3DFeaturesEXT;
        physicalDeviceAttachmentFeedbackLoopDynamicStateFeaturesEXT.pNext = pNext;
        pNext = &physicalDeviceAttachmentFeedbackLoopDynamicStateFeaturesEXT;
        physicalDeviceMutableDescriptorTypeFeaturesEXT.pNext = pNext;
        pNext = &physicalDeviceMutableDescriptorTypeFeaturesEXT;
        physicalDeviceDepthClipControlFeaturesEXT.pNext = pNext;
        pNext = &physicalDeviceDepthClipControlFeaturesEXT;
        physicalDeviceVertexInputDynamicStateFeaturesEXT.pNext = pNext;
        pNext = &physicalDeviceVertexInputDynamicStateFeaturesEXT;
        physicalDeviceExternalMemoryRDMAFeaturesNV.pNext = pNext;
        pNext = &physicalDeviceExternalMemoryRDMAFeaturesNV;
        physicalDeviceColorWriteEnableFeaturesEXT.pNext = pNext;
        pNext = &physicalDeviceColorWriteEnableFeaturesEXT;
        physicalDeviceSynchronization2Features.pNext = pNext;
        pNext = &physicalDeviceSynchronization2Features;
        physicalDeviceHostImageCopyFeaturesEXT.pNext = pNext;
        pNext = &physicalDeviceHostImageCopyFeaturesEXT;
        physicalDevicePrimitivesGeneratedQueryFeaturesEXT.pNext = pNext;
        pNext = &physicalDevicePrimitivesGeneratedQueryFeaturesEXT;
        physicalDeviceLegacyDitheringFeaturesEXT.pNext = pNext;
        pNext = &physicalDeviceLegacyDitheringFeaturesEXT;
        physicalDeviceMultisampledRenderToSingleSampledFeaturesEXT.pNext = pNext;
        pNext = &physicalDeviceMultisampledRenderToSingleSampledFeaturesEXT;
        physicalDevicePipelineProtectedAccessFeaturesEXT.pNext = pNext;
        pNext = &physicalDevicePipelineProtectedAccessFeaturesEXT;
        physicalDeviceVideoMaintenance1FeaturesKHR.pNext = pNext;
        pNext = &physicalDeviceVideoMaintenance1FeaturesKHR;
        physicalDeviceInheritedViewportScissorFeaturesNV.pNext = pNext;
        pNext = &physicalDeviceInheritedViewportScissorFeaturesNV;
        physicalDeviceYcbcr2Plane444FormatsFeaturesEXT.pNext = pNext;
        pNext = &physicalDeviceYcbcr2Plane444FormatsFeaturesEXT;
        physicalDeviceProvokingVertexFeaturesEXT.pNext = pNext;
        pNext = &physicalDeviceProvokingVertexFeaturesEXT;
        physicalDeviceDescriptorBufferFeaturesEXT.pNext = pNext;
        pNext = &physicalDeviceDescriptorBufferFeaturesEXT;
        physicalDeviceShaderIntegerDotProductFeatures.pNext = pNext;
        pNext = &physicalDeviceShaderIntegerDotProductFeatures;
        physicalDeviceFragmentShaderBarycentricFeaturesKHR.pNext = pNext;
        pNext = &physicalDeviceFragmentShaderBarycentricFeaturesKHR;
        physicalDeviceRayTracingMotionBlurFeaturesNV.pNext = pNext;
        pNext = &physicalDeviceRayTracingMotionBlurFeaturesNV;
        physicalDeviceRGBA10X6FormatsFeaturesEXT.pNext = pNext;
        pNext = &physicalDeviceRGBA10X6FormatsFeaturesEXT;
        physicalDeviceDynamicRenderingFeatures.pNext = pNext;
        pNext = &physicalDeviceDynamicRenderingFeatures;
        physicalDeviceImageViewMinLodFeaturesEXT.pNext = pNext;
        pNext = &physicalDeviceImageViewMinLodFeaturesEXT;
        physicalDeviceRasterizationOrderAttachmentAccessFeaturesEXT.pNext = pNext;
        pNext = &physicalDeviceRasterizationOrderAttachmentAccessFeaturesEXT;
        physicalDeviceLinearColorAttachmentFeaturesNV.pNext = pNext;
        pNext = &physicalDeviceLinearColorAttachmentFeaturesNV;
        physicalDeviceGraphicsPipelineLibraryFeaturesEXT.pNext = pNext;
        pNext = &physicalDeviceGraphicsPipelineLibraryFeaturesEXT;
        physicalDeviceDescriptorSetHostMappingFeaturesVALVE.pNext = pNext;
        pNext = &physicalDeviceDescriptorSetHostMappingFeaturesVALVE;
        physicalDeviceNestedCommandBufferFeaturesEXT.pNext = pNext;
        pNext = &physicalDeviceNestedCommandBufferFeaturesEXT;
        physicalDeviceShaderModuleIdentifierFeaturesEXT.pNext = pNext;
        pNext = &physicalDeviceShaderModuleIdentifierFeaturesEXT;
        physicalDeviceImageCompressionControlFeaturesEXT.pNext = pNext;
        pNext = &physicalDeviceImageCompressionControlFeaturesEXT;
        physicalDeviceImageCompressionControlSwapchainFeaturesEXT.pNext = pNext;
        pNext = &physicalDeviceImageCompressionControlSwapchainFeaturesEXT;
        physicalDeviceSubpassMergeFeedbackFeaturesEXT.pNext = pNext;
        pNext = &physicalDeviceSubpassMergeFeedbackFeaturesEXT;
        physicalDeviceOpacityMicromapFeaturesEXT.pNext = pNext;
        pNext = &physicalDeviceOpacityMicromapFeaturesEXT;
#ifdef VK_ENABLE_BETA_EXTENSIONS
        physicalDeviceDisplacementMicromapFeaturesNV.pNext = pNext;
        pNext = &physicalDeviceDisplacementMicromapFeaturesNV;
#endif
        physicalDevicePipelinePropertiesFeaturesEXT.pNext = pNext;
        pNext = &physicalDevicePipelinePropertiesFeaturesEXT;
        physicalDeviceShaderEarlyAndLateFragmentTestsFeaturesAMD.pNext = pNext;
        pNext = &physicalDeviceShaderEarlyAndLateFragmentTestsFeaturesAMD;
        physicalDeviceNonSeamlessCubeMapFeaturesEXT.pNext = pNext;
        pNext = &physicalDeviceNonSeamlessCubeMapFeaturesEXT;
        physicalDevicePipelineRobustnessFeaturesEXT.pNext = pNext;
        pNext = &physicalDevicePipelineRobustnessFeaturesEXT;
        physicalDeviceImageProcessingFeaturesQCOM.pNext = pNext;
        pNext = &physicalDeviceImageProcessingFeaturesQCOM;
        physicalDeviceTilePropertiesFeaturesQCOM.pNext = pNext;
        pNext = &physicalDeviceTilePropertiesFeaturesQCOM;
        physicalDeviceAmigoProfilingFeaturesSEC.pNext = pNext;
        pNext = &physicalDeviceAmigoProfilingFeaturesSEC;
        physicalDeviceAttachmentFeedbackLoopLayoutFeaturesEXT.pNext = pNext;
        pNext = &physicalDeviceAttachmentFeedbackLoopLayoutFeaturesEXT;
        physicalDeviceDepthClampZeroOneFeaturesEXT.pNext = pNext;
        pNext = &physicalDeviceDepthClampZeroOneFeaturesEXT;
        physicalDeviceAddressBindingReportFeaturesEXT.pNext = pNext;
        pNext = &physicalDeviceAddressBindingReportFeaturesEXT;
        physicalDeviceOpticalFlowFeaturesNV.pNext = pNext;
        pNext = &physicalDeviceOpticalFlowFeaturesNV;
        physicalDeviceFaultFeaturesEXT.pNext = pNext;
        pNext = &physicalDeviceFaultFeaturesEXT;
        physicalDevicePipelineLibraryGroupHandlesFeaturesEXT.pNext = pNext;
        pNext = &physicalDevicePipelineLibraryGroupHandlesFeaturesEXT;
        physicalDeviceShaderCoreBuiltinsFeaturesARM.pNext = pNext;
        pNext = &physicalDeviceShaderCoreBuiltinsFeaturesARM;
        physicalDeviceFrameBoundaryFeaturesEXT.pNext = pNext;
        pNext = &physicalDeviceFrameBoundaryFeaturesEXT;
        physicalDeviceDynamicRenderingUnusedAttachmentsFeaturesEXT.pNext = pNext;
        pNext = &physicalDeviceDynamicRenderingUnusedAttachmentsFeaturesEXT;
        physicalDeviceSwapchainMaintenance1FeaturesEXT.pNext = pNext;
        pNext = &physicalDeviceSwapchainMaintenance1FeaturesEXT;
        physicalDeviceDepthBiasControlFeaturesEXT.pNext = pNext;
        pNext = &physicalDeviceDepthBiasControlFeaturesEXT;
        physicalDeviceRayTracingInvocationReorderFeaturesNV.pNext = pNext;
        pNext = &physicalDeviceRayTracingInvocationReorderFeaturesNV;
        physicalDeviceExtendedSparseAddressSpaceFeaturesNV.pNext = pNext;
        pNext = &physicalDeviceExtendedSparseAddressSpaceFeaturesNV;
        physicalDeviceMultiviewPerViewViewportsFeaturesQCOM.pNext = pNext;
        pNext = &physicalDeviceMultiviewPerViewViewportsFeaturesQCOM;
        physicalDeviceRayTracingPositionFetchFeaturesKHR.pNext = pNext;
        pNext = &physicalDeviceRayTracingPositionFetchFeaturesKHR;
        physicalDeviceMultiviewPerViewRenderAreasFeaturesQCOM.pNext = pNext;
        pNext = &physicalDeviceMultiviewPerViewRenderAreasFeaturesQCOM;
        physicalDeviceShaderObjectFeaturesEXT.pNext = pNext;
        pNext = &physicalDeviceShaderObjectFeaturesEXT;
        physicalDeviceShaderTileImageFeaturesEXT.pNext = pNext;
        pNext = &physicalDeviceShaderTileImageFeaturesEXT;
#ifdef VK_USE_PLATFORM_SCREEN_QNX
        physicalDeviceExternalMemoryScreenBufferFeaturesQNX.pNext = pNext;
        pNext = &physicalDeviceExternalMemoryScreenBufferFeaturesQNX;
#endif
        physicalDeviceCooperativeMatrixFeaturesKHR.pNext = pNext;
        pNext = &physicalDeviceCooperativeMatrixFeaturesKHR;
#ifdef VK_ENABLE_BETA_EXTENSIONS
        physicalDeviceShaderEnqueueFeaturesAMDX.pNext = pNext;
        pNext = &physicalDeviceShaderEnqueueFeaturesAMDX;
#endif
        physicalDeviceCubicClampFeaturesQCOM.pNext = pNext;
        pNext = &physicalDeviceCubicClampFeaturesQCOM;
        physicalDeviceYcbcrDegammaFeaturesQCOM.pNext = pNext;
        pNext = &physicalDeviceYcbcrDegammaFeaturesQCOM;
        physicalDeviceCubicWeightsFeaturesQCOM.pNext = pNext;
        pNext = &physicalDeviceCubicWeightsFeaturesQCOM;
        physicalDeviceImageProcessing2FeaturesQCOM.pNext = pNext;
        pNext = &physicalDeviceImageProcessing2FeaturesQCOM;
        physicalDeviceDescriptorPoolOverallocationFeaturesNV.pNext = pNext;
        pNext = &physicalDeviceDescriptorPoolOverallocationFeaturesNV;
        physicalDevicePerStageDescriptorSetFeaturesNV.pNext = pNext;
        pNext = &physicalDevicePerStageDescriptorSetFeaturesNV;
#ifdef VK_USE_PLATFORM_ANDROID_KHR
        physicalDeviceExternalFormatResolveFeaturesANDROID.pNext = pNext;
        pNext = &physicalDeviceExternalFormatResolveFeaturesANDROID;
#endif
        physicalDeviceCudaKernelLaunchFeaturesNV.pNext = pNext;
        pNext = &physicalDeviceCudaKernelLaunchFeaturesNV;
        physicalDeviceSchedulingControlsFeaturesARM.pNext = pNext;
        pNext = &physicalDeviceSchedulingControlsFeaturesARM;
        physicalDeviceRelaxedLineRasterizationFeaturesIMG.pNext = pNext;
        pNext = &physicalDeviceRelaxedLineRasterizationFeaturesIMG;
        physicalDeviceRenderPassStripedFeaturesARM.pNext = pNext;
        pNext = &physicalDeviceRenderPassStripedFeaturesARM;
        physicalDeviceFeatures2KHR.pNext = pNext;

    }


    VkPhysicalDeviceFeatures2KHR requiredFeaturesChain{VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_FEATURES_2_KHR, nullptr};
    VkBaseOutStructure* current = nullptr;

    void ApplyRobustness(const VpDeviceCreateInfo* pCreateInfo) {
#ifdef VK_VERSION_1_1
        VkPhysicalDeviceFeatures2KHR* pFeatures2 = static_cast<VkPhysicalDeviceFeatures2KHR*>(
            vpGetStructure(&this->requiredFeaturesChain, VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_FEATURES_2_KHR));
        if (pFeatures2 != nullptr && (pCreateInfo->flags & VP_DEVICE_CREATE_DISABLE_ROBUST_BUFFER_ACCESS_BIT)) {
            pFeatures2->features.robustBufferAccess = VK_FALSE;
        }
#endif

#ifdef VK_EXT_robustness2
        VkPhysicalDeviceRobustness2FeaturesEXT* pRobustness2FeaturesEXT = static_cast<VkPhysicalDeviceRobustness2FeaturesEXT*>(
            vpGetStructure(&this->requiredFeaturesChain, VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_ROBUSTNESS_2_FEATURES_EXT));
        if (pRobustness2FeaturesEXT != nullptr) {
            if (pCreateInfo->flags & VP_DEVICE_CREATE_DISABLE_ROBUST_BUFFER_ACCESS_BIT) {
                pRobustness2FeaturesEXT->robustBufferAccess2 = VK_FALSE;
            }
            if (pCreateInfo->flags & VP_DEVICE_CREATE_DISABLE_ROBUST_IMAGE_ACCESS_BIT) {
                pRobustness2FeaturesEXT->robustImageAccess2 = VK_FALSE;
            }
        }
#endif
#ifdef VK_EXT_image_robustness
        VkPhysicalDeviceImageRobustnessFeaturesEXT* pImageRobustnessFeaturesEXT =
            static_cast<VkPhysicalDeviceImageRobustnessFeaturesEXT*>(vpGetStructure(
                &this->requiredFeaturesChain, VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_IMAGE_ROBUSTNESS_FEATURES_EXT));
        if (pImageRobustnessFeaturesEXT != nullptr && (pCreateInfo->flags & VP_DEVICE_CREATE_DISABLE_ROBUST_IMAGE_ACCESS_BIT)) {
            pImageRobustnessFeaturesEXT->robustImageAccess = VK_FALSE;
        }
#endif
#ifdef VK_VERSION_1_3
        VkPhysicalDeviceVulkan13Features* pVulkan13Features = static_cast<VkPhysicalDeviceVulkan13Features*>(
            vpGetStructure(&this->requiredFeaturesChain, VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_1_3_FEATURES));
        if (pVulkan13Features != nullptr && (pCreateInfo->flags & VP_DEVICE_CREATE_DISABLE_ROBUST_IMAGE_ACCESS_BIT)) {
            pVulkan13Features->robustImageAccess = VK_FALSE;
        }
#endif
    }

    void ApplyFeatures(const VpDeviceCreateInfo* pCreateInfo) {
        const std::size_t offset = sizeof(VkBaseOutStructure);
        const VkBaseOutStructure* q = reinterpret_cast<const VkBaseOutStructure*>(pCreateInfo->pCreateInfo->pNext);
        while (q) {
            std::size_t count = this->structureSize[q->sType];
            for (std::size_t i = 0, n = count; i < n; ++i) {
                const VkBaseOutStructure* pInputStruct = reinterpret_cast<const VkBaseOutStructure*>(q);
                VkBaseOutStructure* pOutputStruct = reinterpret_cast<VkBaseOutStructure*>(detail::vpGetStructure(&this->requiredFeaturesChain, q->sType));
                const uint8_t* pInputData = reinterpret_cast<const uint8_t*>(pInputStruct) + offset;
                uint8_t* pOutputData = reinterpret_cast<uint8_t*>(pOutputStruct) + offset;
                const VkBool32* input = reinterpret_cast<const VkBool32*>(pInputData);
                VkBool32* output = reinterpret_cast<VkBool32*>(pOutputData);

                output[i] = (output[i] == VK_TRUE || input[i] == VK_TRUE) ? VK_TRUE : VK_FALSE;
            }
            q = q->pNext;
        }

        this->ApplyRobustness(pCreateInfo);
    }

    void PushBack(VkBaseOutStructure* found) { 
        VkBaseOutStructure* last = reinterpret_cast<VkBaseOutStructure*>(&requiredFeaturesChain);
        while (last->pNext != nullptr) {
            last = last->pNext;
        }
        last->pNext = found;
    }

    void Build(const std::vector<VkStructureType>& requiredList) {
        for (std::size_t i = 0, n = requiredList.size(); i < n; ++i) {
            const VkStructureType sType = requiredList[i];
            if (sType == VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_FEATURES_2_KHR) {
                continue;
            }

            VkBaseOutStructure* found = vpExtractStructure(&physicalDeviceFeatures2KHR, sType);
            if (found == nullptr) {
                continue;
            }

            PushBack(found);
        }
    }
}; // struct FeaturesChain

VPAPI_ATTR const VpProfileDesc* vpGetProfileDesc(const char profileName[VP_MAX_PROFILE_NAME_SIZE]) {
    for (uint32_t i = 0; i < profileCount; ++i) {
        if (strncmp(profiles[i].props.profileName, profileName, VP_MAX_PROFILE_NAME_SIZE) == 0) return &profiles[i];
    }
    return nullptr;
}

VPAPI_ATTR std::vector<VpProfileProperties> GatherProfiles(const VpProfileProperties& profile, const char* pBlockName = nullptr) {
    std::vector<VpProfileProperties> profiles;

    if (pBlockName == nullptr) {
        const detail::VpProfileDesc* profile_desc = detail::vpGetProfileDesc(profile.profileName);
        if (profile_desc != nullptr) {
            for (uint32_t profile_index = 0; profile_index < profile_desc->requiredProfileCount; ++profile_index) {
                profiles.push_back(profile_desc->pRequiredProfiles[profile_index]);
            }
        }
    }

    profiles.push_back(profile);

    return profiles;
}

VPAPI_ATTR bool vpCheckVersion(uint32_t actual, uint32_t expected) {
    uint32_t actualMajor = VK_API_VERSION_MAJOR(actual);
    uint32_t actualMinor = VK_API_VERSION_MINOR(actual);
    uint32_t expectedMajor = VK_API_VERSION_MAJOR(expected);
    uint32_t expectedMinor = VK_API_VERSION_MINOR(expected);
    return actualMajor > expectedMajor || (actualMajor == expectedMajor && actualMinor >= expectedMinor);
}

VPAPI_ATTR bool HasExtension(const std::vector<VkExtensionProperties>& list, const VkExtensionProperties& element) {
    for (std::size_t i = 0, n = list.size(); i < n; ++i) {
        if (strcmp(list[i].extensionName, element.extensionName) == 0) {
            return true;
        }
    }

    return false;
}

VPAPI_ATTR bool CheckExtension(const VkExtensionProperties* supportedProperties, size_t supportedSize, const char *requestedExtension) {
    bool found = false;
    for (size_t i = 0, n = supportedSize; i < n; ++i) {
        if (strcmp(supportedProperties[i].extensionName, requestedExtension) == 0) {
            found = true;
            break;
            // Drivers don't actually update their spec version, so we cannot rely on this
            // if (supportedProperties[i].specVersion >= expectedVersion) found = true;
        }
    }
    return found;
}

VPAPI_ATTR bool CheckExtension(const std::vector<const char*>& extensions, const char* extension) {
    for (const char* c : extensions) {
        if (strcmp(c, extension) == 0) {
            return true;
        }
    }
    return false;
}

VPAPI_ATTR void GetExtensions(uint32_t extensionCount, const VkExtensionProperties *pExtensions, std::vector<const char *> &extensions) {
    for (uint32_t i = 0; i < extensionCount; ++i) {
        if (CheckExtension(extensions, pExtensions[i].extensionName)) {
            continue;
        }
        extensions.push_back(pExtensions[i].extensionName);
    }
}

VPAPI_ATTR std::vector<VpBlockProperties> GatherBlocks(
    uint32_t enabledFullProfileCount, const VpProfileProperties* pEnabledFullProfiles,
    uint32_t enabledProfileBlockCount, const VpBlockProperties* pEnabledProfileBlocks) {
    std::vector<VpBlockProperties> results;

    for (std::size_t i = 0; i < enabledFullProfileCount; ++i) {
        const std::vector<VpProfileProperties>& profiles = GatherProfiles(pEnabledFullProfiles[i]);

        for (std::size_t j = 0; j < profiles.size(); ++j) {
            VpBlockProperties block{profiles[j], 0, ""};
            results.push_back(block);
        }
    }

    for (std::size_t i = 0; i < enabledProfileBlockCount; ++i) {
        results.push_back(pEnabledProfileBlocks[i]);
    }

    return results;
}

VPAPI_ATTR VkResult vpGetInstanceProfileSupportSingleProfile(
    uint32_t api_version, const std::vector<VkExtensionProperties>& supported_extensions,
    const VpProfileProperties* pProfile, VkBool32* pSupported, std::vector<VpBlockProperties>& supportedBlocks, std::vector<VpBlockProperties>& unsupportedBlocks) {
    assert(pProfile != nullptr);

    const detail::VpProfileDesc* pProfileDesc = vpGetProfileDesc(pProfile->profileName);
    if (pProfileDesc == nullptr) {
        *pSupported = VK_FALSE;
        return VK_ERROR_UNKNOWN;
    }

    VpBlockProperties block{*pProfile, api_version};

    if (pProfileDesc->props.specVersion < pProfile->specVersion) {
        *pSupported = VK_FALSE;
        unsupportedBlocks.push_back(block);
    }

    // Required API version is built in root profile, not need to check dependent profile API versions
    if (api_version != 0) {
        if (!vpCheckVersion(api_version, pProfileDesc->minApiVersion)) {
            const uint32_t version_min_major = VK_API_VERSION_MAJOR(pProfileDesc->minApiVersion);
            const uint32_t version_min_minor = VK_API_VERSION_MINOR(pProfileDesc->minApiVersion);
            const uint32_t version_min_patch = VK_API_VERSION_PATCH(pProfileDesc->minApiVersion);

            const uint32_t version_major = VK_API_VERSION_MAJOR(api_version);
            const uint32_t version_minor = VK_API_VERSION_MINOR(api_version);
            const uint32_t version_patch = VK_API_VERSION_PATCH(api_version);

            
            *pSupported = VK_FALSE;
            unsupportedBlocks.push_back(block);
        }
    }

    for (uint32_t capability_index = 0; capability_index < pProfileDesc->requiredCapabilityCount; ++capability_index) {
        const VpCapabilitiesDesc& capabilities_desc = pProfileDesc->pRequiredCapabilities[capability_index];

        VkBool32 supported_capabilities = VK_FALSE;
        for (uint32_t variant_index = 0; variant_index < capabilities_desc.variantCount; ++variant_index) {
            const VpVariantDesc& variant_desc = capabilities_desc.pVariants[variant_index];

            VkBool32 supported_variant = VK_TRUE;
            for (uint32_t i = 0; i < variant_desc.instanceExtensionCount; ++i) {
                if (!detail::CheckExtension(supported_extensions.data(), supported_extensions.size(),
                                              variant_desc.pInstanceExtensions[i].extensionName)) {
                    supported_variant = VK_FALSE;
                    memcpy(block.blockName, variant_desc.blockName, VP_MAX_PROFILE_NAME_SIZE * sizeof(char));
                    unsupportedBlocks.push_back(block);
                }
            }

            if (supported_variant == VK_TRUE) {
                supported_capabilities = VK_TRUE;
                memcpy(block.blockName, variant_desc.blockName, VP_MAX_PROFILE_NAME_SIZE * sizeof(char));
                supportedBlocks.push_back(block);
            }
        }

        if (supported_capabilities == VK_FALSE) {
            *pSupported = VK_FALSE;
            return VK_SUCCESS;
        }
    }

    return VK_SUCCESS;
}

enum structure_type {
    STRUCTURE_FEATURE = 0,
    STRUCTURE_PROPERTY,
    STRUCTURE_FORMAT
};

VPAPI_ATTR VkResult vpGetProfileStructureTypes(const VpProfileProperties *pProfile, const char* pBlockName, structure_type type, uint32_t *pStructureTypeCount, VkStructureType *pStructureTypes) {
    VkResult result = pBlockName == nullptr ? VK_SUCCESS : VK_INCOMPLETE;

    std::vector<VkStructureType> results;

    const std::vector<VpProfileProperties>& profiles = detail::GatherProfiles(*pProfile);

    for (std::size_t profile_index = 0, profile_count = profiles.size(); profile_index < profile_count; ++profile_index) {
        const detail::VpProfileDesc* profile_desc = detail::vpGetProfileDesc(profiles[profile_index].profileName);
        if (profile_desc == nullptr) return VK_ERROR_UNKNOWN;

        for (uint32_t capability_index = 0; capability_index < profile_desc->requiredCapabilityCount; ++capability_index) {
            const detail::VpCapabilitiesDesc& capabilities = profile_desc->pRequiredCapabilities[capability_index];

            for (uint32_t variant_index = 0; variant_index < capabilities.variantCount; ++variant_index) {
                const detail::VpVariantDesc& variant = capabilities.pVariants[variant_index];
                if (pBlockName != nullptr) {
                    if (strcmp(variant.blockName, pBlockName) != 0) {
                        continue;
                    }
                    result = VK_SUCCESS;
                }

                uint32_t count = 0;
                const VkStructureType* data = nullptr;

                switch (type) {
                    default:
                    case STRUCTURE_FEATURE:
                        count = variant.featureStructTypeCount;
                        data = variant.pFeatureStructTypes;
                        break;
                    case STRUCTURE_PROPERTY:
                        count = variant.propertyStructTypeCount;
                        data = variant.pPropertyStructTypes;
                        break;
                    case STRUCTURE_FORMAT:
                        count = variant.formatStructTypeCount;
                        data = variant.pFormatStructTypes;
                        break;
                }

                for (uint32_t i = 0; i < count; ++i) {
                    const VkStructureType type = data[i];
                    if (std::find(results.begin(), results.end(), type) == std::end(results)) {
                        results.push_back(type);
                    }
                }
            }
        }
    }

    const uint32_t count = static_cast<uint32_t>(results.size());
    std::sort(results.begin(), results.end());

    if (pStructureTypes == nullptr) {
        *pStructureTypeCount = count;
    } else {
        if (*pStructureTypeCount < count) {
            result = VK_INCOMPLETE;
        } else {
            *pStructureTypeCount = count;
        }

        if (*pStructureTypeCount > 0) {
            memcpy(pStructureTypes, &results[0], *pStructureTypeCount * sizeof(VkStructureType));
        }
    }

    return result;
}

enum ExtensionType {
    EXTENSION_INSTANCE,
    EXTENSION_DEVICE,
};

VPAPI_ATTR VkResult vpGetProfileExtensionProperties(const VpProfileProperties *pProfile, const char* pBlockName, ExtensionType type, uint32_t *pPropertyCount, VkExtensionProperties *pProperties) {
    VkResult result = pBlockName == nullptr ? VK_SUCCESS : VK_INCOMPLETE;

    std::vector<VkExtensionProperties> results;

    const std::vector<VpProfileProperties>& profiles = detail::GatherProfiles(*pProfile, pBlockName);

    for (std::size_t profile_index = 0, profile_count = profiles.size(); profile_index < profile_count; ++profile_index) {
        const detail::VpProfileDesc* profile_desc = detail::vpGetProfileDesc(profiles[profile_index].profileName);
        if (profile_desc == nullptr) return VK_ERROR_UNKNOWN;

        for (uint32_t capability_index = 0; capability_index < profile_desc->requiredCapabilityCount; ++capability_index) {
            const detail::VpCapabilitiesDesc& capabilities = profile_desc->pRequiredCapabilities[capability_index];

            for (uint32_t variant_index = 0; variant_index < capabilities.variantCount; ++variant_index) {
                const detail::VpVariantDesc& variant = capabilities.pVariants[variant_index];
                if (pBlockName != nullptr) {
                    if (strcmp(variant.blockName, pBlockName) != 0) {
                        continue;
                    }
                    result = VK_SUCCESS;
                }

                switch (type) {
                    default:
                    case EXTENSION_INSTANCE:
                        for (uint32_t i = 0; i < variant.instanceExtensionCount; ++i) {
                            if (detail::HasExtension(results, variant.pInstanceExtensions[i])) {
                                continue;
                            }
                            results.push_back(variant.pInstanceExtensions[i]);
                        }
                        break;
                    case EXTENSION_DEVICE:
                        for (uint32_t i = 0; i < variant.deviceExtensionCount; ++i) {
                            if (detail::HasExtension(results, variant.pDeviceExtensions[i])) {
                                continue;
                            }
                            results.push_back(variant.pDeviceExtensions[i]);
                        }
                        break;
                }
            }
        }
    }

    const uint32_t count = static_cast<uint32_t>(results.size());

    if (pProperties == nullptr) {
        *pPropertyCount = count;
    } else {
        if (*pPropertyCount < count) {
            result = VK_INCOMPLETE;
        } else {
            *pPropertyCount = count;
        }
        if (*pPropertyCount > 0) {
            memcpy(pProperties, &results[0], *pPropertyCount * sizeof(VkExtensionProperties));
        }
    }

    return result;
}

} // namespace detail

VPAPI_ATTR VkResult vpGetProfiles(uint32_t *pPropertyCount, VpProfileProperties *pProperties) {
    VkResult result = VK_SUCCESS;

    if (pProperties == nullptr) {
        *pPropertyCount = detail::profileCount;
    } else {
        if (*pPropertyCount < detail::profileCount) {
            result = VK_INCOMPLETE;
        } else {
            *pPropertyCount = detail::profileCount;
        }
        for (uint32_t i = 0; i < *pPropertyCount; ++i) {
            pProperties[i] = detail::profiles[i].props;
        }
    }
    return result;
}

VPAPI_ATTR VkResult vpGetProfileRequiredProfiles(const VpProfileProperties *pProfile, uint32_t *pPropertyCount, VpProfileProperties *pProperties) {
    VkResult result = VK_SUCCESS;

    const detail::VpProfileDesc* pDesc = detail::vpGetProfileDesc(pProfile->profileName);
    if (pDesc == nullptr) return VK_ERROR_UNKNOWN;

    if (pProperties == nullptr) {
        *pPropertyCount = pDesc->requiredProfileCount;
    } else {
        if (*pPropertyCount < pDesc->requiredProfileCount) {
            result = VK_INCOMPLETE;
        } else {
            *pPropertyCount = pDesc->requiredProfileCount;
        }
        for (uint32_t i = 0; i < *pPropertyCount; ++i) {
            pProperties[i] = pDesc->pRequiredProfiles[i];
        }
    }
    return result;
}

VPAPI_ATTR uint32_t vpGetProfileAPIVersion(const VpProfileProperties* pProfile) {
    const std::vector<VpProfileProperties>& profiles = detail::GatherProfiles(*pProfile, nullptr);

    uint32_t major = 0;
    uint32_t minor = 0;
    uint32_t patch = 0;

    for (std::size_t i = 0, n = profiles.size(); i < n; ++i) {
        const detail::VpProfileDesc* pDesc = detail::vpGetProfileDesc(profiles[i].profileName);
        if (pDesc == nullptr) return 0;

        major = std::max<uint32_t>(major, VK_API_VERSION_MAJOR(pDesc->minApiVersion));
        minor = std::max<uint32_t>(minor, VK_API_VERSION_MINOR(pDesc->minApiVersion));
        patch = std::max<uint32_t>(patch, VK_API_VERSION_PATCH(pDesc->minApiVersion));
    }

    return VK_MAKE_API_VERSION(0, major, minor, patch);
}

VPAPI_ATTR VkResult vpGetProfileFallbacks(const VpProfileProperties *pProfile, uint32_t *pPropertyCount, VpProfileProperties *pProperties) {
    VkResult result = VK_SUCCESS;

    const detail::VpProfileDesc* pDesc = detail::vpGetProfileDesc(pProfile->profileName);
    if (pDesc == nullptr) return VK_ERROR_UNKNOWN;

    if (pProperties == nullptr) {
        *pPropertyCount = pDesc->fallbackCount;
    } else {
        if (*pPropertyCount < pDesc->fallbackCount) {
            result = VK_INCOMPLETE;
        } else {
            *pPropertyCount = pDesc->fallbackCount;
        }
        for (uint32_t i = 0; i < *pPropertyCount; ++i) {
            pProperties[i] = pDesc->pFallbacks[i];
        }
    }
    return result;
}

VPAPI_ATTR VkResult vpHasMultipleVariantsProfile(const VpProfileProperties *pProfile, VkBool32 *pHasMultipleVariants) {
    const std::vector<VpProfileProperties>& profiles = detail::GatherProfiles(*pProfile, nullptr);

    for (std::size_t profile_index = 0, profile_count = profiles.size(); profile_index < profile_count; ++profile_index) {
        const detail::VpProfileDesc* pDesc = detail::vpGetProfileDesc(profiles[profile_index].profileName);
        if (pDesc == nullptr) return VK_ERROR_UNKNOWN;

        for (uint32_t capabilities_index = 0, n = pDesc->requiredCapabilityCount; capabilities_index < n; ++capabilities_index) {
            if (pDesc->pRequiredCapabilities[capabilities_index].variantCount > 1) {
                *pHasMultipleVariants = VK_TRUE;
                return VK_SUCCESS;
            }
        }
    }

    *pHasMultipleVariants = VK_FALSE;
    return VK_SUCCESS;
}

VPAPI_ATTR VkResult vpGetInstanceProfileVariantsSupport(const char *pLayerName, const VpProfileProperties *pProfile, VkBool32 *pSupported, uint32_t *pPropertyCount, VpBlockProperties* pProperties) {
    VkResult result = VK_SUCCESS;

    uint32_t api_version = VK_MAKE_API_VERSION(0, 1, 0, 0);
    static PFN_vkEnumerateInstanceVersion pfnEnumerateInstanceVersion =
        (PFN_vkEnumerateInstanceVersion)vkGetInstanceProcAddr(VK_NULL_HANDLE, "vkEnumerateInstanceVersion");
    if (pfnEnumerateInstanceVersion != nullptr) {
        result = pfnEnumerateInstanceVersion(&api_version);
        if (result != VK_SUCCESS) {
            *pSupported = VK_FALSE;
            return result;
        }
    }

    uint32_t supported_instance_extension_count = 0;
    result = vkEnumerateInstanceExtensionProperties(pLayerName, &supported_instance_extension_count, nullptr);
    if (result != VK_SUCCESS) {
        *pSupported = VK_FALSE;
        return result;
    }
    std::vector<VkExtensionProperties> supported_instance_extensions;
    if (supported_instance_extension_count > 0) {
        supported_instance_extensions.resize(supported_instance_extension_count);
    }
    result = vkEnumerateInstanceExtensionProperties(pLayerName, &supported_instance_extension_count, supported_instance_extensions.data());
    if (result != VK_SUCCESS) {
        *pSupported = VK_FALSE;
        return result;
    }

    VkBool32 supported = VK_TRUE;

    // We require VK_KHR_get_physical_device_properties2 if we are on Vulkan 1.0
    if (api_version < VK_API_VERSION_1_1) {
        bool foundGPDP2 = false;
        for (size_t i = 0; i < supported_instance_extensions.size(); ++i) {
            if (strcmp(supported_instance_extensions[i].extensionName, VK_KHR_GET_PHYSICAL_DEVICE_PROPERTIES_2_EXTENSION_NAME) == 0) {
                foundGPDP2 = true;
                break;
            }
        }
        if (!foundGPDP2) {
            supported = VK_FALSE;
        }
    }

    const detail::VpProfileDesc* pProfileDesc = detail::vpGetProfileDesc(pProfile->profileName);
    if (pProfileDesc == nullptr) return VK_ERROR_UNKNOWN;

    std::vector<VpBlockProperties> supported_blocks;
    std::vector<VpBlockProperties> unsupported_blocks;

    result = detail::vpGetInstanceProfileSupportSingleProfile(api_version, supported_instance_extensions, pProfile, &supported, supported_blocks, unsupported_blocks);
    if (result != VK_SUCCESS) {
        *pSupported = supported;
        return result;
    }
 
    for (std::size_t i = 0; i < pProfileDesc->requiredProfileCount; ++i) {
        result = detail::vpGetInstanceProfileSupportSingleProfile(0, supported_instance_extensions, &pProfileDesc->pRequiredProfiles[i], &supported, supported_blocks, unsupported_blocks);
        if (result != VK_SUCCESS) {
            *pSupported = supported;
            return result;
        }
    }

    const std::vector<VpBlockProperties>& blocks = supported ? supported_blocks : unsupported_blocks;

    if (pProperties == nullptr) {
        *pPropertyCount = static_cast<uint32_t>(blocks.size());
    } else {
        if (*pPropertyCount < static_cast<uint32_t>(blocks.size())) {
            result = VK_INCOMPLETE;
        } else {
            *pPropertyCount = static_cast<uint32_t>(blocks.size());
        }
        for (uint32_t i = 0, n = static_cast<uint32_t>(blocks.size()); i < n; ++i) {
            pProperties[i] = blocks[i];
        }
    }

    *pSupported = supported;
    return result;
}

VPAPI_ATTR VkResult vpGetInstanceProfileSupport(const char *pLayerName, const VpProfileProperties *pProfile, VkBool32 *pSupported) {
    uint32_t count = 0;
    return vpGetInstanceProfileVariantsSupport(pLayerName, pProfile, pSupported, &count, nullptr);
}


VPAPI_ATTR VkResult vpCreateInstance(const VpInstanceCreateInfo *pCreateInfo,
                                     const VkAllocationCallbacks *pAllocator, VkInstance *pInstance) {
    if (pCreateInfo == nullptr || pInstance == nullptr) {
        return vkCreateInstance(pCreateInfo == nullptr ? nullptr : pCreateInfo->pCreateInfo, pAllocator, pInstance);
    }

    const std::vector<VpBlockProperties>& blocks = detail::GatherBlocks(
        pCreateInfo->enabledFullProfileCount, pCreateInfo->pEnabledFullProfiles,
        pCreateInfo->enabledProfileBlockCount, pCreateInfo->pEnabledProfileBlocks);

    std::vector<const char*> extensions;
    for (std::uint32_t i = 0, n = pCreateInfo->pCreateInfo->enabledExtensionCount; i < n; ++i) {
        extensions.push_back(pCreateInfo->pCreateInfo->ppEnabledExtensionNames[i]);
    }

    for (std::size_t i = 0, n = blocks.size(); i < n; ++i) {
        const detail::VpProfileDesc* pProfileDesc = detail::vpGetProfileDesc(blocks[i].profiles.profileName);
        if (pProfileDesc == nullptr) return VK_ERROR_UNKNOWN;

        for (std::size_t j = 0, p = pProfileDesc->requiredCapabilityCount; j < p; ++j) {
            const detail::VpCapabilitiesDesc* pCapsDesc = &pProfileDesc->pRequiredCapabilities[j];

            for (std::size_t v = 0, q = pCapsDesc->variantCount; v < q; ++v) {
                const detail::VpVariantDesc* variant = &pCapsDesc->pVariants[v];

                if (strcmp(blocks[i].blockName, "") != 0) {
                    if (strcmp(variant->blockName, blocks[i].blockName) != 0) {
                        continue;
                    }
                }

                detail::GetExtensions(variant->instanceExtensionCount, variant->pInstanceExtensions, extensions);
            }
        }
    }

    VkApplicationInfo appInfo{VK_STRUCTURE_TYPE_APPLICATION_INFO};
    if (pCreateInfo->pCreateInfo->pApplicationInfo != nullptr) {
        appInfo = *pCreateInfo->pCreateInfo->pApplicationInfo;
    } else if (!blocks.empty()) {
        appInfo.apiVersion = vpGetProfileAPIVersion(&blocks[0].profiles);
    }

    VkInstanceCreateInfo createInfo = *pCreateInfo->pCreateInfo;
    createInfo.pApplicationInfo = &appInfo;

    // Need to include VK_KHR_get_physical_device_properties2 if we are on Vulkan 1.0
    if (createInfo.pApplicationInfo->apiVersion < VK_API_VERSION_1_1) {
        bool foundGPDP2 = false;
        for (size_t i = 0; i < extensions.size(); ++i) {
            if (strcmp(extensions[i], VK_KHR_GET_PHYSICAL_DEVICE_PROPERTIES_2_EXTENSION_NAME) == 0) {
                foundGPDP2 = true;
                break;
            }
        }
        if (!foundGPDP2) {
            extensions.push_back(VK_KHR_GET_PHYSICAL_DEVICE_PROPERTIES_2_EXTENSION_NAME);
        }
    }

#ifdef __APPLE__
    bool has_portability_ext = false;
    for (std::size_t i = 0, n = extensions.size(); i < n; ++i) {
        if (strcmp(extensions[i], VK_KHR_PORTABILITY_ENUMERATION_EXTENSION_NAME) == 0) {
            has_portability_ext = true;
            break;
        }
    }

    if (!has_portability_ext) {
        extensions.push_back(VK_KHR_PORTABILITY_ENUMERATION_EXTENSION_NAME);
    }

    createInfo.flags |= VK_INSTANCE_CREATE_ENUMERATE_PORTABILITY_BIT_KHR;
#endif

    if (!extensions.empty()) {
        createInfo.enabledExtensionCount = static_cast<uint32_t>(extensions.size());
        createInfo.ppEnabledExtensionNames = extensions.data();
    }

    return vkCreateInstance(&createInfo, pAllocator, pInstance);
}

VPAPI_ATTR VkResult vpGetPhysicalDeviceProfileVariantsSupport(VkInstance instance, VkPhysicalDevice physicalDevice,
                                                              const VpProfileProperties *pProfile, VkBool32 *pSupported, uint32_t *pPropertyCount, VpBlockProperties* pProperties) {
    VkResult result = VK_SUCCESS;

    uint32_t supported_device_extension_count = 0;
    result = vkEnumerateDeviceExtensionProperties(physicalDevice, nullptr, &supported_device_extension_count, nullptr);
    if (result != VK_SUCCESS) {
        return result;
    }
    std::vector<VkExtensionProperties> supported_device_extensions;
    if (supported_device_extension_count > 0) {
        supported_device_extensions.resize(supported_device_extension_count);
    }
    result = vkEnumerateDeviceExtensionProperties(physicalDevice, nullptr, &supported_device_extension_count, supported_device_extensions.data());
    if (result != VK_SUCCESS) {
        return result;
    }

    // Workaround old loader bug where count could be smaller on the second call to vkEnumerateDeviceExtensionProperties
    if (supported_device_extension_count > 0) {
        supported_device_extensions.resize(supported_device_extension_count);
    }

    const detail::VpProfileDesc* pProfileDesc = detail::vpGetProfileDesc(pProfile->profileName);
    if (pProfileDesc == nullptr) return VK_ERROR_UNKNOWN;

    struct GPDP2EntryPoints {
        PFN_vkGetPhysicalDeviceFeatures2KHR                 pfnGetPhysicalDeviceFeatures2;
        PFN_vkGetPhysicalDeviceProperties2KHR               pfnGetPhysicalDeviceProperties2;
        PFN_vkGetPhysicalDeviceFormatProperties2KHR         pfnGetPhysicalDeviceFormatProperties2;
        PFN_vkGetPhysicalDeviceQueueFamilyProperties2KHR    pfnGetPhysicalDeviceQueueFamilyProperties2;
    };

    std::vector<VpBlockProperties> supported_blocks;
    std::vector<VpBlockProperties> unsupported_blocks;

    struct UserData {
        VkPhysicalDevice physicalDevice;
        std::vector<VpBlockProperties>& supported_blocks;
        std::vector<VpBlockProperties>& unsupported_blocks;
        const detail::VpVariantDesc* variant;
        GPDP2EntryPoints gpdp2;
        uint32_t index;
        uint32_t count;
        detail::PFN_vpStructChainerCb pfnCb;
        bool supported;
    } userData{physicalDevice, supported_blocks, unsupported_blocks};

    // Attempt to load core versions of the GPDP2 entry points
    userData.gpdp2.pfnGetPhysicalDeviceFeatures2 =
        (PFN_vkGetPhysicalDeviceFeatures2KHR)vkGetInstanceProcAddr(instance, "vkGetPhysicalDeviceFeatures2");
    userData.gpdp2.pfnGetPhysicalDeviceProperties2 =
        (PFN_vkGetPhysicalDeviceProperties2KHR)vkGetInstanceProcAddr(instance, "vkGetPhysicalDeviceProperties2");
    userData.gpdp2.pfnGetPhysicalDeviceFormatProperties2 =
        (PFN_vkGetPhysicalDeviceFormatProperties2KHR)vkGetInstanceProcAddr(instance, "vkGetPhysicalDeviceFormatProperties2");
    userData.gpdp2.pfnGetPhysicalDeviceQueueFamilyProperties2 =
        (PFN_vkGetPhysicalDeviceQueueFamilyProperties2KHR)vkGetInstanceProcAddr(instance, "vkGetPhysicalDeviceQueueFamilyProperties2");

    // If not successful, try to load KHR variant
    if (userData.gpdp2.pfnGetPhysicalDeviceFeatures2 == nullptr) {
        userData.gpdp2.pfnGetPhysicalDeviceFeatures2 =
            (PFN_vkGetPhysicalDeviceFeatures2KHR)vkGetInstanceProcAddr(instance, "vkGetPhysicalDeviceFeatures2KHR");
        userData.gpdp2.pfnGetPhysicalDeviceProperties2 =
            (PFN_vkGetPhysicalDeviceProperties2KHR)vkGetInstanceProcAddr(instance, "vkGetPhysicalDeviceProperties2KHR");
        userData.gpdp2.pfnGetPhysicalDeviceFormatProperties2 =
            (PFN_vkGetPhysicalDeviceFormatProperties2KHR)vkGetInstanceProcAddr(instance, "vkGetPhysicalDeviceFormatProperties2KHR");
        userData.gpdp2.pfnGetPhysicalDeviceQueueFamilyProperties2 =
            (PFN_vkGetPhysicalDeviceQueueFamilyProperties2KHR)vkGetInstanceProcAddr(instance, "vkGetPhysicalDeviceQueueFamilyProperties2KHR");
    }

    if (userData.gpdp2.pfnGetPhysicalDeviceFeatures2 == nullptr ||
        userData.gpdp2.pfnGetPhysicalDeviceProperties2 == nullptr ||
        userData.gpdp2.pfnGetPhysicalDeviceFormatProperties2 == nullptr ||
        userData.gpdp2.pfnGetPhysicalDeviceQueueFamilyProperties2 == nullptr) {
        return VK_ERROR_EXTENSION_NOT_PRESENT;
    }


    bool supported = true;

    const std::vector<VpProfileProperties>& profiles = detail::GatherProfiles(*pProfile);

    for (std::size_t i = 0, n = profiles.size(); i < n; ++i) {
        const char* profile_name = profiles[i].profileName;

        const detail::VpProfileDesc* pProfileDesc = detail::vpGetProfileDesc(profile_name);
        if (pProfileDesc == nullptr) return VK_ERROR_UNKNOWN;

        bool supported_profile = true;


        if (pProfileDesc->props.specVersion < pProfile->specVersion) {
            supported_profile = false;
        }

        VpBlockProperties block{profiles[i], pProfileDesc->minApiVersion};

        VkPhysicalDeviceProperties props{};
        vkGetPhysicalDeviceProperties(physicalDevice, &props);
        if (!detail::vpCheckVersion(props.apiVersion, pProfileDesc->minApiVersion)) {
            supported_profile = false;
        }

        for (uint32_t required_capability_index = 0; required_capability_index < pProfileDesc->requiredCapabilityCount; ++required_capability_index) {
            const detail::VpCapabilitiesDesc* required_capabilities = &pProfileDesc->pRequiredCapabilities[required_capability_index];

            bool supported_block = false;

            for (uint32_t variant_index = 0; variant_index < required_capabilities->variantCount; ++variant_index) {
                const detail::VpVariantDesc& variant_desc = required_capabilities->pVariants[variant_index];

                bool supported_variant = true;

                for (uint32_t i = 0; i < variant_desc.deviceExtensionCount; ++i) {
                    const char *requested_extension = variant_desc.pDeviceExtensions[i].extensionName;
                    if (!detail::CheckExtension(supported_device_extensions.data(), supported_device_extensions.size(), requested_extension)) {
                        supported_variant = false;
                    }
                }

                userData.variant = &variant_desc;

                VkPhysicalDeviceFeatures2KHR features{ VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_FEATURES_2_KHR };
                userData.variant->chainers.pfnFeature(
                    static_cast<VkBaseOutStructure*>(static_cast<void*>(&features)), &userData,
                    [](VkBaseOutStructure* p, void* pUser) {
                        UserData* pUserData = static_cast<UserData*>(pUser);
                        pUserData->gpdp2.pfnGetPhysicalDeviceFeatures2(pUserData->physicalDevice,
                                                                        static_cast<VkPhysicalDeviceFeatures2KHR*>(static_cast<void*>(p)));
                        pUserData->supported = true;
                        while (p != nullptr) {
                            if (!pUserData->variant->feature.pfnComparator(p)) {
                                pUserData->supported = false;
                            }
                            p = p->pNext;
                        }
                    }
                );
                if (!userData.supported) {
                    supported_variant = false;
                }

                VkPhysicalDeviceProperties2KHR props{ VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_PROPERTIES_2_KHR };
                userData.variant->chainers.pfnProperty(
                    static_cast<VkBaseOutStructure*>(static_cast<void*>(&props)), &userData,
                    [](VkBaseOutStructure* p, void* pUser) {
                        UserData* pUserData = static_cast<UserData*>(pUser);
                        pUserData->gpdp2.pfnGetPhysicalDeviceProperties2(pUserData->physicalDevice,
                                                                         static_cast<VkPhysicalDeviceProperties2KHR*>(static_cast<void*>(p)));
                        pUserData->supported = true;
                        while (p != nullptr) {
                            if (!pUserData->variant->property.pfnComparator(p)) {
                                pUserData->supported = false;
                            }
                            p = p->pNext;
                        }
                    }
                );
                if (!userData.supported) {
                    supported_variant = false;
                }

                for (uint32_t i = 0; i < userData.variant->formatCount && supported_variant; ++i) {
                    userData.index = i;
                    VkFormatProperties2KHR props{ VK_STRUCTURE_TYPE_FORMAT_PROPERTIES_2_KHR };
                    userData.variant->chainers.pfnFormat(
                        static_cast<VkBaseOutStructure*>(static_cast<void*>(&props)), &userData,
                        [](VkBaseOutStructure* p, void* pUser) {
                            UserData* pUserData = static_cast<UserData*>(pUser);
                            pUserData->gpdp2.pfnGetPhysicalDeviceFormatProperties2(pUserData->physicalDevice, pUserData->variant->pFormats[pUserData->index].format,
                                                                                   static_cast<VkFormatProperties2KHR*>(static_cast<void*>(p)));
                            pUserData->supported = true;
                            while (p != nullptr) {
                                if (!pUserData->variant->pFormats[pUserData->index].pfnComparator(p)) {
                                    pUserData->supported = false;
                                }
                                p = p->pNext;
                            }
                        }
                    );
                    if (!userData.supported) {
                        supported_variant = false;
                    }
                }

                memcpy(block.blockName, variant_desc.blockName, VP_MAX_PROFILE_NAME_SIZE * sizeof(char));
                if (supported_variant) {
                    supported_blocks.push_back(block);
                    supported_block = true;
                    break;
                } else {
                    unsupported_blocks.push_back(block);
                }
            }

            if (!supported_block) {
                supported_profile = false;
            }
        }

        if (!supported_profile) {
            supported = false;
        }
    }

    const std::vector<VpBlockProperties>& blocks = supported ? supported_blocks : unsupported_blocks;

    if (pProperties == nullptr) {
        *pPropertyCount = static_cast<uint32_t>(blocks.size());
    } else {
        if (*pPropertyCount < static_cast<uint32_t>(blocks.size())) {
            result = VK_INCOMPLETE;
        } else {
            *pPropertyCount = static_cast<uint32_t>(blocks.size());
        }
        for (uint32_t i = 0, n = static_cast<uint32_t>(blocks.size()); i < n; ++i) {
            pProperties[i] = blocks[i];
        }
    }

    *pSupported = supported ? VK_TRUE : VK_FALSE;
    return VK_SUCCESS;
}

VPAPI_ATTR VkResult vpGetPhysicalDeviceProfileSupport(VkInstance instance, VkPhysicalDevice physicalDevice,
                                                      const VpProfileProperties *pProfile, VkBool32 *pSupported) {
    uint32_t count = 0;
    return vpGetPhysicalDeviceProfileVariantsSupport(instance, physicalDevice, pProfile, pSupported, &count, nullptr);
}

VPAPI_ATTR VkResult vpCreateDevice(VkPhysicalDevice physicalDevice, const VpDeviceCreateInfo *pCreateInfo,
                                   const VkAllocationCallbacks *pAllocator, VkDevice *pDevice) {
    if (physicalDevice == VK_NULL_HANDLE || pCreateInfo == nullptr || pDevice == nullptr) {
        return vkCreateDevice(physicalDevice, pCreateInfo == nullptr ? nullptr : pCreateInfo->pCreateInfo, pAllocator, pDevice);
    }

    const std::vector<VpBlockProperties>& blocks = detail::GatherBlocks(
        pCreateInfo->enabledFullProfileCount, pCreateInfo->pEnabledFullProfiles,
        pCreateInfo->enabledProfileBlockCount, pCreateInfo->pEnabledProfileBlocks);

    std::unique_ptr<detail::FeaturesChain> chain = std::make_unique<detail::FeaturesChain>();
    std::vector<VkStructureType> structureTypes;

    std::vector<const char*> extensions;
    for (std::uint32_t i = 0, n = pCreateInfo->pCreateInfo->enabledExtensionCount; i < n; ++i) {
        extensions.push_back(pCreateInfo->pCreateInfo->ppEnabledExtensionNames[i]);
    }

    for (std::size_t i = 0, n = blocks.size(); i < n; ++i) {
        const detail::VpProfileDesc* pProfileDesc = detail::vpGetProfileDesc(blocks[i].profiles.profileName);
        if (pProfileDesc == nullptr) return VK_ERROR_UNKNOWN;

        for (std::size_t j = 0, p = pProfileDesc->requiredCapabilityCount; j < p; ++j) {
            const detail::VpCapabilitiesDesc* pCapsDesc = &pProfileDesc->pRequiredCapabilities[j];

            for (std::size_t v = 0, q = pCapsDesc->variantCount; v < q; ++v) {
                const detail::VpVariantDesc* variant = &pCapsDesc->pVariants[v];

                if (strcmp(blocks[i].blockName, "") != 0) {
                    if (strcmp(variant->blockName, blocks[i].blockName) != 0) {
                        continue;
                    }
                }

                for (uint32_t t = 0; t < variant->featureStructTypeCount; ++t) {
                    const VkStructureType type = variant->pFeatureStructTypes[t];
                    if (std::find(structureTypes.begin(), structureTypes.end(), type) == std::end(structureTypes)) {
                        structureTypes.push_back(type);
                    }
                }

                detail::GetExtensions(variant->deviceExtensionCount, variant->pDeviceExtensions, extensions);
            }
        }
    }

    VkBaseOutStructure* pNext = static_cast<VkBaseOutStructure*>(const_cast<void*>(pCreateInfo->pCreateInfo->pNext));
    detail::GatherStructureTypes(structureTypes, pNext);

    chain->Build(structureTypes);

    VkPhysicalDeviceFeatures2KHR* pFeatures = &chain->requiredFeaturesChain;
    if (pCreateInfo->pCreateInfo->pEnabledFeatures) {
        pFeatures->features = *pCreateInfo->pCreateInfo->pEnabledFeatures;
    }

    for (std::size_t i = 0, n = blocks.size(); i < n; ++i) {
        const detail::VpProfileDesc* pProfileDesc = detail::vpGetProfileDesc(blocks[i].profiles.profileName);
        if (pProfileDesc == nullptr) return VK_ERROR_UNKNOWN;

        for (std::size_t j = 0, p = pProfileDesc->requiredCapabilityCount; j < p; ++j) {
            const detail::VpCapabilitiesDesc* pCapsDesc = &pProfileDesc->pRequiredCapabilities[j];

            for (std::size_t v = 0, q = pCapsDesc->variantCount; v < q; ++v) {
                const detail::VpVariantDesc* variant = &pCapsDesc->pVariants[v];

                VkBaseOutStructure* p = reinterpret_cast<VkBaseOutStructure*>(pFeatures);
                if (variant->feature.pfnFiller != nullptr) {
                    while (p != nullptr) {
                        variant->feature.pfnFiller(p);
                        p = p->pNext;
                    }
                }
            }
        }
    }

    chain->ApplyFeatures(pCreateInfo);

    if (pCreateInfo->flags & VP_DEVICE_CREATE_DISABLE_ROBUST_BUFFER_ACCESS_BIT) {
        pFeatures->features.robustBufferAccess = VK_FALSE;
    }

    VkDeviceCreateInfo createInfo{VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO};
    createInfo.pNext = &chain->requiredFeaturesChain;
    createInfo.queueCreateInfoCount = pCreateInfo->pCreateInfo->queueCreateInfoCount;
    createInfo.pQueueCreateInfos = pCreateInfo->pCreateInfo->pQueueCreateInfos;
    createInfo.enabledExtensionCount = static_cast<uint32_t>(extensions.size());
    createInfo.ppEnabledExtensionNames = extensions.data();

    return vkCreateDevice(physicalDevice, &createInfo, pAllocator, pDevice);
}

VPAPI_ATTR VkResult vpGetProfileInstanceExtensionProperties(const VpProfileProperties *pProfile, const char* pBlockName, uint32_t *pPropertyCount, VkExtensionProperties *pProperties) {
    return detail::vpGetProfileExtensionProperties(pProfile, pBlockName, detail::EXTENSION_INSTANCE, pPropertyCount, pProperties);
}

VPAPI_ATTR VkResult vpGetProfileDeviceExtensionProperties(const VpProfileProperties *pProfile, const char* pBlockName, uint32_t *pPropertyCount, VkExtensionProperties *pProperties) {
    return detail::vpGetProfileExtensionProperties(pProfile, pBlockName, detail::EXTENSION_DEVICE, pPropertyCount, pProperties);
}

VPAPI_ATTR VkResult vpGetProfileFeatures(const VpProfileProperties *pProfile, const char* pBlockName, void *pNext) {
    VkResult result = pBlockName == nullptr ? VK_SUCCESS : VK_INCOMPLETE;

    const std::vector<VpProfileProperties>& profiles = detail::GatherProfiles(*pProfile);

    for (std::size_t profile_index = 0, profile_count = profiles.size(); profile_index < profile_count; ++profile_index) {
        const detail::VpProfileDesc* profile_desc = detail::vpGetProfileDesc(profiles[profile_index].profileName);
        if (profile_desc == nullptr) return VK_ERROR_UNKNOWN;

        for (uint32_t capability_index = 0; capability_index < profile_desc->requiredCapabilityCount; ++capability_index) {
            const detail::VpCapabilitiesDesc& capabilities = profile_desc->pRequiredCapabilities[capability_index];

            for (uint32_t variant_index = 0; variant_index < capabilities.variantCount; ++variant_index) {
                const detail::VpVariantDesc& variant = capabilities.pVariants[variant_index];
                if (pBlockName != nullptr) {
                    if (strcmp(variant.blockName, pBlockName) != 0) {
                        continue;
                    }
                    result = VK_SUCCESS;
                }

                if (variant.feature.pfnFiller == nullptr) continue;

                VkBaseOutStructure* p = static_cast<VkBaseOutStructure*>(pNext);
                while (p != nullptr) {
                    variant.feature.pfnFiller(p);
                    p = p->pNext;
                }
            }
        }
    }

    return result;
}

VPAPI_ATTR VkResult vpGetProfileProperties(const VpProfileProperties *pProfile, const char* pBlockName, void *pNext) {
    VkResult result = pBlockName == nullptr ? VK_SUCCESS : VK_INCOMPLETE;

    VkBool32 multiple_variants = VK_FALSE;
    if (vpHasMultipleVariantsProfile(pProfile, &multiple_variants) == VK_ERROR_UNKNOWN) {
        return VK_ERROR_UNKNOWN;
    }
    if (multiple_variants == VK_TRUE && pBlockName == nullptr) {
        return VK_ERROR_UNKNOWN;
    }

    const std::vector<VpProfileProperties>& profiles = detail::GatherProfiles(*pProfile);

    for (std::size_t profile_index = 0, profile_count = profiles.size(); profile_index < profile_count; ++profile_index) {
        const detail::VpProfileDesc* profile_desc = detail::vpGetProfileDesc(profiles[profile_index].profileName);
        if (profile_desc == nullptr) return VK_ERROR_UNKNOWN;

        for (uint32_t capability_index = 0; capability_index < profile_desc->requiredCapabilityCount; ++capability_index) {
            const detail::VpCapabilitiesDesc& capabilities = profile_desc->pRequiredCapabilities[capability_index];

            for (uint32_t variant_index = 0; variant_index < capabilities.variantCount; ++variant_index) {
                const detail::VpVariantDesc& variant = capabilities.pVariants[variant_index];
                if (pBlockName != nullptr) {
                    if (strcmp(variant.blockName, pBlockName) != 0) {
                        continue;
                    }
                    result = VK_SUCCESS;
                }

                if (variant.property.pfnFiller == nullptr) continue;
                
                VkBaseOutStructure* p = static_cast<VkBaseOutStructure*>(pNext);
                while (p != nullptr) {
                    variant.property.pfnFiller(p);
                    p = p->pNext;
                }
            }
        }
    }

    return result;
}

VPAPI_ATTR VkResult vpGetProfileFormats(const VpProfileProperties *pProfile, const char* pBlockName, uint32_t *pFormatCount, VkFormat *pFormats) {
    VkResult result = pBlockName == nullptr ? VK_SUCCESS : VK_INCOMPLETE;

    std::vector<VkFormat> results;

    const std::vector<VpProfileProperties>& profiles = detail::GatherProfiles(*pProfile);

    for (std::size_t profile_index = 0, profile_count = profiles.size(); profile_index < profile_count; ++profile_index) {
        const detail::VpProfileDesc* profile_desc = detail::vpGetProfileDesc(profiles[profile_index].profileName);
        if (profile_desc == nullptr) return VK_ERROR_UNKNOWN;

        for (uint32_t capability_index = 0; capability_index < profile_desc->requiredCapabilityCount; ++capability_index) {
            const detail::VpCapabilitiesDesc& capabilities = profile_desc->pRequiredCapabilities[capability_index];

            for (uint32_t variant_index = 0; variant_index < capabilities.variantCount; ++variant_index) {
                const detail::VpVariantDesc& variant = capabilities.pVariants[variant_index];
                if (pBlockName != nullptr) {
                    if (strcmp(variant.blockName, pBlockName) != 0) {
                        continue;
                    }
                    result = VK_SUCCESS;
                }

                for (uint32_t i = 0; i < variant.formatCount; ++i) {
                    if (std::find(results.begin(), results.end(), variant.pFormats[i].format) == std::end(results)) {
                        results.push_back(variant.pFormats[i].format);
                    }
                }
            }
        }
    }

    const uint32_t count = static_cast<uint32_t>(results.size());

    if (pFormats == nullptr) {
        *pFormatCount = count;
    } else {
        if (*pFormatCount < count) {
            result = VK_INCOMPLETE;
        } else {
            *pFormatCount = count;
        }

        if (*pFormatCount > 0) {
            memcpy(pFormats, &results[0], *pFormatCount * sizeof(VkFormat));
        }
    }
    return result;
}

VPAPI_ATTR VkResult vpGetProfileFormatProperties(const VpProfileProperties *pProfile, const char* pBlockName, VkFormat format, void *pNext) {
    VkResult result = pBlockName == nullptr ? VK_SUCCESS : VK_INCOMPLETE;

    const std::vector<VpProfileProperties>& profiles = detail::GatherProfiles(*pProfile);

    for (std::size_t i = 0, n = profiles.size(); i < n; ++i) {
        const char* profile_name = profiles[i].profileName;

        const detail::VpProfileDesc* pProfileDesc = detail::vpGetProfileDesc(profile_name);
        if (pProfileDesc == nullptr) return VK_ERROR_UNKNOWN;

        for (uint32_t required_capability_index = 0; required_capability_index < pProfileDesc->requiredCapabilityCount;
                ++required_capability_index) {
            const detail::VpCapabilitiesDesc& required_capabilities = pProfileDesc->pRequiredCapabilities[required_capability_index];

            for (uint32_t required_variant_index = 0; required_variant_index < required_capabilities.variantCount; ++required_variant_index) {
                const detail::VpVariantDesc& variant = required_capabilities.pVariants[required_variant_index];
                if (pBlockName != nullptr) {
                    if (strcmp(variant.blockName, pBlockName) != 0) {
                        continue;
                    }
                    result = VK_SUCCESS;
                }

                for (uint32_t i = 0; i < variant.formatCount; ++i) {
                    if (variant.pFormats[i].format != format) {
                        continue;
                    }

                    VkBaseOutStructure* p = static_cast<VkBaseOutStructure*>(static_cast<void*>(pNext));
                    while (p != nullptr) {
                        variant.pFormats[i].pfnFiller(p);
                        p = p->pNext;
                    }
#if defined(VK_VERSION_1_3) || defined(VK_KHR_format_feature_flags2)
                    VkFormatProperties2KHR* fp2 = static_cast<VkFormatProperties2KHR*>(
                        detail::vpGetStructure(pNext, VK_STRUCTURE_TYPE_FORMAT_PROPERTIES_2_KHR));
                    VkFormatProperties3KHR* fp3 = static_cast<VkFormatProperties3KHR*>(
                        detail::vpGetStructure(pNext, VK_STRUCTURE_TYPE_FORMAT_PROPERTIES_3_KHR));
                    if (fp3 != nullptr) {
                        VkFormatProperties2KHR fp{ VK_STRUCTURE_TYPE_FORMAT_PROPERTIES_2_KHR };
                        variant.pFormats[i].pfnFiller(static_cast<VkBaseOutStructure*>(static_cast<void*>(&fp)));
                        fp3->linearTilingFeatures |= static_cast<VkFormatFeatureFlags2KHR>(fp3->linearTilingFeatures | fp.formatProperties.linearTilingFeatures);
                        fp3->optimalTilingFeatures |= static_cast<VkFormatFeatureFlags2KHR>(fp3->optimalTilingFeatures | fp.formatProperties.optimalTilingFeatures);
                        fp3->bufferFeatures |= static_cast<VkFormatFeatureFlags2KHR>(fp3->bufferFeatures | fp.formatProperties.bufferFeatures);
                    }
                    if (fp2 != nullptr) {
                        VkFormatProperties3KHR fp{ VK_STRUCTURE_TYPE_FORMAT_PROPERTIES_3_KHR };
                        variant.pFormats[i].pfnFiller(static_cast<VkBaseOutStructure*>(static_cast<void*>(&fp)));
                        fp2->formatProperties.linearTilingFeatures |= static_cast<VkFormatFeatureFlags>(fp2->formatProperties.linearTilingFeatures | fp.linearTilingFeatures);
                        fp2->formatProperties.optimalTilingFeatures |= static_cast<VkFormatFeatureFlags>(fp2->formatProperties.optimalTilingFeatures | fp.optimalTilingFeatures);
                        fp2->formatProperties.bufferFeatures |= static_cast<VkFormatFeatureFlags>(fp2->formatProperties.bufferFeatures | fp.bufferFeatures);
                    }
#endif
                }
            }
        }
    }

    return result;
}

VPAPI_ATTR VkResult vpGetProfileFeatureStructureTypes(const VpProfileProperties *pProfile, const char* pBlockName, uint32_t *pStructureTypeCount, VkStructureType *pStructureTypes) {
    return detail::vpGetProfileStructureTypes(pProfile, pBlockName, detail::STRUCTURE_FEATURE, pStructureTypeCount, pStructureTypes);
}

VPAPI_ATTR VkResult vpGetProfilePropertyStructureTypes(const VpProfileProperties *pProfile, const char* pBlockName, uint32_t *pStructureTypeCount, VkStructureType *pStructureTypes) {
    return detail::vpGetProfileStructureTypes(pProfile, pBlockName, detail::STRUCTURE_PROPERTY, pStructureTypeCount, pStructureTypes);
}

VPAPI_ATTR VkResult vpGetProfileFormatStructureTypes(const VpProfileProperties *pProfile, const char* pBlockName, uint32_t *pStructureTypeCount, VkStructureType *pStructureTypes) {
    return detail::vpGetProfileStructureTypes(pProfile, pBlockName, detail::STRUCTURE_FORMAT, pStructureTypeCount, pStructureTypes);
}
