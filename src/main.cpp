// NeedsHUD: needs gauges drawn into the vanilla HUD movie from C++ (no Flash file): vector shapes through the
// ActionScript drawing API and text in the HUD's own font. Prototype 0.3: the Survival Mode cold thermometer in the vanilla bar style and the Needs group (hunger and tiredness tiles), each with its own settings page in SKSE Menu Framework.
// Settings: My Games/Skyrim Special Edition/SKSE/NeedsHUD.ini (1.0.3; read once from the old Data/SKSE/Plugins place). Log: My Games/Skyrim Special Edition/SKSE/NeedsHUD.log.
// Nothing is saved in the game: the gauge reads Survival's own values every tick.

#include <algorithm>
#include <array>
#include <atomic>
#include <chrono>
#include <deque>
#include <filesystem>
#include <cmath>
#include <format>
#include <fstream>
#include <mutex>
#include <unordered_map>
#include <initializer_list>
#include <numbers>
#include <string>
#include <thread>
#include <tuple>
#include <utility>
#include <vector>

#include "Settings.h"

namespace
{
	// ---- where the player's settings live (1.0.3): My Games\Skyrim Special Edition\SKSE\NeedsHUD.ini, next to the log,
	// never in Data (his rule: nothing written at runtime may land in Data / MO2's Overwrite; 1.0.3 code review).
	// UserIniPath() is in Paths.cpp (it needs windows.h, kept out of this file). The old Data\SKSE\Plugins\NeedsHUD.ini is
	// still read when the new file doesn't exist yet, so settings carry over.
	inline const std::filesystem::path LEGACY_INI = "Data/SKSE/Plugins/NeedsHUD.ini";
	// the file to read: the player's own, or (first run after updating) the old one in Data
	std::filesystem::path IniToRead()
	{
		std::error_code ec;
		const auto user = NeedsHUD::UserIniPath();
		if (std::filesystem::exists(user, ec) || !std::filesystem::exists(LEGACY_INI, ec)) {
			return user;
		}
		return LEGACY_INI;
	}

	#include "Translate.inl"
	#include "Draw.inl"
	#include "Snapshot.inl"
	#include "ColdGauge.inl"
	#include "HungerTile.inl"
	#include "TiredTile.inl"
	#include "ThirstTile.inl"
	#include "DiseaseTile.inl"
	#include "ConfigLoader.inl"
	#include "FrostbiteTile.inl"
	#include "VitalityTile.inl"
	#include "DirtTile.inl"
	#include "DrunkTile.inl"
	#include "BodyTile.inl"
	#include "OtherNeedsTiles.inl"
	#include "NeedsGroup.inl"
	#include "FoodSlots.inl"

	// ---- settings: a plain key=value reader (sections only group the keys) ----
	void LoadSettings()
	{
		const auto path = IniToRead();
		std::ifstream file(path);
		if (!file) {
			logger::info("NeedsHUD.ini not found: using the defaults");
			return;
		}
		if (path == LEGACY_INI) {
			logger::info("Settings read from the old place (Data\\SKSE\\Plugins\\NeedsHUD.ini); they are saved to My Games from now on");
		}
		std::string line, section;
		while (std::getline(file, line)) {
			const auto trim = [](std::string s) {
				const auto a = s.find_first_not_of(" \t\r"), b = s.find_last_not_of(" \t\r");
				return a == std::string::npos ? std::string() : s.substr(a, b - a + 1);
			};
			line = trim(line);
			if (line.empty() || line[0] == ';' || line[0] == '#') {
				continue;
			}
			if (line.front() == '[') {
				section = line;
				continue;
			}
			const auto eq = line.find('=');
			if (eq == std::string::npos || (section != "[Cold]" && section != "[Needs]" && section != "[Hunger]" && section != "[Food]")) {
				continue;
			}
			const std::string key = trim(line.substr(0, eq));
			if (key == "sHiddenNeeds" && section == "[Needs]") {
				g_needsOpt.hiddenNeeds = trim(line.substr(eq + 1));
				continue;
			}
			double value = 0.0;
			try {
				value = std::stod(trim(line.substr(eq + 1)));
			} catch (...) {
				continue;
			}
			// both sections hold the same keys
			const auto take = [&](auto& o) {
				if (key == "bEnabled") {
					o.enabled = value != 0.0;
				} else if (key == "iAnchor") {
					o.anchor = std::clamp(static_cast<int>(value), 0, 3);
				} else if (key == "fPosX") {
					o.posX = std::clamp(value, 0.0, 100.0);
				} else if (key == "fPosY") {
					o.posY = std::clamp(value, 0.0, 100.0);
				} else if (key == "fScale") {
					o.scale = std::clamp(value, NeedsHUD::SCALE_MIN, NeedsHUD::SCALE_MAX);  // below 0.5 the blocks blur; above 2 it fills the screen
				} else if (key == "bShowNumbers") {
					if constexpr (requires { o.numbers; }) {  // the food slots have no value line
						o.numbers = value != 0.0;
					}
				} else if (key == "bShowWarmth") {
					if constexpr (requires { o.warmth; }) {
						o.warmth = value != 0.0;
					}
				} else if (key == "bShowWarmthOnChange") {
					if constexpr (requires { o.warmthOnChange; }) {
						o.warmthOnChange = value != 0.0;
					}
				} else if (key == "bShowWetness") {
					if constexpr (requires { o.wetness; }) {
						o.wetness = value != 0.0;
					}
				} else if (key == "bHideInCombat") {
					o.hideInCombat = value != 0.0;
				} else if (key == "bOnlyOnChange") {
					o.onlyOnChange = value != 0.0;
				} else if (key == "bOnlyWorse") {
					o.onlyWorse = value != 0.0;
				} else if (key == "fShowSeconds") {
					o.showSeconds = std::clamp(value, NeedsHUD::SHOW_MIN, NeedsHUD::SHOW_MAX);
				} else if (key == "bKeepWorst") {
					o.keepWorst = value != 0.0;
				}
				if constexpr (requires { o.showHunger; }) {
					if (key == "bShowHunger") {
						o.showHunger = value != 0.0;
					} else if (key == "bShowThirst") {
						o.showThirst = value != 0.0;
					} else if (key == "bShowTiredness") {
						o.showTiredness = value != 0.0;
					} else if (key == "bShowDisease") {
						o.showDisease = value != 0.0;
					} else if (key == "bShowFrostbite") {
						o.showFrostbite = value != 0.0;
					} else if (key == "bShowVitality") {
						o.showVitality = value != 0.0;
					} else if (key == "bShowDirt") {
						o.showDirt = value != 0.0;
					} else if (key == "bShowDrunk") {
						o.showDrunk = value != 0.0;
					} else if (key == "bShowBody") {
						o.showBody = value != 0.0;
					} else if (key == "bShowBladder") {
						o.showBladder = value != 0.0;
					} else if (key == "bShowFun") {
						o.showFun = value != 0.0;
					} else if (key == "bShowSocial") {
						o.showSocial = value != 0.0;
					} else if (key == "bVertical") {
						o.vertical = value != 0.0;
					} else if (key == "iPerLine") {
						o.perLine = std::clamp(static_cast<int>(value), 0, NeedsHUD::PER_LINE_MAX);
					} else if (key == "fSpacing") {
						o.spacing = std::clamp(value, NeedsHUD::SPACING_MIN, NeedsHUD::SPACING_MAX);
					}
				}
			};
			if (section == "[Cold]") {
				take(g_cold);
			} else if (section == "[Food]") {
				take(g_foodOpt);
				if (key == "bHideEmpty") {
					g_foodOpt.hideEmpty = value != 0.0;
				}
				if (key == "bVertical") {
					g_foodOpt.vertical = value != 0.0;
				}
			} else {
				take(g_needsOpt);  // [Needs] ([Hunger] from the first test build reads the same)
			}
		}
		logger::info("Cold gauge: {}, {} corner, {}% / {}%, size {}, numbers {}", g_cold.enabled ? "on" : "off",
			NeedsHUD::ANCHOR_NAMES[g_cold.anchor], g_cold.posX, g_cold.posY,
			g_cold.scale, g_cold.numbers ? "on" : "off");
		logger::info("Needs group: {}, {} corner, {}% / {}%, size {}, numbers {}, hunger {}, tiredness {}", g_needsOpt.enabled ? "on" : "off",
			NeedsHUD::ANCHOR_NAMES[g_needsOpt.anchor], g_needsOpt.posX, g_needsOpt.posY, g_needsOpt.scale, g_needsOpt.numbers ? "on" : "off",
			g_needsOpt.showHunger ? "on" : "off", g_needsOpt.showTiredness ? "on" : "off");
		logger::info("Food slots: {}, {} corner, {}% / {}%, size {}, hide when empty {}", g_foodOpt.enabled ? "on" : "off",
			NeedsHUD::ANCHOR_NAMES[g_foodOpt.anchor], g_foodOpt.posX, g_foodOpt.posY, g_foodOpt.scale, g_foodOpt.hideEmpty ? "on" : "off");
	}

	// ---- settings: written back from the in-game page (the whole file, with its comments, both sections) ----
	template <class T>
	void WriteSection(std::ofstream& a_file, const char* a_section, const char* a_what, const char* a_numbers, const T& a_o)
	{
		a_file << "[" << a_section << "]\n"
			   << "; Show the " << a_what << " (it shows while a needs mod it reads is running). 1 = on, 0 = off.\n"
			   << "bEnabled=" << (a_o.enabled ? 1 : 0) << "\n"
			   << "; The screen corner it is placed from: 0 = top left, 1 = top right, 2 = bottom left, 3 = bottom right.\n"
			   << "iAnchor=" << a_o.anchor << "\n"
			   << "; How far from that corner, as a percent of the screen's width (X) and height (Y), to its nearest edge.\n"
				  "; It always stays fully on screen.\n"
			   << std::format("fPosX={:.1f}\nfPosY={:.1f}\n", a_o.posX, a_o.posY)
			   << "; Size, from 0.5 to 2.0. 1.0 = the designed size.\n"
			   << std::format("fScale={:.2f}\n", a_o.scale)
			   << "; Test aid: a line under it with Survival's raw " << a_numbers << " value. 1 = on, 0 = off.\n"
			   << "bShowNumbers=" << (a_o.numbers ? 1 : 0) << "\n"
			   ;
		if constexpr (requires { a_o.warmth; }) {
			a_file << "; A line under the stage name with your total warmth (all the armor and clothing you wear). 1 = on, 0 = off.\n"
				   << "bShowWarmth=" << (a_o.warmth ? 1 : 0) << "\n"
				   << "; With bShowWarmth: the warmth line only shows for a while (fShowSeconds) after the number changes. 1 = on, 0 = always.\n"
				   << "bShowWarmthOnChange=" << (a_o.warmthOnChange ? 1 : 0) << "\n";
		}
		if constexpr (requires { a_o.wetness; }) {
			a_file << "; With Frostfall: how wet you are, a small bar beside the tube and a line under the stage name. 1 = on, 0 = off.\n"
				   << "bShowWetness=" << (a_o.wetness ? 1 : 0) << "\n";
		}
		a_file << "; Hide it while you are in combat. 1 = on, 0 = off.\n"
			   << "bHideInCombat=" << (a_o.hideInCombat ? 1 : 0) << "\n"
			   << "; Only show it for a while after its stage changes. 1 = on, 0 = always shown.\n"
			   << "bOnlyOnChange=" << (a_o.onlyOnChange ? 1 : 0) << "\n"
			   << "; With bOnlyOnChange: only a change for the worse brings it up (colder, hungrier, more tired). 1 = on, 0 = any change.\n"
			   << "bOnlyWorse=" << (a_o.onlyWorse ? 1 : 0) << "\n"
			   << "; How long it stays after a change, in seconds (2 to 30).\n"
			   << std::format("fShowSeconds={:.0f}\n", a_o.showSeconds)
			   << "; With bOnlyOnChange: keep it shown at the worst stage. 1 = on, 0 = off.\n"
			   << "bKeepWorst=" << (a_o.keepWorst ? 1 : 0) << "\n";
		if constexpr (requires { a_o.showHunger; }) {
			a_file << "; Each tile of the row. 1 = shown, 0 = hidden (the row closes up).\n"
				   << "bShowHunger=" << (a_o.showHunger ? 1 : 0) << "\n"
				   << "; Thirst shows only when a needs mod has it (SunHelm; vanilla Survival Mode has none).\n"
				   << "bShowThirst=" << (a_o.showThirst ? 1 : 0) << "\n"
				   << "bShowTiredness=" << (a_o.showTiredness ? 1 : 0) << "\n"
				   << "; The disease tile shows only while you have a disease.\n"
				   << "bShowDisease=" << (a_o.showDisease ? 1 : 0) << "\n"
				   << "; The frostbite tile (Frostfall) shows only while you have frostbite: one block per frostbitten body part.\n"
				   << "bShowFrostbite=" << (a_o.showFrostbite ? 1 : 0) << "\n"
				   << "; Last Seed's Vitality (shows only while Last Seed runs): where it is now, and the hollow marker where it is heading.\n"
				   << "bShowVitality=" << (a_o.showVitality ? 1 : 0) << "\n"
				   << "; The dirt tile (Keep It Clean; shows once you have washed for the first time): how long since your last wash.\n"
				   << "bShowDirt=" << (a_o.showDirt ? 1 : 0) << "\n"
				   << "; The drunk tile (Realistic Needs and Diseases, Last Seed, SunHelm, iNeed): how drunk you are; shows only while you have had a drink.\n"
				   << "bShowDrunk=" << (a_o.showDrunk ? 1 : 0) << "\n"
				   << "; The body tile (Realistic Needs and Diseases, with its weight feature on): your body weight band, from Muscular to Skinny.\n"
				   << "bShowBody=" << (a_o.showBody ? 1 : 0) << "\n"
				   << "; Bladder, fun and social: only shown when a needs mod feeds them through a config file.\n"
				   << "bShowBladder=" << (a_o.showBladder ? 1 : 0) << "\n"
				   << "bShowFun=" << (a_o.showFun ? 1 : 0) << "\n"
				   << "bShowSocial=" << (a_o.showSocial ? 1 : 0) << "\n"
				   << "; Custom needs from other mods' config files that you switched off (their ids, comma-separated).\n"
				   << "sHiddenNeeds=" << a_o.hiddenNeeds << "\n"
				   << "; 0 = a row (left to right), 1 = a column (top to bottom).\n"
				   << "bVertical=" << (a_o.vertical ? 1 : 0) << "\n"
				   << "; The gap between tiles, from 0 to 250, between their widest parts (frame or longest stage name). Default 20.\n"
				   << std::format("fSpacing={:.0f}\n", a_o.spacing)
				   << "; Line break: how many tiles in a row (or column) before the next one starts, 1 to 10. 0 = all in one line.\n"
				   << "iPerLine=" << a_o.perLine << "\n";
		}
		logger::info("Settings saved: {} {}, {} corner, {:.1f}% / {:.1f}%, size {:.2f}, numbers {}", a_what, a_o.enabled ? "on" : "off",
			NeedsHUD::ANCHOR_NAMES[std::clamp(a_o.anchor, 0, 3)], a_o.posX, a_o.posY, a_o.scale, a_o.numbers ? "on" : "off");
	}

	// the food slots' section (their own keys: no numbers, no worst stage)
	void WriteFood(std::ofstream& a_file, const NeedsHUD::FoodOptions& a_o)
	{
		a_file << "[Food]\n"
			   << "; Show the food slots (Cooking Skill Overhaul's drink, pot meal and baked dish). 1 = on, 0 = off.\n"
			   << "bEnabled=" << (a_o.enabled ? 1 : 0) << "\n"
			   << "; The screen corner they are placed from: 0 = top left, 1 = top right, 2 = bottom left, 3 = bottom right.\n"
			   << "iAnchor=" << a_o.anchor << "\n"
			   << "; How far from that corner, as a percent of the screen's width (X) and height (Y), to their nearest edge.\n"
			   << std::format("fPosX={:.1f}\nfPosY={:.1f}\n", a_o.posX, a_o.posY)
			   << "; Size, from 0.5 to 2.0. 1.0 = the designed size.\n"
			   << std::format("fScale={:.2f}\n", a_o.scale)
			   << "; Hide the slots while none of them holds anything. 1 = on, 0 = always shown.\n"
			   << "bHideEmpty=" << (a_o.hideEmpty ? 1 : 0) << "\n"
			   << "; 0 = a row (drink, pot meal, baked side by side), 1 = a column (one under the other).\n"
			   << "bVertical=" << (a_o.vertical ? 1 : 0) << "\n"
			   << "; Hide them while you are in combat. 1 = on, 0 = off.\n"
			   << "bHideInCombat=" << (a_o.hideInCombat ? 1 : 0) << "\n"
			   << "; Only show them for a while after a slot fills or empties. 1 = on, 0 = always shown.\n"
			   << "bOnlyOnChange=" << (a_o.onlyOnChange ? 1 : 0) << "\n"
			   << "; With bOnlyOnChange: only a slot running out brings them up. 1 = on, 0 = any change.\n"
			   << "bOnlyWorse=" << (a_o.onlyWorse ? 1 : 0) << "\n"
			   << "; How long they stay after a change, in seconds (2 to 30).\n"
			   << std::format("fShowSeconds={:.0f}\n", a_o.showSeconds);
	}

	void SaveSettings(const NeedsHUD::ColdOptions& a_cold, const NeedsHUD::NeedsOptions& a_needs)
	{
		std::ofstream file(NeedsHUD::UserIniPath(), std::ios::trunc);
		if (!file) {
			logger::warn("Could not write {}: the settings last only until the game closes", NeedsHUD::UserIniPath().string());
			return;
		}
		file << "; NeedsHUD settings. The in-game settings page (SKSE Menu Framework) writes this file;\n"
				"; you can also edit it by hand while the game is closed.\n\n";
		WriteSection(file, "Cold", "cold thermometer", "cold", a_cold);
		file << "\n";
		WriteSection(file, "Needs", "needs group", "need", a_needs);
		file << "\n";
		WriteFood(file, NeedsHUD::GetFoodOptions());
		// a translator's test language set by hand stays (Translate.inl reads it)
		file << "\n[General]\n; For translators: sLanguage=GERMAN shows Interface/Translations/NeedsHUD_GERMAN.txt without changing the\n"
				"; game's language. Leave it empty to follow the game.\n"
			 << "sLanguage=" << (tr::g_override.empty() ? "" : tr::g_override) << "\n";
	}

	// ---- the ticks: the gauge 10 times a second, the fire 30 while it burns; the drawing runs on the UI thread ----
	std::atomic_bool g_ticking{ false };
	std::atomic_bool g_inGame{ false };
	std::atomic_bool g_loadPending{ false };  // a save load has started and not finished yet (kPreLoadGame .. kPostLoadGame)
	std::atomic_int  g_conflictWait{ -1 };  // ticks (10 a second) until the needs-mod clash check, -1 = done for this load

	// Two survival mods fighting over the same need, checked once per load (Frostfall's double cold added 2026-11-14).
	// SunHelm and vanilla Survival Mode both running (his ask 2026-11-09): their penalties stack (carry weight can drop to
	// 0, seen in his game); in his game Survival's switch is missing from Settings > Gameplay (cause not proven, 2026-11-09),
	// so the message gives the console fix too. Once per load
	// Checked ~8 s after loading (Survival's own start prompt and SunHelm's start come first), on the UI thread
	void CheckNeedsModClash()
	{
		if (g_conflictWait < 0 || --g_conflictWait > 0) {
			return;
		}
		g_conflictWait = -1;
		auto ui = RE::UI::GetSingleton();
		if (ui && ui->GameIsPaused()) {
			g_conflictWait = 10;  // a menu is up: look again in a second
			return;
		}
		if (SunHelmOn() && g_survivalOn && GV(g_survivalOn) >= 1.0f) {
			logger::info("Warning shown: SunHelm and Survival Mode are both running");
			{
				const std::string text = tr::T(
				"NeedsHUD: SunHelm and Survival Mode are both running. Their needs and penalties stack, and your carry weight "
				"can drop to 0. Turn Survival Mode off in Settings, Gameplay, or if it isn't listed there, from the console: "
				"type set Survival_ModeToggle to 0, close the console and wait a few seconds.");
				SKSE::GetTaskInterface()->AddTask([text]() { RE::DebugMessageBox(text.c_str()); });  // main thread (1.0.3)
			}
		}
		// Frostfall beside another cold system (his ask 2026-11-14): Frostfall's exposure and Survival Mode's cold (which
		// Survival Mode Improved and Starfrost run too) or SunHelm's cold all make the player colder at the same time
		const bool survivalCold = g_survivalOn && GV(g_survivalOn) >= 1.0f;
		const bool sunhelmCold = SunHelmOn() && GlobalOn(g_shCold, g_shColdOff) && !(g_shColdForceOff && GV(g_shColdForceOff) >= 1.0f);
		if (FrostfallOn() && (survivalCold || sunhelmCold)) {
			logger::info("Warning shown: Frostfall and {} both run a cold system", sunhelmCold ? "SunHelm" : "Survival Mode");
			const std::string text = tr::T(sunhelmCold ?
				"NeedsHUD: Frostfall and SunHelm both run a cold system, so you get colder from both at once. Turn the cold off in "
				"SunHelm's menu, or turn exposure off in Frostfall's menu." :
				"NeedsHUD: Frostfall and Survival Mode both run a cold system, so you get colder from both at once. Survival Mode "
				"Improved and Starfrost use Survival Mode's cold too. Turn exposure off in Frostfall's menu, or play without Frostfall.");
			SKSE::GetTaskInterface()->AddTask([text]() { RE::DebugMessageBox(text.c_str()); });  // main thread (1.0.3)
		}
	}

	// a main or loading menu is up (UI thread; menu state belongs to the UI). Whether the player's 3D is loaded comes from
	// the main-thread snapshot (g_view.loaded3D), never from the live player (1.0.3 second review)
	bool MenuBlocksTicks()
	{
		auto ui = RE::UI::GetSingleton();
		return !ui || ui->IsMenuOpen(RE::MainMenu::MENU_NAME) || ui->IsMenuOpen(RE::LoadingMenu::MENU_NAME);
	}

	void StartTicking()
	{
		if (g_ticking.exchange(true)) {
			return;
		}
		std::thread([]() {
			// every 33 ms: the gauge on every third (10 a second), the fire on each while it burns (30 a second)
			for (std::uint64_t n = 0;; ++n) {
				std::this_thread::sleep_for(std::chrono::milliseconds(33));
				if (n % 3 == 0) {
					// first the main thread copies the player's state (Snapshot.inl), then the UI thread draws from that
					// copy: the HUD never reads the live game from the UI thread (1.0.3)
					// (also while no game runs: the snapshot tells the coc fallback below when the player's 3D is there)
					if (!g_loadPending) {
						SKSE::GetTaskInterface()->AddTask([]() {
							if (!g_loadPending) {
								CaptureSnapshot();
							}
						});
					}
					SKSE::GetTaskInterface()->AddUITask([]() {
						if (g_loadPending.load()) {
							return;  // a save load is under way: wait for its "load finished" message
						}
						// quit to the main menu: no message comes, so the next game (a load, or coc from the menu) must not
						// see this one's state (1.0.3 second review)
						auto ui = RE::UI::GetSingleton();
						if (g_inGame && ui && ui->IsMenuOpen(RE::MainMenu::MENU_NAME)) {
							g_inGame = false;
							++g_loadCount;
							ClearSnapshot();
							logger::info("Main menu: the gauges stop until a game runs again");
							return;
						}
						if (MenuBlocksTicks()) {
							return;  // a loading screen or the main menu: nothing to read
						}
						TakeSnapshotView();
						if (!g_view.ok || !g_view.loaded3D) {
							return;  // no main-thread copy yet since the load, or the player isn't in the world
						}
						// a game entered without a load message (coc from the main menu sends neither "new game" nor
						// "game loaded"; found 2026-11-08: the gauges never appeared until a save was loaded) starts here.
						// Never while a save load is under way (checked above; 1.0.3 fix: this fallback once switched the
						// gauges back on in the middle of a load)
						if (!g_inGame) {
							g_inGame = true;
							g_bridgeCheck = true;
							g_modBridgeCheck = true;
							g_conflictWait = 80;
							logger::info("In game without a load message (coc from the main menu?): the gauges start now");
						}
						BridgesCheck();
						ColdGaugeTick();
						PublishFxOn();
						NeedsGaugeTick();
						FoodGroupTick();
						CheckNeedsModClash();
					});
				}
				if (g_inGame && ColdFireWanted()) {
					SKSE::GetTaskInterface()->AddUITask([]() {
						ColdFireTick();
						PublishFxOn();
					});
				}
			}
		}).detach();
	}
}

// ---- the shared options (Settings.h): the page hands a new set over, the gauge takes it on its next tick ----
NeedsHUD::ColdOptions NeedsHUD::GetColdOptions()
{
	std::scoped_lock lock(g_coldLock);
	return g_coldPending;  // always the newest wanted options (1.0.3 second review: no window where the page reads old ones)
}

void NeedsHUD::SetColdOptions(const ColdOptions& a_options, bool a_save)
{
	ColdOptions o = a_options;
	o.anchor = std::clamp(o.anchor, 0, 3);
	o.posX = std::clamp(o.posX, 0.0, 100.0);
	o.posY = std::clamp(o.posY, 0.0, 100.0);
	o.scale = std::clamp(o.scale, SCALE_MIN, SCALE_MAX);
	o.showSeconds = std::clamp(o.showSeconds, SHOW_MIN, SHOW_MAX);
	{
		std::scoped_lock lock(g_coldLock);
		g_coldPending = o;
	}
	g_coldChanged = true;
	if (a_save) {
		SaveSettings(o, NeedsHUD::GetNeedsOptions());
	}
}

NeedsHUD::NeedsOptions NeedsHUD::GetNeedsOptions()
{
	std::scoped_lock lock(g_needsLock);
	return g_needsPending;  // always the newest wanted options
}

void NeedsHUD::SetNeedsOptions(const NeedsOptions& a_options, bool a_save)
{
	NeedsOptions o = a_options;
	o.anchor = std::clamp(o.anchor, 0, 3);
	o.posX = std::clamp(o.posX, 0.0, 100.0);
	o.posY = std::clamp(o.posY, 0.0, 100.0);
	o.scale = std::clamp(o.scale, SCALE_MIN, SCALE_MAX);
	o.spacing = std::clamp(o.spacing, SPACING_MIN, SPACING_MAX);
	o.perLine = std::clamp(o.perLine, 0, PER_LINE_MAX);
	o.showSeconds = std::clamp(o.showSeconds, SHOW_MIN, SHOW_MAX);
	{
		std::scoped_lock lock(g_needsLock);
		g_needsPending = o;
	}
	g_needsChanged = true;
	if (a_save) {
		SaveSettings(NeedsHUD::GetColdOptions(), o);
	}
}

NeedsHUD::FoodOptions NeedsHUD::GetFoodOptions()
{
	std::scoped_lock lock(g_foodLock);
	return g_foodPending;  // always the newest wanted options
}

void NeedsHUD::SetFoodOptions(const FoodOptions& a_options, bool a_save)
{
	FoodOptions o = a_options;
	o.anchor = std::clamp(o.anchor, 0, 3);
	o.posX = std::clamp(o.posX, 0.0, 100.0);
	o.posY = std::clamp(o.posY, 0.0, 100.0);
	o.scale = std::clamp(o.scale, SCALE_MIN, SCALE_MAX);
	o.showSeconds = std::clamp(o.showSeconds, SHOW_MIN, SHOW_MAX);
	{
		std::scoped_lock lock(g_foodLock);
		g_foodPending = o;
	}
	g_foodChanged = true;
	if (a_save) {
		SaveSettings(NeedsHUD::GetColdOptions(), NeedsHUD::GetNeedsOptions());  // writes the food section from the pending set
	}
}

bool NeedsHUD::FoodAvailable()
{
	return g_foodProvider;
}

SKSE_EXPORT constinit auto SKSEPlugin_Version = []() {
	SKSE::PluginVersionData v;
	v.PluginVersion({ Version::MAJOR, Version::MINOR, Version::PATCH, 0 });
	v.PluginName("NeedsHUD");
	v.AuthorName("resharr");
	v.UsesAddressLibrary();
	v.UsesUpdatedStructs();
	return v;
}();

SKSE_PLUGIN_LOAD(const SKSE::LoadInterface* a_skse)
{
	SKSE::Init(a_skse);
	logger::info("NeedsHUD v{}", Version::NAME);
	LoadSettings();
	// the page reads the pending sets (Get*Options): they start as the loaded settings
	g_coldPending = g_cold;
	g_needsPending = g_needsOpt;
	g_foodPending = g_foodOpt;
	SKSE::GetMessagingInterface()->RegisterListener([](SKSE::MessagingInterface::Message* a_msg) {
		switch (a_msg->type) {
		case SKSE::MessagingInterface::kDataLoaded:
			g_mainThreadId = std::this_thread::get_id();  // SKSE messages come on the main thread (GV, Snapshot.inl)
			tr::Load();  // the translation for the game's language, if a translator made one
			// the settings pages: after the translation, so their names are translated (1.0.4 review). Every SKSE plugin is
			// loaded by now, SKSE Menu Framework included (our DLL can load before it, "N" before "S", and its header
			// remembers a missing function for good if asked too early)
			NeedsHUD::RegisterMenu();
			FindColdForms();
			LoadNeedsConfigs(RE::TESDataHandler::GetSingleton());  // other authors' config files (after the built-in tables)
			FindAvailableTiles(RE::TESDataHandler::GetSingleton());  // which tiles the settings page lists (NeedsGroup.inl)
			FindFoodEffects(RE::TESDataHandler::GetSingleton());  // Cooking Skill Overhaul's slot effects (FoodSlots.inl)
			NeedsHUD::RegisterFoodPage();
			WatchHits();
			StartTicking();  // ticks wait until a game is running (load message, or the snapshot's loaded3D for coc)
			break;
		case SKSE::MessagingInterface::kPreLoadGame:
			// order matters (1.0.3 second review): first mark the load and drop the old game's copy, then stop the
			// gauges, so a UI tick running in between can't restart them or draw the old copy
			g_loadPending = true;
			ClearSnapshot();
			++g_loadCount;
			g_inGame = false;
			break;
		case SKSE::MessagingInterface::kNewGame:
			++g_loadCount;  // a new game drops what the old one carried over, like a load (1.0.3 code review)
			ClearSnapshot();
			[[fallthrough]];
		case SKSE::MessagingInterface::kPostLoadGame:
			// main thread, before the save's scripts run again: the bridge script must not call a mod's bridge with the
			// installed-mods value the save kept (that mod may be gone now; 1.0.3 second review)
			if (g_bridgeMods) {
				g_bridgeMods->value = static_cast<float>((g_shMainQuest ? 1 : 0) + (g_inQuest ? 2 : 0));
			}
			g_loadPending = false;
			g_bridgeCheck = true;
			g_modBridgeCheck = true;
			g_inGame = true;
			g_conflictWait = 80;
			StartTicking();
			break;
		default:
			break;
		}
	});
	return true;
}

bool NeedsHUD::TileAvailable(int a_kind)
{
	if (!g_tileAvailKnown) {
		return true;
	}
	return a_kind >= 0 && a_kind < CFG_CUSTOM0 && g_tileAvail[a_kind].load();
}

bool NeedsHUD::WetnessAvailable()
{
	return !g_tileAvailKnown || g_wetnessAvail.load();
}

std::vector<std::pair<std::string, std::string>> NeedsHUD::CustomNeeds()
{
	return g_cfgCustom;  // filled at kDataLoaded, never changed after
}

const char* NeedsHUD::Tr(const char* a_en)
{
	return tr::T(a_en);
}

std::string NeedsHUD::TrF(const char* a_pattern, const std::string& a_arg)
{
	return tr::F(a_pattern, a_arg);
}
