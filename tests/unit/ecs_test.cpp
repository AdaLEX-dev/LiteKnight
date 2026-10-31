// Soul Knight Linux — ECS Tests
#include <doctest/doctest.h>
#include "engine/ecs/Entity.hpp"
#include "engine/ecs/ComponentPool.hpp"
#include "engine/ecs/World.hpp"

using namespace soulknight::engine;
using soulknight::Entity;
using soulknight::EntityIndex;
using soulknight::EntityGeneration;
using soulknight::MakeEntity;
using soulknight::INVALID_ENTITY;

TEST_CASE("Entity creation and validation" * doctest::test_suite("unit")) {
    Entity e1 = MakeEntity(1, 1);
    Entity e2 = MakeEntity(2, 1);

    CHECK(EntityIndex(e1) == 1);
    CHECK(EntityGeneration(e1) == 1);
    CHECK(e1 != e2);
    CHECK(e1 != INVALID_ENTITY);
}

TEST_CASE("ComponentPool basic operations" * doctest::test_suite("unit")) {
    struct TestComponent {
        int value = 0;
    };

    ComponentPool<TestComponent> pool;

    Entity e1 = MakeEntity(1, 1);
    Entity e2 = MakeEntity(2, 1);

    SUBCASE("Add and get component") {
        TestComponent* c1 = pool.Add(e1);
        c1->value = 42;

        TestComponent* found = pool.Get(e1);
        CHECK(found != nullptr);
        CHECK(found->value == 42);
    }

    SUBCASE("Remove component") {
        pool.Add(e1);
        pool.Remove(e1);
        CHECK(pool.Get(e1) == nullptr);
    }

    SUBCASE("Iterate components") {
        pool.Add(e1)->value = 10;
        pool.Add(e2)->value = 20;

        int sum = 0;
        for (auto& comp : pool) {
            sum += comp.value;
        }
        CHECK(sum == 30);
    }
}

TEST_CASE("World entity management" * doctest::test_suite("unit")) {
    World world;

    SUBCASE("Create and destroy entities") {
        Entity e1 = world.CreateEntity();
        Entity e2 = world.CreateEntity();

        CHECK(e1 != e2);
        CHECK(e1 != INVALID_ENTITY);

        world.DestroyEntity(e1);
        // e1 is now invalid
    }

    SUBCASE("Component management through world") {
        struct Position { float x, y; };
        struct Velocity { float x, y; };

        Entity e = world.CreateEntity();
        world.AddComponent<Position>(e, {1.0f, 2.0f});
        world.AddComponent<Velocity>(e, {0.5f, 0.0f});

        auto* pos = world.GetComponent<Position>(e);
        auto* vel = world.GetComponent<Velocity>(e);

        CHECK(pos != nullptr);
        CHECK(vel != nullptr);
        CHECK(pos->x == 1.0f);
        CHECK(vel->x == 0.5f);
    }
}