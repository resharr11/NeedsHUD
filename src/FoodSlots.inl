// ---- the food slots group (2026-11-12, his "next lets add our cooking huds"): Free Cooking's slots, drawn from the
// approved 2026-11-04 mockups (mockups\needshud_mockups_2026-11-04.zip: m62 slots3, m73 effects4, m83 emptyslots,
// m87 baked4, m90 drink3, m69 secret; memory: food slots group). Three slots always on screen, drink, pot meal, baked
// (baked last: it needs an oven), each an iron-rimmed dark square; an empty slot shows its faint symbol (the cup with
// the open oval rim, the steaming pot, the domed oven). A filled slot shows the meal's emblem (Mighty sword, Brainy
// spellbook, Hearty heart, Sneaky detection eye, Spicy flame, Chilly snowflake, Zesty bolt, Earthy sunrise between two
// hills), four level dots (gold dots and a thin gold edge at level 4) and the minutes left under it ("12 min"; his note
// 2026-11-12: a second-by-second countdown is too stressful; "<1 min" in pale red at the end). Secret slots appear
// centred under the row only while they run: homecooked (the spouse's meal: a small farmhouse), the vampire's feeding
// (Vampire Lord head on a violet moon) and the werewolf's (a wolf's head on the blood moon); each its own slot, since
// some mods let a player be both at once (his note 2026-11-12).
//
// The provider is Free Cooking (FreeCooking.esp): each slot, flavour and level has its own magic effect, so the slots
// are read from the player's running effects; nothing else is needed from Free Cooking. No vanilla food provider (his
// call 2026-11-04): without FreeCooking.esp the group never shows and the settings page isn't listed.
// Coordinates: mockup px at 1080p (a slot is 64 wide), around the slot's centre.

	constexpr const char* FOOD_CLIP = "NeedsHUD_Food";
	constexpr const char* FC_FOOD_FILE = "FreeCooking.esp";
	constexpr double SLOT_S = 64.0, SLOT_GAP = 12.0;  // the mockup's slot and the gap between slots
	constexpr double SECRET_DY = SLOT_S + 34.0;       // the secret slots' row, below the main row (top to top)
	constexpr double FOOD_TEXT_Y = SLOT_S / 2 + 3;    // the time's top, below a slot
	// size: the mockup's 64 px slot is big next to the needs tiles (shown at 0.65); 0.8 makes a slot about 51 px at
	// 1080p, the emblems still readable. The time is never under 18 px at 1080p (the needs tiles' minimum)
	constexpr double FOOD_SCALE = 0.8;
	constexpr double FOOD_TIME_PX = 18.0;

	// the colours (lch values from the mockup's g4.py, as hex)
	constexpr std::uint32_t F_BONE = 0xCFC6B2, F_IRON = 0x8F8A80, F_SLOT = 0x0D0D0C;
	constexpr std::uint32_t F_GOLD_DOT = 0xE7A33E, F_GOLD_EDGE = 0xD5A355, F_TIME = 0xE6E1D6, F_TIME_LOW = 0xE6A08A;

	// the eight flavours, in Free Cooking's effect order
	enum : int { kMighty, kBrainy, kHearty, kSneaky, kEarthy, kSpicy, kChilly, kZesty, kFlavours };
	// slots: the three always shown, then the two secret ones
	enum : int { kDrink, kPot, kBaked, kHome, kVamp, kWolf, kSlots };

	struct FoodEffect
	{
		RE::EffectSetting* mgef = nullptr;
		int slot = 0, flavour = 0, level = 0;  // level 1..4; feeding: the slot is decided by the spell (vampire or werewolf)
	};
	std::vector<FoodEffect> g_foodEffects;
	bool g_foodProvider = false;  // FreeCooking.esp is loaded (fixed at kDataLoaded)
	// the feeding spells: both carry the same Mighty effect, so the spell tells whose feeding it is (a hybrid has both)
	RE::SpellItem* g_feedVampSpell = nullptr;  // FC_Feed_Vampire 0x977
	RE::SpellItem* g_feedWolfSpell = nullptr;  // FC_Feed_Werewolf 0x976

#ifndef NH_DRAW_ONLY  // (the Linux render test draws the slots without the game)
	// Free Cooking's effect IDs (FreeCooking.esp, read from the esp 2026-11-12): Mighty, Brainy, Hearty, Sneaky levels
	// 1-4 from the first base, then Earthy, Spicy, Chilly, Zesty levels 1-4 from the second
	void FindFoodEffects(RE::TESDataHandler* a_data)
	{
		g_foodEffects.clear();
		g_foodProvider = a_data && ModActive(a_data, FC_FOOD_FILE);
		if (!g_foodProvider) {
			logger::info("Food slots: Free Cooking not loaded, the group stays hidden");
			return;
		}
		struct Base
		{
			int slot;
			RE::FormID first, second;
		};
		constexpr Base BASES[] = { { kPot, 0x830, 0x910 }, { kDrink, 0x8C0, 0x930 }, { kBaked, 0x990, 0x9C0 } };
		int missing = 0;
		const auto add = [&](RE::FormID a_id, int a_slot, int a_flavour, int a_level) {
			if (auto e = a_data->LookupForm<RE::EffectSetting>(a_id, FC_FOOD_FILE)) {
				g_foodEffects.push_back({ e, a_slot, a_flavour, a_level });
			} else {
				++missing;
			}
		};
		for (const auto& b : BASES) {
			for (int f = 0; f < 4; ++f) {
				for (int l = 0; l < 4; ++l) {
					add(b.first + f * 4 + l, b.slot, f, l + 1);
					add(b.second + f * 4 + l, b.slot, kEarthy + f, l + 1);
				}
			}
		}
		add(0x972, kVamp, kMighty, 1);  // FC_Fx_FeedMighty: vampires and werewolves (the spell decides the slot)
		add(0x973, kVamp, kBrainy, 1);  // FC_Fx_FeedBrainy: vampires only
		g_feedVampSpell = a_data->LookupForm<RE::SpellItem>(0x977, FC_FOOD_FILE);
		g_feedWolfSpell = a_data->LookupForm<RE::SpellItem>(0x976, FC_FOOD_FILE);
		add(0x97B, kHome, kHearty, 1);  // FC_Fx_HomeHearty: the spouse's Homecooked Meal
		logger::info("Food slots: Free Cooking found, {} slot effects{}", g_foodEffects.size(),
			missing ? std::format(" ({} MISSING: an older Free Cooking?)", missing) : std::string());
	}

#endif

	struct FoodSlotRead
	{
		bool   on = false;
		int    flavour = 0, level = 0;
		double left = 0.0;  // seconds
	};

#ifndef NH_DRAW_ONLY
	// what runs in each slot now (a slot holds one effect; if two overlap for a moment, the one with more time left)
	std::array<FoodSlotRead, kSlots> ReadFoodSlots()
	{
		std::array<FoodSlotRead, kSlots> r{};
		if (g_foodEffects.empty()) {
			return r;
		}
		for (const auto& e : g_view.effects) {  // the main-thread snapshot (Snapshot.inl)
			const auto* ae = &e;
			if (e.dispelled) {  // not inactive: Spicy beside another element (Survival) turns part of itself off, but its slot is still full
				continue;
			}
			const auto mgef = e.mgef;
			if (!mgef) {
				continue;
			}
			for (const auto& fe : g_foodEffects) {
				if (fe.mgef != mgef) {
					continue;
				}
				const double left = ae->duration > 0.0f ? std::max(0.0, static_cast<double>(ae->duration - ae->elapsed)) : 0.0;
				int slot = fe.slot;
				if (slot == kVamp) {  // feeding: whose, by the spell it came from
					if (g_feedWolfSpell && ae->spell == g_feedWolfSpell) {
						slot = kWolf;
					} else if (fe.flavour == kMighty && !(g_feedVampSpell && ae->spell == g_feedVampSpell)) {
						slot = kWolf;  // an unknown source: the Mighty part alone is the werewolf's
					}
				}
				auto& s = r[slot];
				if (slot >= kHome) {
					s.left = std::max(s.left, left);
					s.on = true;
				} else if (!s.on || left > s.left) {
					s = { true, fe.flavour, fe.level, left };
				}
				break;
			}
		}
		return r;
	}

#endif

	// ---- drawing ----

	// a path from points (closed)
	void FPoly(GFx& g, std::initializer_list<std::pair<double, double>> a_pts)
	{
		bool first = true;
		std::pair<double, double> start{};
		for (const auto& [x, y] : a_pts) {
			if (first) {
				MoveTo(g, x, y);
				start = { x, y };
				first = false;
			} else {
				LineTo(g, x, y);
			}
		}
		LineTo(g, start.first, start.second);
	}
	void FPolyV(GFx& g, const std::vector<std::pair<double, double>>& a_pts)
	{
		if (a_pts.empty()) {
			return;
		}
		MoveTo(g, a_pts[0].first, a_pts[0].second);
		for (size_t i = 1; i < a_pts.size(); ++i) {
			LineTo(g, a_pts[i].first, a_pts[i].second);
		}
		LineTo(g, a_pts[0].first, a_pts[0].second);
	}
	// a cubic segment from the current point (GFx draws only quadratic curves): sampled into lines
	void CubicTo(GFx& g, double x0, double y0, double x1, double y1, double x2, double y2, double x3, double y3, int n = 8)
	{
		for (int i = 1; i <= n; ++i) {
			const double t = static_cast<double>(i) / n, u = 1.0 - t;
			LineTo(g, u * u * u * x0 + 3 * u * u * t * x1 + 3 * u * t * t * x2 + t * t * t * x3,
				u * u * u * y0 + 3 * u * u * t * y1 + 3 * u * t * t * y2 + t * t * t * y3);
		}
	}
	// the emblems' outline: the mockup's stroke="DARK" stroke-opacity=".85" stroke-width="1.6"
	void EmStroke(GFx& g, double a_w = 1.6)
	{
		Line(g, a_w, 0x000000, 85.0);
	}

	// the slot's frame (slot_frame): dark see-through square, dark edge, iron rim, a faint inner line, four rivets
	void DrawSlotFrame(GFx& g, bool a_gold)
	{
		const double h = SLOT_S / 2;
		NoLine(g);
		Fill(g, F_SLOT, 62.0);
		RoundRectPath(g, -h, -h, SLOT_S, SLOT_S, 6.0);
		EndFill(g);
		Line(g, 3.6, 0x000000, 70.0);
		RoundRectPath(g, -h, -h, SLOT_S, SLOT_S, 6.0);
		Line(g, 1.6, F_IRON);
		RoundRectPath(g, -h, -h, SLOT_S, SLOT_S, 6.0);
		Line(g, 1.0, F_BONE, 22.0);
		RoundRectPath(g, -h + 3, -h + 3, SLOT_S - 6, SLOT_S - 6, 4.0);
		NoLine(g);
		for (const auto& [px, py] : { std::pair{ -h + 5, -h + 5 }, { h - 5, -h + 5 }, { -h + 5, h - 5 }, { h - 5, h - 5 } }) {
			Fill(g, F_IRON);
			CirclePath(g, px, py, 1.3);
			EndFill(g);
		}
		if (a_gold) {  // level 4: a thin gold edge just inside the rim
			Line(g, 1.6, F_GOLD_EDGE, 90.0);
			RoundRectPath(g, -h + 1.5, -h + 1.5, SLOT_S - 3, SLOT_S - 3, 5.0);
			NoLine(g);
		}
	}

	// the empty slots' faint symbols (bone lines at 32%), drawn around the slot's centre
	void DrawEmptySymbol(GFx& g, int a_slot)
	{
		const double cx = 0.0, cy = 0.0, op = 32.0;
		const auto ln = [&](double a_w, double a_op = 32.0) { Line(g, a_w, F_BONE, a_op); };
		if (a_slot == kPot) {  // sym_pot: the bowl, its rim, two handles, two steam curls
			ln(2.2);
			MoveTo(g, cx - 17, cy - 6);
			CurveTo(g, cx - 17, cy + 16, cx, cy + 16);
			CurveTo(g, cx + 17, cy + 16, cx + 17, cy - 6);
			LineTo(g, cx - 17, cy - 6);
			MoveTo(g, cx - 20, cy - 6);
			LineTo(g, cx + 20, cy - 6);
			MoveTo(g, cx - 17, cy - 1);
			LineTo(g, cx - 22, cy - 1);
			MoveTo(g, cx + 17, cy - 1);
			LineTo(g, cx + 22, cy - 1);
			ln(1.6);
			for (double sx : { cx - 6.0, cx + 5.0 }) {
				MoveTo(g, sx, cy - 12);
				CubicTo(g, sx, cy - 12, sx - 3, cy - 16, sx + 3, cy - 18, sx, cy - 22);
			}
		} else if (a_slot == kDrink) {  // sym_cup_oval: the cup with an open oval rim (his pick)
			ln(2.2);
			MoveTo(g, cx - 12, cy - 14);
			LineTo(g, cx - 10, cy + 15);
			LineTo(g, cx + 10, cy + 15);
			LineTo(g, cx + 12, cy - 14);
			MoveTo(g, cx + 11, cy - 7);
			CurveTo(g, cx + 21, cy - 7, cx + 20, cy + 2);
			CurveTo(g, cx + 19, cy + 8, cx + 11, cy + 8);
			ln(1.6);
			MoveTo(g, cx - 12, cy - 14);
			CurveTo(g, cx, cy - 10, cx + 12, cy - 14);
			CurveTo(g, cx, cy - 18, cx - 12, cy - 14);
		} else {  // sym_oven: the domed stone oven on its brick base, an arched mouth with a small fire
			ln(2.2);
			MoveTo(g, cx - 16, cy + 3);
			CurveTo(g, cx - 16, cy - 16, cx, cy - 16);
			CurveTo(g, cx + 16, cy - 16, cx + 16, cy + 3);
			LineTo(g, cx - 16, cy + 3);
			MoveTo(g, cx - 8, cy + 3);
			LineTo(g, cx - 8, cy - 4);
			CurveTo(g, cx - 8, cy - 10, cx, cy - 10);
			CurveTo(g, cx + 8, cy - 10, cx + 8, cy - 4);
			LineTo(g, cx + 8, cy + 3);
			MoveTo(g, cx - 19, cy + 3);
			LineTo(g, cx + 19, cy + 3);
			LineTo(g, cx + 19, cy + 15);
			LineTo(g, cx - 19, cy + 15);
			LineTo(g, cx - 19, cy + 3);
			ln(1.6, op * 0.9);
			MoveTo(g, cx - 3, cy + 2);
			CubicTo(g, cx - 3, cy + 2, cx - 5, cy - 1, cx - 3, cy - 4, cx, cy - 6);
			CubicTo(g, cx, cy - 6, cx, cy - 3, cx + 3, cy - 2, cx + 3, cy + 2);
			ln(1.3, op * 0.8);
			MoveTo(g, cx - 19, cy + 9);
			LineTo(g, cx + 19, cy + 9);
			for (const auto& [x, y0, y1] : { std::tuple{ cx - 10, cy + 3, cy + 9 }, { cx + 4, cy + 3, cy + 9 }, { cx - 3, cy + 9, cy + 15 }, { cx + 11, cy + 9, cy + 15 } }) {
				MoveTo(g, x, y0);
				LineTo(g, x, y1);
			}
		}
		NoLine(g);
	}

	// the flavour emblems (em), centred on cx, cy
	void DrawEmblem(GFx& g, int a_flavour, double cx, double cy)
	{
		switch (a_flavour) {
		case kSpicy:
			EmStroke(g);
			Fill(g, 0xC6805D);
			MoveTo(g, cx, cy + 13);
			CubicTo(g, cx, cy + 13, cx - 10, cy + 11, cx - 12, cy + 2, cx - 6, cy - 5);
			CubicTo(g, cx - 6, cy - 5, cx - 6, cy, cx - 3, cy + 2, cx - 1, cy + 2);
			CubicTo(g, cx - 1, cy + 2, cx - 3, cy - 6, cx + 1, cy - 11, cx + 6, cy - 14);
			CubicTo(g, cx + 6, cy - 14, cx + 5, cy - 8, cx + 13, cy - 5, cx + 12, cy + 3);
			CubicTo(g, cx + 12, cy + 3, cx + 11, cy + 9, cx + 6, cy + 13, cx, cy + 13);
			EndFill(g);
			break;
		case kHearty:
			EmStroke(g);
			Fill(g, 0xA86463);
			MoveTo(g, cx, cy + 12);
			CubicTo(g, cx, cy + 12, cx - 16, cy + 1, cx - 14, cy - 13, cx - 5, cy - 12);
			CubicTo(g, cx - 5, cy - 12, cx - 2, cy - 12, cx, cy - 9, cx, cy - 7);
			CubicTo(g, cx, cy - 7, cx, cy - 9, cx + 2, cy - 12, cx + 5, cy - 12);
			CubicTo(g, cx + 5, cy - 12, cx + 14, cy - 13, cx + 16, cy + 1, cx, cy + 12);
			EndFill(g);
			break;
		case kZesty:
			EmStroke(g);
			Fill(g, 0xD1C186);
			FPoly(g, { { cx + 7, cy - 18 }, { cx - 11, cy + 3 }, { cx - 1, cy + 3 }, { cx - 6, cy + 18 }, { cx + 12, cy - 5 }, { cx + 2, cy - 5 } });
			EndFill(g);
			break;
		case kMighty:
			{  // a broad Nordic sword, turned 35 degrees: wide blade with a fuller, heavy guard, grip, pommel
				const double a = -35.0 * std::numbers::pi / 180.0, ca = std::cos(a), sa = std::sin(a);
				const auto R = [&](double x, double y) { return std::pair{ cx + x * ca - y * sa, cy + x * sa + y * ca }; };
				EmStroke(g);
				Fill(g, 0xC3B4A0);
				FPoly(g, { R(0, -19), R(5.5, -13), R(5.5, 5), R(-5.5, 5), R(-5.5, -13) });
				EndFill(g);
				Line(g, 1.6, 0x847B6F);
				{
					auto [x0, y0] = R(0, -13);
					auto [x1, y1] = R(0, 3);
					MoveTo(g, x0, y0);
					LineTo(g, x1, y1);
				}
				EmStroke(g);
				Fill(g, 0x9F8163);
				FPoly(g, { R(-11, 5), R(11, 5), R(11, 9), R(-11, 9) });
				EndFill(g);
				Fill(g, 0x705441);
				FPoly(g, { R(-2.2, 9), R(2.2, 9), R(2.2, 16), R(-2.2, 16) });
				EndFill(g);
				Fill(g, 0x9F8163);
				{
					auto [px, py] = R(0, 18);
					CirclePath(g, px, py, 3.0);
				}
				EndFill(g);
			}
			break;
		case kChilly:
			{  // a snowflake with branches: dark under-strokes, then the ice colour
				std::vector<std::array<double, 4>> segs;
				for (int d = 0; d < 360; d += 60) {
					const double r = d * std::numbers::pi / 180.0;
					segs.push_back({ cx, cy, cx + 15 * std::cos(r), cy + 15 * std::sin(r) });
					const double bx = cx + 9 * std::cos(r), by = cy + 9 * std::sin(r);
					for (int s : { -1, 1 }) {
						const double rr = r + s * std::numbers::pi / 4.0;
						segs.push_back({ bx, by, bx + 5 * std::cos(rr), by + 5 * std::sin(rr) });
					}
				}
				for (int pass = 0; pass < 2; ++pass) {
					if (pass == 0) {
						Line(g, 6.0, 0x000000, 85.0);
					} else {
						Line(g, 3.0, 0x9CC8D5);
					}
					for (const auto& s : segs) {
						MoveTo(g, s[0], s[1]);
						LineTo(g, s[2], s[3]);
					}
				}
			}
			break;
		case kBrainy:
			{  // an open spellbook with a glowing sigil
				EmStroke(g);
				for (int side : { -1, 1 }) {
					Fill(g, 0xB4AA99);
					MoveTo(g, cx, cy + 11);
					CubicTo(g, cx, cy + 11, cx + side * 6, cy + 7, cx + side * 12, cy + 8, cx + side * 17, cy + 10);
					LineTo(g, cx + side * 17, cy - 8);
					CubicTo(g, cx + side * 17, cy - 8, cx + side * 12, cy - 10, cx + side * 6, cy - 9, cx, cy - 5);
					LineTo(g, cx, cy + 11);
					EndFill(g);
				}
				NoLine(g);
				Fill(g, 0x868FB6, 35.0);
				CirclePath(g, cx, cy - 10, 6.5);
				EndFill(g);
				EmStroke(g);
				Fill(g, 0xC8D0F9);
				FPoly(g, { { cx, cy - 16 }, { cx + 1.6, cy - 11.6 }, { cx + 6, cy - 10 }, { cx + 1.6, cy - 8.4 }, { cx, cy - 4 }, { cx - 1.6, cy - 8.4 },
							 { cx - 6, cy - 10 }, { cx - 1.6, cy - 11.6 } });
				EndFill(g);
			}
			break;
		case kSneaky:
			// the vanilla detection eye, half closed
			EmStroke(g);
			Fill(g, 0xB2AAA0);
			MoveTo(g, cx - 17, cy);
			CurveTo(g, cx, cy - 14, cx + 17, cy);
			CurveTo(g, cx, cy + 14, cx - 17, cy);
			EndFill(g);
			NoLine(g);
			Fill(g, 0x3E484F);
			CirclePath(g, cx, cy, 6.0);
			EndFill(g);
			Fill(g, 0x0B0B0D);
			CirclePath(g, cx, cy, 2.6);
			EndFill(g);
			Line(g, 1.2, 0x000000, 85.0);
			Fill(g, 0x5F5950);
			MoveTo(g, cx - 17, cy);
			CurveTo(g, cx, cy - 14, cx + 17, cy);
			CurveTo(g, cx, cy - 5, cx - 17, cy);
			EndFill(g);
			break;
		default:  // kEarthy: holy, a sunrise between two hills
			{
				Line(g, 2.0, 0xCFB98C);
				for (int d = 15; d <= 165; d += 30) {
					const double r = d * std::numbers::pi / 180.0;
					MoveTo(g, cx + 10 * std::cos(r), cy + 4 - 10 * std::sin(r));
					LineTo(g, cx + 16 * std::cos(r), cy + 4 - 16 * std::sin(r));
				}
				EmStroke(g);
				Fill(g, 0xCFB98C);
				MoveTo(g, cx - 7.5, cy + 4);
				ArcTo(g, cx, cy + 4, 7.5, std::numbers::pi, 2.0 * std::numbers::pi);
				LineTo(g, cx - 7.5, cy + 4);
				EndFill(g);
				Fill(g, 0x686A4C);
				MoveTo(g, cx - 21, cy + 15);
				CurveTo(g, cx - 12, cy - 6, cx + 3, cy + 8);
				LineTo(g, cx + 3, cy + 15);
				LineTo(g, cx - 21, cy + 15);
				EndFill(g);
				Fill(g, 0x59553F);
				MoveTo(g, cx - 7, cy + 15);
				CurveTo(g, cx + 8, cy - 4, cx + 21, cy + 6);
				LineTo(g, cx + 21, cy + 15);
				LineTo(g, cx - 7, cy + 15);
				EndFill(g);
			}
			break;
		}
		NoLine(g);
	}

	// the four level dots (pips): bone, gold at level 4
	void DrawLevelDots(GFx& g, int a_level, double cy)
	{
		const bool gold = a_level >= 4;
		for (int i = 0; i < 4; ++i) {
			const double x = -13.5 + i * 9;
			const bool   on = i < a_level;
			const double r = gold ? 3.1 : 2.6;
			Line(g, 1.0, F_BONE, on ? 100.0 : 35.0);
			if (on) {
				Fill(g, gold ? F_GOLD_DOT : F_BONE);
			}
			CirclePath(g, x, cy, r);
			if (on) {
				EndFill(g);
			}
		}
		NoLine(g);
	}

	// the secret slots' symbols, centred on cx, cy
	void DrawFarmhouse(GFx& g, double cx, double cy)
	{
		const auto st = [&]() { Line(g, 1.2, 0x000000, 80.0); };
		st();
		Fill(g, 0x655141);
		FPoly(g, { { cx - 16, cy + 15 }, { cx - 16, cy + 1 }, { cx + 16, cy + 1 }, { cx + 16, cy + 15 } });
		EndFill(g);
		Fill(g, 0x7F6A49);
		FPoly(g, { { cx - 20, cy + 2 }, { cx - 4, cy - 14 }, { cx + 4, cy - 14 }, { cx + 20, cy + 2 } });
		EndFill(g);
		Line(g, 0.8, 0x5F4D38, 80.0);
		for (const auto& [x0, y0, x1, y1] : { std::tuple{ -14.0, -1.0, -2.0, -12.0 }, { -6.0, 1.0, 3.0, -9.0 }, { 3.0, 1.0, 9.0, -5.0 } }) {
			MoveTo(g, cx + x0, cy + y0);
			LineTo(g, cx + x1, cy + y1);
		}
		st();
		Fill(g, 0x695C53);
		FPoly(g, { { cx + 8, cy - 15 }, { cx + 12, cy - 15 }, { cx + 12, cy - 6 }, { cx + 8, cy - 6 } });
		EndFill(g);
		Line(g, 1.2, 0xB8A996, 55.0);
		MoveTo(g, cx + 10, cy - 17);
		CubicTo(g, cx + 10, cy - 17, cx + 7, cy - 20, cx + 13, cy - 22, cx + 10, cy - 26);
		st();
		Fill(g, 0x42312A);
		FPoly(g, { { cx - 3, cy + 6 }, { cx + 3, cy + 6 }, { cx + 3, cy + 15 }, { cx - 3, cy + 15 } });
		EndFill(g);
		for (double wx : { cx - 12.0, cx + 7.0 }) {
			Fill(g, 0xD2A365);
			FPoly(g, { { wx, cy + 5 }, { wx + 5, cy + 5 }, { wx + 5, cy + 9 }, { wx, cy + 9 } });
			EndFill(g);
		}
		NoLine(g);
	}

	// a moon disc with a bright rim down its right side
	void DrawMoon(GFx& g, double cx, double cy, double r, std::uint32_t a_fill, std::uint32_t a_rim)
	{
		NoLine(g);
		Fill(g, a_fill);
		CirclePath(g, cx, cy, r);
		EndFill(g);
		const double a = std::acos(0.25);
		Line(g, 1.4, a_rim, 85.0);
		MoveTo(g, cx + r * std::cos(-a), cy + r * std::sin(-a));
		ArcTo(g, cx, cy, r, -a, a);
		NoLine(g);
	}

	void DrawVampireHead(GFx& g, double cx, double cy)
	{
		// v_head3: the Vampire Lord constellation's head, solid, on a violet moon, with its swept eyes
		DrawMoon(g, cx, cy, 19.0, 0x322D47, 0x7C7A92);
		constexpr double k = 0.105;
		const auto P = [&](double x, double y) { return std::pair{ cx + (x - 315) * k, cy + (y - 205) * k }; };
		Line(g, 1.4, 0xA09EB7);
		Fill(g, 0x1C1A22);
		{
			auto [x0, y0] = P(190, 140);
			auto [x1, y1] = P(180, 10);
			auto [x2, y2] = P(450, 10);
			auto [x3, y3] = P(440, 140);
			MoveTo(g, x0, y0);
			CubicTo(g, x0, y0, x1, y1, x2, y2, x3, y3, 12);
			for (const auto& [x, y] : { std::pair{ 485.0, 150.0 }, { 430.0, 215.0 }, { 400.0, 250.0 }, { 378.0, 295.0 }, { 368.0, 375.0 }, { 342.0, 312.0 },
					 { 326.0, 345.0 }, { 315.0, 308.0 }, { 304.0, 345.0 }, { 288.0, 312.0 }, { 262.0, 375.0 }, { 252.0, 295.0 }, { 230.0, 250.0 },
					 { 200.0, 215.0 }, { 145.0, 150.0 }, { 190.0, 140.0 } }) {
				auto [px, py] = P(x, y);
				LineTo(g, px, py);
			}
		}
		EndFill(g);
		Line(g, 1.7, 0xD2CFE6);
		for (int d : { 1, -1 }) {
			const auto X = [&](double x) { return 315 + d * (x - 315); };
			auto [x0, y0] = P(X(222), 205);
			auto [x1, y1] = P(X(238), 245);
			auto [x2, y2] = P(X(272), 262);
			auto [x3, y3] = P(X(300), 262);
			auto [x4, y4] = P(X(288), 248);
			MoveTo(g, x0, y0);
			CubicTo(g, x0, y0, x1, y1, x2, y2, x3, y3);
			LineTo(g, x4, y4);
		}
		NoLine(g);
	}

	void DrawWolfHead(GFx& g, double cx, double cy)
	{
		// w_side2: a snarling wolf's head in profile, its mane breaking out of the blood moon to the left
		constexpr std::uint32_t GOLD = 0xC3A071, FANG = 0xD7CABA;
		DrawMoon(g, cx + 5, cy, 17.0, 0x632E21, 0xAC7C54);
		Line(g, 1.4, GOLD);
		Fill(g, 0x281D1A);
		std::vector<std::pair<double, double>> pts;
		for (const auto& [x, y] : { std::pair{ -21, -12 }, { -8, -9 }, { -5, -12 }, { -9, -21 }, { 2, -13 }, { 9, -9 }, { 19, -6 }, { 22, -3 }, { 21, 0 },
				 { 9, 0 }, { 18, 9 }, { 11, 12 }, { 3, 12 }, { -6, 10 }, { -12, 5 }, { -21, 0 }, { -11, -4 } }) {
			pts.emplace_back(cx + x, cy + y);
		}
		FPolyV(g, pts);
		EndFill(g);
		Line(g, 0.9, GOLD, 85.0);
		MoveTo(g, cx - 4, cy + 12);
		CurveTo(g, cx - 11, cy + 18, cx - 19, cy + 11);
		Line(g, 0.6, 0x000000, 70.0);
		for (const auto& [tx, ln, w] : { std::tuple{ 10.5, 5.5, 1.9 }, { 14.5, 4.5, 1.6 }, { 18.3, 3.5, 1.3 } }) {
			Fill(g, FANG);
			FPoly(g, { { cx + tx - w, cy + 0.2 }, { cx + tx, cy + 0.2 + ln }, { cx + tx + w, cy + 0.2 } });
			EndFill(g);
		}
		for (const auto& [tx, ln, w] : { std::tuple{ 12.5, 4.0, 1.6 }, { 16.3, 3.0, 1.3 } }) {
			const double yb = cy + 10.6 - (tx - 10) * 0.45;
			Fill(g, FANG);
			FPoly(g, { { cx + tx - w, yb }, { cx + tx, yb - ln }, { cx + tx + w, yb } });
			EndFill(g);
		}
		NoLine(g);
		Fill(g, GOLD);
		CirclePath(g, cx + 5, cy - 7, 1.3);
		EndFill(g);
	}

	// one slot, around its centre
	void DrawFoodSlot(GFx& g, int a_slot, const FoodSlotRead& a_r)
	{
		g.Invoke("clear", nullptr, nullptr, 0);
		const bool secret = a_slot >= kHome;
		DrawSlotFrame(g, !secret && a_r.on && a_r.level >= 4);
		if (secret) {
			if (a_slot == kHome) {
				DrawFarmhouse(g, 0.0, -2.0);
			} else if (a_slot == kVamp) {
				DrawVampireHead(g, 0.0, -2.0);
			} else {
				DrawWolfHead(g, 0.0, -2.0);
			}
		} else if (!a_r.on) {
			DrawEmptySymbol(g, a_slot);
		} else {
			DrawEmblem(g, a_r.flavour, 0.0, -5.0);
			DrawLevelDots(g, a_r.level, SLOT_S / 2 - 10);
		}
	}

	// the time left in whole minutes of real time, rounded up ("12 min"), like the frostbite tile; "<1 min" for the last
	// minute (his note 2026-11-12: a second-by-second countdown is too stressful)
	int FoodMinutes(double a_seconds)
	{
		return a_seconds < 60.0 ? 0 : static_cast<int>(std::ceil(a_seconds / 60.0));
	}
	std::string FoodTime(double a_seconds)
	{
		const int m = FoodMinutes(a_seconds);
		return m == 0 ? tr::T(std::string("<1 min")) : tr::F("{} min", m);
	}

#ifndef NH_DRAW_ONLY
	// ---- the group ----

	NeedsHUD::FoodOptions g_foodOpt;
	NeedsHUD::FoodOptions g_foodPending;
	std::mutex            g_foodLock;
	std::atomic_bool      g_foodChanged{ false };

	struct FoodSlotClip
	{
		GFx  clip, art, time, timeSh;
		bool made = false;
		int  shownKey = -1;      // what is drawn (on, flavour, level)
		int  shownSecs = -1;     // the time shown (whole minutes; 0 = the last minute)
		bool shownLow = false;
	};
	struct FoodGroupState
	{
		bool   failed = false;
		std::array<FoodSlotClip, kSlots> slots;
		double placedFor = -1.0;  // the screen width it was placed for
		int    secretKey = -1;    // which secret slots are placed
		double shownAlpha = -1.0;
		AutoShow autoShow;
	};
	FoodGroupState g_food;

	void FoodGroupTick()
	{
		auto& n = g_food;
		if (n.failed || !g_foodProvider) {
			return;
		}
		static int seenLoad = 0;
		if (const int load = g_loadCount.load(); load != seenLoad) {
			seenLoad = load;
			n.autoShow = AutoShow{};  // "only on change" compares with the loaded game, not the one before (1.0.3)
		}
		auto ui = RE::UI::GetSingleton();
		auto hud = ui ? ui->GetMenu(RE::HUDMenu::MENU_NAME) : nullptr;
		RE::GFxMovieView* movie = hud ? hud->uiMovie.get() : nullptr;
		GFx root;
		if (!movie || !movie->GetVariable(&root, "_root") || !root.IsDisplayObject()) {
			return;
		}
		GFx box;
		if (!root.GetMember(FOOD_CLIP, &box) || !box.IsDisplayObject()) {
			GFx depth;
			root.Invoke("getNextHighestDepth", &depth, nullptr, 0);
			const GFx args[2]{ MakeString(movie, FOOD_CLIP), depth.IsNumber() ? depth.GetNumber() : 9002.0 };
			root.Invoke("createEmptyMovieClip", nullptr, args, 2);
			if (!root.GetMember(FOOD_CLIP, &box) || !box.IsDisplayObject()) {
				logger::error("Could not add the food slots to the HUD");
				n.failed = true;
				return;
			}
			n = FoodGroupState{};
			JoinHudModes(movie, box, "Food slots");
			logger::info("Food slots added to the HUD");
		}

		if (g_foodChanged.exchange(false)) {
			{
				std::scoped_lock lock(g_foodLock);
				g_foodOpt = g_foodPending;
			}
			for (auto& s : n.slots) {
				for (const char* name : { "timeSh", "time" }) {
					GFx field;
					if (s.clip.IsDisplayObject() && s.clip.GetMember(name, &field) && field.IsDisplayObject()) {
						field.Invoke("removeTextField", nullptr, nullptr, 0);
					}
				}
				s.time = GFx();
				s.timeSh = GFx();
				s.shownKey = -1;
				s.shownSecs = -1;
			}
			n.placedFor = -1.0;
			n.shownAlpha = -1.0;
		}
		const auto& o = g_foodOpt;
		const auto  r = o.enabled ? ReadFoodSlots() : std::array<FoodSlotRead, kSlots>{};
		const bool  anyOn = std::ranges::any_of(r, [](const FoodSlotRead& a_s) { return a_s.on; });
		if (!o.enabled || (o.hideEmpty && !anyOn)) {
			if (n.shownAlpha != 0.0) {
				n.shownAlpha = 0.0;
				box.SetMember("_alpha", GFx(0.0));
			}
			n.autoShow.alpha = -1.0;
			return;
		}

		static constexpr const char* SLOT_CLIPS[kSlots] = { "drink", "pot", "baked", "home", "vamp", "wolf" };
		const double k = MOCK_TO_HUD * FOOD_SCALE * o.scale;
		const double timeF = std::max(FOOD_TIME_PX, 17.0 * FOOD_SCALE * o.scale) / (FOOD_SCALE * o.scale);  // in slot units
		for (int i = 0; i < kSlots; ++i) {
			auto& s = n.slots[i];
			if (!s.made) {
				s.clip = ChildClip(movie, box, SLOT_CLIPS[i], 1.0 + i);
				s.art = s.clip.IsDisplayObject() ? ChildClip(movie, s.clip, "art", 1.0) : GFx();
				s.made = s.art.IsDisplayObject();
				if (!s.made) {
					return;
				}
			}
			if (!s.time.IsDisplayObject()) {
				const double fw = 120.0;
				s.timeSh = TextField(movie, s.clip, "timeSh", 10.0, -fw / 2 + 1.5, FOOD_TEXT_Y + 1.5, fw, timeF, 0x000000);
				s.time = TextField(movie, s.clip, "time", 11.0, -fw / 2, FOOD_TEXT_Y, fw, timeF, F_TIME);
			}
		}

		// place (his ask 2026-11-12: a row or a column): a row has the three slots side by side and the secret ones centred
		// under it; a column has them one under the other, each with its time under it, and the secret ones in a second
		// column on the side away from the screen edge, centred beside it. The block always keeps room for the secret
		// slots, so the main slots never jump when one appears
		const int secretKey = (r[kHome].on ? 1 : 0) | (r[kVamp].on ? 2 : 0) | (r[kWolf].on ? 4 : 0);
		const auto rect = movie->GetVisibleFrameRect();
		const double width = rect.right - rect.left, height = rect.bottom - rect.top;
		if (width != n.placedFor || secretKey != n.secretKey) {
			n.placedFor = width;
			n.secretKey = secretKey;
			const int  anchor = std::clamp(o.anchor, 0, 3);
			const bool fromRight = anchor == 1 || anchor == 3, fromBottom = anchor >= 2;
			const double textH = FOOD_TEXT_Y - SLOT_S / 2 + timeF * 1.4;  // the time under a slot
			const double colW = std::max(SLOT_S, timeF * 3.4);             // a column as wide as "29 min"
			const double vStep = SLOT_S + textH + SLOT_GAP;                // one slot and its time, in a column
			const int count = (secretKey & 1) + ((secretKey >> 1) & 1) + ((secretKey >> 2) & 1);
			double blockW, blockH;
			if (o.vertical) {
				blockW = 2 * colW + SLOT_GAP;
				blockH = 3 * vStep - SLOT_GAP;
			} else {
				blockW = 3 * SLOT_S + 2 * SLOT_GAP;
				blockH = SECRET_DY + SLOT_S + textH;
			}
			const double w = blockW * k, h = blockH * k;
			double left = fromRight ? rect.right - width * o.posX / 100.0 - w : rect.left + width * o.posX / 100.0;
			double top = fromBottom ? rect.bottom - height * o.posY / 100.0 - h : rect.top + height * o.posY / 100.0;
			left = std::clamp(left, rect.left + 6.0, std::max(rect.left + 6.0, rect.right - 6.0 - w));
			top = std::clamp(top, rect.top + 6.0, std::max(rect.top + 6.0, rect.bottom - 6.0 - h));
			box.SetMember("_x", GFx(left));
			box.SetMember("_y", GFx(top));
			box.SetMember("_xscale", GFx(k * 100.0));
			box.SetMember("_yscale", GFx(k * 100.0));
			if (o.vertical) {
				// the main column at the screen edge's side, the secret one beside it
				const double mainX = fromRight ? blockW - colW / 2 : colW / 2;
				const double secretX = fromRight ? colW / 2 : blockW - colW / 2;
				for (int i = 0; i < 3; ++i) {
					n.slots[i].clip.SetMember("_x", GFx(mainX));
					n.slots[i].clip.SetMember("_y", GFx(SLOT_S / 2 + i * vStep));
				}
				double y = SLOT_S / 2 + (3 - count) * vStep / 2;
				for (int i : { kHome, kVamp, kWolf }) {
					auto& sl = n.slots[i];
					sl.clip.SetMember("_x", GFx(secretX));
					sl.clip.SetMember("_y", GFx(y));
					if (r[i].on) {
						y += vStep;
					}
				}
			} else {
				for (int i = 0; i < 3; ++i) {
					n.slots[i].clip.SetMember("_x", GFx(SLOT_S / 2 + i * (SLOT_S + SLOT_GAP)));
					n.slots[i].clip.SetMember("_y", GFx(SLOT_S / 2));
				}
				double x = blockW / 2 - (count * SLOT_S + (count - 1) * SLOT_GAP) / 2 + SLOT_S / 2;
				for (int i : { kHome, kVamp, kWolf }) {
					auto& sl = n.slots[i];
					sl.clip.SetMember("_x", GFx(x));
					sl.clip.SetMember("_y", GFx(SECRET_DY + SLOT_S / 2));
					if (r[i].on) {
						x += SLOT_S + SLOT_GAP;
					}
				}
			}
		}

		// draw what changed; the times count down every tick
		std::vector<std::uint64_t> key;  // every slot's state, one entry each (1.0.4 review: no packing into one number)
		std::vector<int> levels;
		for (int i = 0; i < kSlots; ++i) {
			auto& s = n.slots[i];
			const auto& sr = r[i];
			const bool secret = i >= kHome;
			s.clip.SetMember("_alpha", GFx(secret && !sr.on ? 0.0 : 100.0));
			const int drawKey = sr.on ? 1 + sr.flavour * 8 + sr.level : 0;
			key.push_back(static_cast<std::uint64_t>(drawKey));
			levels.push_back(sr.on ? 0 : 1);  // a slot running out counts as "worse"
			if (drawKey != s.shownKey) {
				s.shownKey = drawKey;
				DrawFoodSlot(s.art, i, sr);
			}
			const int  secs = sr.on ? FoodMinutes(sr.left) : -1;
			const bool low = sr.on && sr.left < 60.0;
			if (secs != s.shownSecs || low != s.shownLow) {
				if (low != s.shownLow || s.shownSecs < 0) {
					GFx format;
					movie->CreateObject(&format, "TextFormat");
					format.SetMember("color", GFx(static_cast<double>(low ? F_TIME_LOW : F_TIME)));
					s.time.Invoke("setNewTextFormat", nullptr, &format, 1);
				}
				s.shownSecs = secs;
				s.shownLow = low;
				const std::string t = sr.on && sr.left > 0.0 ? FoodTime(sr.left) : std::string();
				if (s.time.IsDisplayObject()) {
					s.time.SetMember("text", MakeString(movie, t));  // already translated ("{} min", "<1 min")
					s.timeSh.SetMember("text", MakeString(movie, t));
				}
			}
		}
		// when to show: the whole group, on any slot filling or emptying
		const double alpha = AutoShowAlpha(n.autoShow, o, key, false, NowMs(), levels);
		if (alpha != n.shownAlpha) {
			n.shownAlpha = alpha;
			box.SetMember("_alpha", GFx(alpha));
		}
	}
#endif
