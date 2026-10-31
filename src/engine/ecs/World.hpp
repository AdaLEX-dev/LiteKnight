// Soul Knight Linux — ECS World
#pragma once

#include <vector>
#include <unordered_map>
#include <memory>
#include <cstdint>
#include <iterator>
#include <utility>
#include <engine/core/Types.hpp>
#include "ComponentPool.hpp"

namespace soulknight::engine {

// RTTI-free type ID. The address of a function-local static is stable for the
// lifetime of the program and unique per instantiation. It is intentionally not
// constexpr: reinterpret_cast is not allowed in a constant expression.
template<typename T>
struct TypeId {
    static uintptr_t Value() {
        static const char id = 0;
        return reinterpret_cast<uintptr_t>(&id);
    }
};

class World {
public:
    World() = default;
    ~World() = default;

    Entity CreateEntity() {
        u32 index = static_cast<u32>(entities_.size());
        u32 generation = 1;
        Entity entity = MakeEntity(index, generation);
        entities_.push_back({generation, true});
        return entity;
    }

    void DestroyEntity(Entity entity) {
        u32 index = EntityIndex(entity);
        if (index < entities_.size()) {
            entities_[index].alive = false;
            entities_[index].generation++;
            // Remove all components
            for (auto& [type, pool] : pools_) {
                pool->Remove(entity);
            }
        }
    }

    bool IsAlive(Entity entity) const {
        u32 index = EntityIndex(entity);
        if (index >= entities_.size()) return false;
        return entities_[index].alive && entities_[index].generation == EntityGeneration(entity);
    }

    // Component management
    template<typename T>
    T* AddComponent(Entity entity) {
        return GetPool<T>().Add(entity);
    }

    template<typename T>
    T* AddComponent(Entity entity, T value) {
        T* component = GetPool<T>().Add(entity);
        if (component) {
            *component = std::move(value);
        }
        return component;
    }

    template<typename T>
    T* GetComponent(Entity entity) {
        return GetPool<T>().Get(entity);
    }

    template<typename T>
    const T* GetComponent(Entity entity) const {
        return const_cast<World*>(this)->GetPool<T>().Get(entity);
    }

    template<typename T>
    bool HasComponent(Entity entity) const {
        return GetPool<T>().Has(entity);
    }

    template<typename T>
    void RemoveComponent(Entity entity) {
        GetPool<T>().Remove(entity);
    }

    // Iterate all entities with component T
    template<typename T>
    class Iterator {
    public:
        using iterator_category = std::forward_iterator_tag;
        using value_type = std::pair<Entity, T*>;
        using difference_type = std::ptrdiff_t;
        using pointer = value_type*;
        using reference = value_type&;

        Iterator() = default;
        Iterator(World* world, size_t index) : world_(world), index_(index) {
            Advance();
        }

        bool operator==(const Iterator& other) const { return index_ == other.index_; }
        bool operator!=(const Iterator& other) const { return index_ != other.index_; }

        Iterator& operator++() {
            index_++;
            Advance();
            return *this;
        }

        value_type operator*() {
            u32 dense_index = world_->GetPool<T>().GetDenseIndex(index_);
            Entity entity = MakeEntity(dense_index, world_->entities_[dense_index].generation);
            T* component = world_->GetPool<T>().Get(entity);
            return {entity, component};
        }

    private:
        World* world_ = nullptr;
        size_t index_ = 0;

        void Advance() {
            auto& pool = world_->GetPool<T>();
            while (index_ < pool.Size() && !world_->IsAlive(MakeEntity(index_, world_->entities_[index_].generation))) {
                index_++;
            }
        }
    };

    template<typename T>
    Iterator<T> begin() { return Iterator<T>(this, 0); }
    template<typename T>
    Iterator<T> end() { return Iterator<T>(this, GetPool<T>().Size()); }

    size_t EntityCount() const {
        size_t count = 0;
        for (const auto& e : entities_) if (e.alive) count++;
        return count;
    }

private:
    struct EntityRecord {
        u32 generation = 0;
        bool alive = false;
    };

    std::vector<EntityRecord> entities_;

    struct PoolBase {
        virtual ~PoolBase() = default;
        virtual void Remove(Entity) = 0;
        virtual size_t Size() const = 0;
        virtual u32 GetDenseIndex(size_t) const = 0;
    };

    template<typename T>
    struct TypedPool : PoolBase {
        ComponentPool<T> pool;
        void Remove(Entity e) override { pool.Remove(e); }
        size_t Size() const override { return pool.Size(); }
        u32 GetDenseIndex(size_t i) const override { return static_cast<u32>(i); }
    };

    template<typename T>
    ComponentPool<T>& GetPool() {
        const uintptr_t type_id = TypeId<T>::Value();
        auto it = pools_.find(type_id);
        if (it == pools_.end()) {
            auto pool = std::make_unique<TypedPool<T>>();
            auto* ptr = pool.get();
            pools_[type_id] = std::move(pool);
            return ptr->pool;
        }
        return static_cast<TypedPool<T>*>(it->second.get())->pool;
    }

    std::unordered_map<uintptr_t, std::unique_ptr<PoolBase>> pools_;
};

} // namespace soulknight::engine