// Soul Knight Linux — recovered Rogue starter weapon (Jack and Mary)
#include <doctest/doctest.h>

#include "engine/assets/SceneLoader.hpp"
#include "game/combat/ProjectileSystem.hpp"
#include "game/combat/Weapon.hpp"
#include "platform/filesystem/FileSystem.hpp"

#include <string>

using soulknight::engine::Vec2;
using soulknight::engine::assets::SceneAsset;
using soulknight::engine::assets::SceneLoader;
using soulknight::engine::assets::WeaponDefinition;
using soulknight::game::combat::ProjectileSystem;
using soulknight::game::combat::Weapon;
using soulknight::platform::FileSystem;

TEST_CASE("Rogue starter weapon is the recovered hybrid and not Bad Pistol" *
          doctest::test_suite("unit")) {
    FileSystem filesystem;
    CHECK(filesystem.Initialize());
    SceneAsset scene;
    std::string error;
    CHECK(SceneLoader::Load(filesystem, "scenes/hero_room/scene.json",
                            scene, &error));
    const WeaponDefinition* ranger = nullptr;
    const WeaponDefinition* pistol = nullptr;
    for (const auto& weapon : scene.weapons) {
        if (weapon.id == "weapon_init_ranger") ranger = &weapon;
        if (weapon.id == "weapon_000") pistol = &weapon;
    }
    CHECK(ranger != nullptr);
    CHECK(pistol != nullptr);
    if (ranger == nullptr || pistol == nullptr) return;
    MESSAGE("ranger audio=" << ranger->audio_clip
            << " speed=" << ranger->bullet_speed
            << " pistol audio=" << pistol->audio_clip);
    CHECK(ranger->is_melee);
    CHECK(ranger->audio_clip == "fx_sword1");
    CHECK(ranger->audio_clip != pistol->audio_clip);
    CHECK(ranger->projectile.id == "bullet_107");
    CHECK(ranger->projectile.sprite_index >= 0);
    CHECK(ranger->projectile.sprite_index != pistol->projectile.sprite_index);
    CHECK(ranger->bullet_speed == doctest::Approx(36.0f));
    CHECK(ranger->damage == 4);
    CHECK(ranger->attack_cycle_seconds == doctest::Approx(0.3125f));
    CHECK(ranger->special.kind == "rogue_throw");
    CHECK(ranger->muzzle_sprite.empty());

    Weapon weapon;
    CHECK(weapon.Configure(*ranger));
    ProjectileSystem projectiles;
    const Vec2 owner{0.0f, 0.0f};
    (void)weapon.Update(0.05f, true, owner, {1.0f, 0.0f}, projectiles);
    (void)weapon.Update(0.20f, true, owner, {1.0f, 0.0f}, projectiles);
    CHECK(projectiles.Projectiles().size() >= 1);
    if (!projectiles.Projectiles().empty()) {
        CHECK(projectiles.Projectiles().front().damage == 4);
    }
}
