// ---- the frostbite tile (2026-11-09, his idea: "pretend it's a disease but blue"; "they are several small debuffs
// that pile up so maybe our meter should be how many of them are afflicted"). Frostfall's frostbite is four separate
// 5-minute debuffs (head, body, hands, feet; read from Frostfall.esp / _frost_exposuresystem.psc): the bar counts how
// many are running (1-4 blocks), the frame turns red at all four. Icy blues instead of the disease's bile greens.

	constexpr std::uint32_t FROSTBITE_COLS[5] = { 0xBFE3F2, 0x93C6E2, 0x6AA3CC, 0x4A7FB2, 0x335E92 };
	constexpr std::uint32_t T_SKIN = 0xB08C70, T_SKIN_DK = 0x7E6250, T_ICE = 0xF2FBFF;

	// option A: an open hand, palm toward the viewer; one finger frosted per frostbitten body part (fingertips go
	// pale ice blue, darker at the very tip, with a glint)
	void DrawFrostHand(GFx& g, double ox, double oy, int a_parts, int a_n, bool a_fingersShowCount = true)
	{
		constexpr double K = 1.32;  // the hand fills the icon space like the plague mask
		struct Fg { double x, len, w; };
		const Fg f[4] = { { -12.5 * K, 24 * K, 7.2 * K }, { -4.8 * K, 27 * K, 7.4 * K }, { 2.9 * K, 24.5 * K, 7.2 * K }, { 10.3 * K, 19 * K, 6.6 * K } };  // index..pinky
		const double palmTop = oy - 4 * K, palmBot = oy + 20 * K;
		const auto palm = [&]() { RoundRectPath(g, ox - 16.5 * K, palmTop, 31 * K, palmBot - palmTop, 8 * K); };
		const auto finger = [&](int i) { RoundRectPath(g, ox + f[i].x - f[i].w / 2, palmTop - f[i].len + 6 * K, f[i].w, f[i].len, f[i].w / 2); };
		const auto thumb = [&]() {
			MoveTo(g, ox - 14 * K, oy + 14 * K);
			CurveTo(g, ox - 24 * K, oy + 8 * K, ox - 27 * K, oy - 2 * K);
			CurveTo(g, ox - 28 * K, oy - 7 * K, ox - 23 * K, oy - 7 * K);
			CurveTo(g, ox - 19 * K, oy - 6 * K, ox - 16 * K, oy + 1 * K);
			LineTo(g, ox - 14 * K, oy + 14 * K);
		};
		// dark edge, bone line, skin fill (same treatment as the other tile icons)
		Line(g, T_SW + T_EDGE * 2, 0x000000, 70.0);
		for (int i = 0; i < 4; ++i) finger(i);
		thumb();
		palm();
		Line(g, T_SW, T_BONE, 90.0);
		Fill(g, T_SKIN);
		for (int i = 0; i < 4; ++i) finger(i);
		EndFill(g);
		Fill(g, T_SKIN);
		thumb();
		EndFill(g);
		NoLine(g);
		Fill(g, T_SKIN);
		palm();
		EndFill(g);
		// palm creases
		Line(g, 1.0, T_SKIN_DK, 90.0);
		MoveTo(g, ox - 12 * K, oy + 4 * K);
		CurveTo(g, ox - 2 * K, oy + 9 * K, ox + 10 * K, oy + 3 * K);
		MoveTo(g, ox - 10 * K, oy + 12 * K);
		CurveTo(g, ox - 3 * K, oy + 10 * K, ox + 4 * K, oy + 15 * K);
		NoLine(g);
		// frost on the fingertips: pinky first (the order doesn't mean a body part, just "how many")
		const int frosted = a_fingersShowCount ? std::clamp(a_parts + 1, 0, 4) : 4;
		for (int k = 0; k < frosted; ++k) {
			const int i = 3 - k;
			const double top = palmTop - f[i].len + 6 * K, h = f[i].len * 0.62, w = f[i].w - 1.6, x = ox + f[i].x - w / 2;
			Fill(g, T_ICE, 100.0);
			RoundRectPath(g, x, top + 0.8, w, h, w / 2);
			EndFill(g);
			Fill(g, RampColor(FROSTBITE_COLS, a_parts, a_n));
			RoundRectPath(g, x, top + 0.8, w, h * 0.42, w / 2);
			EndFill(g);
			Fill(g, 0xFFFFFF, 80.0);
			CirclePath(g, x + w * 0.32, top + 3.6, 1.4);
			EndFill(g);
		}
	}

	// a six-armed snowflake with side twigs (options B and C)
	void DrawSnowflake(GFx& g, double cx, double cy, double r, std::uint32_t a_col)
	{
		const auto arms = [&](double w, std::uint32_t c, double a) {
			Line(g, w, c, a);
			for (int k = 0; k < 6; ++k) {
				const double t = k * std::numbers::pi / 3.0 - std::numbers::pi / 2, dx = std::cos(t), dy = std::sin(t);
				MoveTo(g, cx, cy);
				LineTo(g, cx + dx * r, cy + dy * r);
				for (double p : { 0.5, 0.75 }) {
					const double bx = cx + dx * r * p, by = cy + dy * r * p, tw = r * (p < 0.6 ? 0.32 : 0.22);
					for (double s : { -1.0, 1.0 }) {
						const double u = t + s * 0.9;
						MoveTo(g, bx, by);
						LineTo(g, bx + std::cos(u) * tw, by + std::sin(u) * tw);
					}
				}
			}
		};
		arms(2.6 + T_EDGE * 2, 0x000000, 70.0);
		arms(2.6, a_col, 100.0);
		NoLine(g);
		Fill(g, a_col);
		CirclePath(g, cx, cy, r * 0.16);
		EndFill(g);
	}

	// a_icon: 0 hand with frosted fingers (count), 1 hand + snowflake, 2 snowflake only; a_parts 0..3 = 1..4 parts
	void DrawFrostbiteTile(GFx& g, int a_parts, int a_icon)
	{
		const int n = 4, st = std::clamp(a_parts, 0, n - 1);
		g.Invoke("clear", nullptr, nullptr, 0);
		DrawTileFrame(g, st == n - 1);
		const double top = -26, bot = 26, lt = top + 2, lb = bot - 1;
		const double fy = lt + (st + 0.5) / n * (lb - lt);
		if (a_icon == 0) {
			DrawFrostHand(g, -9, 3, st, n, true);
		} else if (a_icon == 1) {
			DrawFrostHand(g, -12, 3, st, n, false);
			DrawSnowflake(g, 7, 13, 11, RampColor(FROSTBITE_COLS, st, n));
		} else {
			DrawSnowflake(g, -8, 0, 22, RampColor(FROSTBITE_COLS, st, n));
		}
		DrawTileBar(g, lt, lb, FROSTBITE_COLS, n, fy, -1, false);
	}

#ifndef NH_DRAW_ONLY  // (the drawing test harness renders the tile without the game)
	// ---- reading Frostfall's frostbite (Frostfall.esp; read 2026-11-09 from _frost_exposuresystem.psc GetFrostbite): four
	// "potions" equipped on the player at Freezing or worse, each with its own keyword on its effects; 300 s each, no cure.
	// Frostfall itself checks them with HasEffectKeyword, so do we (any mod reusing its keywords counts too).
	constexpr const char* FF_FILE = "Frostfall.esp";
	RE::BGSKeyword* g_ffBite[4] = {};  // hands, feet, head, body (the order the line names them)
	constexpr const char* FF_BITE_NAMES[4] = { "Hands", "Feet", "Head", "Body" };

	void FindFrostbite(RE::TESDataHandler* a_data)
	{
		if (!ModActive(a_data, FF_FILE)) {
			return;
		}
		constexpr RE::FormID ids[4] = { 0x069154, 0x069155, 0x069156, 0x069153 };  // _Frost_Frostbite{Hands,Feet,Head,Body}KW
		int found = 0;
		for (int i = 0; i < 4; ++i) {
			g_ffBite[i] = a_data->LookupForm<RE::BGSKeyword>(ids[i], FF_FILE);
			found += g_ffBite[i] ? 1 : 0;
		}
		logger::info("Frostfall frostbite: {} of 4 keywords found", found);
	}

	struct FrostbiteRead
	{
		bool        ok = false;
		int         parts = 0;     // 1..4
		std::string line2;         // "Hands, feet · 4 min"
	};

	FrostbiteRead ReadFrostbite()
	{
		FrostbiteRead r;
		if (!g_ffBite[0]) {
			return r;
		}
		bool   on[4] = {};
		double left = 0.0;  // seconds until the newest one wears off
		for (const auto& e : g_view.effects) {  // the main-thread snapshot (Snapshot.inl)
			if (e.dispelled || !e.mgef) {
				continue;
			}
			for (int i = 0; i < 4; ++i) {
				if (g_ffBite[i] && e.mgef->HasKeyword(g_ffBite[i])) {  // effect records live for the whole session
					on[i] = true;
					if (e.duration > 0.0f) {
						left = std::max(left, static_cast<double>(e.duration - e.elapsed));
					}
				}
			}
		}
		std::string parts;
		for (int i = 0; i < 4; ++i) {
			if (on[i]) {
				++r.parts;
				std::string w = tr::T(std::string(FF_BITE_NAMES[i]));
				if (!parts.empty()) {
					w[0] = static_cast<char>(std::tolower(static_cast<unsigned char>(w[0])));
					parts += ", ";
				}
				parts += w;
			}
		}
		if (r.parts == 0) {
			return r;
		}
		r.ok = true;
		if (r.parts == 4) {
			parts = tr::T(std::string("All four"));
		}
		const int mins = std::max(1, static_cast<int>(std::ceil(left / 60.0)));
		r.line2 = left > 0.0 ? tr::F("{} \xC2\xB7 {} min", parts, mins) : parts;
		return r;
	}
#endif
