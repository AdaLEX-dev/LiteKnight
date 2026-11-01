# UI Asset Audit

Reference-derived UI pixels are LOCAL_ONLY / NOT_FOR_REDISTRIBUTION.

| Asset | Use | Status | Note |
|---|---|---|---|
| menu_backdrop | Main menu backdrop/Knight | RECOVERED/INTEGRATED | actual biaoti 01 backdrop from level1 |
| menu_swirl | Main menu red swirl | RECOVERED/INTEGRATED | actual biaoti 02 brush swirl |
| ui_btn1 | Pause/settings/main-menu SETTINGS frame | RECOVERED/INTEGRATED | nine-sliced brown button |
| ui_btn3 | Settings selected row frame | RECOVERED/INTEGRATED | nine-sliced cyan button |
| ui_btn4 | Main-menu QUIT frame | RECOVERED/INTEGRATED | nine-sliced red button |
| ui_btn5 | Main-menu NEW GAME frame | RECOVERED/INTEGRATED | nine-sliced yellow button |
| ui_btn7 | Main-menu CONTINUE frame | RECOVERED/INTEGRATED | nine-sliced blue button |
| ui_15 | HUD status plate | RECOVERED/INTEGRATED | actual heart/shield/gem plate with three bar slots |
| ui_12 | HUD dynamic bar fill | RECOVERED/INTEGRATED | actual white fill tinted by HP/armor/energy |
| ui_31 | HUD coin icon | RECOVERED/INTEGRATED | actual gold coin sprite |
| ui_94 | HUD pause icon | RECOVERED/INTEGRATED | two-bar pause sprite |
| ui_joy_0 | Ability circular button base | RECOVERED/INTEGRATED | actual reference joystick/skill base |
| ui_joy_3 | Ability lightning icon | RECOVERED/INTEGRATED | actual white lightning skill icon |
| ui_joy_7 | Ability recharge diamond | RECOVERED/INTEGRATED | actual blue diamond/circular-arrow marker |
| ui_2 | Pause/settings panel | RECOVERED/INTEGRATED | actual dark blue panel |
| ui_13 | Pause/settings ribbon | RECOVERED/INTEGRATED | actual red ribbon |
| ui_22 | Pause portrait frame | RECOVERED/INTEGRATED | actual bronze frame |
| ui_38 | Pause button frame | RECOVERED/INTEGRATED | actual brown/gold frame |
| ui_98 | Pause/home icon | RECOVERED/INTEGRATED | actual home sprite |
| ui_92 | Pause/resume icon | RECOVERED/INTEGRATED | actual play sprite |
| ui_93 | Pause/settings icon | RECOVERED/INTEGRATED | actual gear sprite |
| ui_191 | Weapon pickup/chest card | RECOVERED/INTEGRATED | actual item-info card frame |
| ui_14 | Map boss icon | RECOVERED/INTEGRATED | actual UIMap boss_logo |
| ui_113 | Map chest icon | RECOVERED/INTEGRATED | actual UIMap chest_logo |
| ui_114 | Map special icon | RECOVERED/INTEGRATED | actual UIMap special_logo |
| ui_125 | Map home icon | RECOVERED/INTEGRATED | actual UIMap home_logo |
| ui_193 | Map cage icon | RECOVERED/INTEGRATED | actual UIMap cage_logo |
| ui_4 | Map current-room marker | RECOVERED/INTEGRATED | actual UIMap target sprite |
| ui_33 | Item stat heart | RECOVERED/INTEGRATED | actual Ui item-info icon |
| ui_34 | Item stat shield | RECOVERED/INTEGRATED | actual Ui item-info icon |
| ui_35 | Item stat crystal | RECOVERED/INTEGRATED | actual Ui item-info icon |
| ui_37 | Item stat health | RECOVERED/INTEGRATED | actual Ui item-info icon |
| pixel_bold | All UI text | RECOVERED/INTEGRATED | TTF rasterized at 28 px; local-only atlas |
| ui_skill01 | Knight c00 Dual Wield icon | DISCOVERED | dual-wield icon, not used as the lightning ability |
| ui_67 | Energy crystal icon | DISCOVERED | fallback energy icon; ability uses ui_joy_3 |
| ui_btn1/ui_btn3/ui_btn4/ui_btn5/ui_btn7 9-slice borders | Recovered button frames | RECOVERED/INTEGRATED | 5 px native slice border |
| LIGHT KNIGHT wordmark | Main menu title | APPROXIMATED | required project identity; recovered title art is Chinese/Soul Knight branding |
| main-menu vertical button stack | Main menu layout | APPROXIMATED | desktop adaptation of the reference horizontal mobile button bar |
| settings centered rows | Settings desktop composition | APPROXIMATED | recovered panel/frames, native desktop row layout |
| minimap grid mapping | HUD minimap | APPROXIMATED | reference UIMap construction recovered; native room graph-to-grid mapping is policy |
| D-ROOM button | Main-menu developer entry | APPROXIMATED | native opaque blue button; requested project UI, not a reference sprite |
| white D devroom cell | Minimap developer room | APPROXIMATED | intentional native marker on a normal-size map cell |
| UI Design Mode | Persistent HUD editor | APPROXIMATED | native logical layout editor and version 2 JSON schema |
| 30-second review harness | Manual visual review | NATIVE | development tool, no reference counterpart |
| mobile joystick/touch controls | Not present | EXCLUDED | desktop controls only |
| social tab | Not present | EXCLUDED | intentionally omitted |

## Deliberately excluded

- Mobile joystick/touch controls
- Social tab
