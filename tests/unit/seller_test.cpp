// Soul Knight Linux — recovered dungeon seller tests
#include <doctest/doctest.h>

#include "engine/assets/SceneAsset.hpp"
#include "game/items/ItemAssets.hpp"
#include "game/items/SellerSystem.hpp"
#include "platform/filesystem/FileSystem.hpp"

using soulknight::engine::Vec2;
using soulknight::engine::assets::SceneAsset;
using soulknight::game::items::ItemAssets;
using soulknight::game::items::SellerPurchase;
using soulknight::game::items::SellerSystem;
using soulknight::platform::FileSystem;

TEST_CASE("Recovered r_sell stock is deterministic and playable" *
          doctest::test_suite("unit")) {
    FileSystem filesystem;
    CHECK(filesystem.Initialize());
    ItemAssets assets;
    std::string error;
    CHECK(assets.Load(filesystem, "scenes/items/items.json", &error));
    SceneAsset scene;
    scene.id = "r_sell_visual";
    scene.spawn = {20.0f, 15.0f};
    SellerSystem seller;
    CHECK(seller.Configure(assets, scene, 1, 12345, nullptr,
                           "r_sell_visual"));
    CHECK(seller.Offers().size() >= 3);
    CHECK(seller.Offers()[0].item_id == "health_pot");
    CHECK(seller.Offers()[0].price == 25);
    CHECK(seller.Offers()[1].item_id == "energy_pot");
    CHECK(seller.Offers()[1].price == 20);
    CHECK(seller.Offers()[2].kind == "weapon");
    CHECK(seller.Offers()[2].price > 0);

    const auto* nearby = seller.FindNearby(seller.Offers()[0].position, 2.0f);
    CHECK(nearby != nullptr);
    CHECK(nearby->item_id == "health_pot");
}

TEST_CASE("Seller purchase deducts coins, applies stock and persists sold state" *
          doctest::test_suite("unit")) {
    FileSystem filesystem;
    CHECK(filesystem.Initialize());
    ItemAssets assets;
    std::string error;
    CHECK(assets.Load(filesystem, "scenes/items/items.json", &error));
    SceneAsset scene;
    scene.id = "r_sell_visual";
    scene.spawn = {20.0f, 15.0f};
    SellerSystem seller;
    CHECK(seller.Configure(assets, scene, 1, 7, nullptr, "r_sell_visual"));
    int coins = 100;
    int hp = 2;
    int energy = 0;
    CHECK(seller.Purchase(0, coins, hp, 6, energy, 160) ==
          SellerPurchase::Success);
    CHECK(coins == 75);
    CHECK(hp == 4);
    CHECK(seller.Offers()[0].sold);
    CHECK(seller.FindNearby(seller.Offers()[0].position, 2.0f) == nullptr);

    CHECK(seller.Purchase(0, coins, hp, 6, energy, 160) ==
          SellerPurchase::Sold);
    CHECK(seller.Purchase(1, coins, hp, 6, energy, 160) ==
          SellerPurchase::Success);
    CHECK(energy == 80);
    CHECK(coins == 55);

    // Persistence is by recovered item id; a new seller restores exact sold
    // offers without saving transient positions.
    SellerSystem restored;
    CHECK(restored.Configure(assets, scene, 1, 7, nullptr,
                             "r_sell_visual"));
    restored.RestoreSold({"health_pot", "energy_pot"});
    CHECK(restored.Offers()[0].sold);
    CHECK(restored.Offers()[1].sold);
    CHECK_FALSE(restored.Offers()[2].sold);
}

TEST_CASE("Recovered special room definitions carry exact stock fields" *
          doctest::test_suite("unit")) {
    FileSystem filesystem;
    CHECK(filesystem.Initialize());
    ItemAssets assets;
    std::string error;
    CHECK(assets.Load(filesystem, "scenes/items/items.json", &error));
    const auto* statue = assets.FindSpecialObject("statue_09");
    CHECK(statue != nullptr);
    CHECK(statue->role == "statue");
    CHECK(statue->price == 15);
    CHECK(statue->statue_index == 9);
    const auto* gold = assets.FindSpecialObject("mineral_gold");
    CHECK(gold != nullptr);
    CHECK(gold->hp == 40);
    CHECK(gold->mineral_count == 10);
    CHECK(gold->drop_item == "coin_2");
    const auto* crystal = assets.FindSpecialObject("mineral_crystal");
    CHECK(crystal != nullptr);
    CHECK(crystal->drop_item == "energy");
    const auto* mercenary = assets.FindSpecialObject("npc_04");
    CHECK(mercenary != nullptr);
    CHECK(mercenary->price == 15);
    const auto* creature = assets.FindSpecialObject("npc_mount_creature");
    const auto* mech = assets.FindSpecialObject("npc_mount_mech");
    CHECK(creature != nullptr);
    CHECK(mech != nullptr);
    CHECK(creature->price == 15);
    CHECK(mech->price == 30);
}

TEST_CASE("Seller rejects purchases without enough coins" *
          doctest::test_suite("unit")) {
    FileSystem filesystem;
    CHECK(filesystem.Initialize());
    ItemAssets assets;
    std::string error;
    CHECK(assets.Load(filesystem, "scenes/items/items.json", &error));
    SceneAsset scene;
    scene.id = "r_sell_visual";
    scene.spawn = {20.0f, 15.0f};
    SellerSystem seller;
    CHECK(seller.Configure(assets, scene, 1, 3, nullptr, "r_sell_visual"));
    int coins = 0;
    int hp = 2;
    int energy = 0;
    CHECK(seller.Purchase(0, coins, hp, 6, energy, 160) ==
          SellerPurchase::NoMoney);
    CHECK(coins == 0);
    CHECK(hp == 2);
    CHECK_FALSE(seller.Offers()[0].sold);
}
