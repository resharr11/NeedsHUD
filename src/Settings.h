#pragma once

#include <cctype>
#include <filesystem>
#include <string>
#include <utility>
#include <vector>

// The player's options, shared by the HUD code (main.cpp) and the in-game settings page (Menu.cpp).
// The settings page runs in SKSE Menu Framework's render callback, the gauge in an SKSE UI task: the page never
// touches the gauge's own copy, it hands a new set over (SetColdOptions) and the gauge picks it up on its next tick.

namespace NeedsHUD
{
	// the player's settings file: My Games\Skyrim Special Edition\SKSE\NeedsHUD.ini (Paths.cpp; 1.0.3)
	std::filesystem::path UserIniPath();

	// defaults: his chosen placement (2026-11-05): bottom right corner, 0.6% / 12.4%, size 1.0
	struct ColdOptions
	{
		bool   enabled = true;
		int    anchor = 3;    // the screen corner it is placed from: 0 top left, 1 top right, 2 bottom left, 3 bottom right
		double posX = 0.6;    // percent of the screen width, from that corner's side to the gauge's near edge
		double posY = 12.4;   // percent of the screen height, from that corner's top or bottom to the gauge's near edge
		double scale = 1.0;   // 0.5 .. 2.0 (1.0 = the designed size)
		bool   warmth = true;  // a "Warmth N" line under the stage name: the total warmth of what you wear (2026-11-08, his: just the word)
		bool   warmthOnChange = false;  // the warmth line only shows for a while (showSeconds) after the number changes (2026-11-10, his ask)
		bool   wetness = true;  // Frostfall's wetness bar and line (2026-11-09, his yes to a checkbox)
		bool   numbers = false;  // test aid, off by default for players (his call 2026-11-09)
		// when to show (2026-11-08, his options): hidden in combat; or only for a while after its stage changes
		// (the worst stage can stay up as a warning)
		bool   hideInCombat = false;
		bool   onlyOnChange = false;
		bool   onlyWorse = false;      // with onlyOnChange: only a change for the worse brings it up (2026-11-10, his ask)
		double showSeconds = 8.0;   // how long it stays after a change
		bool   keepWorst = true;    // with onlyOnChange: stays up at the worst stage (Numb, Starving, Debilitated, freezing water)
	};

	// the Needs group (hunger, tiredness; thirst later): the row of need tiles, placed together with the same options
	// as the thermometer, and each tile switchable. Defaults (2026-11-08): bottom right, the row ending just left of
	// the thermometer at its default place, its top level with the tube's tip (where he approved the hunger tile)
	struct NeedsOptions
	{
		bool   enabled = true;
		int    anchor = 3;
		double posX = 7.3;
		double posY = 24.9;  // the smaller tile (0.65) keeps its top level with the thermometer's tip
		double scale = 1.0;
		bool   numbers = false;  // off by default for players (his call 2026-11-09); a line under each stage name with Survival's raw value
		bool   showHunger = true;
		bool   showThirst = true;     // only when a needs mod has thirst (SunHelm; vanilla Survival has none)
		bool   showTiredness = true;
		bool   showDisease = true;    // only while the player has a disease
		bool   showFrostbite = true;  // only while Frostfall's frostbite runs (one block per body part)
		bool   showVitality = true;   // Last Seed's Vitality (only with Last Seed running)
		bool   showDirt = true;       // Keep It Clean's dirt (2026-11-09)
		bool   showDrunk = true;      // drunkenness (RND, Last Seed), only while there is drink in you (2026-11-09)
		bool   showBody = true;       // RND's body weight, only while its weight feature is on (2026-11-09)
		bool   showBladder = true;    // bladder, fun, social: fed only by other authors' config files (2026-11-09)
		bool   showFun = true;
		bool   showSocial = true;
		std::string hiddenNeeds;      // custom needs (config files) the player switched off: their ids, comma-separated
		bool   vertical = false;   // false: a row (left to right), true: a column (top to bottom)
		// when to show (2026-11-08, his options): hidden in combat; or only for a while after its stage changes
		// (the worst stage can stay up as a warning)
		bool   hideInCombat = false;
		bool   onlyOnChange = false;
		bool   onlyWorse = false;      // with onlyOnChange: only a change for the worse brings it up (2026-11-10, his ask)
		double showSeconds = 8.0;   // how long it stays after a change
		bool   keepWorst = true;    // with onlyOnChange: stays up at the worst stage (Numb, Starving, Debilitated, freezing water)
		double spacing = 20.0;     // the gap between tiles (between their widest parts: frame or stage name), design pixels
		int    perLine = 0;        // line break (his ask 2026-11-09): tiles per row (or column) before a new one starts; 0 = one line
	};
	// the food slots (2026-11-12): Free Cooking's drink, pot meal and baked slots (+ the secret homecooked and feeding
	// slots). Default place (his pick 2026-11-12, "make that position the default"): top right, 2% / 2%
	struct FoodOptions
	{
		bool   enabled = true;
		int    anchor = 1;
		double posX = 2.0;
		double posY = 2.0;
		double scale = 1.0;
		bool   hideEmpty = false;     // hide the whole group while no slot holds anything (the design: always shown)
		bool   vertical = false;      // false: a row (drink, pot, baked side by side), true: a column (his ask 2026-11-12)
		bool   hideInCombat = false;
		bool   onlyOnChange = false;  // only show for a while after a slot fills or empties
		bool   onlyWorse = false;     // with onlyOnChange: only a slot running out brings it up
		double showSeconds = 8.0;
		bool   keepWorst = false;     // not used by the food slots (AutoShowAlpha reads it)
	};

	constexpr double SPACING_MIN = 0.0, SPACING_MAX = 250.0;
	constexpr int    PER_LINE_MAX = 10;

	// limits shared by the ini reader and the settings page
	constexpr const char* ANCHOR_NAMES[4]{ "Top left", "Top right", "Bottom left", "Bottom right" };
	constexpr double SCALE_MIN = 0.5, SCALE_MAX = 2.0;
	constexpr double SHOW_MIN = 2.0, SHOW_MAX = 30.0;  // seconds

	ColdOptions GetColdOptions();                                // the options as they stand
	void        SetColdOptions(const ColdOptions& a_options, bool a_save);  // applied on the gauge's next tick; a_save writes NeedsHUD.ini

	NeedsOptions GetNeedsOptions();
	void         SetNeedsOptions(const NeedsOptions& a_options, bool a_save);

	FoodOptions GetFoodOptions();
	void        SetFoodOptions(const FoodOptions& a_options, bool a_save);
	bool        FoodAvailable();  // Free Cooking is loaded (the food slots' provider); fixed at kDataLoaded

	bool TileAvailable(int a_kind);
	bool WetnessAvailable();  // Frostfall or a config file can feed the thermometer's wetness bar; fixed at kDataLoaded  // a mod in this load order can feed this tile (0 hunger .. 11 social); fixed at kDataLoaded

	void RegisterMenu();
	void RegisterFoodPage();  // after kDataLoaded: the Food slots page, only with Free Cooking

	// the custom needs other authors' config files add (id, name in the menu); fixed once the game's data has loaded
	std::vector<std::pair<std::string, std::string>> CustomNeeds();

	// a phrase in the game's language when a translator made a NeedsHUD_<LANGUAGE>.txt (Translate.inl), else the English
	const char* Tr(const char* a_en);
	std::string TrF(const char* a_pattern, const std::string& a_arg);  // one {} filled in

	inline std::string LowerId(std::string a_s)
	{
		for (auto& c : a_s) {
			c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
		}
		return a_s;
	}
	// is this custom need switched off? (ids compared without case)
	inline bool NeedHidden(const NeedsOptions& a_o, const std::string& a_id)
	{
		const std::string want = LowerId(a_id);
		std::string cur;
		for (char c : a_o.hiddenNeeds + ",") {
			if (c == ',') {
				if (LowerId(cur) == want) {
					return true;
				}
				cur.clear();
			} else if (c != ' ') {
				cur += c;
			}
		}
		return false;
	}
	inline void SetNeedHidden(NeedsOptions& a_o, const std::string& a_id, bool a_hidden)
	{
		std::string out, cur;
		const std::string want = LowerId(a_id);
		for (char c : a_o.hiddenNeeds + ",") {
			if (c == ',') {
				if (!cur.empty() && LowerId(cur) != want) {
					out += (out.empty() ? "" : ",") + cur;
				}
				cur.clear();
			} else if (c != ' ') {
				cur += c;
			}
		}
		if (a_hidden) {
			out += (out.empty() ? "" : ",") + a_id;
		}
		a_o.hiddenNeeds = out;
	}  // the settings page in SKSE Menu Framework (if it is installed); call once all plugins are loaded
}
