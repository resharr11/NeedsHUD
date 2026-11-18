// ---- the thirst tile (2026-11-08): the approved NeedsHUD needs design (mockups\needshud_mockups_2026-11-04.zip,
// g4.py o_waterskin as used by sheet49.py / final3 + small_gauge + frame_box_iron), drawn live from C++ like the other
// tiles. A leather waterskin: rounded bag with a short neck, wooden stopper, cord and a stitched seam; the water inside
// (pale, wavy surface) drains as you get thirsty. Shown when a needs mod has thirst (SunHelm; vanilla Survival has none).
// Same frame, bar and pointer as the hunger tile (HungerTile.inl); coordinates are the mockup's 1080p px around the
// tile centre.

	constexpr std::uint32_t THIRST_COLS[5] = { 0xB8D7DD, 0xA2C1C7, 0x8AA8AE, 0x739196, 0x5D797F };  // lch(L, 11, 215)
	constexpr std::uint32_t T_LEATHER = 0x7A5636, T_SKIN_DARK = 0x3A2A1C, T_STOPPER = 0xA37B52, T_SURFACE = 0xF2FBFF;

	// a cubic curve sampled into points (GFx draws only quadratic curves)
	void CubicPoints(std::vector<std::pair<double, double>>& a_out, double x0, double y0, double x1, double y1, double x2, double y2,
		double x3, double y3, int n)
	{
		for (int i = 1; i <= n; ++i) {
			const double t = static_cast<double>(i) / n, u = 1.0 - t;
			a_out.emplace_back(u * u * u * x0 + 3 * u * u * t * x1 + 3 * u * t * t * x2 + t * t * t * x3,
				u * u * u * y0 + 3 * u * u * t * y1 + 3 * u * t * t * y2 + t * t * t * y3);
		}
	}

	bool InsidePoly(const std::vector<std::pair<double, double>>& a_p, double a_x, double a_y)
	{
		bool in = false;
		for (size_t i = 0, j = a_p.size() - 1; i < a_p.size(); j = i++) {
			const auto& [xi, yi] = a_p[i];
			const auto& [xj, yj] = a_p[j];
			if (((yi > a_y) != (yj > a_y)) && (a_x < (xj - xi) * (a_y - yi) / (yj - yi) + xi)) {
				in = !in;
			}
		}
		return in;
	}

	// a_pos: 0..1 down the bar (the water's level), a_st of a_n stages
	void DrawThirstTile(GFx& g, double a_pos, int a_st, int a_n)
	{
		g.Invoke("clear", nullptr, nullptr, 0);
		const int n = std::max(1, a_n);
		const int st = std::clamp(a_st, 0, n - 1);
		DrawTileFrame(g, st == n - 1);

		const double cx = 0.0, cy = 0.0;
		const double ox = cx - 12, top = cy - 26, bot = cy + 26;
		const double lt = top + 16, lb = bot - 1;  // RANGE['thirst'] (g3.py): where the level moves
		const double fy = lt + std::clamp(a_pos, 0.0, 1.0) * (lb - lt);
		const double b = bot - 1, t = top + 3;

		// the bag: M ox-4,t+8 C ... (o_waterskin's path), sampled
		std::vector<std::pair<double, double>> bag{ { ox - 4, t + 8 } };
		CubicPoints(bag, ox - 4, t + 8, ox - 6, t + 14, ox - 21, t + 16, ox - 21, b - 12, 10);
		CubicPoints(bag, ox - 21, b - 12, ox - 21, b, ox - 8, b + 1, ox, b, 8);
		CubicPoints(bag, ox, b, ox + 10, b + 1, ox + 21, b - 2, ox + 20, b - 13, 8);
		CubicPoints(bag, ox + 20, b - 13, ox + 19, t + 17, ox + 6, t + 14, ox + 4, t + 8, 10);
		const auto bagPath = [&]() { PolyPath(g, bag); };
		NoLine(g);
		Fill(g, T_SKIN_DARK);
		bagPath();
		EndFill(g);

		// the water: the bag below a wavy surface (ten little humps, 4.6 px apart, from x = ox-23)
		const double yy = std::max(fy, t + 12);
		const auto waveY = [&](double a_x) {
			const double u = (a_x - (ox - 23)) / 4.6;  // which hump
			const int    k = static_cast<int>(std::floor(u));
			const double f = u - k;
			const double amp = (k % 2 == 0) ? -2.0 : 2.0;  // the mockup's q2.3,(-2|2) 4.6,0: a quadratic hump
			return yy + amp * 2.0 * f * (1.0 - f) * 1.0;
		};
		std::vector<std::pair<double, double>> water;
		{
			// walk the bag's outline, keeping what is under the surface; where the outline crosses the surface, the
			// water's edge follows the waves (sampled) to the next crossing
			std::vector<std::pair<double, double>> pts;
			const size_t m = bag.size();
			const auto under = [&](const std::pair<double, double>& p) { return p.second >= waveY(p.first); };
			for (size_t i = 0; i < m; ++i) {
				const auto& p = bag[i];
				const auto& q = bag[(i + 1) % m];
				const bool pin = under(p), qin = under(q);
				if (pin) {
					pts.push_back(p);
				}
				if (pin != qin) {
					// the crossing, by bisection on the segment
					double lo = 0.0, hi = 1.0;
					for (int it = 0; it < 20; ++it) {
						const double mid = (lo + hi) / 2;
						const std::pair<double, double> r{ p.first + (q.first - p.first) * mid, p.second + (q.second - p.second) * mid };
						if (under(r) == pin) {
							lo = mid;
						} else {
							hi = mid;
						}
					}
					const std::pair<double, double> c{ p.first + (q.first - p.first) * lo, p.second + (q.second - p.second) * lo };
					pts.push_back(c);
					if (pin && !qin) {
						// leaving the water: follow the surface to where the outline comes back under it
						size_t j = (i + 1) % m;
						for (size_t steps = 0; steps < m && !under(bag[j]); ++steps) {
							j = (j + 1) % m;
						}
						const auto& a = bag[(j + m - 1) % m];
						const double xa = c.first, xb = (a.first + bag[j].first) / 2;
						const int    ns = std::max(2, static_cast<int>(std::abs(xb - xa) / 0.8));
						for (int k = 1; k < ns; ++k) {
							const double x = xa + (xb - xa) * k / ns;
							pts.emplace_back(x, waveY(x));
						}
					}
				}
			}
			water = std::move(pts);
		}
		if (water.size() >= 3) {
			Fill(g, RampColor(THIRST_COLS, st, n));
			PolyPath(g, water);
			EndFill(g);
		}
		// the surface line, only inside the bag
		Line(g, 1.5, T_SURFACE);
		bool drawing = false;
		for (double x = ox - 23; x <= ox + 23.01; x += 0.6) {
			const double y = waveY(x);
			if (InsidePoly(bag, x, y)) {
				if (!drawing) {
					MoveTo(g, x, y);
					drawing = true;
				} else {
					LineTo(g, x, y);
				}
			} else {
				drawing = false;
			}
		}
		NoLine(g);

		// the leather rim (a wide brown band just inside the outline; its outer half is hidden under the edge below)
		Line(g, 5.0, T_LEATHER);
		bagPath();
		// the stitched seam down the middle: dashes 2.5 on, 2 off
		{
			std::vector<std::pair<double, double>> seam{ { ox, t + 14 } };
			CubicPoints(seam, ox, t + 14, ox + 1, t + 24, ox - 1, b - 10, ox, b - 2, 30);
			Line(g, 1.3, T_LEATHER);
			double along = 0.0;
			for (size_t i = 1; i < seam.size(); ++i) {
				const double dx = seam[i].first - seam[i - 1].first, dy = seam[i].second - seam[i - 1].second;
				const double l = std::sqrt(dx * dx + dy * dy);
				if (std::fmod(along, 4.5) < 2.5) {
					MoveTo(g, seam[i - 1].first, seam[i - 1].second);
					LineTo(g, seam[i].first, seam[i].second);
				}
				along += l;
			}
		}
		// the outline (edge_and_line)
		Edged(g, bagPath);
		// neck tie, wooden stopper, cord
		Line(g, 1.4, 0x000000, 70.0);
		Fill(g, T_LEATHER);
		RoundRectPath(g, ox - 4.5, t + 1, 9, 8, 1.5);
		EndFill(g);
		Fill(g, T_STOPPER);
		RoundRectPath(g, ox - 3, t - 5, 6, 7, 2);
		EndFill(g);
		Line(g, 1.8, T_LEATHER);
		MoveTo(g, ox - 4, t + 5);
		CurveTo(g, ox - 16, t - 2, ox - 20, t + 12);
		NoLine(g);

		DrawTileBar(g, lt, lb, THIRST_COLS, n, fy);
	}
