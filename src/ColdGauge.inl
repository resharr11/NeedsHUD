// The cold thermometer, approved design (mockup scratchpad mock/_m_chrome.py, 2026-11-04):
//  - the vanilla HUD bar turned upright: its own top ornament (hudmenu.swf shape 730, VanillaFrame.inl), then the
//    bar's bands (dark outer band, light line, see-through dark inside) running down the sides and round the bulb
//    as one piece of glass;
//  - mercury: one silver shape (column, flared neck, ball) with the bars' one-sided sheen; the ball is a mirror ball
//    (darker toward the bottom, dark rim, crisp highlight upper left);
//  - the scale: one strip on the left with six equal blocks: warm (only while a heat source warms you), Comfortable
//    (empty), Chilly, Very Cold, Freezing, Numb. The marks never move;
//  - the stage name (and, as a test aid, the raw cold value) under it.
//  - cold protection from a config file's [ColdProtection] (Cooking Skill Overhaul's Spicy; his design 2026-11-15, after
//    Zelda's gauge): while a buff makes every place count N stages warmer, each cold block takes the colour of the stage
//    N above it (the top N become unpainted, like Comfortable), so the safe zone grows and the bottom block shows the
//    worst you can reach; no extra line or mark. The place arrow never moves for it: the colour under the arrow says
//    what the place can still do to you. While the buff keeps freezing water harmless, the water look stays away.
//  - red light line: only while your own cold is in the last stage (Numb, Frigid).
//
// Survival Mode's cold value (ccQDRSSE001-SurvivalMode.esl Survival_ColdNeedValue 0x81B) runs 0-1000:
// Comfortable < 120, Chilly < 300, Very Cold < 500, Freezing < 800, Numb up to 1000 (death). It never goes below 0;
// being warmed by a fire is Survival's Update.esm global Survival_TemperatureLevel (0x2EDD) = 1 ("near heat";
// read from Survival_NeedColdQuest's own properties: temperatureLevelNearHeat = 1).

	constexpr RE::FormID COLD_VALUE = 0x81B;     // Survival_ColdNeedValue
	constexpr RE::FormID SURVIVAL_ON = 0x826;    // Survival_ModeEnabled
	constexpr RE::FormID TEMPERATURE_LEVEL = 0x2EDD;  // Update.esm Survival_TemperatureLevel
	constexpr const char* SURVIVAL_FILE = "ccQDRSSE001-SurvivalMode.esl";

	// vanilla Survival's cold stage bounds; Survival Mode Improved - SKSE keeps its own in globals (read at load)
	std::array<double, 6> STAGE_VALUES{ 0.0, 120.0, 300.0, 500.0, 800.0, 1000.0 };
	constexpr std::array<const char*, 5> STAGE_NAMES{ "Comfortable", "Chilly", "Very Cold", "Freezing", "Numb" };

	constexpr std::uint32_t LABEL = 0xECE7DB;   // text
	constexpr std::uint32_t RED = 0xE0705A;     // Numb
	constexpr std::uint32_t CYAN = 0x57E4F4;    // glow while the cold hurts you
	constexpr std::uint32_t BAND_DARK = 0x231F20, BAND_LIGHT = 0xBBBDBF;  // the vanilla frame's two colours (60% alpha)
	constexpr std::uint32_t INSIDE = 0x121011;  // the tube's near-black inside

	#include "VanillaFrame.inl"

	// the options the gauge draws with (only the UI task touches this copy); new ones from the settings page wait in
	// g_coldPending until the next tick takes them (see Settings.h)
	NeedsHUD::ColdOptions g_cold;
	NeedsHUD::ColdOptions g_coldPending;
	std::mutex            g_coldLock;
	std::atomic_bool      g_coldChanged{ false };

	RE::TESGlobal* g_coldValue = nullptr;
	RE::TESGlobal* g_survivalOn = nullptr;
	// Survival_FreezingWaterDamage: Survival gives it to you while you swim in freezing water (Survival_FreezingWaterCheck,
	// checked every 3 s) and takes it away when you get out
	constexpr RE::FormID FREEZING_WATER_SPELL = 0x882;
	RE::SpellItem* g_freezingWater = nullptr;
	// Survival's afflictions (24 h abilities; read from the esl 2026-11-08): Weakened 0x910 (hunger), Addled 0x911 (lack of
	// sleep), Frostbitten 0x913 (cold); SMI and Starfrost keep the same records
	RE::SpellItem* g_afflictions[3]{};
	// afflicted only while one of its effects still runs (his test 2026-11-09: a Cure Disease potion cures afflictions too,
	// "like other diseases" per UESP, but HasSpell kept saying yes and the Addled badge stayed)
	bool Afflicted(int a_kind)
	{
		if (!g_afflictions[a_kind]) {
			return false;
		}
		for (const auto& e : g_view.effects) {  // the main-thread snapshot (Snapshot.inl)
			if (e.spell == g_afflictions[a_kind] && !e.dispelled) {
				return true;
			}
		}
		return false;
	}
	// hunger (the hunger tile, HungerTile.inl): Survival_HungerNeedValue 0..1000 and the Well Fed bonus spell
	constexpr RE::FormID HUNGER_VALUE = 0x81A, WELL_FED_SPELL = 0x876;
	RE::TESGlobal*  g_hungerValue = nullptr;
	RE::SpellItem*  g_wellFed = nullptr;
	// tiredness (the tiredness tile, TiredTile.inl): Survival_ExhaustionNeedValue and its max (globals), and the three
	// rested bonuses Survival gives after a good sleep (Skyrim.esm spells, its Survival_NeedExhaustion.ApplyNormalRestedBonus)
	constexpr RE::FormID TIRED_VALUE = 0x816, TIRED_MAX = 0x84A;
	constexpr RE::FormID RESTED_SPELL = 0xFB981, WELL_RESTED_SPELL = 0xFB984, LOVERS_COMFORT_SPELL = 0xCDA1D;
	RE::TESGlobal* g_tiredValue = nullptr;
	RE::TESGlobal* g_tiredMax = nullptr;
	RE::SpellItem* g_rested = nullptr;
	RE::SpellItem* g_wellRested = nullptr;
	RE::SpellItem* g_loversComfort = nullptr;
	// Free Cooking (his cooking mod) as a provider: its cooked drinks carry FC_Refreshing, which fully counters Drained
	// while it lasts (a "fake Refreshed"); the tiredness tile shows it with the bronze cup badge (mockups refresh.png)
	constexpr const char* FREE_COOKING_FILE = "FreeCooking.esp";
	constexpr RE::FormID  FC_REFRESHING = 0xA6A;
	RE::EffectSetting*    g_fcRefreshing = nullptr;

	// ---- other needs mods as sources (2026-11-08, his "make our mod flexible, use their names and number of
	// stages"; read from their own files, see memory needs-mods-data) ----
	// SunHelm Survival: its own need values (0 = fine, rising) and switches (SunHelmSurvival.esp, a full esp)
	constexpr const char* SUNHELM_FILE = "SunHelmSurvival.esp";
	RE::TESGlobal* g_shEnabled = nullptr;   // 1 while SunHelm runs
	RE::TESGlobal* g_shHunger = nullptr;
	RE::TESGlobal* g_shThirst = nullptr;
	RE::TESGlobal* g_shFatigue = nullptr;
	RE::TESGlobal* g_shCold = nullptr;
	RE::TESGlobal* g_shHungerOff = nullptr;  // its MCM's per-need switches (1 = that need is off)
	RE::TESGlobal* g_shThirstOff = nullptr;
	RE::TESGlobal* g_shFatigueOff = nullptr;
	RE::TESGlobal* g_shColdOff = nullptr;
	RE::TESGlobal* g_shColdForceOff = nullptr;
	RE::TESGlobal* g_shAmbient = nullptr;    // the cold this place drags you to (our place arrow)
	RE::TESGlobal* g_shNearHeat = nullptr;
	RE::TESGlobal* g_shInWater = nullptr;    // in freezing water
	RE::TESGlobal* g_shColdCap = nullptr;
	// Survival Mode Improved - SKSE: writes vanilla Survival's own values, but keeps its stage limits in globals
	constexpr const char* SMI_FILE = "SurvivalModeImproved.esp";
	std::array<double, 6> g_hungerBoundsSmi{ -1.0 };  // [0] < 0: not found (vanilla's own limits are used)
	std::array<double, 6> g_tiredBoundsSmi{ -1.0 };
	RE::TESGlobal* g_smiAmbient = nullptr;  // SMI_CurrentAmbientTemp: the cold this place drags you to (600+ = all the way)
	constexpr double SMI_AMBIENT_MAX = 600.0;  // its NeedCold ColdMaxStageThreshold
	double g_smiColdStage1 = -1.0;          // SMI's cold stage 1 limit (Starfrost: Warm ends there)
	// Starfrost (runs on SMI; its own stage names, only three stages per need mean something, read 2026-11-08 from
	// its 2.0.0 esp): its meal bonuses (Starfrost.esp SPEL Fed 0x80E, Well Fed 0x813, Lover's Cooking 0x817)
	constexpr const char* STARFROST_FILE = "Starfrost.esp";
	bool              g_starfrost = false;
	RE::SpellItem*    g_sfFed[3]{ nullptr, nullptr, nullptr };
	constexpr const char* SF_FED_NAMES[3] = { "Fed", "Well Fed", "Lover's Cooking" };
	int               g_coldSlots = 6;      // the thermometer's blocks: 6 (Warm, Comfortable, 4 colder); Starfrost 5
	// Frostfall 3.4.1 (Chesko; read 2026-11-09 from its esp + script sources): its exposure is the cold, 0..120, rising as
	// you get colder, in six 20-point bands (_frost_exposuresystem.psc): warm, comfortable, chilly, very cold, freezing,
	// freezing to death (120 = death or its max). Its booleans are 2 = yes, 1 = no
	constexpr const char* FROSTFALL_FILE = "Frostfall.esp";
	RE::TESGlobal* g_ffRunning = nullptr;     // FrostfallRunning: 2 once started (MCM or its stars prompt)
	RE::TESGlobal* g_ffExposureOn = nullptr;  // _Frost_Setting_ExposureOn
	RE::TESGlobal* g_ffExposure = nullptr;    // _Frost_AttributeExposure
	RE::TESGlobal* g_ffTarget = nullptr;      // _Frost_ExposureTarget: where the exposure is heading here (our place arrow)
	RE::TESGlobal* g_ffNearFire = nullptr;    // _Frost_NearFire
	RE::TESGlobal* g_ffWarmth = nullptr;      // _Frost_AttributeWarmth: Frostfall's own warmth of what you wear
	RE::TESGlobal* g_ffWet = nullptr;         // _Frost_AttributeWetness: 0..750
	constexpr double FF_MAX_EXPOSURE = 120.0;
	// Frostfall's wet levels (_frost_wetnesssystem.psc): damp above 200, wet above 550, drenched above 700, max 750. Only
	// these three change your cold (its exposure target +10/+20/+40), so only they are shown (his call 2026-11-09: three
	// equal blocks, hidden while dry)
	constexpr double FF_WET_BOUNDS[4] = { 200.0, 550.0, 700.0, 750.0 };
	constexpr const char* FF_WET_NAMES[3] = { "Damp", "Wet", "Drenched" };
	int FrostfallWetLevel(double a_wet)  // -1 dry, 0 damp, 1 wet, 2 drenched
	{
		if (a_wet <= FF_WET_BOUNDS[0]) {
			return -1;
		}
		return a_wet > FF_WET_BOUNDS[2] ? 2 : (a_wet > FF_WET_BOUNDS[1] ? 1 : 0);
	}

	// Frostfall's cold water (read 2026-11-09 from Frostfall.esp _Frost_ColdWaterSpell and _frost_coldwatereffect*.psc):
	// while swimming away from a fire, one of its effects runs by the water's temperature: cold (5-10 degrees; exposure
	// toward 40), extremely cold (0-5; toward 60), frigid (below 0; toward 80, and with "frigid water is lethal" on, the
	// default, you black out and die after 30 s + 15 s per Glacial Swimmer rank). Cool water (10+) does nothing.
	// We read which effect runs, so the gauge always agrees with Frostfall (his call 2026-11-09: cold and extremely cold
	// show the water look with the true mercury; frigid and lethal is Survival's treatment: sink + "Deadly")
	RE::EffectSetting* g_ffWaterFx[4] = {};  // cold, extremely cold, frigid, frigid (Glacial Swimmer: no stamina loss)
	RE::TESGlobal*     g_ffWaterLethal = nullptr;  // _Frost_Setting_FrigidWaterIsLethal (2 = on)
	constexpr const char* FF_WATER_NAMES[4] = { "", "Cold Water", "Icy Water", "Frigid Water" };
	std::atomic_int g_ffWaterLvl{ 0 };  // 0 none, 1 cold, 2 extremely cold, 3 frigid (set by the gauge's tick)
	int FrostfallWaterLevel()
	{
		if (!g_ffWaterFx[0]) {
			return 0;
		}
		int lvl = 0;
		for (const auto& e : g_view.effects) {  // the main-thread snapshot (Snapshot.inl)
			if (e.dispelled || e.inactive) {
				continue;
			}
			for (int i = 0; i < 4 && e.mgef; ++i) {
				if (e.mgef == g_ffWaterFx[i]) {
					lvl = std::max(lvl, i < 3 ? i + 1 : 3);
				}
			}
		}
		return lvl;
	}
	bool FrostfallWaterDeadly(int a_lvl) { return a_lvl == 3 && (!g_ffWaterLethal || GV(g_ffWaterLethal) >= 2.0f); }

	bool SunHelmOn() { return g_shEnabled && GV(g_shEnabled) >= 1.0f; }
	// its drinking (_SunHelmMain.AlcoholDrink, read 2026-11-09): the property _SHDrinksConsumed counts drinks; at _SHNumDrinks
	// (its MCM slider) it casts _SHDrunkSpell (120 s) and starts over; each thirst update also resets the count
	RE::TESQuest*  g_shMainQuest = nullptr;  // _SHMainQuest (script _SunHelmMain)
	RE::TESGlobal* g_shNumDrinks = nullptr;
	RE::SpellItem* g_shDrunkSpell = nullptr;

	// ---- Last Seed 5.3 (read 2026-11-09 from LastSeed.esp + its script sources; data in memory needs-mods-data) ----
	constexpr const char* LASTSEED_FILE = "LastSeed.esp";
	RE::TESGlobal* g_lsRunning = nullptr;   // LastSeedRunning: 2 while it runs
	RE::TESGlobal* g_lsValue[4] = {};       // hunger, thirst, fatigue (0..120, rising = worse), vitality (0..160, higher = better)
	RE::TESGlobal* g_lsOn[4] = {};          // _Seed_Setting_SystemEnabled_*: 2 = that need is on
	RE::TESGlobal* g_lsVitalityTarget = nullptr;  // _Seed_VitalityTarget: where vitality is heading
	RE::SpellItem* g_lsRested[5] = {};      // its rested bonuses: Rested, Well Rested, Lover's Comfort, Father's Love, Mother's Love
	bool LastSeedOn() { return g_lsRunning && GV(g_lsRunning) >= 2.0f; }
	bool LastSeedNeedOn(int a_i) { return LastSeedOn() && g_lsValue[a_i] && !(g_lsOn[a_i] && GV(g_lsOn[a_i]) < 2.0f); }
	// its alcohol (_Seed_AlcoholBaseSystem, read 2026-11-09): _Seed_AttributeDrunk 0..120, five levels by 20 (Sober, Relaxed,
	// Tipsy, Drunk, Very Drunk; SeedUtil's own words), you pass out at 100; on while _Seed_Setting_AlcoholSystemEnabled is 2
	RE::TESGlobal* g_lsDrunk = nullptr;
	RE::TESGlobal* g_lsDrunkOn = nullptr;

	// ---- iNeed 1.90 (read 2026-11-09 from iNeed.esp + its script sources in iNeed.bsa): its needs live in its quest
	// script _SNQuestScript (on quest _SNQuest), not in globals: HungerState / ThirstState / FatigueState 0..110, HIGHER =
	// better; a need with rate 0 is switched off in its MCM. Its stage is the penalty global: -2 well sated/hydrated/rested
	// (> 95 after eating/drinking/sleeping), -1 fine (> 50), 0 "a bit" (<= 50), then at 0 its debuffs grow with time:
	// 1 mild, 2 moderate, 3 severe
	constexpr const char* INEED_FILE = "iNeed.esp";
	RE::TESQuest*  g_inQuest = nullptr;
	RE::TESGlobal* g_inPenalty[3] = {};  // _SNHungerPenalty, _SNThirstPenalty, _SNFatiguePenalty
	// main thread (CaptureSnapshot): quest flags are live game state; the UI thread reads the copy (1.0.3 second review)
	bool INeedQuestRunning() { return g_inQuest && g_inQuest->IsRunning(); }
	bool INeedOn() { return g_inQuest && g_view.iNeedRunning; }
	// its drinking (_SNQuestScript.Drunk, read 2026-11-09): the plain variable DrunkCount counts drinks (each restarts a
	// ~3 minute timer that clears it); past 3 (men) or 2 (women, property IsMale) the next drink adds _SNDrunkSpell
	RE::SpellItem* g_inDrunkSpell = nullptr;

	// ---- Realistic Needs and Diseases 2.3.6 (read 2026-11-09 from RealisticNeedsandDiseases.esp + its script sources):
	// hunger, thirst and sleep are point globals RISING = worse, capped at their last level global; stage = the first
	// level the points reach (RND_*CountScript); runs while RND_State is 1 (started in its MCM)
	constexpr const char* RND_FILE = "RealisticNeedsandDiseases.esp";
	RE::TESGlobal* g_rndState = nullptr;
	RE::TESGlobal* g_rndPoints[3] = {};    // RND_HungerPoints, RND_ThirstPoints, RND_SleepPoints
	RE::TESGlobal* g_rndLevels[3][6] = {};  // RND_*Level00.. (hunger 6 levels, thirst and sleep 5)
	bool RndOn() { return g_rndState && GV(g_rndState) >= 1.0f && g_rndPoints[0]; }
	// its drunkenness (RND_InebriationCountScript): points 0..cap (Level04, 150), +10/30/45 a drink, -10 an hour; Sober below
	// Level01 (30), Dizzy, Drunk, Wasted from Level03 (90). Its body weight (RND_HungerTimerScript): the player's own body
	// slider 0..100, moved by hunger, only while its "Weight Feature" (RND_DisableWeightGlobal, misnamed: 1 = on) is on
	RE::TESGlobal* g_rndDrunk = nullptr;      // RND_InebriationPoints
	RE::TESGlobal* g_rndDrunkLv[5] = {};      // RND_InebriationLevel00..04
	RE::TESGlobal* g_rndWeightOn = nullptr;   // RND_DisableWeightGlobal
	// ---- the Papyrus bridges (NeedsHUD.esp, 1.0.3): values other needs mods keep only inside their scripts. The C++
	// plugin never reads script properties itself (rule for all his plugins; 1.0.2 did it from the UI thread and a
	// player crashed loading a save with SunHelm), so NeedsHUD_ModBridgeEffectScript copies them into these globals
	// once a second; -1 = not known yet (bridge not run, mod's quest not running)
	constexpr RE::FormID MOD_BRIDGE_SPELL = 0x80C;   // SPEL NeedsHUD_ModBridge (ability, hidden)
	RE::SpellItem*  g_modBridgeSpell = nullptr;
	std::atomic_bool g_modBridgeCheck{ true };       // make sure the player has it (set again on every load)
	RE::TESGlobal*  g_bridgeSHDrinks = nullptr;      // 0x803 SunHelm _SHDrinksConsumed
	RE::TESGlobal*  g_bridgeINState[3] = {};         // 0x804-0x806 iNeed HungerState / ThirstState / FatigueState
	RE::TESGlobal*  g_bridgeINRate[3] = {};          // 0x807-0x809 iNeed HungerRate / ThirstRate / FatigueRate
	RE::TESGlobal*  g_vampireSeenDay = nullptr;      // 0x80A when we first saw Sanguinare (GameDaysPassed), -1 = none
	RE::TESGlobal*  g_bridgeMods = nullptr;          // 0x80D which bridges run: 1 SunHelm + 2 iNeed (set after every load)
	double BridgeValue(RE::TESGlobal* a_g) { return a_g ? static_cast<double>(GV(a_g)) : -1.0; }
	// iNeed's need a_i (0 hunger, 1 thirst, 2 fatigue): its state 0..110, or its rate (0 = switched off); -1 if not known
	double INeedState(int a_i) { return BridgeValue(g_bridgeINState[a_i]); }
	double INeedRate(int a_i) { return BridgeValue(g_bridgeINRate[a_i]); }
	bool FrostfallOn()
	{
		return g_ffExposure && g_ffRunning && GV(g_ffRunning) >= 2.0f && !(g_ffExposureOn && GV(g_ffExposureOn) < 2.0f);
	}
	bool GlobalOn(RE::TESGlobal* a_value, RE::TESGlobal* a_off) { return a_value && !(a_off && GV(a_off) >= 1.0f); }

	// a plugin that is actually loaded (ESL or not). LookupModByName also finds plugins that are only installed: an
	// unticked plugin then logged as "values MISSING" (his RND test 2026-11-09), so such a file is named in the log once
	const RE::TESFile* ModActive(RE::TESDataHandler* a_data, const char* a_file)
	{
		if (!a_data || !a_file) {
			return nullptr;
		}
		if (auto f = a_data->LookupLoadedModByName(a_file)) {
			return f;
		}
		if (auto f = a_data->LookupLoadedLightModByName(a_file)) {
			return f;
		}
		static std::vector<std::string> told;
		if (a_data->LookupModByName(a_file) && std::find(told.begin(), told.end(), a_file) == told.end()) {
			told.emplace_back(a_file);
			logger::info("{} is installed but not active (unticked in the plugin list): NeedsHUD ignores it", a_file);
		}
		return nullptr;
	}

	void FindOtherNeedsMods(RE::TESDataHandler* a_data)
	{
		const auto glob = [&](RE::FormID a_id, const char* a_file) { return LookupGlobal(a_data, a_id, a_file); };
		if (ModActive(a_data, SUNHELM_FILE)) {
			g_shEnabled = glob(0x02EB63, SUNHELM_FILE);
			g_shHunger = glob(0x00EAAE, SUNHELM_FILE);
			g_shThirst = glob(0x05C472, SUNHELM_FILE);
			g_shFatigue = glob(0x021E3F, SUNHELM_FILE);
			g_shCold = glob(0x6A13C5, SUNHELM_FILE);
			g_shHungerOff = glob(0x752707, SUNHELM_FILE);
			g_shMainQuest = a_data->LookupForm<RE::TESQuest>(0x000D61, SUNHELM_FILE);
			g_shNumDrinks = glob(0x9DF881, SUNHELM_FILE);
			g_shDrunkSpell = a_data->LookupForm<RE::SpellItem>(0x377265, SUNHELM_FILE);
			logger::info("SunHelm drinking: quest {}, drinks limit {}, drunk spell {}", g_shMainQuest ? "found" : "MISSING",
				g_shNumDrinks ? "found" : "MISSING", g_shDrunkSpell ? "found" : "MISSING");
			g_shThirstOff = glob(0x752708, SUNHELM_FILE);
			g_shFatigueOff = glob(0x752709, SUNHELM_FILE);
			g_shColdOff = glob(0x75270A, SUNHELM_FILE);
			g_shColdForceOff = glob(0x83132C, SUNHELM_FILE);
			g_shAmbient = glob(0x989757, SUNHELM_FILE);
			g_shNearHeat = glob(0x88743B, SUNHELM_FILE);
			g_shInWater = glob(0x88743C, SUNHELM_FILE);
			g_shColdCap = glob(0xA08085, SUNHELM_FILE);
			logger::info("SunHelm: switch {}, hunger {}, thirst {}, fatigue {}, cold {}, place cold {}, heat {}, water {}",
				g_shEnabled ? "found" : "MISSING", g_shHunger ? "found" : "MISSING", g_shThirst ? "found" : "MISSING",
				g_shFatigue ? "found" : "MISSING", g_shCold ? "found" : "MISSING", g_shAmbient ? "found" : "MISSING",
				g_shNearHeat ? "found" : "MISSING", g_shInWater ? "found" : "MISSING");
		}
		if (ModActive(a_data, LASTSEED_FILE)) {
			g_lsRunning = glob(0x00B162, LASTSEED_FILE);
			constexpr RE::FormID vals[4] = { 0x0029CD, 0x0029CE, 0x0029CF, 0x004FC4 };
			constexpr RE::FormID ons[4] = { 0x007073, 0x007074, 0x007075, 0x007076 };
			for (int i = 0; i < 4; ++i) {
				g_lsValue[i] = glob(vals[i], LASTSEED_FILE);
				g_lsOn[i] = glob(ons[i], LASTSEED_FILE);
			}
			g_lsVitalityTarget = glob(0x44B08A, LASTSEED_FILE);
			constexpr RE::FormID rested[5] = { 0x1BDD20, 0x1BDD22, 0x1BDD23, 0x1BDD24, 0x1BDD25 };
			for (int i = 0; i < 5; ++i) {
				g_lsRested[i] = a_data->LookupForm<RE::SpellItem>(rested[i], LASTSEED_FILE);
			}
			g_lsDrunk = glob(0x010830, LASTSEED_FILE);
			g_lsDrunkOn = glob(0x01082F, LASTSEED_FILE);
			logger::info("Last Seed: alcohol {}, its switch {}", g_lsDrunk ? "found" : "MISSING", g_lsDrunkOn ? "found" : "MISSING");
			logger::info("Last Seed: running switch {}, hunger {}, thirst {}, fatigue {}, vitality {}, vitality target {}, rested {}",
				g_lsRunning ? "found" : "MISSING", g_lsValue[0] ? "found" : "MISSING", g_lsValue[1] ? "found" : "MISSING",
				g_lsValue[2] ? "found" : "MISSING", g_lsValue[3] ? "found" : "MISSING", g_lsVitalityTarget ? "found" : "MISSING",
				g_lsRested[0] ? "found" : "MISSING");
		}
		if (ModActive(a_data, INEED_FILE)) {
			g_inQuest = a_data->LookupForm<RE::TESQuest>(0x000D62, INEED_FILE);
			g_inDrunkSpell = a_data->LookupForm<RE::SpellItem>(0x00C4F2, INEED_FILE);
			constexpr RE::FormID pens[3] = { 0x0012DB, 0x004378, 0x0012DC };
			for (int i = 0; i < 3; ++i) {
				g_inPenalty[i] = glob(pens[i], INEED_FILE);
			}
			logger::info("iNeed: quest {}, penalties hunger {}, thirst {}, fatigue {}", g_inQuest ? "found" : "MISSING",
				g_inPenalty[0] ? "found" : "MISSING", g_inPenalty[1] ? "found" : "MISSING", g_inPenalty[2] ? "found" : "MISSING");
		}
		if (ModActive(a_data, RND_FILE)) {
			g_rndState = glob(0x012C4C, RND_FILE);
			constexpr RE::FormID pts[3] = { 0x002884, 0x002E07, 0x00BAC2 };
			constexpr RE::FormID lv[3][6] = { { 0x002DE9, 0x002DEA, 0x002DEB, 0x002DEC, 0x002DED, 0x002DEE },
				{ 0x002E0A, 0x002E0B, 0x002E0C, 0x002E0D, 0x002E0E, 0 }, { 0x02B5B3, 0x00BAC5, 0x00BAC6, 0x00BAC7, 0x00BAC8, 0 } };
			int found = 0;
			for (int i = 0; i < 3; ++i) {
				g_rndPoints[i] = glob(pts[i], RND_FILE);
				found += g_rndPoints[i] ? 1 : 0;
				for (int k = 0; k < 6; ++k) {
					g_rndLevels[i][k] = lv[i][k] ? glob(lv[i][k], RND_FILE) : nullptr;
					found += g_rndLevels[i][k] ? 1 : 0;
				}
			}
			logger::info("Realistic Needs and Diseases: state {}, {} of 19 need globals found", g_rndState ? "found" : "MISSING", found);
			g_rndDrunk = glob(0x0069A0, RND_FILE);
			constexpr RE::FormID dl[5] = { 0x00699E, 0x00699A, 0x00699B, 0x00699C, 0x00699D };
			int dfound = 0;
			for (int k = 0; k < 5; ++k) {
				g_rndDrunkLv[k] = glob(dl[k], RND_FILE);
				dfound += g_rndDrunkLv[k] ? 1 : 0;
			}
			g_rndWeightOn = glob(0x155DDA, RND_FILE);
			logger::info("Realistic Needs and Diseases: drunkenness {} ({} of 5 levels), weight switch {}", g_rndDrunk ? "found" : "MISSING",
				dfound, g_rndWeightOn ? "found" : "MISSING");
		}
		if (ModActive(a_data, FROSTFALL_FILE)) {
			g_ffRunning = glob(0x06DCFB, FROSTFALL_FILE);
			g_ffExposureOn = glob(0x068113, FROSTFALL_FILE);
			g_ffExposure = glob(0x00183D, FROSTFALL_FILE);
			g_ffTarget = glob(0x0805E4, FROSTFALL_FILE);
			g_ffNearFire = glob(0x064AFD, FROSTFALL_FILE);
			g_ffWarmth = glob(0x067B8F, FROSTFALL_FILE);
			g_ffWet = glob(0x06458C, FROSTFALL_FILE);
			constexpr RE::FormID waterFx[4] = { 0x009F75, 0x009F76, 0x009F77, 0x042B8E };  // _Frost_ColdWater_Effect1/2/3/3_NoStamina
			int waterFound = 0;
			for (int i = 0; i < 4; ++i) {
				g_ffWaterFx[i] = a_data->LookupForm<RE::EffectSetting>(waterFx[i], FROSTFALL_FILE);
				waterFound += g_ffWaterFx[i] ? 1 : 0;
			}
			g_ffWaterLethal = glob(0x00E5B7, FROSTFALL_FILE);
			logger::info("Frostfall cold water: {} of 4 water effects found, lethal switch {}", waterFound, g_ffWaterLethal ? "found" : "MISSING");
			logger::info("Frostfall: running switch {}, exposure {}, exposure target {}, near fire {}, warmth {}, wetness {}",
				g_ffRunning ? "found" : "MISSING", g_ffExposure ? "found" : "MISSING", g_ffTarget ? "found" : "MISSING",
				g_ffNearFire ? "found" : "MISSING", g_ffWarmth ? "found" : "MISSING", g_ffWet ? "found" : "MISSING");
		}
		if (ModActive(a_data, SURVIVAL_FILE)) {
			// read the scales from the game's globals instead of assuming vanilla's numbers, so any mod that edits them
			// (Survival Mode Improved, tweaks, patches) is followed: each need's stage limits where they are globals, and
			// each need's max ends its last stage (vanilla 1000 / 960 / 1000; SMI 468 / 578 / 900)
			const auto read = [&](std::array<double, 6>& a_out, std::initializer_list<std::pair<RE::FormID, const char*>> a_ids, RE::FormID a_max) {
				std::array<double, 6> b{ 0.0 };
				int i = 1;
				for (const auto& [id, file] : a_ids) {
					auto gl = glob(id, file);
					if (!gl) {
						return false;
					}
					b[i++] = gl->value;
				}
				const auto mx = glob(a_max, SURVIVAL_FILE);
				b[5] = std::max(mx ? static_cast<double>(mx->value) : 1000.0, b[4]);  // = b[4]: the need stops at its last limit
				a_out = b;
				return true;
			};
			// our five stages start at the mod's stage 2 (stage 1 only marks the rested / well fed bonus), so stages 2-5
			// are our limits 1-4 and the need's max ends the last one. Hunger's limits are Survival's own globals
			const bool h = read(g_hungerBoundsSmi, { { 0x802, SURVIVAL_FILE }, { 0x803, SURVIVAL_FILE }, { 0x804, SURVIVAL_FILE }, { 0x805, SURVIVAL_FILE } }, 0x80C);
			// tiredness and cold limits live in vanilla's scripts (fixed numbers), but SMI keeps them in its esp
			bool t = false, c = false;
			if (ModActive(a_data, SMI_FILE)) {
				std::array<double, 6> cold{ 0.0 };
				t = read(g_tiredBoundsSmi, { { 0xA18, SMI_FILE }, { 0xA19, SMI_FILE }, { 0xA1A, SMI_FILE }, { 0xA1B, SMI_FILE } }, 0x84A);
				c = read(cold, { { 0xD21, SMI_FILE }, { 0xD22, SMI_FILE }, { 0xD23, SMI_FILE }, { 0xD24, SMI_FILE } }, 0x84B);
				if (c) {
					STAGE_VALUES = cold;
				}
				// its C++ replaces Survival's cold script (the quest's script is removed in its esp), so our bridge can't
				// read the place's cold level; it keeps its own: SMI_CurrentAmbientTemp (read 2026-11-08, its 1.7.0)
				g_smiAmbient = glob(0xD1D, SMI_FILE);
				if (const auto s1 = glob(0xD20, SMI_FILE)) {
					g_smiColdStage1 = s1->value;
				}
				// Starfrost: needs SMI (it is one of its masters)
				if (ModActive(a_data, STARFROST_FILE)) {
					g_starfrost = true;
					for (int i = 0; i < 3; ++i) {
						g_sfFed[i] = a_data->LookupForm<RE::SpellItem>(std::array<RE::FormID, 3>{ 0x80E, 0x813, 0x817 }[i], STARFROST_FILE);
					}
					g_coldSlots = 5;
					logger::info("Starfrost: found (its stage names; meal bonuses {}/{}/{})", g_sfFed[0] ? "found" : "MISSING",
						g_sfFed[1] ? "found" : "MISSING", g_sfFed[2] ? "found" : "MISSING");
				}
			}
			if (!c) {
				if (const auto mx = glob(0x84B, SURVIVAL_FILE); mx && mx->value > STAGE_VALUES[4]) {
					STAGE_VALUES[5] = mx->value;  // cold's max (vanilla 1000)
				}
			}
			logger::info("Needs scales: hunger {} (max {}), tiredness {}, cold {} (max {}){}", h ? "from globals" : "vanilla's",
				g_hungerBoundsSmi[5], t ? "from Survival Mode Improved" : "vanilla's", c ? "from Survival Mode Improved" : "vanilla's",
				STAGE_VALUES[5], g_smiAmbient ? "; place cold from Survival Mode Improved" : "");
		}
	}
	std::atomic_bool g_wetNow{ false };  // set on the UI thread by the gauge's tick; read by the ticker thread

	bool InFreezingWater()
	{
		return SnapHasSpell(g_freezingWater);
	}

	RE::TESGlobal* g_temperatureLevel = nullptr;

	// the bridge (NeedsHUD.esp, ESL): the place's cold level lives only in Survival's Papyrus script
	// (Survival_NeedCold.currentColdLevel) and C++ must not read script properties, so a hidden ability on the player
	// runs NeedsHUD_ColdBridgeEffectScript, which copies it into this global twice a second (-1 = not known)
	constexpr const char*   BRIDGE_FILE = "NeedsHUD.esp";
	constexpr RE::FormID    PLACE_COLD = 0x800;     // GLOB NeedsHUD_PlaceColdLevel
	constexpr RE::FormID    BRIDGE_SPELL = 0x802;   // SPEL NeedsHUD_ColdBridge (ability, hidden)
	RE::TESGlobal*          g_placeCold = nullptr;
	RE::SpellItem*          g_bridgeSpell = nullptr;
	std::atomic_bool        g_bridgeCheck{ true };  // make sure the player has the ability (set again on every load)

	// After each load (and once at the start), on the game's MAIN thread: give the player each bridge ability whose mod is
	// installed and take it away when its mod is gone, so removing Survival, SunHelm or iNeed mid-playthrough leaves no
	// script calling a missing mod (1.0.3 code review). Also tells the mod bridge which mods are installed.
	void BridgesCheck()
	{
		const bool cold = g_bridgeCheck.exchange(false);
		const bool mods = g_modBridgeCheck.exchange(false);
		if (!cold && !mods) {
			return;
		}
		SKSE::GetTaskInterface()->AddTask([cold, mods]() {
			auto player = RE::PlayerCharacter::GetSingleton();
			if (!player) {
				g_bridgeCheck = g_bridgeCheck || cold;
				g_modBridgeCheck = g_modBridgeCheck || mods;
				return;
			}
			auto keep = [&](RE::SpellItem* a_spell, bool a_wanted, const char* a_what) {
				if (!a_spell) {
					return;
				}
				const bool has = player->HasSpell(a_spell);
				if (a_wanted && !has) {
					player->AddSpell(a_spell);
					logger::info("{} ability added to the player", a_what);
				} else if (!a_wanted && has) {
					player->RemoveSpell(a_spell);
					logger::info("{} ability removed from the player (its mod is gone)", a_what);
				}
			};
			if (cold) {
				keep(g_bridgeSpell, g_coldValue != nullptr, "Bridge");  // Survival Mode's plugin is loaded
			}
			if (mods) {
				const int which = (g_shMainQuest ? 1 : 0) + (g_inQuest ? 2 : 0);
				if (g_bridgeMods) {  // the bridge script has no way to ask which mods are installed without SKSE
					g_bridgeMods->value = static_cast<float>(which);
				}
				keep(g_modBridgeSpell, which != 0, "Mod bridge");
			}
		});
	}
	// counts save loads (kPreLoadGame): each tick compares it with the last load it saw and drops what it carried over
	// from the session before (eased bars, a hit's fire or frost, the Sanguinare fallback clock)
	std::atomic_int         g_loadCount{ 0 };

	// ---- hit by fire / frost (HitFx.inl draws it): Survival Mode's own rule, Survival_PlayerMagicHitInfo
	// (OnMagicEffectApply on the player: an effect with MagicDamageFrost or one of Survival_FrostbitePoisonEffects
	// chills, MagicDamageFire warms). The same engine event in C++, plus the hit event as a safety net for weapon
	// enchantments (a Nexus test found OnMagicEffectApply missing hand-held weapon enchantments on an object; research
	// 2026-11-05). Event sinks run on the game's main thread and only store the time; the gauge's tick reads it.
	constexpr RE::FormID KW_FIRE = 0x1CEAD, KW_FROST = 0x1CEAE;  // Skyrim.esm MagicDamageFire / MagicDamageFrost
	constexpr RE::FormID FROSTBITE_POISONS = 0x9A8;             // FLST Survival_FrostbitePoisonEffects (Survival esl)
	RE::BGSListForm*          g_frostbitePoisons = nullptr;
	std::atomic<std::int64_t> g_fireAt{ -100000 }, g_frostAt{ -100000 };        // ms of the last hit
	std::atomic<std::int64_t> g_fireFrom{ -100000 }, g_frostFrom{ -100000 };    // ms the effect started from nothing
	constexpr double FIRE_HOLD = 2.0, FIRE_FADE = 0.7, ICE_HOLD = 3.0, ICE_FADE = 0.35;  // seconds (ice snaps off, like Zelda's)
	constexpr bool   FROST_FLIPBOOK = true;  // frost hit drawn by the pre-rendered frost.swf (FrostTick), not DrawIceCase

	std::int64_t NowMs()
	{
		return std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::steady_clock::now().time_since_epoch()).count();
	}

	int ElementOf(const RE::EffectSetting* a_effect)  // 1 fire, 2 frost, 0 neither (Survival checks frost first)
	{
		if (!a_effect) return 0;
		if (a_effect->HasKeywordID(KW_FROST)) return 2;
		if (a_effect->HasKeywordID(KW_FIRE)) return 1;
		if (g_frostbitePoisons && g_frostbitePoisons->HasForm(a_effect)) return 2;
		return 0;
	}

	// a hit keeps the effect at full; only a hit after it has fully gone starts it again (grows in), so continuous
	// flames or a stream of ice spikes (the event comes 12+ times a second) hold it steady instead of flickering
	void MarkHit(int a_element)
	{
		const std::int64_t now = NowMs();
		const auto mark = [now](std::atomic<std::int64_t>& a_at, std::atomic<std::int64_t>& a_from, double a_gone) {
			if (now - a_at.load() > static_cast<std::int64_t>(a_gone * 1000.0)) {
				a_from = now;
			}
			a_at = now;
		};
		if (a_element == 1) {
			mark(g_fireAt, g_fireFrom, FIRE_HOLD + FIRE_FADE);
		} else if (a_element == 2) {
			mark(g_frostAt, g_frostFrom, ICE_HOLD + ICE_FADE);
		}
	}

	struct HitWatch : RE::BSTEventSink<RE::TESMagicEffectApplyEvent>, RE::BSTEventSink<RE::TESHitEvent>
	{
		RE::BSEventNotifyControl ProcessEvent(const RE::TESMagicEffectApplyEvent* a_event, RE::BSTEventSource<RE::TESMagicEffectApplyEvent>*) override
		{
			// on the player, and not cast by the player: your own cloak's self effect is untagged in vanilla
			// (FireCloakFFSelf / FrostCloakFFSelf: MagicCloak only), but some cloak self effects carry the fire or
			// frost keyword (atronach, J'zargo, frost draugr) and a mod could give one to the player. Fire with no
			// caster (traps, hazards) still counts
			const auto player = RE::PlayerCharacter::GetSingleton();
			if (a_event && a_event->target && a_event->target.get() == player && a_event->caster.get() != player) {
				MarkHit(ElementOf(RE::TESForm::LookupByID<RE::EffectSetting>(a_event->magicEffect)));
			}
			return RE::BSEventNotifyControl::kContinue;
		}

		// a weapon hit on the player (not blocked): the attacker's weapon enchantment, as equipped (a player-made one
		// lives on the inventory entry), else the weapon's own
		RE::BSEventNotifyControl ProcessEvent(const RE::TESHitEvent* a_event, RE::BSTEventSource<RE::TESHitEvent>*) override
		{
			const auto player = RE::PlayerCharacter::GetSingleton();
			if (!a_event || !a_event->target || a_event->target.get() != player || a_event->cause.get() == player ||
				a_event->flags.any(RE::TESHitEvent::Flag::kHitBlocked)) {
				return RE::BSEventNotifyControl::kContinue;
			}
			auto weapon = RE::TESForm::LookupByID<RE::TESObjectWEAP>(a_event->source);
			if (!weapon) {
				return RE::BSEventNotifyControl::kContinue;
			}
			RE::EnchantmentItem* ench = nullptr;
			if (auto attacker = a_event->cause ? a_event->cause->As<RE::Actor>() : nullptr) {
				for (const bool left : { false, true }) {
					auto entry = attacker->GetEquippedEntryData(left);
					if (entry && entry->object == weapon && (ench = entry->GetEnchantment()) != nullptr) {
						break;
					}
				}
			}
			if (!ench) {
				ench = weapon->formEnchanting;
			}
			if (ench) {
				for (auto effect : ench->effects) {
					if (effect) {
						MarkHit(ElementOf(effect->baseEffect));
					}
				}
			}
			return RE::BSEventNotifyControl::kContinue;
		}
	};
	HitWatch g_hitWatch;

	void WatchHits()
	{
		if (auto events = RE::ScriptEventSourceHolder::GetSingleton()) {
			events->AddEventSink<RE::TESMagicEffectApplyEvent>(&g_hitWatch);
			events->AddEventSink<RE::TESHitEvent>(&g_hitWatch);
			logger::info("Watching for fire and frost hits on the player");
		}
	}

	// how strong each effect is now (0..1): in fast, held while hits keep coming, then the flames die down and the
	// ice thaws
	double FxStrength(std::int64_t a_hitAt, std::int64_t a_from, std::int64_t a_now, double a_hold, double a_fade)
	{
		const double since = (a_now - a_hitAt) / 1000.0, started = (a_now - a_from) / 1000.0;
		if (since < 0.0 || since > a_hold + a_fade) return 0.0;
		const double grow = started < 0.12 ? 0.35 + 0.65 * std::max(started, 0.0) / 0.12 : 1.0;  // only at the start
		const double fade = since < a_hold ? 1.0 : 1.0 - (since - a_hold) / a_fade;               // after the last hit
		return std::min(grow, fade);
	}

	void FindDiseases(RE::TESDataHandler* a_data);  // DiseaseTile.inl
	void FindFrostbite(RE::TESDataHandler* a_data);  // FrostbiteTile.inl
	void FindKeepItClean(RE::TESDataHandler* a_data);  // DirtTile.inl
	void FindDirtAndBlood(RE::TESDataHandler* a_data);  // DirtTile.inl
	void FindBathingInSkyrim(RE::TESDataHandler* a_data);  // DirtTile.inl

	void FindColdForms()
	{
		auto data = RE::TESDataHandler::GetSingleton();
		if (data) {
			FindOtherNeedsMods(data);  // SunHelm works without Survival Mode
			FindDiseases(data);        // Skyrim's diseases show without Survival Mode too
			FindFrostbite(data);       // Frostfall's frostbite tile
			FindKeepItClean(data);     // Keep It Clean's dirt tile
			FindDirtAndBlood(data);    // Dirt and Blood's dirt tile (first when several are loaded)
			FindBathingInSkyrim(data); // Bathing in Skyrim's dirt tile (then this, then Keep It Clean)
		}
		// NeedsHUD.esp's own records, looked up before the Survival check below: the mod bridge serves SunHelm and iNeed
		// without Survival Mode, and the cold bridge ability must be known to be taken off when Survival Mode is removed
		// (1.0.3 second review: both were after the early return)
		if (data && ModActive(data, BRIDGE_FILE)) {
			g_placeCold = LookupGlobal(data, PLACE_COLD, BRIDGE_FILE);
			g_bridgeSpell = data->LookupForm<RE::SpellItem>(BRIDGE_SPELL, BRIDGE_FILE);
			// the mod bridges (1.0.3): SunHelm's drink count and iNeed's needs, copied by Papyrus into our globals
			g_modBridgeSpell = data->LookupForm<RE::SpellItem>(MOD_BRIDGE_SPELL, BRIDGE_FILE);
			g_bridgeSHDrinks = LookupGlobal(data, 0x803, BRIDGE_FILE);
			for (int i = 0; i < 3; ++i) {
				g_bridgeINState[i] = LookupGlobal(data, 0x804 + i, BRIDGE_FILE);
				g_bridgeINRate[i] = LookupGlobal(data, 0x807 + i, BRIDGE_FILE);
			}
			g_vampireSeenDay = LookupGlobal(data, 0x80A, BRIDGE_FILE);
			g_bridgeMods = LookupGlobal(data, 0x80D, BRIDGE_FILE);
		}
		logger::info("Bridge ({}): place cold {}, ability {}", BRIDGE_FILE, g_placeCold ? "found" : "MISSING (no arrow)",
			g_bridgeSpell ? "found" : "MISSING (no arrow)");
		logger::info("Mod bridge ({}): ability {}, SunHelm drinks {}, iNeed values {}, vampirism clock {}", BRIDGE_FILE,
			g_modBridgeSpell ? "found" : "MISSING", g_bridgeSHDrinks ? "found" : "MISSING",
			g_bridgeINState[2] && g_bridgeINRate[2] ? "found" : "MISSING", g_vampireSeenDay && g_bridgeMods ? "found" : "MISSING");
		if (!ModActive(data, SURVIVAL_FILE)) {
			logger::info("Survival Mode not installed: the cold gauge shows only SunHelm's or Frostfall's cold");
			return;
		}
		g_coldValue = LookupGlobal(data, COLD_VALUE, SURVIVAL_FILE);
		g_survivalOn = LookupGlobal(data, SURVIVAL_ON, SURVIVAL_FILE);
		g_temperatureLevel = LookupGlobal(data, TEMPERATURE_LEVEL, "Update.esm");
		g_frostbitePoisons = data->LookupForm<RE::BGSListForm>(FROSTBITE_POISONS, SURVIVAL_FILE);
		g_freezingWater = data->LookupForm<RE::SpellItem>(FREEZING_WATER_SPELL, SURVIVAL_FILE);
		g_afflictions[0] = data->LookupForm<RE::SpellItem>(0x910, SURVIVAL_FILE);
		g_afflictions[1] = data->LookupForm<RE::SpellItem>(0x911, SURVIVAL_FILE);
		g_afflictions[2] = data->LookupForm<RE::SpellItem>(0x913, SURVIVAL_FILE);
		logger::info("Afflictions: Weakened {}, Addled {}, Frostbitten {}", g_afflictions[0] ? "found" : "MISSING",
			g_afflictions[1] ? "found" : "MISSING", g_afflictions[2] ? "found" : "MISSING");
		g_hungerValue = LookupGlobal(data, HUNGER_VALUE, SURVIVAL_FILE);
		g_wellFed = data->LookupForm<RE::SpellItem>(WELL_FED_SPELL, SURVIVAL_FILE);
		logger::info("Hunger value {}, Well Fed spell {}", g_hungerValue ? "found" : "MISSING", g_wellFed ? "found" : "MISSING");
		g_tiredValue = LookupGlobal(data, TIRED_VALUE, SURVIVAL_FILE);
		g_tiredMax = LookupGlobal(data, TIRED_MAX, SURVIVAL_FILE);
		g_rested = data->LookupForm<RE::SpellItem>(RESTED_SPELL, "Skyrim.esm");
		g_wellRested = data->LookupForm<RE::SpellItem>(WELL_RESTED_SPELL, "Skyrim.esm");
		g_loversComfort = data->LookupForm<RE::SpellItem>(LOVERS_COMFORT_SPELL, "Skyrim.esm");
		// Free Cooking's Refreshing is gone (2026-11-12, his "drop the fake drained": brewed drinks give back the rest an
		// outdoor sleep took instead), and its record 0xA6A is now the camp-rest effect carried by every brewed drink: looking
		// it up would show a false Refreshed while Drained. The badge stays unused (g_fcRefreshing stays empty)
		logger::info("Tiredness value {}, max {}, rested spells {}/{}/{}", g_tiredValue ? "found" : "MISSING", g_tiredMax ? "found" : "MISSING",
			g_rested ? "found" : "MISSING", g_wellRested ? "found" : "MISSING", g_loversComfort ? "found" : "MISSING");
		logger::info("Freezing water spell {}", g_freezingWater ? "found" : "MISSING (the freezing water look won't show)");
		logger::info("Frostbite poison list {}", g_frostbitePoisons ? "found" : "MISSING (frostbite venom won't show)");
		logger::info("Survival Mode: cold value {}, mode switch {}, temperature level {}", g_coldValue ? "found" : "MISSING",
			g_survivalOn ? "found" : "MISSING", g_temperatureLevel ? "found" : "MISSING");
	}

	int ColdStage(double a_value)
	{
		for (int i = 0; i < 5; ++i) {
			if (a_value < STAGE_VALUES[i + 1]) {
				return i;
			}
		}
		return 4;
	}

	// 0 = no cold .. 1 = death, each cold stage the same share
	double ColdPos(double a_value)
	{
		a_value = std::clamp(a_value, 0.0, STAGE_VALUES[5]);
		const int i = ColdStage(a_value);
		const double w = STAGE_VALUES[i + 1] - STAGE_VALUES[i];
		const double frac = w > 0.0 ? (a_value - STAGE_VALUES[i]) / w : 1.0;  // a last stage with no width counts as full
		return (i + std::clamp(frac, 0.0, 1.0)) / 5.0;
	}

	// the coldest the place can make you, by Survival's own rule (Survival_NeedCold.GetColdStageMaximum, as Free
	// Cooking's Spicy uses it): cold levels 1/4/7/10/13 let the cold reach Comfortable/Chilly/Very Cold/Freezing/Numb,
	// so the arrow points at that stage's far end; level 0 lets it fall to nothing
	double PlaceColdValue(int a_level)
	{
		if (a_level >= 13) return STAGE_VALUES[5];
		if (a_level >= 10) return STAGE_VALUES[4];
		if (a_level >= 7) return STAGE_VALUES[3];
		if (a_level >= 4) return STAGE_VALUES[2];
		if (a_level >= 1) return STAGE_VALUES[1];
		return 0.0;
	}

	// ---- geometry, in the gauge's local units (1 = one unit of the vanilla bar shape; x across, y down from the tip) ----
	constexpr double UNIT_TO_HUD = 0.6;          // HUD stage units per local unit at fScale 1 (0.9 px at 1080p, like the real bar)
	constexpr double SCALE_TOP = 29.55;          // y of the scale's top (warm end); the scale is 150 long, six blocks of 25
	constexpr double SCALE_LEN = 150.0;
	constexpr double BULB_Y = 201.55, BULB_R = 12.5;
	constexpr double RING_LIGHT = BULB_R + 2.6;  // the light line's radius round the bulb
	constexpr double BANDS_FROM = 33.55;         // y where our bands take over from the vanilla ornament
	constexpr double COL_HALF = 8.65;            // half width of the mercury column
	constexpr double STRIP_X = -23.25, STRIP_W = 6.5, STRIP_GAP = 1.6;

	double ScaleY(double a_p) { return SCALE_TOP + a_p * SCALE_LEN; }  // a_p: 0 top of the warm block .. 1 death

	// down the left side at x = -a_left, a smooth fillet into the circle (radius a_rad), round the bottom and up the
	// right side at x = a_right (the vanilla frame's bands sit very slightly off centre); a_close fills it
	void UShape(GFx& g, double a_left, double a_right, double a_rad, double a_from, bool a_close)
	{
		constexpr double FILLET = 4.0, DA = 18.0 * std::numbers::pi / 180.0;
		const double yjl = BULB_Y - std::sqrt(std::max(a_rad * a_rad - a_left * a_left, 0.0));
		const double yjr = BULB_Y - std::sqrt(std::max(a_rad * a_rad - a_right * a_right, 0.0));
		const double a1 = std::atan2(yjl - BULB_Y, -a_left) - DA + 2.0 * std::numbers::pi;  // left, a bit further round (y down)
		const double a2 = std::atan2(yjr - BULB_Y, a_right) + DA;                           // right, a bit further round
		MoveTo(g, -a_left, a_from);
		LineTo(g, -a_left, yjl - FILLET);
		CurveTo(g, -a_left, yjl, a_rad * std::cos(a1), BULB_Y + a_rad * std::sin(a1));
		ArcTo(g, 0.0, BULB_Y, a_rad, a1, a2);  // decreasing angle: down the left, under, up the right
		CurveTo(g, a_right, yjr, a_right, yjr - FILLET);
		LineTo(g, a_right, a_from);
		if (a_close) {
			LineTo(g, -a_left, a_from);
		}
	}

	void DrawFrameShape(GFx& a_clip, const auto& a_cmds, std::uint32_t a_rgb, double a_alpha)
	{
		NoLine(a_clip);
		Fill(a_clip, a_rgb, a_alpha);
		for (const auto& c : a_cmds) {
			if (c.op == 'M') {
				MoveTo(a_clip, c.a, c.b);
			} else if (c.op == 'L') {
				LineTo(a_clip, c.a, c.b);
			} else {
				CurveTo(a_clip, c.a, c.b, c.c, c.d);
			}
		}
		EndFill(a_clip);
	}

	// the parts that never change: the top ornament (into the masked clip "orn"), its mask, and the scale strip
	void DrawStatic(RE::GFxMovieView* a_movie, GFx& a_orn, GFx& a_mask, GFx& g, bool a_red)
	{
		a_orn.Invoke("clear", nullptr, nullptr, 0);
		DrawFrameShape(a_orn, FRAME_0, FRAME_0_RGB, FRAME_0_ALPHA);
		DrawFrameShape(a_orn, FRAME_1, a_red ? RED : FRAME_1_RGB, FRAME_1_ALPHA);
		a_mask.Invoke("clear", nullptr, nullptr, 0);
		FillRect(a_mask, -30.0, -5.0, 60.0, BANDS_FROM + 5.0, 0.0, 0xFFFFFF);
		(void)a_movie;
		(void)g;
	}

	int g_coldShift = 0;  // [ColdProtection] StageShift right now (set by the gauge's tick, UI thread)
	int  ColdProtectionShift();      // ConfigLoader.inl
	bool ColdProtectionWaterSafe();  // ConfigLoader.inl

	struct Block
	{
		std::uint32_t base, hi;
	};
	constexpr Block WARM_BLOCK{ 0xA87A48, 0xD6B282 };
	constexpr std::array<Block, 4> COLD_BLOCKS{ { { 0x84A6C0, 0xBED2E2 }, { 0x7094B0, 0xA8C2D6 }, { 0x4A6E8E, 0x82A0BA }, { 0x2E4C6E, 0x607C9A } } };

	void DrawStrip(RE::GFxMovieView* a_movie, GFx& g)
	{
		const double y0 = ScaleY(0.0) - STRIP_GAP, y1 = ScaleY(1.0) + STRIP_GAP;
		const double x0 = STRIP_X - STRIP_GAP, w = STRIP_W + 2.0 * STRIP_GAP;
		FillRect(g, x0, y0, w, y1 - y0, 0.0, BAND_DARK, 75.0);
		NoLine(g);
		Line(g, 1.8, BAND_DARK, 60.0);
		RoundRectPath(g, x0 - 0.9, y0 - 0.9, w + 1.8, y1 - y0 + 1.8, 0.0);
		Line(g, 0.9, BAND_LIGHT, 60.0);
		RoundRectPath(g, x0 - 0.9, y0 - 0.9, w + 1.8, y1 - y0 + 1.8, 0.0);
		NoLine(g);
		const auto block = [&](double a_top, double a_bottom, const Block& b) {
			GradFill(a_movie, g, false,
				{ { b.base, 100.0, 0.0 }, { b.hi, 100.0, 0.32 }, { b.base, 100.0, 0.42 }, { b.base, 75.0, 1.0 } },
				STRIP_X, a_top, STRIP_W, a_bottom - a_top);
			RoundRectPath(g, STRIP_X, a_top, STRIP_W, a_bottom - a_top, 0.0);
			EndFill(g);
		};
		// g_coldSlots equal blocks: Warm, the unpainted Comfortable, then the colder ones (6 slots: our four blues;
		// fewer: the blues spread from the lightest to the darkest)
		const int    slots = std::clamp(g_coldSlots, 3, 6);
		const double h = 1.0 / slots;
		block(ScaleY(0.0) + STRIP_GAP / 2, ScaleY(h) - STRIP_GAP / 2, WARM_BLOCK);
		const int cold = slots - 2;
		const int shift = std::clamp(g_coldShift, 0, cold);
		for (int i = 0; i < cold; ++i) {
			// cold protection: this block shows the stage `shift` above it; above the first cold stage = unpainted
			const int j = i - shift;
			if (j < 0) {
				continue;
			}
			const int c = cold == 1 ? 3 : static_cast<int>(std::lround(j * 3.0 / (cold - 1)));
			block(ScaleY((i + 2) * h) + STRIP_GAP / 2, ScaleY((i + 3) * h) - STRIP_GAP / 2, COLD_BLOCKS[c]);
		}
	}

	#include "HitFx.inl"

	// mercury look (2026-11-08, his pick "5 it is" of mockups\mercury_v5_2026-11-08.png): a domed top (convex
	// meniscus) with a thin darker rim and no shine on it, bright neutral mirror silver, a one-sided reflection
	// across the column and one soft highlight streak. While the liquid boils the dome flattens (boiling breaks the
	// calm surface, and the boil's surface strip is drawn flat); g_domeK eases 1 <-> 0 over about half a second.
	constexpr double MERC_DOME = 1.5;
	bool   g_boiling = false;  // set by the burn's tick (BurnTick) while the boil plays
	double g_domeK = 1.0;      // 1 = full dome, 0 = flat (eased by the gauge's tick)

	// everything that moves: the tube, the mercury at a_p (0 = top of the warm block .. 1 = death), the glow
	void DrawGauge(RE::GFxMovieView* a_movie, GFx& g, double a_p, bool a_hurts, bool a_red, double a_arrow = -1.0,
		double a_fire = 0.0, double a_ice = 0.0, double a_time = 0.0)
	{
		g.Invoke("clear", nullptr, nullptr, 0);

		// glow while the cold hurts you: a soft cyan halo round the bulb and up the tube's right side, under everything
		// else; none on the left, where it showed through the scale strip
		if (a_hurts) {
			// each stroke fades in over the bulb's upper left (short arcs of rising alpha), then runs on at full
			const auto glow = [&](double a_width, double a_alpha, double a_left, double a_right, double a_rad) {
				constexpr int    FADE = 8;
				constexpr double FADE_ARC = 40.0 * std::numbers::pi / 180.0;
				const double aj = std::atan2(-std::sqrt(std::max(a_rad * a_rad - a_left * a_left, 0.0)), -a_left) + 2.0 * std::numbers::pi;
				const double aEnd = aj - FADE_ARC;
				for (int k = 0; k < FADE; ++k) {
					const double s = aj - FADE_ARC * k / FADE, e = aj - FADE_ARC * (k + 1) / FADE;
					Line(g, a_width, CYAN, a_alpha * (k + 1) / (FADE + 1));
					MoveTo(g, a_rad * std::cos(s), BULB_Y + a_rad * std::sin(s));
					ArcTo(g, 0.0, BULB_Y, a_rad, s, e);
				}
				Line(g, a_width, CYAN, a_alpha);
				// from the end of the fade on round as UShape does: down, under, up the right side
				const double yjr = BULB_Y - std::sqrt(std::max(a_rad * a_rad - a_right * a_right, 0.0));
				const double a2 = std::atan2(yjr - BULB_Y, a_right) + 18.0 * std::numbers::pi / 180.0;
				MoveTo(g, a_rad * std::cos(aEnd), BULB_Y + a_rad * std::sin(aEnd));
				ArcTo(g, 0.0, BULB_Y, a_rad, aEnd, a2);
				CurveTo(g, a_right, yjr, a_right, yjr - 4.0);
				LineTo(g, a_right, BANDS_FROM + 6.0);
			};
			NoLine(g);
			glow(4.0, 14.0, 16.0, 15.8, RING_LIGHT + 4.0);
			glow(1.8, 30.0, 15.3, 15.1, RING_LIGHT + 3.3);
			NoLine(g);
		}
		DrawStrip(a_movie, g);

		// the tube's near-black inside, then the bulb's (one shape with fillets, same colour)
		// (his "black square", 2026-11-08: at the very top, under the tip, the vanilla ornament's own dark plate (60%)
		// lies under ours, so the glass there came out darker, 97%, a dark rectangle zoomed out; there ours is 80%, so
		// the two together are the 92% of the rest of the tube)
		FillRect(g, -10.05, SCALE_TOP, 19.75, BANDS_FROM - SCALE_TOP, 0.0, INSIDE, 80.0);
		FillRect(g, -10.05, BANDS_FROM, 19.75, 167.0 - (BANDS_FROM - SCALE_TOP), 0.0, INSIDE, 92.0);
		NoLine(g);
		Fill(g, INSIDE, 92.0);
		UShape(g, 10.05, 9.7, RING_LIGHT - 0.975, BULB_Y - 11.4, true);
		EndFill(g);
		// the bar's bands carrying on down the sides and round the bulb
		Line(g, 2.75, BAND_DARK, 60.0);
		UShape(g, 13.375, 13.125, RING_LIGHT + 2.35, BANDS_FROM, false);
		// the light line: red at Numb, warm orange while fire licks the gauge (red wins)
		Line(g, 1.95, a_red ? RED : (a_fire > 0.4 ? 0xF0A052 : BAND_LIGHT), a_red ? 60.0 : 60.0 + 30.0 * std::min(a_fire, 1.0));
		UShape(g, 11.025, 10.725, RING_LIGHT, BANDS_FROM, false);
		NoLine(g);

		// the mercury: one shape (column, flared neck, ball)
		const double top = ScaleY(a_p);
		const double yjn = BULB_Y - std::sqrt(BULB_R * BULB_R - COL_HALF * COL_HALF);
		constexpr double FL = 5.0;
		const double an = std::atan2(yjn - BULB_Y, -COL_HALF) - 16.0 * std::numbers::pi / 180.0 + 2.0 * std::numbers::pi;
		const double pr = -std::numbers::pi - (an - 2.0 * std::numbers::pi);  // the mirror point on the right, going on round (decreasing)
		const double DOME = MERC_DOME * g_domeK;  // the top bulges up: its edges sit lower than its middle
		const auto mercury = [&]() {
			MoveTo(g, -COL_HALF, top + DOME);
			LineTo(g, -COL_HALF, yjn - FL);
			CurveTo(g, -COL_HALF, yjn, BULB_R * std::cos(an), BULB_Y + BULB_R * std::sin(an));
			ArcTo(g, 0.0, BULB_Y, BULB_R, an, pr);
			CurveTo(g, COL_HALF, yjn, COL_HALF, yjn - FL);
			LineTo(g, COL_HALF, top + DOME);
			CurveTo(g, 0.0, top - DOME, -COL_HALF, top + DOME);
		};
		// a slice's outline: a plain box, or for the slice at the very top, the box with the dome on it (so the
		// shading runs over the dome as one smooth surface, with no line under it)
		const auto slicePath = [&](double a_half, double a_y0, double a_y1) {
			if (a_y0 <= top + 0.01) {
				MoveTo(g, -a_half, top + DOME);
				CurveTo(g, 0.0, top - DOME, a_half, top + DOME);
				LineTo(g, a_half, a_y1);
				LineTo(g, -a_half, a_y1);
				LineTo(g, -a_half, top + DOME);
			} else {
				RoundRectPath(g, -a_half, a_y0, 2 * a_half, a_y1 - a_y0, 0.0);
			}
		};
		{
			// a "box" gradient turned a quarter runs top to bottom over the box's width, turned about its centre: so the
			// box is a square as tall as the span
			const double span = BULB_Y - top;
			GradFill(a_movie, g, false, { { 0xD3D4D5, 100.0, 0.0 }, { 0xB6B7B9, 100.0, 1.0 } }, -span / 2.0, top, span, span,
				std::numbers::pi / 2.0);  // bright neutral mirror silver
		}
		mercury();
		EndFill(g);
		// the column's sheen (the bars' lighting from the left), fading out where the liquid widens into the ball
		// (in the ball the slices span its full width: the gradient's end colours carry on past the column's edges,
		// so no edge shows where the column would have stopped)
		const auto sheen = [&](double a_y0, double a_y1, double a_k, double a_half) {
			GradFill(a_movie, g, false,
				{ { 0x000000, 12.0 * a_k, 0.0 }, { 0xFFFFFF, 50.0 * a_k, 0.33 }, { 0xFFFFFF, 0.0, 0.43 }, { 0x000000, 0.0, 0.45 },
					{ 0x000000, 30.0 * a_k, 1.0 } },
				-COL_HALF, a_y0, 2 * COL_HALF, a_y1 - a_y0);
			slicePath(a_half, a_y0, a_y1);
			EndFill(g);
		};
		const double fadeFrom = std::max(top, yjn - FL);
		if (fadeFrom > top) {
			sheen(top, fadeFrom, 1.0, COL_HALF);
		}
		constexpr int SLICES = 24;  // fine enough that the steps don't show
		for (int i = 0; i < SLICES; ++i) {
			const double y0 = fadeFrom + (BULB_Y - fadeFrom) * i / SLICES, y1 = fadeFrom + (BULB_Y - fadeFrom) * (i + 1) / SLICES;
			// as wide as the ball, but never over the glass bands: inside the tube's dark inside (10 either side), or
			// the bulb's dark inside lower down
			const double ri = RING_LIGHT - 0.975, dy = std::max(std::abs(y0 - BULB_Y), std::abs(y1 - BULB_Y));
			const double wall = (y1 < BULB_Y - 11.4) ? 9.7 : std::sqrt(std::max(ri * ri - dy * dy, 0.0));
			sheen(y0, y1, 1.0 - (i + 0.5) / SLICES, std::min(BULB_R, std::max(wall, 9.7)));
		}
		{
			// the reflection across the column (a mirror cylinder in a dark tube): darker at the left edge, bright, a
			// soft dark band where it reflects the tube, a faint light rim on the far edge; it fades out over the last
			// stretch above the neck so no seam shows where the column widens into the ball
			const double y1 = yjn - FL;
			constexpr int CS = 24;
			const double fade0 = std::max(top + DOME + 1.0, y1 - 30.0);
			for (int i = 0; i <= CS; ++i) {
				const double a0 = i == 0 ? top : fade0 + (y1 - fade0) * (i - 1) / CS;
				const double a1 = i == 0 ? fade0 : fade0 + (y1 - fade0) * i / CS;
				if (a1 <= a0) {
					continue;
				}
				const double k = i == 0 ? 1.0 : 1.0 - (i - 0.5) / CS;
				GradFill(a_movie, g, false,
					{ { 0x000000, 30.0 * k, 0.0 }, { 0x000000, 0.0, 0.18 }, { 0x000000, 0.0, 0.5 }, { 0x000000, 34.0 * k, 0.66 },
						{ 0x000000, 14.0 * k, 0.86 }, { 0x000000, 26.0 * k, 1.0 } },
					-COL_HALF, a0, 2 * COL_HALF, a1 - a0);
				slicePath(COL_HALF, a0, a1);
				EndFill(g);
			}
			// one soft highlight streak down the lit side, fading in under the dome and out into the ball
			const double sx = -COL_HALF * 0.42, s0 = top + DOME + 1.4, s1 = yjn + 4.0, sf = std::max(s0 + 12.0, s1 - 24.0);
			for (int i = 0; i < 6; ++i) {
				Line(g, 1.0, 0xFFFFFF, 60.0 * (i + 0.5) / 6.0);
				MoveTo(g, sx, s0 + 2.0 * i);
				LineTo(g, sx, s0 + 2.0 * (i + 1));
			}
			if (sf > s0 + 12.0) {
				Line(g, 1.0, 0xFFFFFF, 60.0);
				MoveTo(g, sx, s0 + 12.0);
				LineTo(g, sx, sf);
			}
			for (int i = 0; i < 8; ++i) {
				Line(g, 1.0, 0xFFFFFF, 60.0 * (1.0 - (i + 0.5) / 8.0));
				MoveTo(g, sx, sf + (s1 - sf) * i / 8.0);
				LineTo(g, sx, sf + (s1 - sf) * (i + 1) / 8.0);
			}
			// no shine on top: the convex surface curves away and reflects darker at its rim (fades as the dome flattens)
			if (DOME > 0.05) {
				Line(g, 0.9, 0x000000, 35.0 * g_domeK);
				const auto rimX = [&](double t) { return COL_HALF * (2.0 * t - 1.0); };
				const auto rimY = [&](double t) { return top + DOME * (1.0 - 4.0 * t * (1.0 - t)) + 0.45; };
				MoveTo(g, rimX(0.0) + 0.4, rimY(0.0));
				for (int i = 1; i <= 12; ++i) {
					const double t = i / 12.0;
					LineTo(g, rimX(t) - (i == 12 ? 0.4 : 0.0), rimY(t));
				}
			}
			NoLine(g);
		}
		// the ball: no change at its top, darker toward the bottom, a dim ground reflection, dark rim low down
		GradFill(a_movie, g, false,
			{ { 0x000000, 0.0, 0.0 }, { 0x000000, 0.0, 0.45 }, { 0x000000, 28.0, 0.7 }, { 0x000000, 12.0, 0.86 }, { 0x000000, 40.0, 1.0 } },
			-BULB_R, BULB_Y - BULB_R, 2 * BULB_R, 2 * BULB_R, std::numbers::pi / 2.0);
		CirclePath(g, 0.0, BULB_Y, BULB_R);
		EndFill(g);
		GradFill(a_movie, g, true, { { 0x000000, 0.0, 0.62 }, { 0x000000, 50.0, 1.0 } }, -BULB_R * 1.35,
			BULB_Y - BULB_R * 0.35 - BULB_R * 1.35, BULB_R * 2.7, BULB_R * 2.7);
		CirclePath(g, 0.0, BULB_Y, BULB_R);
		EndFill(g);
		// a faint light edge low on the right, and the crisp highlight upper left
		Line(g, 0.9, 0xECF0F2, 45.0);
		{
			const double r = BULB_R - 1.6, a0 = 20.0 * std::numbers::pi / 180.0, a1 = 60.0 * std::numbers::pi / 180.0;
			MoveTo(g, r * std::cos(a0), BULB_Y + r * std::sin(a0));
			ArcTo(g, 0.0, BULB_Y, r, a0, a1);
		}
		NoLine(g);
		Fill(g, 0xFCFDFE, 95.0);
		EllipsePath(g, -BULB_R * 0.42, BULB_Y - BULB_R * 0.44, BULB_R * 0.18, BULB_R * 0.09, -42.0 * std::numbers::pi / 180.0);
		EndFill(g);

		// the place arrow: how cold this place can make you (a_arrow on the same 0..1 scale as the liquid; < 0 = not
		// known). A notched arrowhead on the right of the tube pointing at it, drawn like the vanilla frame: a dark
		// edge under a light fill, so it reads on snow and in the dark
		if (a_arrow >= 0.0) {
			const double y = ScaleY(std::clamp(a_arrow, 0.0, 1.0));
			constexpr double TIP = 16.6, BACK = 25.4, NOTCH = 22.6, HALF = 6.6;
			const auto head = [&]() {
				MoveTo(g, TIP, y);
				LineTo(g, BACK, y - HALF);
				LineTo(g, NOTCH, y);
				LineTo(g, BACK, y + HALF);
				LineTo(g, TIP, y);
			};
			Line(g, 1.5, BAND_DARK, 85.0);
			Fill(g, 0xE4E6E8, 100.0);
			head();
			EndFill(g);
			NoLine(g);
			// the lit upper half a touch brighter (light from the upper left, as on the bars)
			Fill(g, 0xFFFFFF, 35.0);
			MoveTo(g, TIP, y);
			LineTo(g, BACK, y - HALF);
			LineTo(g, NOTCH, y);
			LineTo(g, TIP, y);
			EndFill(g);
		}
		DrawIceCase(g, a_ice);                        // over everything, like Zelda's
		(void)a_fire; (void)a_time;  // the fire has its own sprites (ColdFireTick)
	}

	// ---- the gauge in the HUD movie ----

	// ---- when to show (shared by the thermometer and the Needs group): hidden in combat if asked; with "only when the
	// stage changes", up for showSeconds after each change (and when first seen), and at the worst stage if keepWorst.
	// It fades in and out over about 0.4 s (a quarter each tick). Returns the alpha to use (0-100).
	// the player's total warmth rating (all worn armor and clothing), as Survival reads it
	bool g_ffShown = false;  // the gauge shows Frostfall's cold (set by its tick)
	double WarmthNow()
	{
		if (g_ffShown && g_ffWarmth) {
			return GV(g_ffWarmth);  // Frostfall counts warmth its own way (its armor table), not the game's rating
		}
		return g_view.warmth;  // GetWarmthRating, read on the main thread (Snapshot.inl)
	}

	struct AutoShow
	{
		// what is shown, as a full list (1.0.4 review: one 64-bit number packed from every tile lost the first tiles once
		// eleven or more showed, so a change there never showed the group)
		std::vector<std::uint64_t> lastKey;
		bool         seen = false;     // false: nothing seen yet
		std::int64_t changedAt = 0;
		double       alpha = -1.0;
		std::vector<std::uint64_t> pendingKey;  // a new stage not yet held long enough to count
		bool         pending = false;
		std::int64_t pendingSince = 0;
		std::vector<int> lastLevels;       // onlyWorse: how bad each part was at the last counted change (higher = worse)
	};

	// a stage change only counts once it has held this long: standing at the edge of a fire's reach, or with the value right
	// on a stage line, the stage can flip back and forth, and each flip used to show the gauge again (2026-11-10)
	constexpr std::int64_t SHOW_SETTLE_MS = 2000;

	template <class Opt>
	double AutoShowAlpha(AutoShow& a_s, const Opt& a_o, std::vector<std::uint64_t> a_key, bool a_worst, std::int64_t a_now, const std::vector<int>& a_levels)
	{
		if (!a_s.seen) {  // the first look after a load: counts at once
			a_s.seen = true;
			a_s.lastKey = a_key;
			a_s.changedAt = a_now;
			a_s.lastLevels = a_levels;
		} else if (a_key == a_s.lastKey) {
			a_s.pending = false;  // it came back before it settled: no change
		} else if (!a_s.pending || a_key != a_s.pendingKey) {
			a_s.pending = true;
			a_s.pendingKey = a_key;
			a_s.pendingSince = a_now;
		} else if (a_now - a_s.pendingSince >= SHOW_SETTLE_MS) {
			// onlyWorse (his ask 2026-11-10): getting warmer after a door or by a fire, eating, sleeping stay quiet; only a
			// part getting worse counts (a tile that wasn't there before counts as worse)
			bool worse = a_levels.size() != a_s.lastLevels.size();
			for (size_t i = 0; !worse && i < a_levels.size(); ++i) {
				worse = a_levels[i] > a_s.lastLevels[i];
			}
			a_s.lastKey = a_key;
			a_s.lastLevels = a_levels;
			if (!a_o.onlyWorse || worse) {
				a_s.changedAt = a_now;
			}
			a_s.pending = false;
		}
		const bool combat = g_view.combat;
		bool want = !(a_o.hideInCombat && combat);
		if (want && a_o.onlyOnChange) {
			want = (a_now - a_s.changedAt) < static_cast<std::int64_t>(a_o.showSeconds * 1000.0) || (a_o.keepWorst && a_worst);
		}
		const double target = want ? 100.0 : 0.0;
		a_s.alpha = a_s.alpha < 0.0 ? target : (target > a_s.alpha ? std::min(target, a_s.alpha + 25.0) : std::max(target, a_s.alpha - 25.0));
		return a_s.alpha;
	}

	// Frostfall's wetness: a small bar left of the stage strip (left, so the place arrow on the right only ever points at
	// the mercury), three equal blocks Damp / Wet / Drenched, water rising piecewise inside each, deeper aqua per block
	// (approved mockup mockups\wetness_gauge_approved_2026-11-09.png); hidden while dry
	void DrawWetnessBar(GFx& g, double a_wet)
	{
		const int level = FrostfallWetLevel(a_wet);
		if (level < 0) {
			return;
		}
		constexpr double X0 = -35.0, W = 6.0;
		const double top = SCALE_TOP, len = SCALE_LEN, bl = len / 3.0;
		const double lo = FF_WET_BOUNDS[level], hi = FF_WET_BOUNDS[level + 1];
		const double frac = (level + std::clamp((a_wet - lo) / (hi - lo), 0.0, 1.0)) / 3.0;
		constexpr std::uint32_t COLS[3] = { 0x7FCDE6, 0x3FA9D3, 0x1D76A2 };
		// frame like the stage strip, then the empty well
		Line(g, 0.9, 0x8A8D91);
		Fill(g, 0x2B2D30);
		RoundRectPath(g, X0 - 1.6, top - 1.6, W + 3.2, len + 3.2, 1.2);
		EndFill(g);
		FillRect(g, X0, top, W, len, 0.0, 0x141518);
		for (int k = 0; k < 3; ++k) {
			const double full = frac * 3.0 - k;
			if (full <= 0.0) {
				break;
			}
			const double h = bl * std::min(1.0, full);
			FillRect(g, X0, top + len - k * bl - h, W, h, 0.0, COLS[k]);
		}
		// the surface, then the block dividers over it
		const double y = top + len - len * frac;
		Line(g, 0.9, 0xE8F6FF, 90.0);
		MoveTo(g, X0, y + 0.4);
		LineTo(g, X0 + W, y + 0.4);
		Line(g, 1.2, 0x2B2D30);
		for (int k = 1; k < 3; ++k) {
			const double ty = top + len - k * bl;
			MoveTo(g, X0 - 1.6, ty);
			LineTo(g, X0 + W + 1.6, ty);
		}
		// a drop on top
		const double x = X0 + W / 2.0, yy = top - 8.0;
		Line(g, 0.6, 0x1D2B35);
		Fill(g, 0x7FBBDC);
		MoveTo(g, x, yy - 5.0);
		CurveTo(g, x + 3.0, yy - 1.5, x + 3.0, yy + 1.0);
		CurveTo(g, x + 3.0, yy + 4.0, x, yy + 4.0);
		CurveTo(g, x - 3.0, yy + 4.0, x - 3.0, yy + 1.0);
		CurveTo(g, x - 3.0, yy - 1.5, x, yy - 5.0);
		EndFill(g);
		NoLine(g);
	}

	struct ColdGaugeState
	{
		bool   failed = false;
		int    shown = -1;          // alpha last set: 1 shown, 0 hidden, -1 not yet (or must be set again)
		AutoShow autoShow;          // when to show (combat, stage changes)
		double shownAlpha = -1.0;   // the alpha last set while on
		int    shownWarmth = -1;    // the warmth line last written
		int    warmLast = -1;              // warmthOnChange: the warmth number last seen
		std::int64_t warmChangedAt = 0;    // when it last changed
		double warmAlpha = -1.0;           // the warmth line's alpha last set
		double shownDomeK = -1.0;   // the mercury's dome last drawn (flattens while it boils)
		bool   staticDrawn = false;
		bool   wasRed = false;
		double shownP = -1.0;     // where the mercury is drawn (eases toward the target)
		double shownArrow = -2.0; // where the place arrow is drawn (-1 = none)
		bool   fxShown = false;   // fire or ice drawn last time (one more redraw clears it)
		int    shownValue = -1;
		int    shownWet = -2;     // Frostfall's wetness last drawn (-1 = none shown)
		std::string shownWetText{ "\x01" };  // the wetness line last written
		int    shownKey = -1;     // stage and flags last drawn
		int    shownShift = 0;    // cold protection's stage shift last drawn (DrawStrip)
		double placedFor = -1.0;  // visible frame width the position was worked out for
		// the fire's own clip (over the drawing): the red-hot tint and the blob sprites, made on the first fire
		bool             fireMade = false;
		bool             fireOn = false;
		double           fireScale = -1.0;
		GFx              fireClip[2], fireTint;  // [0] behind the gauge, [1] see-through in front of it
		std::vector<GFx> fireSprites[2][FP_COUNT];
		int              fireUsed[2][FP_COUNT]{};  // sprites of each layer and pass shown last frame
		// the pre-rendered burn (2026-11-08): Interface\NeedsHUD\burn.swf, a flipbook of the fire layer only, played
		// over the gauge by gotoAndStop from this tick
		bool   burnMade = false;
		int    burnFrame = -1;
		GFx    burnClip, boilClip, boilMask, surfClip;
		double maskTop = -1.0;  // the liquid top the boil mask was drawn for
		double maskDome = -1.0; // and the dome it had
		bool   frostMade = false, frostOn = false;
		int    frostFrame = -1;
		double frostScale = -1.0;
		GFx    frostBox, frostClip;
		bool   waterMade = false, waterOn = false;
		int    waterFrame = -1;
		double waterScale = -1.0;
		GFx    waterBox, waterClip;
	};
	ColdGaugeState g_gauge;
	std::vector<FireBlob> g_fireBlobs;

	constexpr const char* GAUGE_CLIP = "NeedsHUD_Cold";

	// Show and hide with the vanilla HUD (the Tween menu, inventory, books, sleep/wait...): hudmenu.swf keeps its
	// elements in HUDMovieBaseInstance.HudElements, and on every mode change (SetModes) sets each one's _visible to
	// whether it has a member named after the new mode. So our clip joins that list and copies the health bar's
	// mode members: it is shown exactly where the health bar is (research 2026-11-04, hudmenu.swf SetModes).
	void JoinHudModes(RE::GFxMovieView* a_movie, GFx& a_box, const char* a_what = "Cold gauge")
	{
		GFx health, list;
		if (!a_movie->GetVariable(&health, "_root.HUDMovieBaseInstance.Health") || !health.IsDisplayObject() ||
			!a_movie->GetVariable(&list, "_root.HUDMovieBaseInstance.HudElements") || !list.IsArray()) {
			logger::warn("HUD modes not found: the {} stays up in every HUD mode", a_what);
			return;
		}
		// the mode flags are the health bar's own members set to true (All, TweenMode... as hudmenu.swf sets them)
		std::vector<std::string> modes;
		health.VisitMembers([&](const char* a_name, const GFx& a_value) {
			if (a_name && a_value.IsBool() && a_value.GetBool()) {
				modes.emplace_back(a_name);
			}
		});
		std::string copied;
		for (const auto& mode : modes) {
			a_box.SetMember(mode.c_str(), GFx(true));
			copied += (copied.empty() ? "" : ", ") + mode;
		}
		list.PushBack(a_box);
		GFx visible;
		if (health.GetMember("_visible", &visible) && visible.IsBool()) {
			a_box.SetMember("_visible", visible);  // the current mode, until the next change
		}
		logger::info("{} follows the HUD modes of the health bar: {}", a_what, copied);
	}

	GFx ChildClip(RE::GFxMovieView* a_movie, GFx& a_parent, const char* a_name, double a_depth)
	{
		GFx clip;
		if (a_parent.GetMember(a_name, &clip) && clip.IsDisplayObject()) {
			return clip;
		}
		const GFx args[2]{ MakeString(a_movie, a_name), a_depth };
		a_parent.Invoke("createEmptyMovieClip", nullptr, args, 2);
		a_parent.GetMember(a_name, &clip);
		return clip;
	}

	// new options from the settings page: take them, and make the tick place, size and redraw everything again
	// (the labels are made once with their size and place, so they are removed and made anew)
	void TakeNewColdOptions(GFx* a_box)
	{
		if (!g_coldChanged.exchange(false)) {
			return;
		}
		{
			std::scoped_lock lock(g_coldLock);
			g_cold = g_coldPending;
		}
		g_gauge.placedFor = -1.0;
		g_gauge.shownKey = -1;
		g_gauge.shownValue = -1;
		g_gauge.shownWarmth = -1;
		g_gauge.warmLast = -1;
		g_gauge.warmAlpha = -1.0;
		g_gauge.shownWet = -2;
		g_gauge.shownWetText = "\x01";
		g_gauge.shown = -1;  // the show/hide check runs again
		if (a_box && a_box->IsDisplayObject()) {
			for (const char* name : { "nameSh", "name", "warmSh", "warm", "numSh", "num", "wetSh", "wetL" }) {
				GFx field;
				if (a_box->GetMember(name, &field) && field.IsDisplayObject()) {
					field.Invoke("removeTextField", nullptr, nullptr, 0);
				}
			}
		}
	}

	// a thermometer fed by another author's config file (ConfigLoader.inl): already on the gauge's own scale
	struct ColdCfgRead
	{
		int                      n = 6, idx = 0;
		double                   target = 0.0, arrow = -1.0, raw = 0.0, wetV = -1.0, wetRaw = 0.0;
		bool                     nearHeat = false, inWater = false;
		std::vector<std::string> names, wetNames;
		std::string              word;
	};
	bool ConfigColdRead(ColdCfgRead& a_out);  // ConfigLoader.inl
	int  g_coldSlotsBase = -1;                 // the built-in mods' block count (Starfrost 5, else 6)

	void ColdGaugeTick()
	{
		if (g_gauge.failed) {
			return;
		}
		static int seenLoad = 0;
		if (const int load = g_loadCount.load(); load != seenLoad) {
			seenLoad = load;
			g_gauge.shownP = -1.0;  // the mercury jumps to the loaded save's cold instead of easing from the old one
			g_gauge.autoShow = AutoShow{};  // "only on change" compares with the loaded game, not the one before (1.0.3)
			g_fireAt = g_frostAt = g_fireFrom = g_frostFrom = -100000;  // a hit just before the load doesn't burn on after it
		}
		auto ui = RE::UI::GetSingleton();
		if (!ui) {
			return;
		}
		auto hud = ui->GetMenu(RE::HUDMenu::MENU_NAME);
		RE::GFxMovieView* movie = hud ? hud->uiMovie.get() : nullptr;
		if (!movie) {
			return;
		}
		GFx root;
		if (!movie->GetVariable(&root, "_root") || !root.IsDisplayObject()) {
			return;
		}

		// find or build our clip (the HUD movie can be rebuilt, so look every time)
		GFx box;
		if (!root.GetMember(GAUGE_CLIP, &box) || !box.IsDisplayObject()) {
			GFx depth;
			root.Invoke("getNextHighestDepth", &depth, nullptr, 0);
			const GFx args[2]{ MakeString(movie, GAUGE_CLIP), depth.IsNumber() ? depth.GetNumber() : 9000.0 };
			root.Invoke("createEmptyMovieClip", nullptr, args, 2);
			if (!root.GetMember(GAUGE_CLIP, &box) || !box.IsDisplayObject()) {
				logger::error("Could not add the cold gauge to the HUD");
				g_gauge.failed = true;
				return;
			}
			g_gauge = ColdGaugeState{};
			JoinHudModes(movie, box);
			logger::info("Cold gauge added to the HUD");
		}
		// children: the vanilla ornament (masked so only its top shows), its mask, and the drawing
		GFx orn = ChildClip(movie, box, "orn", 1.0);
		GFx mask = ChildClip(movie, box, "ornMask", 2.0);
		GFx g = ChildClip(movie, box, "g", 3.0);
		if (!orn.IsDisplayObject() || !mask.IsDisplayObject() || !g.IsDisplayObject()) {
			return;
		}

		TakeNewColdOptions(&box);

		// shown only while it is switched on and Survival Mode is on (by alpha: _visible belongs to the HUD's modes,
		// see JoinHudModes)
		// which mod's cold to show: SunHelm while it runs with its cold on, else Survival Mode (vanilla or Survival Mode
		// Improved, which writes vanilla's own values)
		// a config file's cold (its mod running) wins over every built-in source
		static ColdCfgRead cc;
		const bool cfgCold = ConfigColdRead(cc);
		if (g_coldSlotsBase < 0) {
			g_coldSlotsBase = g_coldSlots;
		}
		g_coldSlots = cfgCold ? std::clamp(cc.n, 3, 6) : g_coldSlotsBase;
		const bool sunhelm = !cfgCold && SunHelmOn() && GlobalOn(g_shCold, g_shColdOff) && !(g_shColdForceOff && GV(g_shColdForceOff) >= 1.0f);
		// Frostfall once started (it has no hunger or thirst: players pair it with another needs mod, often SunHelm with
		// SunHelm's cold off); it wins over Survival Mode's cold, which it doesn't know about
		const bool frostfall = !cfgCold && !sunhelm && !g_starfrost && FrostfallOn();
		const bool survival = g_coldValue && g_survivalOn && GV(g_survivalOn) >= 1.0f;
		const bool on = g_cold.enabled && (cfgCold || sunhelm || frostfall || survival);
		g_ffShown = frostfall;
		if (!on) {
			if (g_gauge.shown != 0) {
				g_gauge.shown = 0;
				box.SetMember("_alpha", GFx(0.0));
			}
			g_gauge.autoShow.alpha = -1.0;  // comes back without a fade
			g_gauge.shownAlpha = -1.0;
			return;
		}
		g_gauge.shown = 1;  // the alpha itself is set below, once the stage is known (when to show)
		g_ffWaterLvl = frostfall ? FrostfallWaterLevel() : 0;
		// cold protection ([ColdProtection] in a config file): the biggest stage shift, and whether freezing water is harmless
		const bool waterSafe = ColdProtectionWaterSafe();
		g_coldShift = ColdProtectionShift();
		g_wetNow = !waterSafe && (cfgCold ? cc.inWater : sunhelm ? (g_shInWater && GV(g_shInWater) >= 1.0f) : frostfall ? g_ffWaterLvl.load() > 0 : InFreezingWater());  // the fire/frost/water tick runs while this is set

		// place and size it: from the chosen screen corner, offset by the set percents of the visible screen; the
		// gauge counts as a box as wide as its labels ("Comfortable" is the widest part) and as tall as tip to last
		// label, and is always kept fully on screen
		const auto rect = movie->GetVisibleFrameRect();
		const double width = rect.right - rect.left, height = rect.bottom - rect.top;
		const double s = UNIT_TO_HUD * g_cold.scale;
		if (width != g_gauge.placedFor) {
			g_gauge.placedFor = width;
			const double halfW = 38.0 * g_cold.scale;
			const double tall = (BULB_Y + RING_LIGHT + 2.35 + 1.4 + 6.0) * s + 15.0 * g_cold.scale * 1.5 +
			                    (g_cold.warmth ? 12.0 * g_cold.scale * 1.5 : 0.0) +
			                    (g_cold.numbers ? 12.0 * g_cold.scale * 1.6 : 15.0 * g_cold.scale * 0.2);
			const int  anchor = std::clamp(g_cold.anchor, 0, 3);
			const bool fromRight = anchor == 1 || anchor == 3, fromBottom = anchor >= 2;
			double cx = fromRight ? rect.right - width * g_cold.posX / 100.0 - halfW : rect.left + width * g_cold.posX / 100.0 + halfW;
			double y = fromBottom ? rect.bottom - height * g_cold.posY / 100.0 - tall : rect.top + height * g_cold.posY / 100.0;
			cx = std::clamp(cx, rect.left + 6.0 + halfW, std::max(rect.left + 6.0 + halfW, rect.right - 6.0 - halfW));
			y = std::clamp(y, rect.top + 6.0, std::max(rect.top + 6.0, rect.bottom - 6.0 - tall));
			box.SetMember("_x", GFx(cx));  // the box's origin is the tube's centre line, at the top tip
			box.SetMember("_y", GFx(y));
			for (GFx* c : { &orn, &mask, &g }) {
				c->SetMember("_xscale", GFx(s * 100.0));
				c->SetMember("_yscale", GFx(s * 100.0));
			}
		}

		// the cold now, as the six blocks of the scale: 0 Warm, 1 Comfortable, then the four colder stages (both mods
		// have this shape); idx = the stage shown, target/arrowTarget on the same 0..1 scale (0 = top of the warm block)
		static constexpr const char* VANILLA_COLD_NAMES[6] = { "Warm", "Comfortable", "Chilly", "Very Cold", "Freezing", "Numb" };
		static constexpr const char* SUNHELM_COLD_NAMES[6] = { "Warm", "Comfortable", "Chilly", "Cold", "Freezing", "Frigid" };
		static constexpr const char* STARFROST_COLD_NAMES[6] = { "Warm", "Comfortable", "Cold", "Very Cold", "Freezing", "Freezing" };
		static constexpr const char* FROSTFALL_COLD_NAMES[6] = { "Warm", "Comfortable", "Chilly", "Very Cold", "Freezing", "Freezing to Death" };
		double value = 0.0, target = 0.0, arrowTarget = -1.0;
		int    idx = 1;
		bool   nearHeat = false;
		const char* const* coldNames = VANILLA_COLD_NAMES;
		static std::array<const char*, 6> cfgNames{};
		if (cfgCold) {
			// a config file: its own stages as the blocks (ConfigLoader.inl worked out the positions)
			for (int i = 0; i < 6; ++i) {
				cfgNames[i] = i < static_cast<int>(cc.names.size()) ? cc.names[i].c_str() : "";
			}
			coldNames = cfgNames.data();
			value = cc.raw;
			idx = cc.idx;
			nearHeat = cc.nearHeat;
			target = cc.target;
			arrowTarget = cc.arrow;
		} else if (sunhelm) {
			// SunHelm's own stages (its _SHColdSystem: 0/25/150/250/450/700, capped at its cold cap, 900): one block
			// each, the value piecewise inside; the place arrow is the cold this place drags you to (_SHAmbientTemperature)
			coldNames = SUNHELM_COLD_NAMES;
			const double cap = g_shColdCap ? std::max(701.0, static_cast<double>(GV(g_shColdCap))) : 900.0;
			const std::array<double, 7> b{ 0.0, 25.0, 150.0, 250.0, 450.0, 700.0, cap };
			const auto pos = [&](double a_v) {
				a_v = std::clamp(a_v, 0.0, cap);
				int i = 0;
				while (i < 5 && a_v >= b[i + 1]) {
					++i;
				}
				return (i + std::clamp((a_v - b[i]) / (b[i + 1] - b[i]), 0.0, 1.0)) / 6.0;
			};
			value = GV(g_shCold);
			idx = 0;
			while (idx < 5 && value >= b[idx + 1]) {
				++idx;
			}
			nearHeat = g_shNearHeat && GV(g_shNearHeat) >= 1.0f;
			target = pos(value);
			arrowTarget = nearHeat ? 0.0 : (g_shAmbient ? pos(GV(g_shAmbient)) : -1.0);
			// in freezing water SunHelm drives your cold toward its cap (_SHHeatDetection.ApplyFreezingWater sets the cold
			// level limit to _SHColdLevelCap and adds 20 a check) while its ambient global stays the place's: the arrow points
			// where the cold is really heading, the bottom (his ask 2026-11-10)
			if (g_shInWater && GV(g_shInWater) >= 1.0f) {
				arrowTarget = 1.0;
			}
		} else if (frostfall) {
			// Frostfall: six equal 20-point bands, one block each; the place arrow is where its exposure is heading
			// (its own target already counts fire, shelter, weather and clothes)
			coldNames = FROSTFALL_COLD_NAMES;
			const auto pos = [](double a_v) { return std::clamp(a_v, 0.0, FF_MAX_EXPOSURE) / FF_MAX_EXPOSURE; };
			value = GV(g_ffExposure);
			idx = std::clamp(static_cast<int>(value / 20.0), 0, 5);
			nearHeat = g_ffNearFire && GV(g_ffNearFire) >= 2.0f;
			target = pos(value);
			arrowTarget = g_ffTarget ? pos(GV(g_ffTarget)) : -1.0;
			// in cold water Frostfall's water effects pull exposure toward 40 / 60 / 80 (_frost_coldwatereffect1-3), beside
			// its exposure target: the arrow points at the colder of the two (lethal frigid water: the bottom, set below)
			if (const int lvl = g_ffWaterLvl.load(); lvl > 0) {
				arrowTarget = std::max(arrowTarget, pos(20.0 + 20.0 * std::min(lvl, 3)));
			}
		} else if (g_starfrost) {
			// Starfrost (on SMI): five blocks, its own stages: Warm below SMI's stage 1, its two "neutral" stages as one
			// Comfortable block (his call 2026-11-08: only the stages that mean something), then Cold, Very Cold, Freezing
			coldNames = STARFROST_COLD_NAMES;
			value = GV(g_coldValue);
			const double s1 = g_smiColdStage1 > 0.0 ? g_smiColdStage1 : 100.0;
			const std::array<double, 6> b{ 0.0, s1, STAGE_VALUES[2], STAGE_VALUES[3], STAGE_VALUES[4], STAGE_VALUES[5] };
			const auto pos = [&](double a_v) {
				a_v = std::clamp(a_v, 0.0, b[5]);
				int i = 0;
				while (i < 4 && a_v >= b[i + 1]) {
					++i;
				}
				const double w = b[i + 1] - b[i];
				return (i + (w > 0.0 ? std::clamp((a_v - b[i]) / w, 0.0, 1.0) : 1.0)) / 5.0;
			};
			idx = 0;
			while (idx < 4 && value >= b[idx + 1]) {
				++idx;
			}
			nearHeat = g_temperatureLevel && std::lround(GV(g_temperatureLevel)) == 1;
			target = pos(value);
			if (nearHeat) {
				arrowTarget = 0.0;
			} else if (g_smiAmbient) {
				const double amb = GV(g_smiAmbient);
				arrowTarget = pos(amb >= SMI_AMBIENT_MAX ? b[5] : amb);
			}
		} else {
			// Survival: the five cold blocks for the cold value; while a heat source warms you and you are Comfortable,
			// it climbs on into the warm block (Survival's value can't go below 0)
			value = GV(g_coldValue);
			const int stage = ColdStage(value);
			nearHeat = g_temperatureLevel && std::lround(GV(g_temperatureLevel)) == 1;
			idx = (nearHeat && stage == 0) ? 0 : stage + 1;
			target = 1.0 / 6.0 + ColdPos(value) * 5.0 / 6.0;
			if (nearHeat && stage == 0) {
				target = (value / STAGE_VALUES[1]) * (2.0 / 6.0);  // 0 (top of the warm block) .. Comfortable's bottom
			}
			// the place arrow, from the bridge: how cold this place can make you; near a fire it swings up into the
			// warm block. Spicy never moves it (it shows the place, not you)
			// (the bridge ability itself is given on the main thread after each load: BridgesCheck)
			const int placeLevel = g_placeCold ? static_cast<int>(std::lround(GV(g_placeCold))) : -1;
			if (nearHeat) {
				arrowTarget = 0.0;
			} else if (g_smiAmbient) {
				// Survival Mode Improved: your cold climbs to the place's ambient cold and stops there (600+: all the way)
				const double amb = GV(g_smiAmbient);
				arrowTarget = 1.0 / 6.0 + ColdPos(amb >= SMI_AMBIENT_MAX ? STAGE_VALUES[5] : amb) * 5.0 / 6.0;
			} else if (placeLevel >= 0) {
				arrowTarget = 1.0 / 6.0 + ColdPos(PlaceColdValue(placeLevel)) * 5.0 / 6.0;
			}
		}
		// freezing water: under Survival it kills (its damage spell while you swim in it), so the mercury sinks to the
		// very bottom as the warning, whatever the cold value says, and climbs back to the truth when you get out (his
		// call, 2026-11-08). Under SunHelm the water only drives your cold up fast (Frigid is what hurts), so the mercury
		// keeps telling the truth and the water look shows why it climbs (his call, 2026-11-08)
		const bool deadlyWater = g_wetNow.load() && !cfgCold && !sunhelm && !g_starfrost && (!frostfall || FrostfallWaterDeadly(g_ffWaterLvl.load()));  // Starfrost's water drains stamina; the cold is what hurts; Frostfall's frigid water kills when lethal is on
		if (deadlyWater) {
			target = 1.0;
			arrowTarget = 1.0;  // the place arrow sinks with it (his ask 2026-11-10: it stood still while the mercury dropped)
		}
		// red is a warning about you, not the place: only while your own cold is in the lowest range (Numb, Frigid)
		// (his rule, 2026-11-05); the halo while it hurts: Chilly or colder
		const bool red = idx == std::clamp(g_coldSlots, 3, 6) - 1;
		const bool hurts = idx >= 2;
		if (!g_gauge.staticDrawn || red != g_gauge.wasRed) {
			DrawStatic(movie, orn, mask, g, red);
			orn.Invoke("setMask", nullptr, &mask, 1);
			g_gauge.staticDrawn = true;
			g_gauge.wasRed = red;
			g_gauge.shownKey = -1;
		}
		// ease toward the target (a fifth of the way each tick, snapping when close)
		double p = g_gauge.shownP < 0.0 ? target : g_gauge.shownP + (target - g_gauge.shownP) * 0.2;
		if (std::abs(target - p) < 0.0008) {
			p = target;
		}
		// the arrow eases the same way (it appears and goes at once)
		double arrow = arrowTarget;
		// in freezing water an arrow that wasn't showing starts from the mercury's level, so it sinks along with it
		const double arrowFrom = g_gauge.shownArrow >= 0.0 ? g_gauge.shownArrow : (g_wetNow.load() && g_gauge.shownP >= 0.0 ? g_gauge.shownP : -1.0);
		if (arrowTarget >= 0.0 && arrowFrom >= 0.0) {
			arrow = arrowFrom + (arrowTarget - arrowFrom) * 0.2;
			if (std::abs(arrowTarget - arrow) < 0.0008) {
				arrow = arrowTarget;
			}
		}
		// hit by fire / frost (flavour): flames hold 2 s after the last hit and die down over 0.7 s; ice holds 3 s and
		// thaws over 1.2 s. While either shows, the gauge is redrawn every tick so the flames move
		const std::int64_t now = NowMs();
		const double ice = FROST_FLIPBOOK ? 0.0 : FxStrength(g_frostAt, g_frostFrom, now, ICE_HOLD, ICE_FADE);  // frost: its own flipbook
		const bool fx = ice > 0.0;  // the fire has its own clip and tick (ColdFireTick)
		const bool frostbitten = Afflicted(2);  // Survival's Frostbitten affliction: the badge (under any needs mod)
		// Frostfall's wetness (its own bar left of the strip; hidden while dry)
		const double wetV = !g_cold.wetness ? 0.0 : cfgCold ? std::max(0.0, cc.wetV) : (frostfall && g_ffWet) ? static_cast<double>(GV(g_ffWet)) : 0.0;
		const int    wetLvl = FrostfallWetLevel(wetV);
		const int    wetShown = wetLvl < 0 ? -1 : static_cast<int>(std::lround(wetV));
		const int key = idx * 4 + (nearHeat ? 2 : 0) + (red ? 1 : 0) + (g_wetNow.load() ? 100 : 0) + (sunhelm ? 1000 : 0) + (g_starfrost ? 2000 : 0) + (frostbitten ? 4000 : 0) + (frostfall ? 8000 : 0) + g_ffWaterLvl.load() * 16000 + (cfgCold ? 100000 + cc.n * 1000000 : 0);  // the labels change in freezing water
		{
			// when to show: a "stage" here is a cold stage (Comfortable to Numb), freezing water, frostbite, wetness; worst = Numb
			// or the water. Not Warm: Survival shows Warm while a heat source is near, and nearly every inn, house and shop
			// has a fire, so each door flipped Comfortable / Warm and showed the gauge twice per building (a player's report
			// on Nexus, 2026-11-10). Warm still shows on the gauge; it just doesn't bring it up. Near heat itself doesn't count
			// either (SunHelm and Frostfall have the same flag).
			// A gear change (warmth) counts, while the warmth line is on: putting on a fur cloak shows the new number
			const bool survivalWarm = !cfgCold && !sunhelm && !frostfall && !g_starfrost && idx == 0;
			const int  showIdx = survivalWarm ? 1 : idx;
			// every part as its own entry (1.0.4 review: no packing into one number)
			const std::vector<std::uint64_t> showKey{ static_cast<std::uint64_t>(showIdx), static_cast<std::uint64_t>(g_wetNow.load()), static_cast<std::uint64_t>(sunhelm),
				static_cast<std::uint64_t>(g_starfrost), static_cast<std::uint64_t>(frostbitten), static_cast<std::uint64_t>(frostfall), static_cast<std::uint64_t>(g_ffWaterLvl.load()),
				static_cast<std::uint64_t>(cfgCold), static_cast<std::uint64_t>(cfgCold ? cc.n : 0), static_cast<std::uint64_t>(wetLvl + 1),
				static_cast<std::uint64_t>(g_cold.warmth ? std::llround(WarmthNow()) : 0) };
			// how bad each part is, for onlyWorse (higher = worse): the stage, freezing water and how deep, frostbite, wetness, and
			// less warmth from what you wear
			const std::vector<int> levels{ showIdx, g_wetNow.load() ? 1 : 0, g_ffWaterLvl.load(), frostbitten ? 1 : 0, wetLvl + 1,
				g_cold.warmth ? -static_cast<int>(std::lround(WarmthNow())) : 0 };
			const double alpha = AutoShowAlpha(g_gauge.autoShow, g_cold, showKey, red || g_wetNow.load(), now, levels);
			if (alpha != g_gauge.shownAlpha) {
				g_gauge.shownAlpha = alpha;
				box.SetMember("_alpha", GFx(alpha));
			}
		}
		// the warmth line only for a while after the number changes (warmthOnChange, 2026-11-10, his ask): it fades in and out
		// like the gauge; its place under the stage name stays (the lines under it don't jump)
		if (g_cold.warmth) {
			double warmTarget = 100.0;
			if (g_cold.warmthOnChange) {
				const int w = static_cast<int>(std::lround(WarmthNow()));
				if (w != g_gauge.warmLast) {
					g_gauge.warmLast = w;
					g_gauge.warmChangedAt = now;
				}
				warmTarget = (now - g_gauge.warmChangedAt) < static_cast<std::int64_t>(g_cold.showSeconds * 1000.0) ? 100.0 : 0.0;
			}
			const double wa = g_gauge.warmAlpha < 0.0 ? warmTarget :
			                  warmTarget > g_gauge.warmAlpha ? std::min(warmTarget, g_gauge.warmAlpha + 25.0) : std::max(warmTarget, g_gauge.warmAlpha - 25.0);
			if (wa != g_gauge.warmAlpha) {
				GFx f;
				bool set = false;
				for (const char* name : { "warmSh", "warm" }) {
					if (box.GetMember(name, &f) && f.IsDisplayObject()) {
						f.SetMember("_alpha", GFx(wa));
						set = true;
					}
				}
				if (set) {
					g_gauge.warmAlpha = wa;  // (not yet made: set again once the line exists)
				}
			}
		}
		// the dome flattens while the liquid boils and comes back after (a step of 0.2 a tick: about half a second)
		g_domeK = g_boiling ? std::max(0.0, g_domeK - 0.2) : std::min(1.0, g_domeK + 0.2);
		const bool domeChanged = g_domeK != g_gauge.shownDomeK;
		g_gauge.shownDomeK = g_domeK;
		const bool warmthChanged = g_cold.warmth && std::lround(WarmthNow()) != g_gauge.shownWarmth;
		if (!fx && !g_gauge.fxShown && std::abs(p - g_gauge.shownP) < 0.0004 && std::abs(arrow - g_gauge.shownArrow) < 0.0004 &&
			key == g_gauge.shownKey && !warmthChanged && !domeChanged && wetShown == g_gauge.shownWet && g_coldShift == g_gauge.shownShift) {
			return;
		}
		g_gauge.fxShown = fx;
		g_gauge.shownP = p;
		g_gauge.shownArrow = arrow;
		g_gauge.shownKey = key;
		g_gauge.shownWet = wetShown;
		g_gauge.shownShift = g_coldShift;
		DrawGauge(movie, g, p, hurts, red, arrow, 0.0, ice, (now % 100000) / 1000.0);  // fire: its own clip
		if (frostbitten) {
			DrawAfflictionBadge(g, -17.0, 1.0, 7.5, 2);  // up left of the tube's top, clear of the scale and the place arrow
		}
		if (wetLvl >= 0) {
			DrawWetnessBar(g, wetV);
		}

		// labels under the bulb, in HUD units (not scaled with the drawing: sized and spaced by fScale instead)
		const int whole = static_cast<int>(std::lround(value));
		const double textY = (BULB_Y + RING_LIGHT + 2.35 + 1.4 + 6.0) * s;
		const double nameSize = 15.0 * g_cold.scale, numberSize = 12.0 * g_cold.scale;
		const double fieldW = 240.0 * g_cold.scale, sh = std::max(1.0, 1.5 * g_cold.scale);  // field width, shadow offset
		GFx nameShadow = TextField(movie, box, "nameSh", 10.0, -fieldW / 2 + sh, textY + sh, fieldW, nameSize, 0x000000);
		GFx name = TextField(movie, box, "name", 11.0, -fieldW / 2, textY, fieldW, nameSize, LABEL);
		// in freezing water the stage and the number don't matter (the water kills): both say so (his call, 2026-11-08)
		const bool wet = g_wetNow.load();
		const std::string stageName = wet ? (frostfall ? FF_WATER_NAMES[std::clamp(g_ffWaterLvl.load(), 1, 3)] : "Freezing Water") : coldNames[std::clamp(idx, 0, 5)];
		SetText(movie, nameShadow, stageName);
		SetText(movie, name, stageName);
		// the total warmth of what you wear (Survival uses the same number: Survival_NeedCold, PlayerRef.GetWarmthRating())
		double lineY = textY + nameSize * 1.5;
		if (g_cold.warmth) {
			const int warmth = static_cast<int>(std::lround(WarmthNow()));
			if (warmth != g_gauge.shownWarmth) {
				g_gauge.shownWarmth = warmth;
				GFx warmSh = TextField(movie, box, "warmSh", 14.0, -fieldW / 2 + sh, lineY + sh, fieldW, numberSize, 0x000000);
				GFx warm = TextField(movie, box, "warm", 15.0, -fieldW / 2, lineY, fieldW, numberSize, 0xD8D2C4);
				const std::string text = tr::F("Warmth {}", warmth);
				SetText(movie, warmSh, text);
				SetText(movie, warm, text);
			}
			lineY += numberSize * 1.5;
		}
		const bool deadly = wet && !cfgCold && !sunhelm && !g_starfrost && (!frostfall || FrostfallWaterDeadly(g_ffWaterLvl.load()));  // Survival's freezing water and Frostfall's lethal frigid water kill by themselves
		const int shownNumber = deadly ? -2 : whole;  // -2: the water line (-1 means nothing drawn yet)
		if (g_cold.numbers && shownNumber != g_gauge.shownValue) {
			g_gauge.shownValue = shownNumber;
			const double numY = lineY;
			GFx numShadow = TextField(movie, box, "numSh", 12.0, -fieldW / 2 + sh, numY + sh, fieldW, numberSize, 0x000000);
			GFx num = TextField(movie, box, "num", 13.0, -fieldW / 2, numY, fieldW, numberSize, 0xB8B2A6);
			const std::string text = deadly ? std::string("Deadly") : std::format("{} {}", tr::T(cfgCold ? cc.word : std::string(frostfall ? "Exposure" : "Cold")), whole);  // Frostfall's own word
			SetText(movie, numShadow, text);
			SetText(movie, num, text);
		}
		// Frostfall's wetness line, in pale water blue, under the others (empty while dry)
		{
			const std::string wetWord = (cfgCold && cc.wetNames.size() == 3 && wetLvl >= 0) ? cc.wetNames[wetLvl] : wetLvl >= 0 ? std::string(FF_WET_NAMES[wetLvl]) : std::string();
			const std::string wetText = wetLvl < 0 ? std::string() :
			                            g_cold.numbers ? std::format("{} {}", tr::T(wetWord), cfgCold ? static_cast<int>(std::lround(cc.wetRaw)) : wetShown) : wetWord;
			if (wetText != g_gauge.shownWetText) {
				g_gauge.shownWetText = wetText;
				const double wetY = lineY + (g_cold.numbers ? numberSize * 1.5 : 0.0);
				GFx wetSh = TextField(movie, box, "wetSh", 16.0, -fieldW / 2 + sh, wetY + sh, fieldW, numberSize, 0x000000);
				GFx wetL = TextField(movie, box, "wetL", 17.0, -fieldW / 2, wetY, fieldW, numberSize, 0xA9CBE0);
				SetText(movie, wetSh, wetText);
				SetText(movie, wetL, wetText);
			}
		}
	}

	// ---- the fire's own tick, 30 times a second while it burns (the gauge's tick is 10; the research found 10 too
	// few for flame motion). It only moves, scales, turns and fades sprites made once: one SetDisplayInfo each.
	// ---- the pre-rendered burn (2026-11-08 in-game test): Skyrim's own on-fire look (FireFXShader: VaporTile01 skin,
	// edge glow, FXFireAtlas02 wisps, 0.2 s in / 1.5 s out) rendered offline as the fire layer only, packed into
	// Interface\\NeedsHUD\\burn.swf. Frames 0-5 catch (the loop's own first frames fading in), 6-41 a seamless loop
	// that keeps playing while the fire keeps hitting, 42-105 burn out (straight on from the loop's last frame).
	// A playhead walks the frames at 30 a second: a continuous fire (an aura, a held Flames stream) only loops, it
	// never restarts. The gauge itself (liquid, stages, arrow) stays drawn live underneath, so it always tells the truth.
	constexpr bool   FIRE_FLIPBOOK = true;
	constexpr double BURN_UX = -63.666666666666664, BURN_UY = -92.22222222222223;  // the bitmap's top left, in gauge units
	// (2026-11-10: burn.swf v3 follows the gauge's fixed outline only, no ghost of the place arrow; narrower, so x moved)
	constexpr double BURN_PXU = 1.35;                                              // bitmap pixels per gauge unit
	constexpr int    BURN_IN = 6, BURN_LOOP0 = 6, BURN_LOOPN = 36, BURN_OUT0 = 42, BURN_LAST = 105;
	constexpr double BURN_HOLD = 1.25;  // seconds without a hit before it burns out (an aura hits about once a second)
	// boiling water inside (players know a pot of boiling water): boil.swf = bubbles over the whole liquid, masked
	// live to where the liquid is (a bubble vanishes at the surface: it pops); surf.swf = the surface strip (domes,
	// droplets, steam) moved to the liquid's surface each tick. Same playhead and frames as the burn.
	constexpr double BOIL_UX = -9.15, BOIL_UY = 29.55, BOIL_PXU = 3.0;
	constexpr double SURF_UX = -9.15, SURF_UY = -22.0;  // the strip's top, relative to the liquid's surface

	struct BurnPlay
	{
		int          phase = 0;      // 0 off, 1 catching, 2 looping, 3 burning out
		double       head = 0.0;     // frame (0-based, fractional: it advances by real time)
		std::int64_t lastMs = 0;
	};
	BurnPlay g_burn;

	void BurnTick(RE::GFxMovieView* a_movie, GFx& a_box, std::int64_t a_now)
	{
		const double sinceHit = (a_now - g_fireAt.load()) / 1000.0;
		const bool   hot = sinceHit >= 0.0 && sinceHit < BURN_HOLD;
		auto&        b = g_burn;
		const double steps = b.phase == 0 ? 0.0 : std::clamp((a_now - b.lastMs) * 0.03, 0.0, 6.0);  // 30 frames a second
		b.lastMs = a_now;
		switch (b.phase) {
		case 0:
			if (hot) {
				b.phase = 1;
				b.head = 0.0;
			}
			break;
		case 1:
			b.head += steps;
			if (b.head >= BURN_IN) {
				b.phase = 2;
				b.head = BURN_LOOP0 + BURN_IN + (b.head - BURN_IN);  // the catch showed the loop's first 6 frames
			}
			break;
		case 2:
			b.head += steps;
			while (b.head >= BURN_LOOP0 + BURN_LOOPN) {
				if (hot) {
					b.head -= BURN_LOOPN;  // around again
				} else {
					b.phase = 3;           // out of the loop's end, straight into the burn-out
					break;
				}
			}
			break;
		case 3:
			if (hot) {                   // hit again while burning out: catch again
				b.phase = 1;
				b.head = 0.0;
				break;
			}
			b.head += steps;
			if (b.head > BURN_LAST) {
				b.phase = 0;
			}
			break;
		}
		const bool on = b.phase != 0;
		g_boiling = on;  // the mercury's dome flattens while it boils
		if (!g_gauge.burnMade) {
			if (!on) {
				return;
			}
			g_gauge.fireClip[1] = ChildClip(a_movie, a_box, "fx", 5.0);
			if (!g_gauge.fireClip[1].IsDisplayObject()) {
				return;
			}
			g_gauge.boilMask = ChildClip(a_movie, g_gauge.fireClip[1], "boilMask", 2.0);
			g_gauge.boilClip = ChildClip(a_movie, g_gauge.fireClip[1], "boil", 3.0);
			g_gauge.surfClip = ChildClip(a_movie, g_gauge.fireClip[1], "surf", 4.0);
			g_gauge.burnClip = ChildClip(a_movie, g_gauge.fireClip[1], "burn", 10.0);
			if (!g_gauge.burnClip.IsDisplayObject() || !g_gauge.boilClip.IsDisplayObject() || !g_gauge.surfClip.IsDisplayObject() ||
				!g_gauge.boilMask.IsDisplayObject()) {
				return;
			}
			const GFx url = MakeString(a_movie, "NeedsHUD/burn.swf");
			g_gauge.burnClip.Invoke("loadMovie", nullptr, &url, 1);
			const GFx urlB = MakeString(a_movie, "NeedsHUD/boil.swf");
			g_gauge.boilClip.Invoke("loadMovie", nullptr, &urlB, 1);
			const GFx urlS = MakeString(a_movie, "NeedsHUD/surf.swf");
			g_gauge.surfClip.Invoke("loadMovie", nullptr, &urlS, 1);
			g_gauge.maskTop = -1.0;
			g_gauge.burnMade = true;
			g_gauge.burnFrame = -1;
			logger::info("Burn flipbook: loadMovie NeedsHUD/burn.swf, boil.swf, surf.swf");
		}
		const double scale = UNIT_TO_HUD * g_cold.scale * 100.0;
		if (scale != g_gauge.fireScale) {
			g_gauge.fireScale = scale;
			GFx::DisplayInfo di;
			di.SetScale(scale, scale);
			g_gauge.fireClip[1].SetDisplayInfo(di);
		}
		if (!on) {
			if (g_gauge.fireOn) {
				g_gauge.fireOn = false;
				const GFx last(static_cast<double>(BURN_LAST + 1));  // the empty last frame, then hidden as well
				g_gauge.burnClip.Invoke("gotoAndStop", nullptr, &last, 1);
				g_gauge.boilClip.Invoke("gotoAndStop", nullptr, &last, 1);
				g_gauge.surfClip.Invoke("gotoAndStop", nullptr, &last, 1);
				g_gauge.burnClip.SetMember("_visible", GFx(false));
				g_gauge.fireClip[1].SetMember("_visible", GFx(false));
				g_gauge.burnFrame = -1;
				logger::info("Burn flipbook: off ({:.2f} s after the last hit)", sinceHit);
			}
			return;
		}
		if (!g_gauge.fireOn) {
			g_gauge.fireOn = true;
			g_gauge.burnClip.SetMember("_visible", GFx(true));
			g_gauge.fireClip[1].SetMember("_visible", GFx(true));
			logger::info("Burn flipbook: on");
		}
		// the liquid's surface now (where the mercury is drawn), and the boil mask drawn to it when it moves
		const double top = ScaleY(std::clamp(g_gauge.shownP < 0.0 ? 0.5 : g_gauge.shownP, 0.0, 1.0));
		const double dome = MERC_DOME * g_domeK;  // the mask follows the mercury's top (domed, or flat while it boils)
		if (std::abs(top - g_gauge.maskTop) > 0.05 || std::abs(dome - g_gauge.maskDome) > 0.01) {
			g_gauge.maskTop = top;
			g_gauge.maskDome = dome;
			auto& m = g_gauge.boilMask;
			m.Invoke("clear", nullptr, nullptr, 0);
			NoLine(m);
			Fill(m, 0xFFFFFF, 100.0);
			MoveTo(m, -COL_HALF, top + dome);
			CurveTo(m, 0.0, top - dome, COL_HALF, top + dome);
			LineTo(m, COL_HALF, BULB_Y);
			LineTo(m, -COL_HALF, BULB_Y);
			LineTo(m, -COL_HALF, top + dome);
			EndFill(m);
			Fill(m, 0xFFFFFF, 100.0);
			CirclePath(m, 0.0, BULB_Y, BULB_R);
			EndFill(m);
		}
		const int f = std::clamp(static_cast<int>(b.head), 0, BURN_LAST);
		if (f != g_gauge.burnFrame) {
			// place and size every time: loadMovie finishes a frame or two later and may reset the clips
			const auto place = [](GFx& a_clip, double a_x, double a_y, double a_pxu) {
				GFx::DisplayInfo di;
				di.SetPosition(a_x, a_y);
				di.SetScale(100.0 / a_pxu, 100.0 / a_pxu);
				a_clip.SetDisplayInfo(di);
			};
			place(g_gauge.burnClip, BURN_UX, BURN_UY, BURN_PXU);
			place(g_gauge.boilClip, BOIL_UX, BOIL_UY, BOIL_PXU);
			place(g_gauge.surfClip, SURF_UX, top + SURF_UY, BOIL_PXU);
			const GFx mask[1]{ g_gauge.boilMask };
			g_gauge.boilClip.Invoke("setMask", nullptr, mask, 1);
			const GFx frame(static_cast<double>(f + 1));
			g_gauge.burnClip.Invoke("gotoAndStop", nullptr, &frame, 1);
			g_gauge.boilClip.Invoke("gotoAndStop", nullptr, &frame, 1);
			g_gauge.surfClip.Invoke("gotoAndStop", nullptr, &frame, 1);
			g_gauge.burnFrame = f;
		}
	}

	// ---- the pre-rendered frost (2026-11-08, his call: "the same logic as the fire", no stream): Skyrim's own frost hit
	// (FrostFXShader 0x1F03A: still frost skin + falling cold-mist wisps from MagicIceWisps, 0.25 s in / 1.5 s out)
	// rendered offline over the whole gauge (he wants it covered, the gauge still readable: frost alpha capped at
	// 0.5), with Skyrim's own frost speckle (a crisper vein texture was tried and he judged it worse), creeping in from
	// the edges like frost on a window and thawing back out. Interface\NeedsHUD\frost.swf: 0-17 creep in (blends into the loop's first
	// frame), 18-53 a seamless loop while frost keeps hitting, 54-117 thaw. No frozen liquid (his call: not needed).
	constexpr double FROST_UX = -70.33333333333333, FROST_UY = -20.0, FROST_PXU = 1.35;  // frost.swf v4 (2026-11-09): its own layer over the gauge, no snapshot
	constexpr int    FROST_IN = 18, FROST_LOOP0 = 18, FROST_LOOPN = 36, FROST_LAST = 117;
	constexpr double FROST_HOLD = 1.25;  // seconds without a frost hit before it thaws (a held Frostbite hits often)
	BurnPlay g_frost;

	// one flipbook's playhead: catch -> loop while hit -> out; a hit while going out catches again
	void AdvancePlay(BurnPlay& b, bool a_hot, std::int64_t a_now, int a_in, int a_loop0, int a_loopN, int a_last, int a_jump)
	{
		const double steps = b.phase == 0 ? 0.0 : std::clamp((a_now - b.lastMs) * 0.03, 0.0, 6.0);  // 30 frames a second
		b.lastMs = a_now;
		switch (b.phase) {
		case 0:
			if (a_hot) {
				b.phase = 1;
				b.head = 0.0;
			}
			break;
		case 1:
			b.head += steps;
			if (b.head >= a_in) {
				b.phase = 2;
				b.head = a_jump + (b.head - a_in);
			}
			break;
		case 2:
			b.head += steps;
			while (b.head >= a_loop0 + a_loopN) {
				if (a_hot) {
					b.head -= a_loopN;
				} else {
					b.phase = 3;
					break;
				}
			}
			break;
		case 3:
			if (a_hot) {
				b.phase = 1;
				b.head = 0.0;
				break;
			}
			b.head += steps;
			if (b.head > a_last) {
				b.phase = 0;
			}
			break;
		}
	}

	void FrostTick(RE::GFxMovieView* a_movie, GFx& a_box, std::int64_t a_now)
	{
		const double sinceHit = (a_now - g_frostAt.load()) / 1000.0;
		const bool   cold = sinceHit >= 0.0 && sinceHit < FROST_HOLD;
		auto&        b = g_frost;
		AdvancePlay(b, cold, a_now, FROST_IN, FROST_LOOP0, FROST_LOOPN, FROST_LAST, FROST_LOOP0);
		const bool on = b.phase != 0;
		if (!g_gauge.frostMade) {
			if (!on) {
				return;
			}
			g_gauge.frostBox = ChildClip(a_movie, a_box, "frostFx", 6.0);  // over the fire's layer, under the labels
			if (!g_gauge.frostBox.IsDisplayObject()) {
				return;
			}
			g_gauge.frostClip = ChildClip(a_movie, g_gauge.frostBox, "frost", 1.0);
			if (!g_gauge.frostClip.IsDisplayObject()) {
				return;
			}
			const GFx url = MakeString(a_movie, "NeedsHUD/frost.swf");
			g_gauge.frostClip.Invoke("loadMovie", nullptr, &url, 1);
			g_gauge.frostMade = true;
			g_gauge.frostFrame = -1;
			logger::info("Frost flipbook: loadMovie NeedsHUD/frost.swf");
		}
		const double scale = UNIT_TO_HUD * g_cold.scale * 100.0;
		if (scale != g_gauge.frostScale) {
			g_gauge.frostScale = scale;
			GFx::DisplayInfo di;
			di.SetScale(scale, scale);
			g_gauge.frostBox.SetDisplayInfo(di);
		}
		if (!on) {
			if (g_gauge.frostOn) {
				g_gauge.frostOn = false;
				const GFx last(static_cast<double>(FROST_LAST + 1));
				g_gauge.frostClip.Invoke("gotoAndStop", nullptr, &last, 1);
				g_gauge.frostClip.SetMember("_visible", GFx(false));
				g_gauge.frostBox.SetMember("_visible", GFx(false));
				g_gauge.frostFrame = -1;
				logger::info("Frost flipbook: off ({:.2f} s after the last hit)", sinceHit);
			}
			return;
		}
		if (!g_gauge.frostOn) {
			g_gauge.frostOn = true;
			g_gauge.frostClip.SetMember("_visible", GFx(true));
			g_gauge.frostBox.SetMember("_visible", GFx(true));
			logger::info("Frost flipbook: on");
		}
		const int f = std::clamp(static_cast<int>(b.head), 0, FROST_LAST);
		if (f != g_gauge.frostFrame) {
			GFx::DisplayInfo di;  // every time: loadMovie finishes a frame or two later and may reset the clip
			di.SetPosition(FROST_UX, FROST_UY);
			di.SetScale(100.0 / FROST_PXU, 100.0 / FROST_PXU);
			g_gauge.frostClip.SetDisplayInfo(di);
			const GFx frame(static_cast<double>(f + 1));
			g_gauge.frostClip.Invoke("gotoAndStop", nullptr, &frame, 1);
			g_gauge.frostFrame = f;
		}
	}

	// ---- freezing water (2026-11-08, his picks: "Submerged", stays readable, half under, our frost on the upper half):
	// while Survival has you in freezing water, cold water rises to half the gauge (waterline with waves and slush ice,
	// Skyrim's MagicCausticTile caustics, FXBubble01 bubbles, cold blue), and the upper half freezes with the frost-hit
	// frost climbing up from the waterline (his call). Out: it drains (no drips). water.swf: 0-43 rise (water speeds up, the frost carries on at the same speed), 44-79 loop, 80-126 the rise in reverse (frost slides back down, then the water drains; his call: symmetric).
	constexpr double WATER_UX = -52.55555555555556, WATER_UY = -17.22222222222222, WATER_PXU = 1.35;
	constexpr int    WATER_IN = 44, WATER_LOOP0 = 44, WATER_LOOPN = 36, WATER_LAST = 126;
	BurnPlay g_water;

	void WaterTick(RE::GFxMovieView* a_movie, GFx& a_box, std::int64_t a_now)
	{
		const bool wet = g_wetNow.load();  // set by the gauge's tick from whichever mod runs the cold (Survival or SunHelm)
		auto&      b = g_water;
		AdvancePlay(b, wet, a_now, WATER_IN, WATER_LOOP0, WATER_LOOPN, WATER_LAST, WATER_LOOP0);
		const bool on = b.phase != 0;
		if (!g_gauge.waterMade) {
			if (!on) {
				return;
			}
			g_gauge.waterBox = ChildClip(a_movie, a_box, "waterFx", 4.0);  // under the fire and the frost hit
			if (!g_gauge.waterBox.IsDisplayObject()) {
				return;
			}
			g_gauge.waterClip = ChildClip(a_movie, g_gauge.waterBox, "water", 1.0);
			if (!g_gauge.waterClip.IsDisplayObject()) {
				return;
			}
			const GFx url = MakeString(a_movie, "NeedsHUD/water.swf");
			g_gauge.waterClip.Invoke("loadMovie", nullptr, &url, 1);
			g_gauge.waterMade = true;
			g_gauge.waterFrame = -1;
			logger::info("Water flipbook: loadMovie NeedsHUD/water.swf");
		}
		const double scale = UNIT_TO_HUD * g_cold.scale * 100.0;
		if (scale != g_gauge.waterScale) {
			g_gauge.waterScale = scale;
			GFx::DisplayInfo di;
			di.SetScale(scale, scale);
			g_gauge.waterBox.SetDisplayInfo(di);
		}
		if (!on) {
			if (g_gauge.waterOn) {
				g_gauge.waterOn = false;
				const GFx last(static_cast<double>(WATER_LAST + 1));
				g_gauge.waterClip.Invoke("gotoAndStop", nullptr, &last, 1);
				g_gauge.waterClip.SetMember("_visible", GFx(false));
				g_gauge.waterBox.SetMember("_visible", GFx(false));
				g_gauge.waterFrame = -1;
				logger::info("Water flipbook: off (out of the freezing water)");
			}
			return;
		}
		if (!g_gauge.waterOn) {
			g_gauge.waterOn = true;
			g_gauge.waterClip.SetMember("_visible", GFx(true));
			g_gauge.waterBox.SetMember("_visible", GFx(true));
			logger::info("Water flipbook: on (in freezing water)");
		}
		const int f = std::clamp(static_cast<int>(b.head), 0, WATER_LAST);
		if (f != g_gauge.waterFrame) {
			GFx::DisplayInfo di;  // every time: loadMovie finishes a frame or two later and may reset the clip
			di.SetPosition(WATER_UX, WATER_UY);
			di.SetScale(100.0 / WATER_PXU, 100.0 / WATER_PXU);
			g_gauge.waterClip.SetDisplayInfo(di);
			const GFx frame(static_cast<double>(f + 1));
			g_gauge.waterClip.Invoke("gotoAndStop", nullptr, &frame, 1);
			g_gauge.waterFrame = f;
		}
	}

	// whether the fire, frost or water look is up: written by the UI thread after each tick, read by the ticker thread
	// (plain struct fields can't be shared between threads; 1.0.3 code review)
	std::atomic_bool g_fxOn{ false };
	void PublishFxOn() { g_fxOn = g_gauge.fireOn || g_gauge.frostOn || g_gauge.waterOn; }
	bool ColdFireWanted()
	{
		// keep ticking while the burn is still shown, so it is always switched off (it could be up when a menu paused the ticks)
		const std::int64_t now = NowMs();
		return g_fxOn.load() || g_wetNow.load() || now - g_fireAt.load() < static_cast<std::int64_t>((FIRE_HOLD + FIRE_FADE) * 1000.0) + 300 ||
		       now - g_frostAt.load() < static_cast<std::int64_t>((FROST_HOLD + 0.3) * 1000.0);
	}

	void ColdFireTick()
	{
		if (g_gauge.failed || g_gauge.shown != 1) {
			return;
		}
		auto ui = RE::UI::GetSingleton();
		auto hud = ui ? ui->GetMenu(RE::HUDMenu::MENU_NAME) : nullptr;
		RE::GFxMovieView* movie = hud ? hud->uiMovie.get() : nullptr;
		GFx root, box;
		if (!movie || !movie->GetVariable(&root, "_root") || !root.IsDisplayObject() || !root.GetMember(GAUGE_CLIP, &box) ||
			!box.IsDisplayObject()) {
			return;
		}
		const std::int64_t now = NowMs();
		WaterTick(movie, box, now);
		if constexpr (FROST_FLIPBOOK) {
			FrostTick(movie, box, now);
		}
		if constexpr (FIRE_FLIPBOOK) {
			BurnTick(movie, box, now);
			return;
		}
		const double fire = FxStrength(g_fireAt, g_fireFrom, now, FIRE_HOLD, FIRE_FADE);
		if (!g_gauge.fireMade) {
			if (fire <= 0.0) {
				return;
			}
			// made once: one clip behind everything (under the ornament) and one over the drawing (under the
			// labels), the tint in the front one under its blobs, and every pass's sprite pool in drawing order
			// (glow, body, orange, red, strands)
			g_gauge.fireClip[0] = ChildClip(movie, box, "fxBack", 0.0);
			g_gauge.fireClip[1] = ChildClip(movie, box, "fx", 5.0);
			if (!g_gauge.fireClip[0].IsDisplayObject() || !g_gauge.fireClip[1].IsDisplayObject()) {
				return;
			}
			g_gauge.fireTint = ChildClip(movie, g_gauge.fireClip[1], "tint", 1.0);
			DrawFireTint(movie, g_gauge.fireTint);
			for (int l = 0; l < 2; ++l) {
				for (int p = 0; p < FP_COUNT; ++p) {
					g_gauge.fireSprites[l][p].clear();
					for (int i = 0; i < FIRE_PASSES[p].most; ++i) {
						const std::string name = std::format("p{}_{}", p, i);
						GFx sp = ChildClip(movie, g_gauge.fireClip[l], name.c_str(), 10.0 + p * 200.0 + i);
						DrawFireSprite(movie, sp, p);
						sp.SetDisplayInfo(GFx::DisplayInfo(false));
						g_gauge.fireSprites[l][p].push_back(sp);
					}
					g_gauge.fireUsed[l][p] = 0;
				}
			}
			g_gauge.fireMade = true;
			logger::info("Fire sprites made");
		}
		const double scale = UNIT_TO_HUD * g_cold.scale * 100.0;
		if (scale != g_gauge.fireScale) {
			g_gauge.fireScale = scale;
			GFx::DisplayInfo di;
			di.SetScale(scale, scale);
			g_gauge.fireClip[0].SetDisplayInfo(di);
			g_gauge.fireClip[1].SetDisplayInfo(di);
		}
		if (fire <= 0.0) {
			if (g_gauge.fireOn) {
				g_gauge.fireOn = false;
				g_gauge.fireClip[0].SetDisplayInfo(GFx::DisplayInfo(false));
				g_gauge.fireClip[1].SetDisplayInfo(GFx::DisplayInfo(false));
			}
			return;
		}
		if (!g_gauge.fireOn) {
			g_gauge.fireOn = true;
			g_gauge.fireClip[0].SetDisplayInfo(GFx::DisplayInfo(true));
			g_gauge.fireClip[1].SetDisplayInfo(GFx::DisplayInfo(true));
		}
		{
			GFx::DisplayInfo di;
			di.SetAlpha(std::min(1.0, fire * 1.6) * 100.0);
			g_gauge.fireTint.SetDisplayInfo(di);
		}
		FireBlobs(fire, (now % 100000) / 1000.0, g_fireBlobs);
		for (int l = 0; l < 2; ++l) {
			const double layerAlpha = l == 0 ? 1.0 : FIRE_FRONT;
			int used[FP_COUNT]{};
			for (const auto& b : g_fireBlobs) {
				auto& pool = g_gauge.fireSprites[l][b.pass];
				int& k = used[b.pass];
				if (b.alpha * layerAlpha <= 0.005 || k >= static_cast<int>(pool.size())) {
					continue;
				}
				GFx::DisplayInfo di;
				di.Set(b.x, b.y, b.rot, b.rx * 2.0, b.ry * 2.0, std::min(b.alpha, 1.0) * layerAlpha * 100.0, true);  // the sprite's radius is 50
				pool[k++].SetDisplayInfo(di);
			}
			for (int p = 0; p < FP_COUNT; ++p) {
				for (int k = used[p]; k < g_gauge.fireUsed[l][p]; ++k) {
					g_gauge.fireSprites[l][p][k].SetDisplayInfo(GFx::DisplayInfo(false));
				}
				g_gauge.fireUsed[l][p] = used[p];
			}
		}
	}

