#pragma once

#include <cstdint>
#include <vulkan/vulkan_core.h>

struct DeviceAddress {
    DeviceAddress() noexcept = default;

    DeviceAddress(DeviceAddress&& old) noexcept = default;

    DeviceAddress(const DeviceAddress& addr) = default;

    DeviceAddress(const uint64_t ptr_in) : ptr{ptr_in} {}

    DeviceAddress& operator=(DeviceAddress&& old) noexcept = default;

    DeviceAddress& operator=(const DeviceAddress& other) = default;

    DeviceAddress& operator=(const uint64_t ptr_in) {
        ptr = ptr_in;
        return *this;
    }

    ~DeviceAddress() = default;

    operator uint64_t() const {
        return ptr;
    }

    uint32_t high_bits() const {
        return static_cast<uint32_t>((ptr >> 32) & 0x00000000FFFFFFFF);
    }

    uint32_t low_bits() const {
        return static_cast<uint32_t>(ptr & 0xFFFFFFFF);
    }

    DeviceAddress& operator+=(const size_t val) {
        ptr += val;
        return *this;
    }

private:
    uint64_t ptr = {};
};
