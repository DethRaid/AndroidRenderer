#pragma once

#include <array>

#include <volk.h>

template <typename ParentStruct>
struct ExtensibleStruct : ParentStruct {
    template <typename ExtensionStruct>
    void add_extension(ExtensionStruct* extension);

    ParentStruct* operator*();

    const ParentStruct* operator*() const;

    std::array<uint8_t, 2048> extension_struct_storage = {};

    uint8_t* write_ptr = nullptr;

    void* p_next_chain_head = nullptr;
};

template <typename ParentStruct>
template <typename ExtensionStruct>
void ExtensibleStruct<ParentStruct>::add_extension(ExtensionStruct* extension) {
    extension->pNext = p_next_chain_head;
    p_next_chain_head = extension;
}

template <typename ParentStruct>
ParentStruct* ExtensibleStruct<ParentStruct>::operator*() {
    this->pNext = p_next_chain_head;
    return this;
}

template <typename ParentStruct>
const ParentStruct* ExtensibleStruct<ParentStruct>::operator*() const {
    this->pNext = p_next_chain_head;
    return this;
}
