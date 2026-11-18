// ---- the dirt tile (2026-11-09, his "check the most popular bathing and dirty mods", then "keep it clean installed"):
// the approved 2026-11-04 design (mockups\needshud_mockups_2026-11-04.zip, g4.py o_tunic4 + small_gauge +
// frame_box_iron), drawn live from C++ like the other tiles. A medieval tunic (long sleeves, flared body, laced V collar,
// belt): the clean linen drains from the collar down as you get dirtier, smudges spread stage by stage (2, 4, 6), stench
// lines rise from the third stage (one), three at the worst. His picks: 7th tile of the Needs row; Keep It Clean's own
// words shortened: Clean, Not Fresh, Dirty, Very Dirty. Coordinates are the mockup's 1080p px around the tile centre.

	constexpr std::uint32_t DIRT_COLS[5] = { 0xD9CAB3, 0xB9AC97, 0x9D927F, 0x807665, 0x665D4F };  // ramp(85, 14, ...) (g4 muted)
	constexpr std::uint32_t T_GRIME = 0x4A3A26, T_SMUDGE = 0x3A2C1C, T_BELT = 0x6B4A2C, T_STENCH = 0xB9B48A;
	constexpr std::uint32_t T_BLOOD = 0x7A1A14;  // Dirt and Blood's bloodstains: the smudges turn dark blood red

	// the part of a closed polygon under a wavy line (the clean linen below the grime), the surface followed between
	// the places the outline crosses it (as the waterskin's water)
	template <class WaveFn>
	Poly BelowWave(const Poly& a_poly, WaveFn a_wave)
	{
		// the outline in short steps, so the crossings are found close
		Poly fine;
		for (size_t i = 0; i < a_poly.size(); ++i) {
			const auto& p = a_poly[i];
			const auto& q = a_poly[(i + 1) % a_poly.size()];
			const double l = std::hypot(q.first - p.first, q.second - p.second);
			const int    k = std::max(1, static_cast<int>(l / 1.0));
			for (int j = 0; j < k; ++j) {
				fine.emplace_back(p.first + (q.first - p.first) * j / k, p.second + (q.second - p.second) * j / k);
			}
		}
		Poly pts;
		const size_t m = fine.size();
		const auto under = [&](const std::pair<double, double>& p) { return p.second >= a_wave(p.first); };
		for (size_t i = 0; i < m; ++i) {
			const auto& p = fine[i];
			const auto& q = fine[(i + 1) % m];
			const bool pin = under(p), qin = under(q);
			if (pin) {
				pts.push_back(p);
			}
			if (pin != qin) {
				double lo = 0.0, hi = 1.0;
				for (int it = 0; it < 20; ++it) {
					const double mid = (lo + hi) / 2;
					const std::pair<double, double> r{ p.first + (q.first - p.first) * mid, p.second + (q.second - p.second) * mid };
					(under(r) == pin ? lo : hi) = mid;
				}
				const std::pair<double, double> c{ p.first + (q.first - p.first) * lo, p.second + (q.second - p.second) * lo };
				pts.push_back(c);
				if (pin && !qin) {
					size_t j = (i + 1) % m;
					for (size_t steps = 0; steps < m && !under(fine[j]); ++steps) {
						j = (j + 1) % m;
					}
					const double xa = c.first, xb = fine[j].first;
					const int    ns = std::max(2, static_cast<int>(std::abs(xb - xa) / 0.8));
					for (int k = 1; k < ns; ++k) {
						const double x = xa + (xb - xa) * k / ns;
						pts.emplace_back(x, a_wave(x));
					}
				}
			}
		}
		return pts;
	}

	// a_pos: 0..1 down the bar (how dirty), a_st of a_n stages; a_bloody: Dirt and Blood's bloodstains (blood-red smudges,
	// at least two)
	void DrawDirtTile(GFx& g, double a_pos, int a_st, int a_n, bool a_bloody = false)
	{
		g.Invoke("clear", nullptr, nullptr, 0);
		const int n = std::max(1, a_n);
		const int st = std::clamp(a_st, 0, n - 1);
		DrawTileFrame(g, st == n - 1);

		const double cx = 0.0, cy = 0.0;
		const double ox = cx - 12, top = cy - 26, bot = cy + 26;
		const double lt = top + 5, lb = bot - 2;  // RANGE['dirt'] (g4.py): where the clean line moves
		const double fy = lt + std::clamp(a_pos, 0.0, 1.0) * (lb - lt);
		const double t = top + 3, b = bot;
		// the four looks of the approved design (clean, then 2, 4, 6 smudges with stench from the third), spread over the
		// mod's stage count
		const int look = n <= 1 ? 0 : static_cast<int>(std::lround(3.0 * st / (n - 1)));

		const Poly body{ { ox - 7, t }, { ox + 7, t }, { ox + 15, t + 3 }, { ox + 27, t + 22 }, { ox + 21, t + 26 }, { ox + 13, t + 14 },
			{ ox + 13, t + 28 }, { ox + 17, b }, { ox - 17, b }, { ox - 13, t + 28 }, { ox - 13, t + 14 }, { ox - 21, t + 26 },
			{ ox - 27, t + 22 }, { ox - 15, t + 3 } };
		NoLine(g);
		Fill(g, T_GRIME);
		PolyPath(g, body);
		EndFill(g);

		// the clean linen under a wavy line (five humps 12 px wide from x = ox-30, the mockup's Q curves +-3)
		const auto wave = [&](double a_x) {
			const double u = (a_x - (ox - 30)) / 12.0;
			const int    k = static_cast<int>(std::floor(u));
			const double f = u - k;
			return fy + ((k % 2) ? 3.0 : -3.0) * 2.0 * f * (1.0 - f);
		};
		const Poly linen = BelowWave(body, wave);
		if (linen.size() >= 3) {
			Fill(g, RampColor(DIRT_COLS, st, n));
			PolyPath(g, linen);
			EndFill(g);
		}

		// smudges (inside the tunic)
		static constexpr double BLOBS[6][5] = { { -6, 20, 5, 3.5, -20 }, { 6, 33, 5, 3.2, 25 }, { -20, 21, 3, 2.2, 40 }, { 4, 12, 4, 2.6, -35 },
			{ -4, 41, 5.5, 2.8, 5 }, { 21, 21, 3, 2.2, -40 } };
		static constexpr int SMUDGES[4] = { 0, 2, 4, 6 };
		const int smudges = a_bloody ? std::max(2, SMUDGES[look]) : SMUDGES[look];
		for (int i = 0; i < smudges; ++i) {
			const auto& s = BLOBS[i];
			Fill(g, a_bloody ? T_BLOOD : T_SMUDGE, a_bloody ? 90.0 : 75.0);
			EllipsePath(g, ox + s[0], t + s[1], s[2], s[3], s[4] * std::numbers::pi / 180.0);
			EndFill(g);
		}

		// the outline, the V collar and its laces, the belt
		Edged(g, [&]() { PolyPath(g, body); });
		Line(g, 1.6, 0x000000, 70.0);
		MoveTo(g, ox - 7, t);
		LineTo(g, ox, t + 10);
		LineTo(g, ox + 7, t);
		Line(g, 1.1, 0x000000, 70.0);
		MoveTo(g, ox - 3, t + 3);
		LineTo(g, ox + 3, t + 6);
		MoveTo(g, ox + 3, t + 3);
		LineTo(g, ox - 3, t + 6);
		Line(g, 2.4, T_BELT);
		MoveTo(g, ox - 13.5, t + 28);
		LineTo(g, ox + 13.5, t + 28);
		NoLine(g);

		// stench: wiggly lines rising off the tunic (one at the third look, three at the worst)
		if (look >= 2) {
			const std::vector<double> xs = look == 2 ? std::vector<double>{ ox } : std::vector<double>{ ox - 14, ox, ox + 14 };
			for (double x : xs) {
				const double y0 = x == ox ? t + 1 : t + 5;
				Poly w{ { x, y0 } };
				CubicPoints(w, x, y0, x - 3.5, y0 - 2.5, x + 3.5, y0 - 4.5, x, y0 - 7, 8);
				CubicPoints(w, x, y0 - 7, x - 3.5, y0 - 9.5, x + 3.5, y0 - 11.5, x, y0 - 14, 8);
				const auto path = [&]() {
					MoveTo(g, w[0].first, w[0].second);
					for (size_t i = 1; i < w.size(); ++i) {
						LineTo(g, w[i].first, w[i].second);
					}
				};
				Line(g, 1.8 + T_EDGE * 2.0, 0x000000, 70.0);
				path();
				Line(g, 1.8, T_STENCH);
				path();
			}
			NoLine(g);
		}

		DrawTileBar(g, lt, lb, DIRT_COLS, n, fy);
	}

#ifndef NH_DRAW_ONLY
	// ---- reading Keep It Clean (read 2026-11-09 from Keep It Clean.esp + SBBathBeingDirtyMGEFScript.psc): dirt is the hours
	// since the last wash, (now + its dirty-place extra time - LastWashingTime) x 24, with its own MCM thresholds: Clean up
	// to the soap's length (4 h), then "no longer fresh", Dirty (16 h), Very Dirty (24 h). Nothing runs until the first
	// wash (LastWashingTime 0), and neither does the tile.
	constexpr const char* KIC_FILE = "Keep It Clean.esp";
	RE::TESGlobal* g_kicWashed = nullptr;    // SBBathLastWashingTimeGLOBAL (days)
	RE::TESGlobal* g_kicExtra = nullptr;     // SBBATHLocationDirtyTimeVariation (days added by dirty places)
	RE::TESGlobal* g_kicLimits[3] = {};      // SoapEffectLength, TimetobeDirty, TimetobeVeryDirty (hours)

	void FindKeepItClean(RE::TESDataHandler* a_data)
	{
		if (!ModActive(a_data, KIC_FILE)) {
			return;
		}
		g_kicWashed = LookupGlobal(a_data, 0x0EA4EE, KIC_FILE);
		g_kicExtra = LookupGlobal(a_data, 0x253785, KIC_FILE);
		g_kicLimits[0] = LookupGlobal(a_data, 0x0F314B, KIC_FILE);
		g_kicLimits[1] = LookupGlobal(a_data, 0x0F314C, KIC_FILE);
		g_kicLimits[2] = LookupGlobal(a_data, 0x16F9F6, KIC_FILE);
		logger::info("Keep It Clean: last wash {}, limits {}/{}/{}", g_kicWashed ? "found" : "MISSING", g_kicLimits[0] ? "found" : "MISSING",
			g_kicLimits[1] ? "found" : "MISSING", g_kicLimits[2] ? "found" : "MISSING");
	}

	struct DirtRead
	{
		bool   ok = false;
		double hours = 0.0;
		double bounds[5]{};  // Clean, Not Fresh, Dirty, Very Dirty (the last block as long as Dirty's, then full)
	};

	DirtRead ReadDirt()
	{
		DirtRead r;
		const double days = DaysPassed();
		if (!g_kicWashed || days < 0.0 || GV(g_kicWashed) <= 0.0f) {
			return r;  // not started (washed once) or not installed
		}
		const double now = days + (g_kicExtra ? GV(g_kicExtra) : 0.0f);
		const double soap = g_kicLimits[0] ? GV(g_kicLimits[0]) : 4.0;
		const double dirty = std::max(soap, g_kicLimits[1] ? static_cast<double>(GV(g_kicLimits[1])) : 16.0);
		const double very = std::max(dirty, g_kicLimits[2] ? static_cast<double>(GV(g_kicLimits[2])) : 24.0);
		r.ok = true;
		r.hours = std::max(0.0, (now - GV(g_kicWashed)) * 24.0);
		const double b[5] = { 0.0, soap, dirty, very, very + std::max(1.0, very - dirty) };
		std::copy(std::begin(b), std::end(b), std::begin(r.bounds));
		return r;
	}

	// ---- reading Dirt and Blood - Dynamic Visuals (Nexus SE 38886; read 2026-11-09 from its ESL + Dirty_Script.psc): the
	// player always has one of nine abilities: Recently washed, Slightly / Somewhat / Quite Dirty, Covered in Dirt (one step
	// every 12-96 random game hours, its MCM), or the bloody version of a dirt level (a weapon hit turns dirt level n into
	// Bloodstains n, blood n into blood n+1; blood fades back into dirt a level worse). No hours to read: the stage is the level.
	constexpr const char* DNB_FILE = "Dirt and Blood - Dynamic Visuals.esp";
	RE::SpellItem* g_dnbDirt[5] = {};   // Clean 0x80C, Dirt1-3 0x806-0x808, Dirt4 0x838
	RE::SpellItem* g_dnbBlood[5] = {};  // [1..4]: Blood1-3 0x809-0x80B, Blood4 0x839

	void FindDirtAndBlood(RE::TESDataHandler* a_data)
	{
		if (!ModActive(a_data, DNB_FILE)) {
			return;
		}
		constexpr RE::FormID DIRT[5] = { 0x80C, 0x806, 0x807, 0x808, 0x838 };
		constexpr RE::FormID BLOOD[5] = { 0, 0x809, 0x80A, 0x80B, 0x839 };
		int found = 0;
		for (int i = 0; i < 5; ++i) {
			g_dnbDirt[i] = a_data->LookupForm<RE::SpellItem>(DIRT[i], DNB_FILE);
			g_dnbBlood[i] = BLOOD[i] ? a_data->LookupForm<RE::SpellItem>(BLOOD[i], DNB_FILE) : nullptr;
			found += (g_dnbDirt[i] ? 1 : 0) + (g_dnbBlood[i] ? 1 : 0);
		}
		logger::info("Dirt and Blood: {} of 9 dirt and blood spells found", found);
	}

	struct DnbRead
	{
		bool        ok = false;
		int         level = 0;       // 0 clean .. 4 covered
		bool        bloody = false;
		std::string name;            // the mod's own name of that state (from the game: any language)
	};

	// ---- reading Bathing in Skyrim - Renewed 2.7.11 (read 2026-11-09 from its ESL + .pex, no sources): the player's dirt is
	// mzinDirtinessPercentage 0..1 (mzinBatheQuest.UpdateActorDirtPercent writes it each dirt update, every
	// mzinDirtinessUpdateInterval game hours), five tiers from MCM thresholds (value >= threshold): Clean 0, Not Dirty 0.2,
	// Slightly Dirty 0.6, Quite Dirty 0.98, Filthy 1.0; runs while mzinBathingInSkyrimEnabled is 1
	constexpr const char* BIS_FILE = "Bathing in Skyrim.esp";
	RE::TESGlobal* g_bisOn = nullptr;        // mzinBathingInSkyrimEnabled 0x00C
	RE::TESGlobal* g_bisDirt = nullptr;      // mzinDirtinessPercentage 0xDA8
	RE::TESGlobal* g_bisLimits[5] = {};      // mzinDirtinessThresholdTier0-4: 0x032, 0xDAA, 0xDAB, 0xDAC, 0x06F
	std::string    g_bisNames[5];            // its tier spells' names (any language)

	void FindBathingInSkyrim(RE::TESDataHandler* a_data)
	{
		if (!ModActive(a_data, BIS_FILE)) {
			return;
		}
		g_bisOn = LookupGlobal(a_data, 0x00C, BIS_FILE);
		g_bisDirt = LookupGlobal(a_data, 0xDA8, BIS_FILE);
		constexpr RE::FormID LIMITS[5] = { 0x032, 0xDAA, 0xDAB, 0xDAC, 0x06F };
		constexpr RE::FormID TIERS[5] = { 0x043, 0x044, 0x045, 0x046, 0x06D };  // mzinDirtinessTier0-4Spell
		static constexpr const char* FALLBACK[5] = { "Clean", "Not Dirty", "Slightly Dirty", "Quite Dirty", "Filthy" };
		int found = 0;
		for (int i = 0; i < 5; ++i) {
			g_bisLimits[i] = LookupGlobal(a_data, LIMITS[i], BIS_FILE);
			found += g_bisLimits[i] ? 1 : 0;
			const auto spell = a_data->LookupForm<RE::SpellItem>(TIERS[i], BIS_FILE);
			g_bisNames[i] = (spell && spell->GetName() && *spell->GetName()) ? spell->GetName() : FALLBACK[i];
		}
		logger::info("Bathing in Skyrim: switch {}, dirt {}, {} of 5 tier limits found", g_bisOn ? "found" : "MISSING", g_bisDirt ? "found" : "MISSING", found);
	}

	bool BathingInSkyrimOn() { return g_bisDirt && (!g_bisOn || GV(g_bisOn) >= 1.0f); }

	DnbRead ReadDirtAndBlood()
	{
		DnbRead r;
		const auto player = RE::PlayerCharacter::GetSingleton();
		if (!player || !g_dnbDirt[0]) {
			return r;
		}
		for (int i = 4; i >= 0 && !r.ok; --i) {  // the worst one, should a script leave two behind
			for (bool blood : { true, false }) {
				RE::SpellItem* s = blood ? g_dnbBlood[i] : g_dnbDirt[i];
				if (s && SnapHasSpell(s)) {
					r.ok = true;
					r.level = i;
					r.bloody = blood;
					r.name = s->GetName() ? s->GetName() : "";
					break;
				}
			}
		}
		return r;
	}
#endif
