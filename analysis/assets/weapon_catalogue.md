# Reference Weapon Catalogue

Recovered player-facing names and serialized combat fields from the
SoulKnight 2.6.8 game data. `energy_cost` is the serialized
`RGWeapon.consume`; it is the per-attack energy cost, not ammunition.
Damage is the first serialized `BulletInfoInspector.damage` entry.

- localized names: **301**
- WeaponInfo definitions: **349**
- serialized weapon prefabs with a gun/bullet component: **296**
- integrated native weapons: **4**

## Integrated weapons (CONFIRMED)

| Internal id | Player name | Damage | Energy | Cycle (s) | Projectile |
|---|---|---:|---:|---:|---|
| `weapon_000` | Bad Pistol | 3 | 0 | 0.3333333432674408 | `bullet_0` |
| `weapon_002` | AK-47 | 3 | 1 | 0.1666666716337204 | `bullet_14` |
| `weapon_048` | Next-gen SMG | 3 | 1 | 0.5 | `bullet_11` |
| `weapon_287` | Revolver Gold | 4 | 1 | 0.13333334028720856 | `bullet_14` |

## Catalogue excerpt

| Internal id | Player name | Level | Type | Damage | Energy |
|---|---|---:|---|---:|---:|
| `weapon_000` | Bad Pistol | 0 | Pistol | 3 | 0 |
| `weapon_001` | P250 Pistol | 0 | Pistol | 3 | 0 |
| `weapon_002` | AK-47 | 0 | Gun | 3 | 1 |
| `weapon_003` | Shotgun | 0 | ShotGun | 3 | 3 |
| `weapon_004` | Shotgun Pro | 0 | ShotGun | 3 | 4 |
| `weapon_005` | UZI | 0 | Gun | 2 | 0 |
| `weapon_006` | Splitter Cannon | 1 | Gun | 6 | 3 |
| `weapon_007` | Snow Fox L | 0 | Gun | 2 | 0 |
| `weapon_008` | Snow Fox XL | 1 | Gun | 2 | 1 |
| `weapon_009` | Snow Fox XXL | 2 | Gun | 2 | 2 |
| `weapon_010` | Desert Eagle | 0 | Pistol | 4 | 1 |
| `weapon_011` | Alien Eagle | 1 | Pistol | 4 | 0 |
| `weapon_012` | Frost Eagle | 1 | Pistol | 4 | 1 |
| `weapon_013` | Flame Eagle | 1 | Pistol | 4 | 1 |
| `weapon_014` | Plasma Eagle | 1 | Pistol | 3 | 1 |
| `weapon_015` | Revolver | 0 | Pistol | 8 | 3 |
| `weapon_016` | Twin-barrel Pistol | 0 | Pistol | 3 | 2 |
| `weapon_017` | Assault Rifle | 0 | Gun | 3 | 1 |
| `weapon_018` | Assault Rocket | 2 | Gun | 3 | 1 |
| `weapon_019` | Assault Rifle Pro | 2 | Gun | 3 | 1 |
| `weapon_020` | Shaky Blaster | 0 | ShotGun | 2 | 1 |
| `weapon_021` | Blaster | 0 | Gun | 3 | 1 |
| `weapon_022` | Twin-barrel Rifle | 1 | ShotGun | 5 | 3 |
| `weapon_023` | Splitter Gun | 0 | Gun | 5 | 2 |
| `weapon_024` | Volcanic Blaster | 0 | Gun | 4 | 1 |
| `weapon_025` | M4 | 0 | Gun | 4 | 1 |
| `weapon_026` | Improved SMG | 0 | Gun | 3 | 2 |
| `weapon_027` | MP5 | 0 | Gun | 3 | 1 |
| `weapon_028` | Snow Fox Vintage | 4 | Gun | 4 | 3 |
| `weapon_029` | Shotgun Galaxy | 1 | ShotGun | 3 | 2 |
| `weapon_030` | Sidewinder Red | 1 | ShotGun | 3 | 4 |
| `weapon_031` | Sidewinder Green | 1 | ShotGun | 3 | 4 |
| `weapon_032` | Sniper Rifle | 0 | Rifle | 10 | 3 |
| `weapon_033` | Soul Calibre | 4 | Rifle | 25 | 9 |
| `weapon_034` | Glacier | 2 | Rifle | 10 | 4 |
| `weapon_035` | Aurora | 3 | Rifle | 16 | 6 |
| `weapon_036` | Furnace | 2 | Gun | 5 | 2 |
| `weapon_037` | Fine Machine Gun | 0 | Gun | 2 | 2 |
| `weapon_038` | Knight's Fist | 2 | ShotGun | 4 | 3 |
| `weapon_039` | Gas Blaster | 1 | Gun | 3 | 1 |
| `weapon_040` | Flame Blaster | 1 | ShotGun | 3 | 2 |
| `weapon_041` | Gatling Gun | 3 | ShotGun | 2 | 2 |
| `weapon_042` | Fusion Drill | 3 | Laser | 3 | 1 |
| `weapon_043` | Rocket Gun | 3 | Rocket | 15 | 5 |
| `weapon_044` | Implosion | 4 | Rocket | 16 | 6 |
| `weapon_045` | Ion Railgun | 0 | Hold | 6 | 4 |
| `weapon_046` | Ion Laser | 3 | Laser | 4 | 1 |
| `weapon_047` | Missile Battery | 4 | Rocket | 2 | 2 |
| `weapon_048` | Next-gen SMG | 1 | Gun | 3 | 1 |
| `weapon_049` | Next-next-gen SMG | 2 | Gun | 2 | 2 |
| `weapon_050` | Guardian Rifle | 1 | Gun | 3 | 1 |
| `weapon_051` | Guardian Railgun | 0 | Hold | 4 | 3 |
| `weapon_052` | Shotgun M1 | 0 | ShotGun | 3 | 3 |
| `weapon_053` | Shotgun M2 | 1 | ShotGun | 2 | 1 |
| `weapon_054` | Shotgun M3 | 2 | ShotGun | 2 | 3 |
| `weapon_055` | SMG M2 | 1 | Gun | 2 | 1 |
| `weapon_056` | Rocket Gun M1 | 4 | Rocket | 16 | 5 |
| `weapon_057` | SMG M1 | 0 | Gun | 3 | 1 |
| `weapon_058` | SMG Helix | 1 | Gun | 2 | 1 |
| `weapon_059` | Prototype Railgun | 2 | Hold | 6 | 5 |
| `weapon_060` | Pioneer | 0 | Pistol | 4 | 1 |
| `weapon_061` | Blind Missile Battery | 0 | Rocket | 4 | 4 |
| `weapon_062` | Machete | 0 | Sword | 6 | 0 |
| `weapon_063` | Broadsword | 0 | Sword | 12 | 0 |
| `weapon_064` | Katana | 0 | Sword | 8 | 0 |
| `weapon_065` | Pirate Saber | 0 | Sword | 7 | 0 |
| `weapon_066` | Double Blade Sword | 0 | Sword | 8 | 0 |
| `weapon_067` | Rapier | 0 | Sword | 6 | 0 |
| `weapon_068` | Axe | 0 | Axe | 6 | 0 |
| `weapon_069` | Battle Axe | 0 | Axe | 10 | 0 |
| `weapon_070` | Flame Axe | 3 | Axe | 8 | 3 |
| `weapon_071` | Mace | 1 | Axe | 13 | 0 |
| `weapon_072` | Pitchfork | 0 | Spear | 8 | 0 |
| `weapon_073` | Spear | 0 | Spear | 8 | 0 |
| `weapon_074` | Laser Sword Blue | 2 | Sword | 8 | 0 |
| `weapon_075` | Laser Sword Red | 2 | Sword | 12 | 0 |
| `weapon_076` | Trident | 2 | Spear | 8 | 3 |
| `weapon_077` | Grand Knight's Sword | 5 | Sword | 8 | 2 |
| `weapon_078` | Flame Sword | 3 | Sword | 8 | 4 |
| `weapon_079` | Frost Sword | 3 | Sword | 8 | 4 |
| `weapon_080` | Bow | 0 | Bow | 4 | 2 |
| `weapon_081` | Strong Bow | 0 | Bow | 4 | 2 |
| `weapon_082` | Composite Bow | 0 | Bow | 4 | 2 |
| `weapon_083` | Hunter Bow | 1 | Bow | 4 | 2 |
| `weapon_084` | Crystal Bow | 2 | Bow | 4 | 3 |
| `weapon_085` | Frost Bow | 2 | Bow | 4 | 3 |
| `weapon_086` | Flame Bow | 2 | Bow | 4 | 3 |
| `weapon_087` | Jade Bow | 2 | Bow | 4 | 3 |
| `weapon_088` | Hero bow | 3 | Bow | 4 | 5 |
| `weapon_089` | Magic Bow | 4 | Bow | 2 | 6 |
| `weapon_090` | Plunger | 2 | Strange | 5 | 2 |
| `weapon_091` | Broom | 1 | Strange | 3 | 1 |
| `weapon_092` | Laser Shotgun | 3 | ShortLaser | 4 | 4 |
| `weapon_093` | Fish | 2 | Strange | 4 | 3 |
| `weapon_094` | Laser Fish | 4 | Strange | 2 | 0 |
| `weapon_095` | Shield | 5 | Strange | 1 | 2 |
| `weapon_096` | Basketball | 0 | Strange | 5 | 1 |
| `weapon_097` | Football | 0 | Strange | 4 | 1 |
| `weapon_098` | Green Onions | 1 | Strange | 4 | 1 |
| `weapon_099` | Meat | 1 | Strange | 5 | 0 |
| `weapon_100` | Badminton Racket | 2 | Strange | 6 | 2 |
| `weapon_101` | Ice Breaker | 2 | ShortLaser | 8 | 3 |
| `weapon_102` | Nasty Laser | 2 | ShortLaser | 5 | 1 |
| `weapon_103` | Arbitrator | 1 | ShortLaser | 3 | 1 |
| `weapon_104` | The Judge | 1 | ShortLaser | 8 | 3 |
| `weapon_105` | Triple Crossbow | 1 | Bow | 6 | 3 |
| `weapon_106` | Crossbow | 0 | Bow | 8 | 2 |
| `weapon_107` | Crossbow Plus | 2 | Bow | 15 | 3 |
| `weapon_108` | Illusion | 4 | Laser | 6 | 2 |
| `weapon_109` | Horn | 1 | Strange | 1 | 2 |
| `weapon_110` | Grenade SMG | 4 | Gun | 3 | 1 |
| `weapon_111` | Bazooka | 1 | Rocket | 8 | 4 |
| `weapon_112` | Magic Staff | 0 | Staff | 4 | 3 |
| `weapon_113` | Fine Magic Staff | 4 | Staff | 4 | 6 |
| `weapon_114` | Staff Of Thunder | 3 | Staff | 4 | 4 |
| `weapon_115` | Staff Of Nature | 0 | Staff | 4 | 2 |
| `weapon_116` | Staff of Skeleton | 5 | Staff | 0 | 5 |
| `weapon_117` | Staff Of Frost | 2 | Staff | 4 | 4 |
| `weapon_118` | Staff Of Flame | 2 | Staff | 4 | 3 |
| `weapon_119` | Staff Of Light | 1 | Staff | 4 | 2 |
| `weapon_120` | Caliburn | 5 | Sword | 10 | 20 |
| `weapon_121` | Electric Therapy | 2 | Laser | 2 | 0 |
| `weapon_122` | Snow Ape's Longbow | 5 | Bow | 8 | 4 |
| `weapon_123` | Staff of Shooting Stars | 5 | Staff | 10 | 9 |
| `weapon_124` | TNT | 0 | Strange | 10 | 3 |
| `weapon_125` | Blowpipe | 1 | Strange | 6 | 2 |
| `weapon_126` | Feathered Crossbow | 1 | Bow | 8 | 3 |
| `weapon_127` | Crystal Crab's Katana | 5 | Sword | 8 | 0 |
| `weapon_128` | Dragon Bros' Sniper Rifle | 5 | Gun | 3 | 0 |
| `weapon_129` | Red Dragon | 0 | Gun | 3 | 1 |
| `weapon_130` | Bleach | 0 | Gun | 5 | 1 |
| `weapon_131` | Sandworm | 5 | ShotGun | 4 | 8 |
| `weapon_132` | Varkolyn Assault Rifle | 5 | Gun | 3 | 0 |
| `weapon_133` | Goblin Spear | 0 | Spear | 5 | 0 |
| `weapon_134` | Cannibal Plant | 5 | ShotGun | 6 | 3 |
| `weapon_135` | One Punch | 5 | Strange | 999 | 99 |
| `weapon_136` | PKP | 0 | Gun | 3 | 1 |
| `weapon_137` | Floating Gun | 5 | Strange | 4 | 1 |
| `weapon_138` | Floating Laser | 5 | Strange | 4 | 1 |
| `weapon_139` | Assault Shotgun | 0 | ShotGun | 3 | 3 |
| `weapon_140` | Cherry Blossom | 2 | ShotGun | 8 | 3 |
| `weapon_141` | Bomber | 5 | Rocket | 4 | 2 |
| `weapon_142` | Bouncing Sniper Rifle | 3 | Rifle | 10 | 4 |
| `weapon_143` | Bouncing Assault Rifle | 1 | Gun | 3 | 1 |
| `weapon_144` | Knight Spear | 1 | Spear | 12 | 0 |
| `weapon_145` | Staff Of Wizard | 5 | Staff | 3 | 3 |
| `weapon_146` | Plunger Plus | 3 | Strange | 6 | 3 |
| `weapon_147` | Charged Railgun | 3 | Hold | 12 | 7 |
| `weapon_148` | Splash Railgun | 2 | Hold | 6 | 6 |
| `weapon_149` | Laser Sword Green | 2 | Sword | 10 | 0 |
| `weapon_150` | Umbrella | 0 | Strange | 4 | 1 |
| `weapon_151` | M14 | 0 | Gun | 4 | 2 |
| `weapon_152` | Old Sniper Rifle | 0 | Rifle | 10 | 6 |
| `weapon_153` | Old Rocket Launcher | 0 | Rocket | 8 | 8 |
| `weapon_154` | Jumper | 2 | ShortLaser | 3 | 2 |
| `weapon_155` | Cleaner | 3 | ShortLaser | 6 | 5 |
| `weapon_156` | Webber | 2 | ShortLaser | 1 | 3 |
| `weapon_157` | Laser | 2 | Spear | 8 | 2 |
| `weapon_158` | Wizard's Staff | 2 | Staff | 3 | 3 |
| `weapon_159` | Cluster Missile | 2 | Rocket | 9 | 4 |
| `weapon_160` | Black Hole Missile | 3 | Rocket | 1 | 1 |
| `weapon_161` | Heavenly Sword | 1 | Strange | 8 | 0 |
| `weapon_162` | Ninja Stars | 0 | Throw | 3 | 6 |
| `weapon_163` | Wrenches | 1 | Throw | 8 | 5 |
| `weapon_164` | Spike Knives | 1 | Throw | 2 | 8 |
| `weapon_165` | Ice Spikes | 2 | Throw | 8 | 8 |
| `weapon_166` | Boomerang | 1 | Throw | 6 | 3 |
| `weapon_167` | Hand Grenade | 3 | Throw | 7 | 8 |
| `weapon_168` | Molotov Cocktail | 0 | Throw | 2 | 3 |
| `weapon_169` | Butcher's Knife | 0 | Sword | 4 | 0 |
| `weapon_170` | Eagle of Ice and Fire | 2 | Pistol | 4 | 2 |
| `weapon_171` | Paper Slip | 0 | Throw | 2 | 3 |
| `weapon_172` | Windforce | 2 | Bow | 4 | 4 |
| `weapon_173` | Rainbow | 3 | ShotGun | 4 | 4 |
| `weapon_174` | Staff of Illusion | 4 | Staff | 0 | 10 |
| `weapon_175` | Snowman Eagle | 5 | Pistol | 4 | 2 |
| `weapon_176` | Agitated Trunk | 5 | Spear | 5 | 1 |
| `weapon_177` | Carrot | 1 | Strange | 4 | 1 |
| `weapon_178` | Mini UZI | 0 | Gun | 1 | 0 |
| `weapon_179` | SMG M3 | 1 | Gun | 4 | 1 |
| `weapon_180` | RYB Assault Rifle | 1 | Gun | 3 | 1 |
| `weapon_181` | Assault Rifle Pro+ | 3 | Gun | 3 | 1 |
| `weapon_182` | Poison Eagle | 1 | Pistol | 4 | 1 |
| `weapon_183` | Grenade Pistol | 1 | Pistol | 4 | 1 |
| `weapon_184` | Next-next-next-gen SMG | 3 | Gun | 2 | 4 |
| `weapon_185` | Bouncing Railgun | 2 | Hold | 4 | 4 |
| `weapon_186` | Bassball Bat | 2 | Strange | 8 | 2 |
| `weapon_187` | Ninja Stars Plus | 2 | Throw | 10 | 6 |
| `weapon_188` | Buckler | 2 | Strange | 8 | 3 |
| `weapon_189` | Iron Claw | 0 | Strange | 1 | 0 |
| `weapon_190` | Sickle | 1 | Axe | 9 | 0 |
| `weapon_191` | Hammer | 0 | Axe | 8 | 0 |
| `weapon_192` | Wooden Hammer | 0 | Axe | 12 | 0 |
| `weapon_193` | Stone Hammer | 2 | Axe | 20 | 0 |
| `weapon_194` | Bow Plus | 2 | Bow | 6 | 5 |
| `weapon_195` | Trekking Pole | 0 | Strange | 0 | 0 |
| `weapon_196` | Void Sword | 2 | Sword | 12 | 2 |
| `weapon_197` | Long-handled Axe | 0 | Axe | 8 | 0 |
| `weapon_198` | Meat Grinder | 3 | Axe | 10 | 0 |
| `weapon_199` | Executioner | 1 | Axe | 13 | 0 |
| `weapon_200` | Laser Therapy | 2 | Laser | 1 | 0 |
| `weapon_201` | Halberd | 1 | Spear | 10 | 0 |
| `weapon_202` | Woodstick | 0 | Spear | 7 | 0 |
| `weapon_203` | Bamboo | 1 | Throw | 5 | 1 |
| `weapon_204` | Javelin | 0 | Throw | 8 | 2 |
| `weapon_205` | Frost Spear | 2 | Throw | 11 | 2 |
| `weapon_206` | Throwing Axe | 0 | Throw | 6 | 1 |
| `weapon_207` | Heavy Hunter Axe | 2 | Throw | 16 | 3 |
| `weapon_208` | Vine | 1 | Strange | 8 | 0 |
| `weapon_209` | Crossbow Air | 1 | Bow | 5 | 2 |
| `weapon_210` | Tidal Staff | 0 | Staff | 3 | 3 |
| `weapon_211` | Reusable Health Pot | 1 | Strange | 4 | 0 |
| `weapon_212` | Reusable Energy Pot | 1 | Strange | 4 | 0 |
| `weapon_213` | Reusable Restoration Potion | 2 | Strange | 4 | 0 |
| `weapon_214` | Extra Crown | 5 | Strange | 0 | 5 |
| `weapon_215` | The Emperor's New Gun | 5 | Strange | 6 | 5 |
| `weapon_216` | Bayonet Rifle | 1 | Rifle | 12 | 3 |
| `weapon_217` | Money Gun | 5 | Gun | 2 | 1 |
| `weapon_218` | Assault Sniper Rifle | 1 | Rifle | 8 | 3 |
| `weapon_219` | EM Sniper Rifle | 2 | Rifle | 6 | 4 |
| `weapon_220` | Explosive Crossbow | 4 | Bow | 12 | 4 |
| `weapon_221` | Explosive Warhammer | 3 | Axe | 8 | 5 |
| `weapon_222` | Thunder Warhammer | 3 | Axe | 8 | 3 |
| `weapon_223` | Green Basin | 1 | Strange | 8 | 1 |
| `weapon_224` | Electric Ninja Stars | 3 | Throw | 3 | 3 |
| `weapon_225` | weapon_225 | None |  | 10 | 0 |
| `weapon_230` | Sawed-off Shotgun | 0 | ShotGun | 8 | 3 |
| `weapon_231` | Groundwater | 0 | Pistol | 3 | 0 |
| `weapon_233` | Rainbow Horse | 4 | Strange | 3 | 1 |
| `weapon_234` | Meteo Laser Gun | 4 | ShortLaser | 1 | 10 |
| `weapon_235` | Green Essence | 0 | Throw | 2 | 3 |
| `weapon_236` | Deadly Frisbee | 0 | Strange | 3 | 2 |
| `weapon_237` | Advanced Scientific Calculator | 0 | Strange | 3 | 2 |
| `weapon_238` | Retouch-free Camera | 0 | Strange | 2 | 2 |
| `weapon_239` | Burp Gun | 1 | Gun | 3 | 1 |
| `weapon_240` | Fantastic Gun | 5 | Pistol | 5 | 2 |
| `weapon_241` | 40m Long Blade | 0 | Sword | 12 | 0 |
| `weapon_242` | Box | 0 | Strange | 0 | 2 |
| `weapon_243` | Grand Wizard's Old Staff | 1 | Staff | 3 | 3 |
| `weapon_244` | Nirvana Cross | 5 | Strange | 0 | 0 |
| `weapon_245` | Laser Sword Gold | 4 | Sword | 8 | 0 |
| `weapon_246` | Thunder Sword | 4 | Sword | 8 | 6 |
| `weapon_247` | Star Bow | 4 | Bow | 4 | 5 |
| `weapon_248` | Weeping Eagle | 3 | Strange | 1 | 3 |
| `weapon_249` | Next-next-next-next-gen SMG | 4 | ShortLaser | 2 | 5 |
| `weapon_250` | Snow Fox Rose Gold | 4 | ShotGun | 4 | 3 |
| `weapon_251` | Nunchaku | 2 | Sword | 4 | 0 |
| `weapon_252` | Super Buddy | 5 | Rocket | 500 | 50 |
| `weapon_253` | Magic Gloves | 5 | Strange | 3 | 4 |
| `weapon_254` | Staff of Anubis | 5 | Staff | 0 | 4 |
| `weapon_255` | Dead Star Laser Gun | 5 | ShortLaser | 6 | 2 |
| `weapon_256` | AK47 Gold | 0 | Gun | 3 | 1 |
| `weapon_257` | Desert Eagle Gold | 0 | Pistol | 6 | 1 |
| `weapon_258` | Blue Fire Gatling | 3 | ShotGun | 2 | 1 |
| `weapon_259` | Coilgun | 0 | Hold | 6 | 3 |
| `weapon_260` | Brownian Coilgun | 2 | ShotGun | 3 | 5 |
| `weapon_261` | Ion Coilgun | 1 | Hold | 8 | 4 |
| `weapon_264` | Bubble Gun | 3 | ShotGun | 1 | 4 |
| `weapon_265` | Lasso Gun | 1 | Gun | 2 | 0 |
| `weapon_266` | Broken Hilt | 0 | Sword | 8 | 0 |
| `weapon_267` | Damaged Blade | 0 | Throw | 5 | 4 |
| `weapon_268` | Reforged Sacred Sword | 3 | Sword | 8 | 0 |
| `weapon_270` | Bayonet in a Gourd | 3 | Sword | 8 | 10 |
| `weapon_273` | Trumpet | 0 | Strange | 0 | 8 |
| `weapon_274` | Fist of Heaven | 4 | Hold | 3 | 6 |
| `weapon_275` | Sweet Talk | 2 | Gun | 2 | 1 |
| `weapon_276` | Death Note | 5 | Strange | 0 | 6 |
| `weapon_277` | Firebolt | 3 | Gun | 2 | 1 |
| `weapon_278` | Double Therapy | 4 | Laser | 1 | 0 |
| `weapon_279` | Grilled Fish | 2 | Strange | 5 | 3 |
| `weapon_280` | Grilled Laser Fish | 4 | Strange | 3 | 0 |
| `weapon_281` | Grilled Green Onion | 1 | Strange | 5 | 1 |
| `weapon_282` | Grilled Meat | 1 | Strange | 6 | 0 |
| `weapon_283` | Grilled Heavenly Sword | 2 | Strange | 10 | 0 |
| `weapon_284` | Grilled Carrot | 1 | Strange | 5 | 1 |
| `weapon_285` | Gold Staff | 2 | Staff | 4 | 3 |
| `weapon_286` | Fine Magic Staff Gold | 5 | Staff | 4 | 6 |
| `weapon_287` | Revolver Gold | 2 | Pistol | 4 | 1 |
| `weapon_288` | Sniper Rifle Gold | 1 | Rifle | 14 | 3 |
| `weapon_289` | Sniper Rifle Gold Pro | 3 | Rifle | 16 | 4 |
| `weapon_290` | Rainbow Gatling | 3 | ShotGun | 2 | 2 |
| `weapon_291` | Fist Bump | 5 | Strange | 8 | 0 |
| `weapon_292` | Pill | 2 | Strange | 3 | 2 |
| `weapon_293` | Happy New Year | 0 | Strange | 2 | 2 |
| `weapon_294` | Ranger's Revolver | 4 | Pistol | 4 | 6 |
| `weapon_295` | Toxic Webber | 3 | ShortLaser | 3 | 3 |
| `weapon_296` | Thunderstorm Warhammer | 4 | Axe | 8 | 6 |
| `weapon_297` | Ion Railgun M2 | 2 | Hold | 8 | 4 |
| `weapon_298` | EM Railgun | 3 | Hold | 4 | 3 |
| `weapon_299` | Giant Axe | 2 | Throw | 6 | 0 |
| `weapon_300` | Verbal Fire | 3 | Strange | 3 | 4 |
| `weapon_301` | Golden Cudgel | 5 | Spear | 6 | 0 |
| `weapon_900` | Candied Hawberries | None |  | 6 | 0 |
| `weapon_901` | Firecrackers | None |  | 5 | 3 |
| `weapon_902` | Channeling Monkey | None |  | 2 | 10 |
| `weapon_903` | Rocket Fireworks | None |  | 12 | 4 |

## UNKNOWN / not claimed

- Attack cadence is only recovered for the four integrated weapons.
- Melee/non-projectile entries without a `bulletsInfo` component are
  listed by name and metadata but do not claim bullet damage.
- Skin/variant localization keys that do not correspond to a base
  `weapon_XXX` prefab are not treated as separate weapons.
