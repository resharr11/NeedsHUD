// ---- the vitality tile (2026-11-09, his call "lets do it"): Last Seed's Vitality, 0..160, higher = better, eight equal
// stages (Dying, Ruined, Weakened, Drained, Healthy, Very Healthy, Vigorous, Peak Condition). The bar reads like every
// other tile (good at the top, bad at the bottom): the pointer is where vitality is now, the small hollow marker where
// it is heading (Last Seed's _Seed_VitalityTarget: 160 minus what hunger, thirst, fatigue, cold and disease cost).

	// top (good) to bottom (bad): healthy green to blood red
	constexpr std::uint32_t VITALITY_COLS[5] = { 0x9DBE6A, 0x9BA557, 0xB08F4A, 0xA8603C, 0x8C3430 };

	// option A: a heart (his pick 2026-11-09, "less cartoony, more Skyrim"): an anatomical heart like Skyrim's Daedra
	// Heart and Briarheart, tilted, with the great vessels on top, muted blood tones, a few dark veins; it darkens and
	// dulls toward Dying
	void DrawVitalityHeart(GFx& g, double ox, double oy, int st, int n)
	{
		static constexpr std::uint32_t FLESH[5] = { 0x9C3B33, 0x8A3630, 0x74322D, 0x5C2E2A, 0x463029 };
		static constexpr std::uint32_t SHADE[5] = { 0x5E211D, 0x531F1C, 0x471E1B, 0x3A1D1A, 0x2E1D1A };
		const std::uint32_t flesh = RampColor(FLESH, st, n), shade = RampColor(SHADE, st, n);
		// the body: broad at the top right, tapering to an apex low on the left
		const auto body = [&]() {
			MoveTo(g, ox - 16, oy - 8);
			CurveTo(g, ox - 23, oy + 6, ox - 8, oy + 22);    // left side down to the rounded apex
			CurveTo(g, ox - 4, oy + 25, ox + 2, oy + 20);
			CurveTo(g, ox + 14, oy + 11, ox + 19, oy + 1);    // apex up the right side
			CurveTo(g, ox + 23, oy - 10, ox + 13, oy - 16);   // round shoulder (right atrium)
			CurveTo(g, ox + 5, oy - 20, ox - 3, oy - 14);     // top
			CurveTo(g, ox - 10, oy - 17, ox - 16, oy - 8);    // left atrium bump
		};
		// the great vessels: the aortic arch curling over, two stubs beside it
		const auto aorta = [&]() {
			MoveTo(g, ox + 1, oy - 14);
			CurveTo(g, ox - 2, oy - 31, ox + 11, oy - 29);
			CurveTo(g, ox + 17, oy - 28, ox + 16, oy - 21);
		};
		const auto stub = [&](double x0, double y0, double x1, double y1) {
			MoveTo(g, x0, y0);
			LineTo(g, x1, y1);
		};
		// vessels first (behind the body): dark edge, bone line, then the flesh tube on top
		const auto vessels = [&]() {
			aorta();
			stub(ox + 7, oy - 16, ox + 8, oy - 25);
			stub(ox - 8, oy - 13, ox - 15, oy - 22);   // the pulmonary trunk, leaning left
			stub(ox - 12, oy - 18, ox - 19, oy - 17);
		};
		Line(g, 7.5 + T_EDGE * 2, 0x000000, 70.0);
		vessels();
		Line(g, 7.5, T_BONE, 90.0);
		vessels();
		Line(g, 4.6, shade);
		vessels();
		NoLine(g);
		Line(g, T_SW + T_EDGE * 2, 0x000000, 70.0);
		body();
		Line(g, T_SW, T_BONE, 90.0);
		Fill(g, flesh);
		body();
		EndFill(g);
		NoLine(g);
		// shading along the left side and the groove between the chambers
		Fill(g, shade, 70.0);
		MoveTo(g, ox - 16, oy - 5);
		CurveTo(g, ox - 21, oy + 8, ox - 7, oy + 21);
		CurveTo(g, ox - 12, oy + 8, ox - 10, oy - 6);
		EndFill(g);
		Line(g, 1.4, shade, 70.0);
		MoveTo(g, ox + 3, oy - 13);
		CurveTo(g, ox + 7, oy + 0, ox - 2, oy + 19);  // the groove between the chambers, curving to the apex
		Line(g, 1.0, shade, 85.0);
		MoveTo(g, ox + 3, oy - 2);
		CurveTo(g, ox + 8, oy + 2, ox + 11, oy + 8);  // a vein on the right chamber
		MoveTo(g, ox - 1, oy + 4);
		CurveTo(g, ox + 3, oy + 9, ox + 4, oy + 14);
		NoLine(g);
		// a dull wet sheen on the right shoulder (no bright cartoon highlight)
		Fill(g, 0xE8C9B8, 28.0);
		EllipsePath(g, ox + 10, oy - 8, 4.0, 2.4, -0.8);
		EndFill(g);
	}

	// option B: a sprouting seed (Last Seed's own symbol): two leaves, green and raised when healthy, brown and drooping
	// when dying
	void DrawVitalitySprout(GFx& g, double ox, double oy, int st, int n)
	{
		const double droop = n > 1 ? static_cast<double>(st) / (n - 1) : 0.0;  // 0 healthy .. 1 dying
		static constexpr std::uint32_t LEAF[5] = { 0x7DB04E, 0x8AA145, 0x9C8E40, 0x8E6E3A, 0x6B5236 };
		const std::uint32_t leafCol = RampColor(LEAF, st, n);
		// the seed in the soil
		const auto soil = [&]() { EllipsePath(g, ox, oy + 19, 22, 5, 0.0); };
		const auto seed = [&]() { EllipsePath(g, ox, oy + 14, 8, 6, 0.0); };
		Line(g, T_SW + T_EDGE * 2, 0x000000, 70.0);
		soil();
		seed();
		Line(g, T_SW, T_BONE, 90.0);
		Fill(g, 0x4A3B2E);
		soil();
		EndFill(g);
		Fill(g, 0x8A6A4A);
		seed();
		EndFill(g);
		NoLine(g);
		// the stem, bending over as it droops
		const double tipX = ox + 10 * droop, tipY = oy - 14 + 10 * droop;
		const auto stem = [&]() {
			MoveTo(g, ox, oy + 9);
			CurveTo(g, ox - 2, oy - 4, tipX, tipY);
		};
		Line(g, 3.4 + T_EDGE * 2, 0x000000, 70.0);
		stem();
		Line(g, 3.4, leafCol);
		stem();
		NoLine(g);
		// two leaves from the tip: raised (healthy) or hanging (dying)
		const auto leaf = [&](double side) {
			const double lift = 14 - 26 * droop;  // the leaf's end above (+) or below (-) the tip
			const double ex = tipX + side * 17, ey = tipY - lift;
			MoveTo(g, tipX, tipY);
			CurveTo(g, tipX + side * 6, ey - 8, ex, ey);
			CurveTo(g, tipX + side * 12, ey + 6, tipX, tipY);
		};
		for (double side : { -1.0, 1.0 }) {
			Line(g, T_SW + T_EDGE * 2, 0x000000, 70.0);
			leaf(side);
			Line(g, T_SW, T_BONE, 90.0);
			Fill(g, leafCol);
			leaf(side);
			EndFill(g);
		}
		NoLine(g);
	}

	// option C: a candle, its flame tall when healthy and guttering when dying
	void DrawVitalityCandle(GFx& g, double ox, double oy, int st, int n)
	{
		const double life = n > 1 ? 1.0 - static_cast<double>(st) / (n - 1) : 1.0;  // 1 healthy .. 0 dying
		const auto wax = [&]() { RoundRectPath(g, ox - 8, oy - 4, 16, 26, 2.0); };
		const auto drip = [&]() { EllipsePath(g, ox - 5, oy + 2, 2.2, 5, 0.0); };
		Line(g, T_SW + T_EDGE * 2, 0x000000, 70.0);
		wax();
		Line(g, T_SW, T_BONE, 90.0);
		Fill(g, 0xD8CCAE);
		wax();
		EndFill(g);
		NoLine(g);
		Fill(g, 0xEFE6CF);
		drip();
		EndFill(g);
		// the wick
		Line(g, 1.6, 0x2B2724);
		MoveTo(g, ox, oy - 4);
		LineTo(g, ox, oy - 8);
		NoLine(g);
		// the flame: height with the life left, from a tall gold flame to a small dull ember
		const double h = 6 + 22 * life, w = 3.5 + 4 * life;
		const std::uint32_t outer = life > 0.4 ? 0xE8A33A : 0xA8603C;
		const auto flame = [&](double s) {
			MoveTo(g, ox, oy - 8 - h * s);
			CurveTo(g, ox + w * s * 1.6, oy - 8 - h * s * 0.35, ox, oy - 7);
			CurveTo(g, ox - w * s * 1.6, oy - 8 - h * s * 0.35, ox, oy - 8 - h * s);
		};
		Fill(g, outer, 95.0);
		flame(1.0);
		EndFill(g);
		Fill(g, 0xFFF1C2, life > 0.25 ? 95.0 : 40.0);
		flame(0.55);
		EndFill(g);
	}

	// the target marker: a small hollow chevron on the LEFT of the bar, pointing at it (the pointer on the right is
	// where vitality is now), so the two never cover each other
	void DrawVitalityTarget(GFx& g, double a_ty)
	{
		const double px = 24 - 3.5;  // just left of the bar
		const auto chevron = [&]() {
			MoveTo(g, px - 9, a_ty - 5.5);
			LineTo(g, px, a_ty);
			LineTo(g, px - 9, a_ty + 5.5);
		};
		Line(g, 2.2 + T_EDGE * 2, 0x000000, 70.0);
		chevron();
		Line(g, 2.2, T_BONE, 95.0);
		chevron();
		NoLine(g);
	}

	// a_value 0..160 (higher = better), a_target 0..160 (where it heads; < 0 none); a_icon 0 heart, 1 sprout, 2 candle
	void DrawVitalityTile(GFx& g, double a_value, double a_target, int a_icon)
	{
		constexpr int n = 8;
		const double v = std::clamp(a_value, 0.0, 160.0);
		const int st = std::clamp(static_cast<int>((160.0 - v) / 20.0), 0, n - 1);  // 0 Peak Condition .. 7 Dying
		g.Invoke("clear", nullptr, nullptr, 0);
		DrawTileFrame(g, st == n - 1);
		const double top = -26, bot = 26, lt = top + 2, lb = bot - 1;
		const auto y = [&](double a_v) { return lt + (160.0 - std::clamp(a_v, 0.0, 160.0)) / 160.0 * (lb - lt); };
		if (a_icon == 0) {
			DrawVitalityHeart(g, -16, 2, st, n);
		} else if (a_icon == 1) {
			DrawVitalitySprout(g, -16, 2, st, n);
		} else {
			DrawVitalityCandle(g, -16, 2, st, n);
		}
		DrawTileBar(g, lt, lb, VITALITY_COLS, n, y(v), -1, true);
		if (a_target >= 0.0) {
			DrawVitalityTarget(g, y(a_target));
		}
	}
