// Soul Knight Linux — ECS Component Pool
#pragma once

#include <vector>
#include <cstddef>
#include <algorithm>
#include <utility>
#include <engine/core/Types.hpp>

namespace soulknight::engine {

// Sparse-set component storage.
//
// sparse_[entity_index] is 1-based (0 means "no component"). This lets a
// freshly resized sparse array distinguish "absent" from dense index 0.
template<typename T>
class ComponentPool {
public:
    using ComponentType = T;

    ComponentPool() = default;
    ~ComponentPool() = default;

    // Add component to entity. Existing components are returned unchanged.
    T* Add(Entity entity) {
        const u32 index = EntityIndex(entity);
        if (index >= sparse_.size()) {
            sparse_.resize(static_cast<size_t>(index) + 1, 0);
        }

        if (sparse_[index] != 0) {
            return &dense_[sparse_[index] - 1];
        }

        sparse_[index] = static_cast<u32>(dense_.size()) + 1;
        dense_entities_.push_back(entity);
        dense_.push_back(T{});
        return &dense_.back();
    }

    // Get component, or nullptr.
    T* Get(Entity entity) {
        const u32 index = EntityIndex(entity);
        if (index >= sparse_.size() || sparse_[index] == 0) return nullptr;
        return &dense_[sparse_[index] - 1];
    }

    const T* Get(Entity entity) const {
        const u32 index = EntityIndex(entity);
        if (index >= sparse_.size() || sparse_[index] == 0) return nullptr;
        return &dense_[sparse_[index] - 1];
    }

    // Remove component using swap-and-pop.
    void Remove(Entity entity) {
        const u32 index = EntityIndex(entity);
        if (index >= sparse_.size() || sparse_[index] == 0) return;

        const u32 dense_index = sparse_[index] - 1;
        const u32 last_index = static_cast<u32>(dense_.size()) - 1;

        if (dense_index != last_index) {
            dense_[dense_index] = std::move(dense_[last_index]);
            const Entity moved_entity = dense_entities_[last_index];
            dense_entities_[dense_index] = moved_entity;
            sparse_[EntityIndex(moved_entity)] = dense_index + 1;
        }

        dense_.pop_back();
        dense_entities_.pop_back();
        sparse_[index] = 0;
    }

    bool Has(Entity entity) const {
        const u32 index = EntityIndex(entity);
        return index < sparse_.size() && sparse_[index] != 0;
    }

    // Dense iteration over components.
    T* begin() { return dense_.data(); }
    T* end() { return dense_.data() + dense_.size(); }
    const T* begin() const { return dense_.data(); }
    const T* end() const { return dense_.data() + dense_.size(); }

    size_t Size() const { return dense_.size(); }
    bool Empty() const { return dense_.empty(); }

    void Clear() {
        dense_.clear();
        dense_entities_.clear();
        std::fill(sparse_.begin(), sparse_.end(), 0);
    }

private:
    std::vector<T> dense_;
    std::vector<Entity> dense_entities_;
    std::vector<u32> sparse_;
};

} // namespace soulknight::engine
