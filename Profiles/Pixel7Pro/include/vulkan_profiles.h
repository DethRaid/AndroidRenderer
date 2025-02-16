
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

#ifndef VULKAN_PROFILES_H_
#define VULKAN_PROFILES_H_ 1

#define VPAPI_ATTR

#ifdef __cplusplus
    extern "C" {
#endif

#include <vulkan/vulkan.h>

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

#ifdef __cplusplus
}
#endif

#endif // VULKAN_PROFILES_H_
