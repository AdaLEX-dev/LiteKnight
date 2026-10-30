# Soul Knight 2.6.8 — Reference Artifact Record

## Immutable Reference APK

- **File:** `original/SoulKnight-2.6.8.apk`
- **SHA-256:** `8faff89079eff748770c28f0ff2243baa88388b352d0128e67ae3ef5aa0137ea`
- **Size:** 104,700,849 bytes (~99.8 MB)
- **Source:** APKMirror (com.ChillyRoom.DungeonShooter_2.6.8-20680_minAPI16)
- **Date acquired:** 2026-10-30

## Package Metadata

| Field | Value |
|-------|-------|
| Package | `com.ChillyRoom.DungeonShooter` |
| Version Code | `20680` |
| Version Name | `2.6.8` |
| Compile SDK | 28 (Android 9) |
| Min SDK | 16 (Android 4.1) |
| Target SDK | 28 (Android 9) |
| Install Location | auto |

## Signing

- **Status:** Signed (release)
- **Certificates:** META-INF/MANIFEST.MF, META-INF/CERT.SF, META-INF/CERT.RSA present
- **Note:** Original production signature preserved; not re-signed

## Architecture

- **Engine:** Unity (IL2CPP backend)
- **Primary Native Library:** `libil2cpp.so` (52.6 MB arm64, 42.5 MB armv7)
- **Unity Version:** 2021.x (evidenced by libunity.so, unity metadata)
- **Java/Kotlin Layer:** Minimal — UnityPlayerActivity + third-party SDKs only
- **Game Logic:** Entirely in native IL2CPP compilation (C# → C++)

## DEX Files

| File | Size | Purpose |
|------|------|---------|
| `classes.dex` | ~3.2 MB | Unity + SDK bootstrap |
| `classes2.dex` | ~1.8 MB | Additional SDKs |
| `assets/audience_network.dex` | ~3.3 MB | Facebook Audience Network |

**Total DEX classes:** ~10,446 (per jadx)
**Game-specific Java classes:** ~5 (BuildConfig, R*, resources)

## Native Libraries (arm64-v8a)

| Library | Size | Role |
|---------|------|------|
| `libil2cpp.so` | 52,641,496 B | **Main game logic** (C# → C++) |
| `libunity.so` | 16,955,952 B | Unity engine runtime |
| `libmain.so` | 6,144 B | Unity entry point (JNI_OnLoad) |
| `libgpg.so` | 3,718,560 B | Google Play Games C++ SDK |
| `libFirebaseCppApp-5.7.0.so` | 2,525,392 B | Firebase C++ SDK |
| `libFirebaseCppAnalytics.so` | 68,160 B | Firebase Analytics |

## Native Libraries (armeabi-v7a)

| Library | Size |
|---------|------|
| `libil2cpp.so` | 42,501,504 B |
| `libunity.so` | 15,563,560 B |
| `libmain.so` | 22,476 B |
| `libgpg.so` | 2,489,616 B |
| `libFirebaseCppApp-5.7.0.so` | 1,707,468 B |
| `libFirebaseCppAnalytics.so` | 26,512 B |

## Permissions

| Permission | Max SDK | Purpose |
|------------|---------|---------|
| INTERNET | — | Network access (ads, analytics, multiplayer) |
| ACCESS_NETWORK_STATE | — | Connectivity detection |
| WRITE_EXTERNAL_STORAGE | 18 | Legacy storage (pre-Scoped Storage) |
| ACCESS_WIFI_STATE | — | Wi-Fi state for analytics |
| VIBRATE | — | Haptic feedback |
| WAKE_LOCK | — | Keep screen on during gameplay |
| BIND_GET_INSTALL_REFERRER_SERVICE | — | Attribution (AppsFlyer) |
| RECEIVE (C2DM) | — | Push notifications (deprecated) |
| BILLING | — | In-app purchases |
| READ_EXTERNAL_STORAGE | — | Legacy storage read |

## Activities

| Activity | Role |
|----------|------|
| `com.unity3d.player.UnityPlayerActivity` | **Main entry** (singleTask, sensorLandscape) |
| `com.facebook.unity.FBUnityLoginActivity` | Facebook login |
| `com.facebook.unity.FBUnityDialogsActivity` | Facebook dialogs |
| `com.facebook.unity.FBUnityAppLinkActivity` | App Links |
| `com.facebook.unity.FBUnityDeepLinkingActivity` | Deep linking |
| `com.facebook.unity.FBUnityGameRequestActivity` | Game requests |
| `com.facebook.unity.FBUnityCreateGameGroupActivity` | Game groups |
| `com.facebook.unity.FBUnityJoinGameGroupActivity` | Game groups |
| `com.facebook.unity.AppInviteDialogActivity` | App invites |
| `com.adcolony.sdk.AdColonyInterstitialActivity` | AdColony ads |
| `com.adcolony.sdk.AdColonyAdViewActivity` | AdColony ads |
| `com.facebook.ads.AudienceNetworkActivity` | Facebook Audience Network |
| `com.unity3d.services.ads.adunit.AdUnitActivity` | Unity Ads |
| `com.chartboost.sdk.CBImpressionActivity` | Chartboost ads |
| `com.applovin.adview.AppLovinInterstitialActivity` | AppLovin ads |
| `com.applovin.sdk.AppLovinWebViewActivity` | AppLovin web view |
| `com.google.android.gms.ads.AdActivity` | AdMob ads |
| `com.vungle.warren.ui.VungleActivity` | Vungle ads |
| `com.vungle.warren.ui.VungleFlexViewActivity` | Vungle flex ads |
| `com.unity.purchasing.googleplay.PurchaseActivity` | IAP (Google Play) |
| `com.unity.purchasing.googleplay.VRPurchaseActivity` | IAP (VR) |
| `com.ironsource.sdk.controller.ControllerActivity` | IronSource ads |
| `com.ironsource.sdk.controller.InterstitialActivity` | IronSource ads |
| `com.ironsource.sdk.controller.OpenUrlActivity` | IronSource ads |
| `com.facebook.FacebookActivity` | Facebook SDK |
| `com.facebook.CustomTabMainActivity` | Facebook Custom Tabs |
| `com.facebook.CustomTabActivity` | Facebook Custom Tabs |
| `com.inmobi.ads.rendering.InMobiAdActivity` | InMobi ads |
| `jp.maio.sdk.android.AdFullscreenActivity` | Maio ads |
| `jp.maio.sdk.android.HtmlBasedAdActivity` | Maio ads |
| `com.mintegral.msdk.activity.MTGCommonActivity` | Mintegral ads |
| `com.mintegral.msdk.reward.player.MTGRewardVideoActivity` | Mintegral rewarded video |
| `com.my.target.common.MyTargetActivity` | myTarget ads |
| `com.tapjoy.TJAdUnitActivity` | Tapjoy ads |
| `com.tapjoy.mraid.view.ActionHandler` | Tapjoy MRAID |
| `com.tapjoy.mraid.view.Browser` | Tapjoy MRAID |
| `com.tapjoy.TJContentActivity` | Tapjoy content |
| `com.ktplay.activity.KTMPermissionsActivity` | KTPlay permissions |
| `com.ktplay.activity.KTShareActivity` | KTPlay sharing |
| `com.amazon.device.ads.AdActivity` | Amazon ads |
| `com.google.games.bridge.NativeBridgeActivity` | Google Play Games |

## Services / Receivers / Providers

- **Firebase:** Analytics, Messaging, Installations, Crashlytics (implied), Auth
- **Google Play Games:** Sign-in, Achievements, Leaderboards
- **AppsFlyer:** Install attribution
- **Facebook:** Analytics, Login, Sharing, App Links
- **Unity Ads / IAP / Analytics:** Monetization & services
- **KTPlay:** Community features
- **Multiple ad networks:** AdColony, AppLovin, Chartboost, IronSource, InMobi, Maio, Mintegral, myTarget, Tapjoy, Vungle, Amazon

## Asset Bundles (assets/AssetBundles/)

| Bundle | Size | Manifest | Likely Content |
|--------|------|----------|----------------|
| `common` | 10.5 MB | ✓ | Shared assets, UI, fonts |
| `level-objects` | 11.6 MB | ✓ | Room objects, props, enemies |
| `scene_plot_defence` | 2.6 MB | ✓ | Defence mode scene |
| `scene_plot` | 11 KB | ✓ | Main plot scenes |
| `scene_game` | 563 KB | ✓ | Core gameplay scene |
| `scene_tutorial` | 563 KB | ✓ | Tutorial scene |
| `hall` | 1.3 MB | ✓ | Main menu / lobby |
| `celler` | 324 KB | ✓ | Cellar/basement content |
| `defence` | 4.4 MB | ✓ | Defence mode |
| `draw` | 636 KB | ✓ | Drawing/gacha system |
| `loading` | 292 KB | ✓ | Loading screens |
| `multi_room` | 663 KB | ✓ | Multiplayer rooms |
| `race` | 53 KB | ✓ | Race mode |
| `room/` | (dir) | — | Individual room bundles |
| `res/` | (dir) | — | Resource bundles |
| `sandbox` | 557 KB | ✓ | Sandbox mode |
| `statements` | 370 KB | ✓ | Achievement statements |
| `test` | 734 KB | ✓ | Test/debug content |
| `tutorial` | 256 KB | ✓ | Tutorial |
| `toturial_advance` | 812 KB | ✓ | Advanced tutorial |
| `level-1` | 25 KB | ✓ | Level 1 data |
| `level-common` | 1.1 MB | ✓ | Common level data |

## Key Files

| File | Description |
|------|-------------|
| `assets/AssetBundles/AssetBundles` | Master bundle index |
| `assets/AssetBundles/AssetBundles.manifest` | Bundle dependency manifest |
| `assets/bundleVersionData.txt` | Bundle version info |
| `assets/DefineCloudSaveID.json` | Cloud save identifiers |
| `assets/google-services-desktop.json` | Firebase config (desktop placeholder) |
| `assets/sensitive_words.txt` | Chat filter (empty in this build) |

## Analysis Status

- ✅ APK hash recorded
- ✅ Manifest parsed
- ✅ Native libraries inventoried
- ✅ DEX structure mapped
- ✅ Asset bundles catalogued
- ⏳ IL2CPP metadata extraction (pending)
- ⏳ Class/method mapping from libil2cpp.so (pending)
- ⏳ Runtime behaviour observation (pending)

---

**Classification:** CONFIRMED — This is a Unity IL2CPP game. All gameplay logic resides in `libil2cpp.so`. The Java layer is purely a Unity bootstrap + third-party SDK container.