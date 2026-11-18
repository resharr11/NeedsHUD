// ---- the tiredness tile (2026-11-08): the approved NeedsHUD needs design (mockups\needshud_mockups_2026-11-04.zip,
// g3.py o_nightcap + g4.py o_nightcap2 + small_gauge + frame_box_iron), drawn live from C++ like the hunger tile.
// A sleepy face under a drooping nightcap, drawn as one outlined silhouette; the stage colour fills it from the
// gauge level down (the face empties as you tire); a small z from Tired, a bigger z from Exhausted. After a good sleep
// (Survival's rested bonus) the face is awake: eyes open, no z. Free Cooking's Refreshing drink while Drained shows as
// Refreshed (face and name) with the Drained block hollow in bronze and a bronze cup badge (mockups refresh.png).
// Survival's own values: Survival_ExhaustionNeedValue (0 rested .. its max global), stages Refreshed <160,
// Drained <340, Tired <560, Exhausted <800, Debilitated (Survival_NeedExhaustion.SetNeedStageValues); its rested
// bonus = one of Skyrim's Rested, Well Rested or Lover's Comfort spells (ApplyNormalRestedBonus).
// Same frame, bar and pointer as the hunger tile (HungerTile.inl); coordinates are the mockup's 1080p px around the
// tile centre.

	constexpr const char* TIRED_NAMES[5] = { "Refreshed", "Drained", "Tired", "Exhausted", "Debilitated" };
	// the tile's bonus states: 1-3 Survival's rested bonuses, 4 Free Cooking's Refreshing countering Drained
	constexpr const char* RESTED_NAMES[5] = { "", "Rested", "Well Rested", "Lover's Comfort", "Refreshed" };
	constexpr int REFRESHING = 4;
	constexpr std::uint32_t TIRED_COLS[5] = { 0xC8C4D6, 0xA7A4B3, 0x888492, 0x6C6975, 0x53515B };  // ramp(300, 10, ...)
	constexpr std::uint32_t T_FACE_LINE = 0x17191C;  // the eyes

	// the six stage bounds, the last one Survival's max (a global, so read it rather than assume 1000)
	void TiredBounds(double (&a_out)[6])
	{
		const double mx = (g_tiredMax && GV(g_tiredMax) > 800.0f) ? static_cast<double>(GV(g_tiredMax)) : 1000.0;  // vanilla 960
		double b[6] = { 0.0, 160.0, 340.0, 560.0, 800.0, mx };
		if (g_tiredBoundsSmi[0] >= 0.0 && g_tiredBoundsSmi[1] > 0.0) {  // Survival Mode Improved's own limits
			for (int i = 1; i < 6; ++i) {  // its limits and its max (578, not Survival's 960)
				b[i] = g_tiredBoundsSmi[i];
			}
		}
		std::copy(std::begin(b), std::end(b), std::begin(a_out));
	}

	// which rested bonus the player has: 0 none, 1 Rested, 2 Well Rested, 3 Lover's Comfort
	int RestedBonus()
	{
		if (SnapHasSpell(g_loversComfort)) {  // the main-thread snapshot (Snapshot.inl)
			return 3;
		}
		if (SnapHasSpell(g_wellRested)) {
			return 2;
		}
		if (SnapHasSpell(g_rested)) {
			return 1;
		}
		return 0;
	}

	// Free Cooking's Refreshing drink is working on the player (Free Cooking itself ends it at Tired or worse)
	bool FcRefreshing()
	{
		return SnapHasEffect(g_fcRefreshing);  // the main-thread snapshot (Snapshot.inl)
	}

	using Poly = std::vector<std::pair<double, double>>;

	Poly CirclePoly(double a_cx, double a_cy, double a_r, int a_n = 32)
	{
		Poly p;
		for (int i = 0; i < a_n; ++i) {
			const double a = 2.0 * std::numbers::pi * i / a_n;
			p.emplace_back(a_cx + a_r * std::cos(a), a_cy + a_r * std::sin(a));
		}
		return p;
	}

	void PolyPath(GFx& g, const Poly& a_p)
	{
		if (a_p.size() < 3) {
			return;
		}
		MoveTo(g, a_p[0].first, a_p[0].second);
		for (size_t i = 1; i < a_p.size(); ++i) {
			LineTo(g, a_p[i].first, a_p[i].second);
		}
		LineTo(g, a_p[0].first, a_p[0].second);
	}

	// Free Cooking's badge (m75.py cup_badge): a bronze ring with a steaming cup, at the tile's top right corner
	void CupBadge(GFx& g, double a_x, double a_y)
	{
		constexpr double s = 1.5;  // the mockup draws it scaled 1.5
		const auto P = [&](double dx, double dy) { return std::pair{ a_x + dx * s, a_y + dy * s }; };
		Line(g, 1.6 * s, T_BRONZE);
		Fill(g, 0x141210);
		CirclePath(g, a_x, a_y, 11 * s);
		EndFill(g);
		// the cup
		Line(g, 1.1 * s, 0x000000, 85.0);
		Fill(g, T_BONE);
		{
			auto [x, y] = P(-5, -2);
			MoveTo(g, x, y);
			for (const auto& [dx, dy] : { std::pair{ -4.0, 5.0 }, std::pair{ 3.0, 5.0 }, std::pair{ 4.0, -2.0 }, std::pair{ -5.0, -2.0 } }) {
				auto [px, py] = P(dx, dy);
				LineTo(g, px, py);
			}
		}
		EndFill(g);
		// the handle: "q3,0 2.5,2.5 q-.5,2 -3,2" from (4, 0)
		Line(g, 1.2 * s, T_BONE);
		{
			auto [x0, y0] = P(4, 0);
			auto [c1x, c1y] = P(7, 0);
			auto [x1, y1] = P(6.5, 2.5);
			auto [c2x, c2y] = P(6, 4.5);
			auto [x2, y2] = P(3.5, 4.5);
			MoveTo(g, x0, y0);
			CurveTo(g, c1x, c1y, x1, y1);
			CurveTo(g, c2x, c2y, x2, y2);
		}
		// two steam wisps: "c-1.5,-1.5 1.5,-3 0,-5" from (-2,-4) and (1.5,-4), as two quadratic pieces each
		Line(g, 1.0 * s, T_BONE, 70.0);
		for (const double wx : { -2.0, 1.5 }) {
			auto [x0, y0] = P(wx, -4);
			auto [c1x, c1y] = P(wx - 1.2, -5.2);
			auto [x1, y1] = P(wx, -6.5);
			auto [c2x, c2y] = P(wx + 1.2, -7.8);
			auto [x2, y2] = P(wx, -9);
			MoveTo(g, x0, y0);
			CurveTo(g, c1x, c1y, x1, y1);
			CurveTo(g, c2x, c2y, x2, y2);
		}
		NoLine(g);
	}

	// a_pos: 0..1 down the bar, a_st of a_n stages; a_bonus: 1-3 rested (awake face), REFRESHING when Free Cooking's
	// drink counters Drained (vanilla's stage 1)
	void DrawTiredTile(GFx& g, double a_pos, int a_st, int a_n, int a_bonus)
	{
		g.Invoke("clear", nullptr, nullptr, 0);
		const int  n = std::max(1, a_n);
		const bool refreshing = a_bonus == REFRESHING;  // a "fake Refreshed": drawn as Refreshed, the pointer stays true
		const bool rested = a_bonus > 0 && !refreshing;
		const int  st = refreshing ? 0 : std::clamp(a_st, 0, n - 1);
		DrawTileFrame(g, !rested && st == n - 1);

		// the nightcap face (o_nightcap): ox, top, bot as in small_gauge
		const double cx = 0.0, cy = 0.0;
		const double ox = cx - 12, t = cy - 26, b = cy + 26;
		const double lt = t + 2, lb = b - 1;  // RANGE['fatigue'] (g4.py): where the level moves
		const double fy = lt + (rested ? 0.0 : std::clamp(a_pos, 0.0, 1.0)) * (lb - lt);      // the pointer
		const double faceY = refreshing ? lt + (0.75 / n) * (lb - lt) : fy;                 // the face's level (m75.py: 120)

		// the silhouette's three parts: the face, the cap, the pompom
		Poly cap{ { ox - 16, b - 22 } };
		QuadPoints(cap, ox - 16, b - 22, ox - 14, t + 4, ox + 8, t + 2, 12);
		QuadPoints(cap, ox + 8, t + 2, ox + 22, t + 2, ox + 24, t + 16, 10);
		QuadPoints(cap, ox + 24, t + 16, ox + 12, t + 10, ox + 12, b - 24, 10);
		const Poly parts[3] = { CirclePoly(ox, b - 15, 14), cap, CirclePoly(ox + 24, t + 18, 4.5, 16) };

		// silhouette(): all the dark edges first, then the bright outlines, then the dark insides, so the three parts
		// read as one shape; then the stage colour below the level
		Line(g, T_SW * 2 + T_EDGE * 2, 0x000000, 70.0);
		for (const auto& p : parts) {
			PolyPath(g, p);
		}
		for (const auto& p : parts) {
			Line(g, T_SW * 2, T_BONE);
			Fill(g, T_BONE);
			PolyPath(g, p);
			EndFill(g);
		}
		NoLine(g);
		for (const auto& p : parts) {
			Fill(g, T_INSIDE);
			PolyPath(g, p);
			EndFill(g);
		}
		for (const auto& p : parts) {
			const auto low = BelowY(p, faceY);
			if (low.size() >= 3) {
				Fill(g, RampColor(TIRED_COLS, rested ? 0 : st, n));
				PolyPath(g, low);
				EndFill(g);
			}
		}

		// the cap's brim over the forehead
		Edged(g, [&]() {
			MoveTo(g, ox - 17, b - 21);
			CurveTo(g, ox, b - 26, ox + 15, b - 23);
		}, 2.2);

		// eyes (his calls 2026-11-10): the first stage is awake (round eyes with a small shine; his "too cute, it's still Skyrim": no blush or smile), the
		// second starts to close (heavy half-lids), the later stages are closed (sleepy curves); a rested bonus is awake
		const int eyes = (rested || st == 0) ? 0 : (st == 1 && n >= 3) ? 1 : 2;
		for (const double d : { -1.0, 1.0 }) {
			const double ex = ox + d * 7, ey = b - 14;
			if (eyes == 0) {
				NoLine(g);
				Fill(g, T_FACE_LINE);
				EllipsePath(g, ex, ey, 2.4, 3.0, 0.0);
				EndFill(g);
				Fill(g, 0xFFFFFF, 90.0);
				CirclePath(g, ex + 0.8, ey - 1.1, 0.9);
				EndFill(g);
			} else if (eyes == 1) {
				// half closed: the lower half of the eye under a heavy, slightly drooping lid
				NoLine(g);
				Fill(g, T_FACE_LINE);
				MoveTo(g, ex - 2.4, ey - 0.4);
				CurveTo(g, ex, ey + 5.6, ex + 2.4, ey - 0.4);
				LineTo(g, ex - 2.4, ey - 0.4);
				EndFill(g);
				Line(g, 1.8, T_FACE_LINE);   // the heavy lid: a soft arc, level (no frown)
				MoveTo(g, ex - 3.2, ey);
				CurveTo(g, ex, ey - 1.8, ex + 3.2, ey);
				NoLine(g);
			} else {
				Line(g, 2.2, T_FACE_LINE);
				MoveTo(g, ex - 4, ey);
				CurveTo(g, ex, ey + 3, ex + 4, ey);
				NoLine(g);
			}
		}

		// z: small at the third stage from the worst (vanilla Tired, o_nightcap2), bigger at the last two (vanilla
		// Exhausted and Debilitated, o_nightcap); with six stages: Tired, then Weary and Exhausted
		if (!rested && st == n - 3) {
			const double x = ox - 27, y = t + 14;
			Edged(g, [&]() {
				MoveTo(g, x, y);
				LineTo(g, x + 5, y);
				LineTo(g, x, y + 5);
				LineTo(g, x + 5, y + 5);
			}, 1.6);
		} else if (!rested && st >= n - 2 && n >= 3) {
			const double x = ox - 28, y = t + 12;
			Edged(g, [&]() {
				MoveTo(g, x, y);
				LineTo(g, x + 7, y);
				LineTo(g, x, y + 7);
				LineTo(g, x + 7, y + 7);
			}, 1.8);
		}

		DrawTileBar(g, lt, lb, TIRED_COLS, n, fy, refreshing ? 1 : -1);
		if (refreshing) {
			CupBadge(g, cx + 58, cy - 40);
		}
	}
