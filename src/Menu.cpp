// NeedsHUD's pages in SKSE Menu Framework's Mod Control Panel: the cold thermometer's and the Needs group's options
// (the same controls on each page), applied live and saved to NeedsHUD.ini as they change.
//
// SKSE Menu Framework 3 (Nexus 120352, by Thiago; needs 3.23 or newer: 3.23 fixed the game hanging when the panel
// closed while a number input was active). Its API header SKSEMenuFramework.h comes from
// github.com/QTR-Modding/SKSE-Menu-Framework-3-API (LGPL-2.1, licence beside it; the framework's README: "The header
// file has a license that does not require mods using it as a library to be open source"). It finds the framework's
// functions at run time (GetModuleHandle + GetProcAddress): nothing is linked, and without the framework the page
// simply isn't there. Kept in its own file because the header brings in windows.h.

// (the header's FontAwesome::UnicodeToUtf8 uses std::wstring_convert, deprecated since C++17 and an error under /sdl:
// CMakeLists.txt silences that deprecation for the whole project, since the PCH may already pull in <locale>)
#include <algorithm>
#include <atomic>
#include <cmath>
#include <cfloat>   // FLT_MIN / FLT_MAX in the header
#include <cstdarg>  // va_list in the header
#include <filesystem>
#include <string>
#include <string_view>
#include <vector>
#include "SKSEMenuFramework.h"

#include "Settings.h"

namespace
{
	namespace UI = ImGuiMCP;
	using NeedsHUD::Tr;
	using NeedsHUD::TrF;

	// a tooltip for the item just drawn, wrapped to a fixed width (his rule 2026-11-12, all his mods: long text in a menu
	// breaks into lines by width; ImGui's SetItemTooltip never wraps, so a long tip ran off the screen). About 32 font
	// heights wide, roughly 60 characters a line, at any screen size or font scale
	void Tip(const char* a_text)
	{
		if (UI::BeginItemTooltip()) {
			UI::PushTextWrapPos(UI::GetFontSize() * 32.0f);
			UI::TextUnformatted(a_text);
			UI::PopTextWrapPos();
			UI::EndTooltip();
		}
	}

	// what differs between the pages: the words (the controls and the defaults are the same for every gauge)
	struct PageText
	{
		const char* title;      // "Cold thermometer"
		const char* intro;
		const char* show;       // "Show the thermometer"
		const char* thing;      // "thermometer", used in the tooltips
		const char* numbers;    // "Show the cold value"
		const char* numbersTip;
	};

	// the snap grid, shared by every page (his idea, 2026-11-08): the position sliders jump in grid steps so gauges
	// line up; switched off, they move freely. Kept for this game session (not saved).
	bool  g_snap = true;
	float g_gridStep = 1.0f;  // percent of the screen

	// one page: its own working copy (the controls edit it, and every change is handed to the gauge)
	template <class T>
	void RenderPage(T& a_edit, bool& a_loaded, T (*a_get)(), void (*a_set)(const T&, bool), const PageText& a_text)
	{
		if (!a_loaded) {
			a_edit = a_get();
			a_loaded = true;
		}
		bool changed = false;  // apply now
		bool save = false;     // and write the file (sliders save once they are let go, not on every step)

		UI::SeparatorText(Tr(a_text.title));
		UI::TextWrapped("%s", Tr(a_text.intro));
		UI::Spacing();

		if (UI::Checkbox(Tr(a_text.show), &a_edit.enabled)) {
			changed = save = true;
		}
		if constexpr (requires { a_edit.showHunger; }) {  // the Needs page: each tile of the row
			// only the tiles a mod in this load order can feed (a player's look at the page 2026-11-10: twelve boxes, most for
			// mods he doesn't have, in one long column); three to a line
			struct TileBox
			{
				int         kind;
				const char* label;
				bool T::*   member;
				const char* tip;
			};
			static const TileBox TILE_BOXES[] = {
				{ 0, "Hunger", &T::showHunger, "The stew bowl. Hidden tiles leave no gap: the row closes up." },
				{ 1, "Thirst", &T::showThirst, "The waterskin. It only shows with a needs mod that has thirst, such as SunHelm (Survival Mode has none)." },
				{ 2, "Tiredness", &T::showTiredness, "The nightcap. Hidden tiles leave no gap: the row closes up." },
				{ 3, "Disease", &T::showDisease, "The plague doctor's mask. It only shows while you have a disease, with its name and, in Survival Mode, its stage." },
				{ 4, "Frostbite", &T::showFrostbite, "The frosted hand (Frostfall). It only shows while you have frostbite: one block and one frosted finger for each frostbitten body part, and the minutes until it wears off." },
				{ 5, "Vitality", &T::showVitality, "The heart (Last Seed). Your Vitality now (the pointer) and where it is heading (the hollow marker on the left)." },
				{ 6, "Dirt", &T::showDirt, "The tunic (Keep It Clean). How long since your last wash: Clean, Not Fresh, Dirty, Very Dirty. It shows once you have washed for the first time." },
				{ 7, "Drunk", &T::showDrunk, "The tankard (Realistic Needs and Diseases, Last Seed, SunHelm, iNeed). How drunk you are, in your needs mod's own words, or the drinks counting toward drunk. It only shows while you have had a drink." },
				{ 8, "Body", &T::showBody, "The torso (Realistic Needs and Diseases, with its weight feature on). Your body weight band, from Muscular to Skinny." },
				{ 9, "Bladder", &T::showBladder, "The dam. Only shows when a needs mod feeds it through a NeedsHUD config file." },
				{ 10, "Fun", &T::showFun, "The masks. Only shows when a needs mod feeds it through a NeedsHUD config file." },
				{ 11, "Social", &T::showSocial, "The two people. Only shows when a needs mod feeds it through a NeedsHUD config file." },
			};
			// the boxes to list: the built-in tiles a loaded mod can feed, then the needs other mods add through config files
			struct Entry
			{
				std::string label;
				bool*       value;     // a built-in tile's switch, or nullptr for a custom need
				std::string id;        // custom needs: the id
				const char* tip;
			};
			std::vector<Entry> entries;
			for (const auto& b : TILE_BOXES) {
				if (NeedsHUD::TileAvailable(b.kind)) {
					entries.push_back({ Tr(b.label), &(a_edit.*b.member), {}, b.tip });
				}
			}
			for (const auto& [id, name] : NeedsHUD::CustomNeeds()) {
				entries.push_back({ Tr(name.c_str()), nullptr, id, "A need added by another mod's NeedsHUD config file." });  // shown translated, kept by id
			}
			// as many boxes on a line as the panel's width holds (his ask 2026-11-10): the cell fits the longest label
			float widest = 0.0f;
			for (const auto& e : entries) {
				widest = std::max(widest, UI::CalcTextSize(e.label.c_str()).x);
			}
			const auto* style = UI::GetStyle();
			const float cell = UI::GetFrameHeight() + (style ? style->ItemInnerSpacing.x + style->ItemSpacing.x + 2.0f * style->CellPadding.x : 16.0f) + widest;
			const int   columns = std::clamp(static_cast<int>(UI::GetContentRegionAvail().x / std::max(cell, 1.0f)), 1,
				  std::max(1, static_cast<int>(entries.size())));
			if (!entries.empty() && UI::BeginTable("##tiles", columns)) {
				for (const auto& e : entries) {
					UI::TableNextColumn();
					if (e.value) {
						if (UI::Checkbox(e.label.c_str(), e.value)) {
							changed = save = true;
						}
					} else {
						bool shown = !NeedsHUD::NeedHidden(a_edit, e.id);
						if (UI::Checkbox((e.label + "###need_" + e.id).c_str(), &shown)) {
							NeedsHUD::SetNeedHidden(a_edit, e.id, !shown);
							changed = save = true;
						}
					}
					Tip(Tr(e.tip));
				}
				UI::EndTable();
			}
			UI::TextDisabled("%s", Tr("Only the needs your installed mods have are listed."));
		}

		UI::SeparatorText(Tr("Place and size"));
		if (UI::Checkbox(Tr("Snap to grid"), &g_snap)) {
			if (g_snap) {  // line the current place up at once
				for (double* v : { &a_edit.posX, &a_edit.posY }) {
					*v = std::clamp(std::round(*v / g_gridStep) * g_gridStep, 0.0, 100.0);
				}
				changed = save = true;
			}
		}
		Tip(Tr("The two position sliders move in grid steps, so gauges line up exactly. Turn it off to place it freely."));
		if (g_snap) {
			UI::SliderFloat(Tr("Grid size"), &g_gridStep, 0.5f, 5.0f, Tr("%.1f%% of the screen"), UI::ImGuiSliderFlags_AlwaysClamp);
			g_gridStep = std::round(g_gridStep * 2.0f) / 2.0f;  // half-percent steps
			Tip(Tr("One grid step, as a percent of the screen's full width (side to side) or full height (top to bottom). 1% of a 1920-wide screen is about 19 pixels. The same grid is used on every page."));
		}
		auto slider = [&](const char* a_label, double& a_value, float a_min, float a_max, const char* a_format,
						  const std::string& a_tip, bool a_grid) {
			float v = static_cast<float>(a_value);
			if (UI::SliderFloat(a_label, &v, a_min, a_max, Tr(a_format), UI::ImGuiSliderFlags_AlwaysClamp)) {
				a_value = (a_grid && g_snap) ? std::clamp(std::round(v / g_gridStep) * g_gridStep, a_min, a_max) : v;
				changed = true;
			}
			if (UI::IsItemDeactivatedAfterEdit()) {
				changed = save = true;
			}
			Tip(Tr(a_tip.c_str()));
		};
		const char* anchors[4] = { Tr(NeedsHUD::ANCHOR_NAMES[0]), Tr(NeedsHUD::ANCHOR_NAMES[1]), Tr(NeedsHUD::ANCHOR_NAMES[2]), Tr(NeedsHUD::ANCHOR_NAMES[3]) };
		if (UI::Combo(Tr("Anchor"), &a_edit.anchor, anchors, 4)) {
			changed = save = true;
		}
		const std::string thing = Tr(a_text.thing);
		Tip(TrF("The screen corner the {} is placed from. The two sliders below measure from it.", thing).c_str());
		// the sliders' names follow the corner; the part after ### keeps each one the same control when its name changes
		const bool fromRight = a_edit.anchor == 1 || a_edit.anchor == 3, fromBottom = a_edit.anchor >= 2;
		const std::string sideLabel = std::string(Tr(fromRight ? "From the right" : "From the left")) + "###side";
		const std::string endLabel = std::string(Tr(fromBottom ? "From the bottom" : "From the top")) + "###end";
		slider(sideLabel.c_str(), a_edit.posX, 0.0f, 100.0f, "%.1f%% of screen width",
			TrF(fromRight ? "How far the {} sits from the right side of the screen, as a percent of the screen's full width (100% = the whole width, side to side)."
			              : "How far the {} sits from the left side of the screen, as a percent of the screen's full width (100% = the whole width, side to side).", thing), true);
		slider(endLabel.c_str(), a_edit.posY, 0.0f, 100.0f, "%.1f%% of screen height",
			TrF(fromBottom ? "How far the {} sits from the bottom of the screen, as a percent of the screen's full height (100% = the whole height, top to bottom). It always stays fully on screen."
			               : "How far the {} sits from the top of the screen, as a percent of the screen's full height (100% = the whole height, top to bottom). It always stays fully on screen.", thing), true);
		slider(Tr("Size"), a_edit.scale, static_cast<float>(NeedsHUD::SCALE_MIN), static_cast<float>(NeedsHUD::SCALE_MAX), "%.2fx",
			"1.00x is the designed size.", false);
		if constexpr (requires { a_edit.vertical; }) {  // the Needs and Food slots pages: how they line up
			const char* LAYOUTS[2] = { Tr("Row"), Tr("Column") };
			int layout = a_edit.vertical ? 1 : 0;
			if (UI::Combo(Tr("Layout"), &layout, LAYOUTS, 2)) {
				a_edit.vertical = layout == 1;
				changed = save = true;
			}
			if constexpr (requires { a_edit.hideEmpty; }) {
				Tip(Tr("Row: the drink, pot meal and baked slots side by side. Column: one under the other, with the homecooked and feeding slots in a second column beside them."));
			} else {
				Tip(Tr("Row: the tiles side by side, left to right. Column: one under the other, top to bottom."));
			}
		}
		if constexpr (requires { a_edit.perLine; }) {  // the Needs page: spacing and line breaks
			slider(Tr("Spacing"), a_edit.spacing, static_cast<float>(NeedsHUD::SPACING_MIN), static_cast<float>(NeedsHUD::SPACING_MAX), "%.0f",
				"The gap between the tiles, measured between their widest parts (the frame or the longest stage name), so the names never overlap. At 0 they just touch.", false);
			if (UI::SliderInt(Tr("Tiles per line"), &a_edit.perLine, 0, NeedsHUD::PER_LINE_MAX, a_edit.perLine == 0 ? Tr("Off") : "%d", UI::ImGuiSliderFlags_AlwaysClamp)) {
				changed = true;
			}
			if (UI::IsItemDeactivatedAfterEdit()) {
				changed = save = true;
			}
			Tip(Tr("Line break: after this many tiles a new row starts (or a new column, in the column layout), growing away from the screen corner. Off keeps every tile in one line. Use it if a long line runs off the screen at a large size."));
		}

		if constexpr (requires { a_edit.warmth; }) {  // the Thermometer page
			UI::SeparatorText(Tr("Extras"));
			if (UI::Checkbox(Tr("Show total warmth"), &a_edit.warmth)) {
				changed = save = true;
			}
			Tip(Tr("Adds a line under the stage name with the total warmth of all the armor and clothing you wear. The higher it is, the slower the cold gets to you."));
			if (a_edit.warmth) {
				UI::Indent();
				if (UI::Checkbox(Tr("Only when it changes"), &a_edit.warmthOnChange)) {
					changed = save = true;
				}
				Tip(Tr("The warmth line only shows for a while after you put on or take off something warm. How long is set by Show for, under When to show."));
				UI::Unindent();
			}
		}
		if constexpr (requires { a_edit.wetness; }) {
			// only when something can feed the wetness bar (Frostfall or a config file); without one the box did nothing
			if (NeedsHUD::WetnessAvailable()) {
				if (UI::Checkbox(Tr("Show wetness"), &a_edit.wetness)) {
					changed = save = true;
				}
				Tip(Tr("With Frostfall: shows how wet you are, as a small bar beside the tube and a line under the stage name. Wet clothes make you colder. Hidden while you are dry."));
			}
		}

		UI::SeparatorText(Tr("When to show"));
		if constexpr (requires { a_edit.hideEmpty; }) {  // the Food slots page: its own words (slots fill and empty, no stages)
			if (UI::Checkbox(Tr("Hide while every slot is empty"), &a_edit.hideEmpty)) {
				changed = save = true;
			}
			Tip(Tr("The slots only show while a meal, drink or baked dish is working on you. Off: the empty slots stay on screen, showing what each one is for."));
			if (UI::Checkbox(Tr("Hide during combat"), &a_edit.hideInCombat)) {
				changed = save = true;
			}
			Tip(Tr("The food slots fade out while you are in combat and come back when the fight is over."));
			if (UI::Checkbox(Tr("Only show when a slot changes"), &a_edit.onlyOnChange)) {
				changed = save = true;
			}
			Tip(Tr("The food slots stay hidden and appear for a while each time a slot fills (you eat or drink) or empties (an effect ends)."));
			if (a_edit.onlyOnChange) {
				UI::Indent();
				slider(Tr("Show for"), a_edit.showSeconds, static_cast<float>(NeedsHUD::SHOW_MIN), static_cast<float>(NeedsHUD::SHOW_MAX),
					"%.0f seconds", "How long they stay on screen after a slot changes.", false);
				if (UI::Checkbox(Tr("Only when an effect ends"), &a_edit.onlyWorse)) {
					changed = save = true;
				}
				Tip(Tr("The food slots only appear when a meal, drink or baked dish wears off, so you know it is time to eat again. Eating doesn't bring them up."));
				UI::Unindent();
			}
		} else {
		if (UI::Checkbox(Tr("Hide during combat"), &a_edit.hideInCombat)) {
			changed = save = true;
		}
		Tip(TrF("The {} fades out while you are in combat and comes back when the fight is over.", thing).c_str());
		if (UI::Checkbox(Tr("Only show when the stage changes"), &a_edit.onlyOnChange)) {
			changed = save = true;
		}
		Tip(TrF("The {} stays hidden and appears for a while each time a stage changes (for example from Peckish to Hungry).", thing).c_str());
		bool warmthTimed = false;
		if constexpr (requires { a_edit.warmthOnChange; }) {
			warmthTimed = a_edit.warmth && a_edit.warmthOnChange;
		}
		if (a_edit.onlyOnChange || warmthTimed) {
			UI::Indent();
			slider(Tr("Show for"), a_edit.showSeconds, static_cast<float>(NeedsHUD::SHOW_MIN), static_cast<float>(NeedsHUD::SHOW_MAX),
				"%.0f seconds", "How long it stays on screen after a stage changes.", false);
			if (a_edit.onlyOnChange) {
				if (UI::Checkbox(Tr("Only when it gets worse"), &a_edit.onlyWorse)) {
					changed = save = true;
				}
				Tip(TrF("The {} only appears when a stage gets worse (colder, hungrier, more tired). Getting better, for example warming up after you go indoors, doesn't bring it up.", thing).c_str());
				if (UI::Checkbox(Tr("Always show at the worst stage"), &a_edit.keepWorst)) {
					changed = save = true;
				}
				Tip(Tr("Keeps it on screen while you are at the deadly end (Numb, freezing water, Starving, Debilitated), so you never miss the warning. Hiding during combat still applies."));
			}
			UI::Unindent();
		}

		}  // the generic "When to show"

		if constexpr (requires { a_edit.numbers; }) {
			UI::SeparatorText(Tr("Testing"));
			if (UI::Checkbox(Tr(a_text.numbers), &a_edit.numbers)) {
				changed = save = true;
			}
			Tip(Tr(a_text.numbersTip));
		}

		UI::Spacing();
		UI::Separator();
		if (UI::Button(Tr("Reset to defaults"))) {
			a_edit = T{};
			changed = save = true;
		}
		UI::TextDisabled("%s", Tr("Changes show on the HUD at once and are saved to NeedsHUD.ini."));

		if (changed) {
			a_set(a_edit, save);
		}
	}

	NeedsHUD::ColdOptions   g_coldEdit;
	NeedsHUD::NeedsOptions  g_needsEdit;
	NeedsHUD::FoodOptions   g_foodEdit;
	bool                    g_coldLoaded = false, g_needsLoaded = false, g_foodLoaded = false;

	void __stdcall RenderCold()
	{
		RenderPage(g_coldEdit, g_coldLoaded, &NeedsHUD::GetColdOptions, &NeedsHUD::SetColdOptions,
			{ "Cold thermometer", "Shows how cold you are. It appears while a mod with cold runs: Survival Mode, SunHelm or Frostfall.", "Show the thermometer",
				"thermometer", "Show the cold value",
				"Adds a line under the stage name with your needs mod's own cold number (Survival's cold, Frostfall's exposure)." });
	}

	void __stdcall RenderFood()
	{
		RenderPage(g_foodEdit, g_foodLoaded, &NeedsHUD::GetFoodOptions, &NeedsHUD::SetFoodOptions,
			{ "Food slots", "Shows what Cooking Skill Overhaul's meals, drinks and baked dishes are doing for you: one slot each for a drink, a pot meal and a baked dish, with the effect, its level and the time left. A homecooked meal and a vampire's or werewolf's feeding get their own slots under the row while they last.",
				"Show the food slots", "food slots", "", "" });
	}

	void __stdcall RenderNeeds()
	{
		RenderPage(g_needsEdit, g_needsLoaded, &NeedsHUD::GetNeedsOptions, &NeedsHUD::SetNeedsOptions,
			{ "Needs", "Shows your needs as a row of tiles, with the stages and names your needs mods use.",
				"Show the needs", "row of needs", "Show the values",
				"Adds a line under each stage name with your needs mod's own number for it." });
	}
}

namespace
{
	// a page's name in the player's language; the Menu Framework reads "/" in a name as a sub-page, so a translation
	// that uses one gets "-" instead (1.0.4 review)
	std::string PageName(const char* a_en)
	{
		std::string s = NeedsHUD::Tr(a_en);
		std::ranges::replace(s, '/', '-');
		return s;
	}
}

void NeedsHUD::RegisterMenu()
{
	static bool registered = false;  // once only
	if (registered) {
		return;
	}
	registered = true;
	if (!SKSEMenuFramework::IsInstalled()) {
		logger::info("SKSE Menu Framework not installed: no in-game settings page (edit NeedsHUD.ini instead)");
		return;
	}
	SKSEMenuFramework::SetSection("NeedsHUD");
	SKSEMenuFramework::AddSectionItem(PageName("Thermometer"), RenderCold);  // page names translated (added after tr::Load)
	SKSEMenuFramework::AddSectionItem(PageName("Needs"), RenderNeeds);
	logger::info("Settings pages added to SKSE Menu Framework (NeedsHUD > Thermometer, Needs)");
}

// the Food slots page: only with Free Cooking (the slots' provider), so it is added once the game's data has loaded
void NeedsHUD::RegisterFoodPage()
{
	static bool registered = false;
	if (registered || !NeedsHUD::FoodAvailable() || !SKSEMenuFramework::IsInstalled()) {
		return;
	}
	registered = true;
	SKSEMenuFramework::AddSectionItem(PageName("Food slots"), RenderFood);
	logger::info("Settings page added to SKSE Menu Framework (NeedsHUD > Food slots)");
}
