// Soul Knight Linux — ECS System Base
#pragma once

#include "World.hpp"

namespace soulknight::engine {

class System {
public:
    virtual ~System() = default;

    // Called once per frame with fixed timestep
    virtual void Update(World& world, float dt) = 0;

    // Called once at startup
    virtual void Initialize(World& world) {}

    // Called once at shutdown
    virtual void Shutdown(World& world) {}

    // System priority (lower = runs first)
    virtual int GetPriority() const { return 0; }
};

template<typename... Components>
class QuerySystem : public System {
public:
    void Update(World& world, float dt) override {
        // Implemented by derived class
    }
};

} // namespace soulknight::engine