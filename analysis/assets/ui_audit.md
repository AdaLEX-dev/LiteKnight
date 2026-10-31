# UI Asset Audit

Reference-derived UI pixels are LOCAL_ONLY / NOT_FOR_REDISTRIBUTION.

| Asset | Use | Status | Note |
|---|---|---|---|
| title_en0 | Main menu logo | RECOVERED/INTEGRATED | local-only title sprite |
| ui_33 | HUD health icon | RECOVERED/INTEGRATED | actual heart sprite |
| ui_34 | HUD armor icon | RECOVERED/INTEGRATED | actual shield sprite |
| ui_67 | HUD energy icon | RECOVERED/INTEGRATED | actual crystal sprite |
| ui_94 | HUD pause button | RECOVERED/INTEGRATED | two-bar pause icon |
| ui_5 | HUD currency icon | RECOVERED/INTEGRATED | coin sprite |
| ui_98 | Pause/home icon | RECOVERED/INTEGRATED | home sprite |
| ui_92 | Pause/resume icon | RECOVERED/INTEGRATED | play sprite |
| ui_93 | Pause/settings icon | RECOVERED/INTEGRATED | gear sprite |
| ui_99 | Power-slot plus | RECOVERED/INTEGRATED | plus sprite |
| ui_btn1 | Pause/settings button frame | RECOVERED/INTEGRATED | nine-sliced |
| ui_btn5 | Main-menu NEW GAME frame | RECOVERED/INTEGRATED | nine-sliced yellow |
| ui_btn7 | Main-menu CONTINUE frame | RECOVERED/INTEGRATED | nine-sliced blue |
| ui_116 | Cloud/save icon | RECOVERED/INTEGRATED | local-only cloud icon |
| ui_skill01 | Knight c00 Dual Wield HUD icon | RECOVERED/INTEGRATED | actual c00 SkillInfo[0] icon; 5.0 s active / 10.0 s cooldown; charge fill uses ui_67 |
| pixel_bold | All UI text | RECOVERED/INTEGRATED | TTF rasterized at 28 px; local-only atlas |
| ui_buff_* | Buff icons | DISCOVERED | not assigned; no fabricated buff UI |
| panel/banner frames | Pause/settings/character panels | APPROXIMATED | native pixel borders; exact frame mapping UNKNOWN |
| HUD outer plate | HUD frame | APPROXIMATED | native frame around recovered icons |
| pickup stats card | Bottom-centre pickup prompt | APPROXIMATED | native panel with recovered weapon icon/name, damage and energy cost |
| minimap / map panel | HUD minimap and M overlay | APPROXIMATED | native room-cell renderer; reference UIMap icon atlas UNKNOWN |
| D-ROOM button | Main-menu developer entry | APPROXIMATED | native opaque blue button; requested project UI, not a reference sprite |
| UI Design Mode | Persistent HUD editor | APPROXIMATED | native logical layout editor and JSON schema |
| main menu background | Main menu | APPROXIMATED | native atmospheric bands, no copied screenshot |
| character portraits | Character station | RECOVERED/INTEGRATED | recovered c00..c14 body sprites |
| weapon icons | Weapon station/HUD | RECOVERED/INTEGRATED | recovered weapon visual sprites |
| mobile joystick/touch controls | Not present | EXCLUDED | desktop controls only |
| social tab | Not present | EXCLUDED | intentionally omitted |

## Deliberately excluded

- Mobile joystick/touch controls
- Social tab
