# Knight c00 Ability Audit

**Status:** Dual Wield implemented from recovered c00 data; exact cooldown
start phase and the single-weapon off-hand duplication remain HYPOTHESIS.
**Reference:** Soul Knight 2.6.8, `c00` prefab, `C01Controller`, global metadata.

## Recovered evidence

| Evidence | Result |
|---|---|
| `c00` GameObject components | `C01Controller`, `RoleAttributePlayer`, `Animator`, `RGCharGray` |
| c00 serialized `skills[0]` | `maxTime=5.0`, `cd=10.0`, `skillType=1`, `price=0`, icon sprite `ui_skill01` |
| c00 serialized `skills[1]` | `maxTime=5.0`, `cd=10.0`, `price=5000`, icon `skill_0_1`; not implemented |
| `C01Controller` fields | `hand2`, `hand3`, `onKnightSkillStart`, `onKnightSkillEnd` |
| `C01Controller` methods | `RoleSkill`, `RoleSkillEnd`, `Hand2Atk`, `Hand3Atk`, `SwitchWeapon`, `AutoLock` |
| `GunInitKnightXXSkillListener` | off-hand `handPositionDic`, `HandState.position`, `shouldFlip`, sprite renderer |
| `RGController` fields | `skillCasting`, `skill_time`, `hand`, `MultiHand[] hands` |
| Skill icon extraction | `ui_skill01` from UI atlas, integrated as HUD power icon |

## Native implementation

- `Q` activates Knight Dual Wield (`Tab` switches weapons; `1`/`2` select slots).
- Active time: `5.0 s` from serialized `maxTime`.
- Cooldown: `10.0 s` from serialized `cd`.
- Resource: no energy cost was recovered for this skill; current cost is `0`.
- During the active window the off-hand weapon fires and renders. If only one
  weapon slot is occupied, the active weapon is used as the off-hand HYPOTHESIS
  copy.
- HUD state: ready / active / cooldown with a recovered-icon cooldown mask.

## Open questions

- Whether the reference cooldown begins at activation or at effect end
  (`HYPOTHESIS`: native implementation starts the 10 s cooldown after the 5 s
  active window).
- Whether a one-weapon Knight duplicates the active weapon or requires a second
  equipped weapon (`HYPOTHESIS`: duplicate the active weapon, supported by the
  recovered off-hand listener and hand2/hand3 paths).
- Exact off-hand animation events and muzzle transform chain remain
  approximations.
