// ---- the drunk tile (2026-11-09, his "add both" after seeing RND's own widgets): how drunk you are, from the needs mod's
// own alcohol value (Realistic Needs and Diseases: Sober, Dizzy, Drunk, Wasted). A wooden tankard with iron bands and a
// foam head that tips over more each stage (upright, then 12, 26 and 45 degrees), dizzy swirls above it (none, one, two,
// three) and ale spilling from the rim at the worst stage. Shown only while there is drink in you. Coordinates are the
// mockup's 1080p px around the tile centre, like the other tiles.

	constexpr std::uint32_t DRUNK_COLS[5] = { 0xE3CF9A, 0xD3B06A, 0xBD8E4C, 0x9E6C37, 0x7D4D27 };  // pale ale to dark amber
	constexpr std::uint32_t K_WOOD = 0x8A6440, K_WOOD_DARK = 0x5E4229, K_BAND = 0x77736B, K_FOAM = 0xEFE8D6, K_ALE = 0xC98F3A,
							K_SWIRL = 0xE9DFA8;

	// a polygon turned by a_deg degrees (clockwise on screen) around (a_px, a_py)
	Poly Turned(const Poly& a_p, double a_px, double a_py, double a_deg)
	{
		const double a = a_deg * std::numbers::pi / 180.0, c = std::cos(a), s = std::sin(a);
		Poly out;
		out.reserve(a_p.size());
		for (const auto& [x, y] : a_p) {
			const double dx = x - a_px, dy = y - a_py;
			out.emplace_back(a_px + dx * c - dy * s, a_py + dx * s + dy * c);
		}
		return out;
	}

	// a small spiral (the cartoon "dizzy" mark), a_turns turns out from the centre
	void Swirl(GFx& g, double a_cx, double a_cy, double a_r, double a_rot)
	{
		Poly w;
		const int steps = 26;
		for (int i = 0; i <= steps; ++i) {
			const double t = static_cast<double>(i) / steps;
			const double a = a_rot + t * 2.0 * std::numbers::pi * 1.6;
			const double r = a_r * (0.15 + 0.85 * t);
			w.emplace_back(a_cx + r * std::cos(a), a_cy + r * 0.8 * std::sin(a));
		}
		const auto path = [&]() {
			MoveTo(g, w[0].first, w[0].second);
			for (size_t i = 1; i < w.size(); ++i) {
				LineTo(g, w[i].first, w[i].second);
			}
		};
		Line(g, 1.5 + T_EDGE * 2.0, 0x000000, 70.0);
		path();
		Line(g, 1.5, K_SWIRL);
		path();
		NoLine(g);
	}

	// a_pos: 0..1 down the bar, a_st of a_n stages (a_n 4 for RND)
	void DrawDrunkTile(GFx& g, double a_pos, int a_st, int a_n, bool a_cap = true)
	{
		g.Invoke("clear", nullptr, nullptr, 0);
		const int n = std::max(1, a_n);
		const int st = std::clamp(a_st, 0, n - 1);
		DrawTileFrame(g, st == n - 1 && n > 1);

		const double top = -26, bot = 26;
		const double lt = top + 5, lb = bot - 2;
		const double fy = lt + std::clamp(a_pos, 0.0, 1.0) * (lb - lt);
		// the four looks spread over the mod's stage count
		const int look = n <= 1 ? 0 : static_cast<int>(std::lround(3.0 * st / (n - 1)));
		static constexpr double TILT[4] = { 0.0, -12.0, -26.0, -40.0 };
		const double tilt = TILT[look];

		// the tankard, upright: a slightly tapered wooden body, foam head above, handle on the right; turned around the
		// bottom left corner (it tips to the left, the handle rises)
		const double ox = -16 + 3.0 * look, by = bot - 1;  // it slides right as it tips, to stay inside the frame  // bottom centre
		const double hw = 12.0, h = 33;       // half width at the bottom, height of the wood
		const double px = ox - hw, py = by;   // the pivot
		const Poly body{ { ox - hw, by }, { ox + hw, by }, { ox + hw - 1.5, by - h }, { ox - hw + 1.5, by - h } };
		const double ft = by - h;  // the rim
		Poly foam{ { ox - hw + 0.5, ft + 2 } };
		for (int i = 0; i <= 4; ++i) {  // four bumps over the rim
			const double x0 = ox - hw + 0.5 + i * (2 * hw - 1) / 4.0;
			const double x1 = x0 + (2 * hw - 1) / 4.0;
			if (i < 4) {
				QuadPoints(foam, x0, ft - 2.5, (x0 + x1) / 2, ft - 8.5 - (i % 2) * 1.5, x1, ft - 2.5, 6);
			}
		}
		foam.emplace_back(ox + hw - 0.5, ft + 2);
		Poly handle;  // a C loop on the right side
		{
			const double hx = ox + hw - 1, hy0 = ft + 5, hy1 = by - 6;
			Poly outer{ { hx, hy0 } };
			CubicPoints(outer, hx, hy0, hx + 11, hy0 - 1, hx + 11, hy1 + 1, hx, hy1, 10);
			Poly inner{ { hx, hy1 - 4 } };
			CubicPoints(inner, hx, hy1 - 4, hx + 6.5, hy1 - 4, hx + 6.5, hy0 + 4, hx, hy0 + 4, 10);
			handle = outer;
			handle.insert(handle.end(), inner.begin(), inner.end());
		}
		const Poly tb = Turned(body, px, py, tilt), tf = Turned(foam, px, py, tilt), th = Turned(handle, px, py, tilt);

		// ale spilling over the lip at the worst look: a stream from the low rim corner and a puddle
		if (look == 3) {
			const auto lip = Turned(Poly{ { ox - hw + 1.5, ft } }, px, py, tilt)[0];
			Poly stream{ { lip.first - 1.5, lip.second - 1 } };
			CubicPoints(stream, lip.first - 1.5, lip.second - 1, lip.first - 7, lip.second + 2, lip.first - 6, by - 10, lip.first - 4, by, 8);
			stream.emplace_back(lip.first + 0.5, by);
			CubicPoints(stream, lip.first + 0.5, by, lip.first - 2, by - 10, lip.first - 2.5, lip.second + 3, lip.first + 1.5, lip.second + 1, 8);
			NoLine(g);
			Fill(g, K_ALE, 90.0);
			PolyPath(g, stream);
			EndFill(g);
			Fill(g, K_ALE, 80.0);
			EllipsePath(g, lip.first - 2, by + 0.5, 9, 2.2, 0.0);
			EndFill(g);
		}

		NoLine(g);
		Fill(g, K_WOOD);
		PolyPath(g, th);
		EndFill(g);
		Fill(g, K_WOOD);
		PolyPath(g, tb);
		EndFill(g);
		// the shaded left staves
		{
			const Poly shade{ { ox - hw, by }, { ox - hw + 6, by }, { ox - hw + 7, by - h }, { ox - hw + 1.5, by - h } };
			Fill(g, K_WOOD_DARK, 70.0);
			PolyPath(g, Turned(shade, px, py, tilt));
			EndFill(g);
		}
		// two iron bands
		for (double fy0 : { by - 5.0, by - h + 6.0 }) {
			const Poly band{ { ox - hw + 0.2, fy0 + 1.6 }, { ox + hw - 0.2, fy0 + 1.6 }, { ox + hw - 0.6, fy0 - 1.6 }, { ox - hw + 0.6, fy0 - 1.6 } };
			Fill(g, K_BAND);
			PolyPath(g, Turned(band, px, py, tilt));
			EndFill(g);
		}
		Fill(g, K_FOAM);
		PolyPath(g, tf);
		EndFill(g);

		Edged(g, [&]() { PolyPath(g, th); });
		Edged(g, [&]() { PolyPath(g, tb); });
		Edged(g, [&]() { PolyPath(g, tf); }, 1.4);
		NoLine(g);

		// dizzy swirls above (one, two, three)
		static constexpr double SW[3][3] = { { -8, -21, 0.4 }, { -24, -15, 2.2 }, { 7, -17, 4.1 } };
		for (int i = 0; i < look; ++i) {
			Swirl(g, ox + SW[i][0] + 6, SW[i][1] - 4, 5.0, SW[i][2]);
		}

		// the red cap marks the pass-out end (RND's blackout, Last Seed's 100); none when the mod has nothing after its last stage
		DrawTileBar(g, lt, lb, DRUNK_COLS, n, fy, -1, a_cap);
	}
