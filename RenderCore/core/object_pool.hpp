//
// Created by gold1 on 9/4/2022.
//

#ifndef SAHRENDERER_OBJECT_POOL_HPP
#define SAHRENDERER_OBJECT_POOL_HPP

#include <vector>

#include <tl/optional.hpp>

template <typename ObjectType>
class ObjectPool;

template <typename ObjectType>
struct PooledObject {
    uint32_t index = 0xFFFFFFFF;

    ObjectPool<ObjectType>* pool = nullptr;

    ObjectType* operator->() const;

    ObjectType& operator*() const;

    operator bool() const;

    bool operator!() const;

    bool is_valid() const;
};

template <typename ObjectType>
class ObjectPool {
public:
    ObjectPool();

    template <typename CreateFunc, typename DestroyFunc>
    explicit ObjectPool(CreateFunc&& creator_in, DestroyFunc&& deleter_in);

    ~ObjectPool();

    PooledObject<ObjectType> add_object(ObjectType&& object);

    PooledObject<ObjectType> create_object();

    ObjectType& get_object(const PooledObject<ObjectType>& handle);

    ObjectType free_object(const PooledObject<ObjectType>& handle);

    ObjectType free_object(uint32_t index);

    std::vector<ObjectType>& get_data();

    const std::vector<ObjectType>& get_data() const;

    ObjectType& operator[](uint32_t index);

    const ObjectType& operator[](uint32_t index) const;

private:
    std::function<ObjectType()> creator;

    std::function<void(ObjectType&&)> deleter;

    std::vector<ObjectType> objects;

    std::vector<PooledObject<ObjectType>> available_handles;
};

template <typename ObjectType>
struct std::hash<PooledObject<ObjectType>> {
    size_t operator()(const PooledObject<ObjectType>& value) const noexcept {
        return std::hash<uint32_t>{}(value.index);
    }
};

template <typename ObjectType>
ObjectType* PooledObject<ObjectType>::operator->() const {
    auto& objects = pool->get_data();
    return &objects[index];
}

template <typename ObjectType>
ObjectType& PooledObject<ObjectType>::operator*() const {
    auto& objects = pool->get_data();
    return objects[index];
}

template <typename ObjectType>
PooledObject<ObjectType>::operator bool() const {
    return is_valid();
}

template <typename ObjectType>
bool PooledObject<ObjectType>::operator!() const {
    return !operator bool();
}

template <typename ObjectType>
bool PooledObject<ObjectType>::is_valid() const {
    return index != 0xFFFFFFFF && pool != nullptr;
}

template <typename ObjectType>
PooledObject<ObjectType> ObjectPool<ObjectType>::add_object(ObjectType&& object) {
    auto handle = PooledObject<ObjectType>{0u, this};

    if (available_handles.empty()) {
        handle.index = static_cast<uint32_t>(objects.size());
        objects.emplace_back(std::move(object));

    } else {
        handle = available_handles.back();
        available_handles.pop_back();

        objects[handle.index] = std::move(object);
    }

    return handle;
}

template <typename ObjectType>
ObjectType& ObjectPool<ObjectType>::get_object(const PooledObject<ObjectType>& handle) {
    return objects[handle.index];
}

template <typename ObjectType>
std::vector<ObjectType>& ObjectPool<ObjectType>::get_data() {
    return objects;
}

template <typename ObjectType>
const std::vector<ObjectType>& ObjectPool<ObjectType>::get_data() const {
    return objects;
}

template <typename ObjectType>
ObjectType ObjectPool<ObjectType>::free_object(const PooledObject<ObjectType>& handle) {
    auto object = objects[handle.index];
    objects[handle.index] = {};
    available_handles.emplace_back(handle);

    return object;
}

template <typename ObjectType>
ObjectType& ObjectPool<ObjectType>::operator[](uint32_t index) {
    return objects[index];
}

template <typename ObjectType>
const ObjectType& ObjectPool<ObjectType>::operator[](uint32_t index) const {
    return objects[index];
}

template <typename ObjectType>
ObjectType ObjectPool<ObjectType>::free_object(uint32_t index) {
    auto object = objects[index];
    objects[index] = {};
    available_handles.emplace_back(PooledObject<ObjectType>{.index = index, .pool = this});

    return object;
}

template <typename ObjectType>
template <typename CreateFunc, typename DestroyFunc>
ObjectPool<ObjectType>::ObjectPool(CreateFunc&& creator_in, DestroyFunc&& deleter_in) :
        creator{creator_in}, deleter{deleter_in} {}

template <typename ObjectType>
ObjectPool<ObjectType>::~ObjectPool() {
    for (auto& obj: objects) {
        deleter(std::move(obj));
    }
}

template <typename ObjectType>
PooledObject<ObjectType> ObjectPool<ObjectType>::create_object() {
    if (!available_handles.empty()) {
        auto obj = available_handles.back();
        available_handles.pop_back();
        return obj;
    }

    return add_object(creator());
}

template <typename ObjectType>
ObjectPool<ObjectType>::ObjectPool() : ObjectPool([]() { return ObjectType{}; }, [](ObjectType&& obj) {}) {

}

#endif //SAHRENDERER_OBJECT_POOL_HPP
