// Soul Knight Linux — evidence-backed combat health state
#pragma once

#include <algorithm>

namespace soulknight::game::combat {

struct DamageEvent {
    int damage = 0;
    int camp = 0;
    int critic = 0;
    int through = 0;
    int repel = 0;
};

// Native subset of the recovered RoleAttribute / GetHurt pipeline:
// defence subtraction, armour absorption with HP overflow, HP clamp and death.
// Badass, mount defence, one-shot shields, buffs and reborn cards are not
// implemented in this first combat slice.
struct HealthState {
    int hp = 0;
    int max_hp = 0;
    int armor = 0;
    int max_armor = 0;
    int defence = 0;
    int camp = 0;
    bool can_hurt = true;
    bool dead = false;

    bool Alive() const { return hp > 0 && !dead; }

    void Reset(int new_max_hp, int new_max_armor = 0, int new_defence = 0) {
        max_hp = new_max_hp;
        hp = new_max_hp;
        max_armor = new_max_armor;
        armor = new_max_armor;
        defence = new_defence;
        can_hurt = true;
        dead = false;
    }

    // Returns true when the target accepted the hit. Damage from the same camp
    // is ignored, matching the recovered alliance/camp filters.
    bool ApplyDamage(const DamageEvent& event) {
        if (!can_hurt || dead || event.damage <= 0) return false;
        if (event.camp == camp) return false;

        int remaining = std::max(0, event.damage - defence);

        // RGController.GetHurt selects the armour path while armor >= 1.
        if (armor >= 1) {
            const int armor_after = armor - remaining;
            if (armor_after >= 0) {
                armor = armor_after;
                return true;
            }
            armor = 0;
            remaining = -armor_after;
        }

        hp = std::max(0, hp - remaining);
        if (hp == 0) {
            dead = true;
            can_hurt = false;
        }
        return true;
    }

    // Native enemy path: RGEController.GetHurt has no armour field; it
    // subtracts defence, clamps HP and triggers death at <= 0.
    bool ApplyEnemyDamage(const DamageEvent& event) {
        if (!can_hurt || dead || event.damage <= 0) return false;
        if (event.camp == camp) return false;
        const int remaining = std::max(0, event.damage - defence);
        hp = std::max(0, hp - remaining);
        if (hp == 0) {
            dead = true;
            can_hurt = false;
        }
        return true;
    }
};

} // namespace soulknight::game::combat
