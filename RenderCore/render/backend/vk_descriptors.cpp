#include "render/backend/vk_descriptors.hpp"
#include <algorithm>
#include <tracy/Tracy.hpp>

#include "render/backend/render_backend.hpp"

namespace vkutil {
    constexpr auto variable_descriptor_array_max_size = 65536u;

    /**
     * Some heuristics for checking if a binding is _probably_ a variable-count descriptor array
     *
     * Probably not generalizable beyond my use case
     */
    static bool is_descriptor_array(const VkDescriptorSetLayoutBinding& binding) {
        return binding.descriptorCount == variable_descriptor_array_max_size && (binding.descriptorType == VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER);
    }

    VkDescriptorPool
    createPool(
        VkDevice device, const DescriptorAllocator::PoolSizes& poolSizes, int count,
        VkDescriptorPoolCreateFlags flags
    ) {
        std::vector<VkDescriptorPoolSize> sizes;
        sizes.reserve(poolSizes.sizes.size());
        for (auto sz : poolSizes.sizes) {
            sizes.push_back({sz.first, uint32_t(sz.second * count)});
        }
        VkDescriptorPoolCreateInfo pool_info = {};
        pool_info.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
        pool_info.flags = flags;
        pool_info.maxSets = count;
        pool_info.poolSizeCount = (uint32_t)sizes.size();
        pool_info.pPoolSizes = sizes.data();

        VkDescriptorPool descriptorPool;
        vkCreateDescriptorPool(device, &pool_info, nullptr, &descriptorPool);

        return descriptorPool;
    }

    void DescriptorAllocator::reset_pools() {
        for (auto p : usedPools) {
            vkResetDescriptorPool(device, p, 0);
        }

        freePools = usedPools;
        usedPools.clear();
        currentPool = VK_NULL_HANDLE;
    }

    bool DescriptorAllocator::allocate(
        VkDescriptorSet* set, VkDescriptorSetLayout layout,
        const VkDescriptorSetVariableDescriptorCountAllocateInfo* variable_count_info
    ) {
        if (currentPool == VK_NULL_HANDLE) {
            currentPool = grab_pool();
            usedPools.push_back(currentPool);
        }

        VkDescriptorSetAllocateInfo allocInfo = {};
        allocInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
        allocInfo.pNext = variable_count_info;

        allocInfo.pSetLayouts = &layout;
        allocInfo.descriptorPool = currentPool;
        allocInfo.descriptorSetCount = 1;


        VkResult allocResult = vkAllocateDescriptorSets(device, &allocInfo, set);
        bool needReallocate = false;

        switch (allocResult) {
        case VK_SUCCESS:
            //all good, return
            return true;

            break;
        case VK_ERROR_FRAGMENTED_POOL:
        case VK_ERROR_OUT_OF_POOL_MEMORY:
            //reallocate pool
            needReallocate = true;
            break;
        default:
            //unrecoverable error
            return false;
        }

        if (needReallocate) {
            //allocate a new pool and retry
            currentPool = grab_pool();
            usedPools.push_back(currentPool);

            allocResult = vkAllocateDescriptorSets(device, &allocInfo, set);

            //if it still fails then we have big issues
            if (allocResult == VK_SUCCESS) {
                return true;
            }
        }

        return false;
    }

    void DescriptorAllocator::init(VkDevice newDevice) {
        device = newDevice;
    }

    void DescriptorAllocator::cleanup() {
        //delete every pool held
        for (auto p : freePools) {
            vkDestroyDescriptorPool(device, p, nullptr);
        }
        for (auto p : usedPools) {
            vkDestroyDescriptorPool(device, p, nullptr);
        }
    }

    VkDescriptorPool DescriptorAllocator::grab_pool() {
        if (freePools.size() > 0) {
            VkDescriptorPool pool = freePools.back();
            freePools.pop_back();
            return pool;
        } else {
            return createPool(device, descriptorSizes, 100000, 0);
        }
    }


    void DescriptorLayoutCache::init(VkDevice newDevice) {
        device = newDevice;
    }

    VkDescriptorSetLayout
    DescriptorLayoutCache::create_descriptor_layout(VkDescriptorSetLayoutCreateInfo* info) {
        DescriptorLayoutInfo layout_info;
        layout_info.bindings.reserve(info->bindingCount);
        bool is_sorted = true;
        int32_t last_binding_idx = -1;
        for (uint32_t i = 0; i < info->bindingCount; i++) {
            layout_info.bindings.push_back(info->pBindings[i]);

            //check that the bindings are in strict increasing order
            if (static_cast<int32_t>(info->pBindings[i].binding) > last_binding_idx) {
                last_binding_idx = static_cast<int32_t>(info->pBindings[i].binding);
            } else {
                is_sorted = false;
            }
        }
        if (!is_sorted) {
            std::sort(
                layout_info.bindings.begin(), layout_info.bindings.end(),
                [](const VkDescriptorSetLayoutBinding& a, const VkDescriptorSetLayoutBinding& b) {
                    return a.binding < b.binding;
                }
            );
        }
        auto flags_create_info = VkDescriptorSetLayoutBindingFlagsCreateInfo{};
        auto flags = std::vector<VkDescriptorBindingFlags>{};
        const auto& last_binding = info->pBindings[info->bindingCount - 1];
        if (is_descriptor_array(last_binding)) {
            flags.resize(info->bindingCount);
            flags.back() = VK_DESCRIPTOR_BINDING_UPDATE_AFTER_BIND_BIT |
                VK_DESCRIPTOR_BINDING_VARIABLE_DESCRIPTOR_COUNT_BIT |
                VK_DESCRIPTOR_BINDING_PARTIALLY_BOUND_BIT;
            flags_create_info = VkDescriptorSetLayoutBindingFlagsCreateInfo{
                .sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_BINDING_FLAGS_CREATE_INFO,
                .bindingCount = static_cast<uint32_t>(flags.size()),
                .pBindingFlags = flags.data(),
            };
            info->pNext = &flags_create_info;
        }

        auto it = layoutCache.find(layout_info);
        if (it != layoutCache.end()) {
            return (*it).second;
        } else {
            VkDescriptorSetLayout layout;
            vkCreateDescriptorSetLayout(device, info, nullptr, &layout);

            //layoutCache.emplace()
            //add to sampler_cache
            layoutCache[layout_info] = layout;
            return layout;
        }
    }

    void DescriptorLayoutCache::cleanup() {
        //delete every descriptor layout held
        for (auto pair : layoutCache) {
            vkDestroyDescriptorSetLayout(device, pair.second, nullptr);
        }
    }

    vkutil::DescriptorBuilder
    DescriptorBuilder::begin(RenderBackend& backend, vkutil::DescriptorAllocator& allocator) {
        return DescriptorBuilder{backend, allocator};
    }

    DescriptorBuilder&
    DescriptorBuilder::bind_buffer(
        uint32_t binding, const DescriptorBuilder::BufferInfo& info,
        VkDescriptorType type, VkShaderStageFlags stage_flags
    ) {
        auto& allocator = backend.get_global_allocator();
        const auto& buffer_actual = allocator.get_buffer(info.buffer);

        auto& vk_info = buffer_infos_to_delete.emplace_back(
            std::vector{
                VkDescriptorBufferInfo{
                    .buffer = buffer_actual.buffer,
                    .offset = info.offset,
                    .range = info.range > 0 ? info.range : buffer_actual.create_info.size,
                }
            }
        );

        return bind_buffer(binding, vk_info.data(), type, stage_flags);
    }

    DescriptorBuilder&
    DescriptorBuilder::bind_buffer_array(
        const uint32_t binding, const std::vector<BufferInfo>& infos,
        const VkDescriptorType type, const VkShaderStageFlags stage_flags
    ) {
        auto& allocator = backend.get_global_allocator();
        auto& vk_infos = buffer_infos_to_delete.emplace_back();

        vk_infos.reserve(infos.size());

        for (const auto& info : infos) {
            const auto& buffer_actual = allocator.get_buffer(info.buffer);

            vk_infos.emplace_back(
                VkDescriptorBufferInfo{
                    .buffer = buffer_actual.buffer,
                    .offset = info.offset,
                    .range = info.range > 0 ? info.range : buffer_actual.create_info.size,
                }
            );
        }

        return bind_buffer(binding, vk_infos.data(), type, stage_flags, static_cast<uint32_t>(vk_infos.size()));
    }

    DescriptorBuilder& DescriptorBuilder::bind_image(
        const uint32_t binding, const ImageInfo& info, const VkDescriptorType type, const VkShaderStageFlags stage_flags
    ) {
        auto& allocator = backend.get_global_allocator();
        const auto& image_actual = allocator.get_texture(info.image);
        auto image_view = image_actual.image_view;
        if(info.mip_level) {
            image_view = image_actual.mip_views[*info.mip_level];
        }
        if (type == VK_DESCRIPTOR_TYPE_INPUT_ATTACHMENT) {
            image_view = image_actual.attachment_view;
        }

        auto& vk_info = image_infos_to_delete.emplace_back(
            std::vector{
                VkDescriptorImageInfo{
                    .sampler = info.sampler,
                    .imageView = image_view,
                    .imageLayout = info.image_layout,
                }
            }
        );

        return bind_image(binding, vk_info.data(), type, stage_flags);
    }

    DescriptorBuilder& DescriptorBuilder::bind_image_array(
        const uint32_t binding, const std::vector<ImageInfo>& infos, const VkDescriptorType type,
        const VkShaderStageFlags stage_flags
    ) {
        auto& allocator = backend.get_global_allocator();
        auto& vk_infos = image_infos_to_delete.emplace_back();

        vk_infos.reserve(infos.size());

        for (const auto& info : infos) {
            const auto& image_actual = allocator.get_texture(info.image);
            auto image_view = image_actual.image_view;
            if (info.mip_level) {
                image_view = image_actual.mip_views[*info.mip_level];
            }
            if (type == VK_DESCRIPTOR_TYPE_INPUT_ATTACHMENT) {
                image_view = image_actual.attachment_view;
            }

            vk_infos.emplace_back(
                VkDescriptorImageInfo{
                    .sampler = info.sampler,
                    .imageView = image_view,
                    .imageLayout = info.image_layout,
                }
            );
        }

        return bind_image(binding, vk_infos.data(), type, stage_flags, static_cast<uint32_t>(vk_infos.size()));
    }

    DescriptorBuilder& DescriptorBuilder::bind_buffer(
        const uint32_t binding, const VkDescriptorBufferInfo* buffer_infos, const VkDescriptorType type,
        const VkShaderStageFlags stageFlags, const uint32_t count
    ) {
        VkDescriptorSetLayoutBinding newBinding{};

        newBinding.descriptorCount = count;
        newBinding.descriptorType = type;
        newBinding.pImmutableSamplers = nullptr;
        newBinding.stageFlags = stageFlags;
        newBinding.binding = binding;

        bindings.push_back(newBinding);

        VkWriteDescriptorSet newWrite{};
        newWrite.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
        newWrite.pNext = nullptr;

        newWrite.descriptorCount = count;
        newWrite.descriptorType = type;
        newWrite.pBufferInfo = buffer_infos;
        newWrite.dstBinding = binding;

        writes.push_back(newWrite);
        return *this;
    }

    DescriptorBuilder& DescriptorBuilder::bind_image(
        uint32_t binding, VkDescriptorImageInfo* imageInfo, VkDescriptorType type, VkShaderStageFlags stageFlags,
        uint32_t count
    ) {
        VkDescriptorSetLayoutBinding newBinding{};

        newBinding.descriptorCount = count;
        newBinding.descriptorType = type;
        newBinding.pImmutableSamplers = nullptr;
        newBinding.stageFlags = stageFlags;
        newBinding.binding = binding;

        bindings.push_back(newBinding);

        VkWriteDescriptorSet newWrite{};
        newWrite.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
        newWrite.pNext = nullptr;

        newWrite.descriptorCount = count;
        newWrite.descriptorType = type;
        newWrite.pImageInfo = imageInfo;
        newWrite.dstBinding = binding;

        writes.push_back(newWrite);
        return *this;
    }

    bool DescriptorBuilder::build(VkDescriptorSet& set, VkDescriptorSetLayout& layout) {
        // Hack because Vulkan compatibility rules are stupid
        if (is_descriptor_array(bindings.back())) {
            bindings.back().descriptorCount = variable_descriptor_array_max_size;
        }

        //build layout first
        VkDescriptorSetLayoutCreateInfo layoutInfo{};
        layoutInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
        layoutInfo.pNext = nullptr;

        layoutInfo.pBindings = bindings.data();
        layoutInfo.bindingCount = static_cast<uint32_t>(bindings.size());

        layout = cache.create_descriptor_layout(&layoutInfo);

        auto success = false;

        //allocate descriptor
        if (is_descriptor_array(bindings.back())) {
            const auto count_info = VkDescriptorSetVariableDescriptorCountAllocateInfo{
                .sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_VARIABLE_DESCRIPTOR_COUNT_ALLOCATE_INFO,
                .descriptorSetCount = 1,
                .pDescriptorCounts = &variable_descriptor_array_max_size,
            };
            success = alloc.allocate(&set, layout, &count_info);
        } else {
            success = alloc.allocate(&set, layout);
        }

        if (!success) {
            return false;
        }

        //write descriptor

        for (VkWriteDescriptorSet& w : writes) {
            w.dstSet = set;
        }

        {
            ZoneScopedN("vkUpdateDescriptorSets");
            vkUpdateDescriptorSets(
                alloc.device, static_cast<uint32_t>(writes.size()), writes.data(), 0,
                nullptr
            );
        }

        return true;
    }

    bool DescriptorBuilder::build(VkDescriptorSet& set) {
        ZoneScoped;

        VkDescriptorSetLayout layout;
        return build(set, layout);
    }

    tl::optional<VkDescriptorSet> DescriptorBuilder::build() {
        VkDescriptorSet set;
        const auto success = build(set);
        if (success) {
            return set;
        } else {
            return tl::nullopt;
        }
    }

    DescriptorBuilder::DescriptorBuilder(
        RenderBackend& backend_in,
        vkutil::DescriptorAllocator& allocator_in
    ) :
        backend{backend_in}, cache{backend.get_descriptor_cache()}, alloc{allocator_in} {
        writes.reserve(32);
        bindings.reserve(32);
    }


    bool DescriptorLayoutCache::DescriptorLayoutInfo::operator==(
        const DescriptorLayoutInfo& other
    ) const {
        if (other.bindings.size() != bindings.size()) {
            return false;
        } else {
            //compare each of the bindings is the same. Bindings are sorted so they will match
            for (int i = 0; i < bindings.size(); i++) {
                if (other.bindings[i].binding != bindings[i].binding) {
                    return false;
                }
                if (other.bindings[i].descriptorType != bindings[i].descriptorType) {
                    return false;
                }
                if (other.bindings[i].descriptorCount != bindings[i].descriptorCount) {
                    return false;
                }
                if (other.bindings[i].stageFlags != bindings[i].stageFlags) {
                    return false;
                }
            }
            return true;
        }
    }

    size_t DescriptorLayoutCache::DescriptorLayoutInfo::hash() const {
        using std::size_t;
        using std::hash;

        size_t result = hash<size_t>()(bindings.size());

        for (const VkDescriptorSetLayoutBinding& b : bindings) {
            //pack the binding data into a single int64. Not fully correct but its ok
            size_t binding_hash = b.binding | b.descriptorType << 8 | b.descriptorCount << 16 |
                b.stageFlags << 24;

            //shuffle the packed binding data and xor it with the main hash
            result ^= hash<size_t>()(binding_hash);
        }

        return result;
    }
}
