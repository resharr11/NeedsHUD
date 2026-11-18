// ---- bladder, fun and social (2026-11-09, his "lets do it now"): the approved 2026-11-04 designs (mockup scripts
// g3.py / g4.py: o_dam5, o_masks_bone2 with _mask2, o_people / o_people2; muted palette), fed only by config files.
// And the tile for an author's own art (custom needs, or Icons on a built-in tile): our frame, bar and text, their
// picture loaded into the tile (NeedsGroup.inl SyncIcons). Coordinates are the mockup's 1080p px around the tile centre.

	constexpr std::uint32_t BLADDER_COLS[5] = { 0x96B5C3, 0x81A0AE, 0x6C8B98, 0x587683, 0x446270 };
	constexpr std::uint32_t SOCIAL_COLS[5] = { 0xDDC0B9, 0xB49B95, 0x8C7872, 0x6C5B56, 0x4F423E };
	constexpr std::uint32_t FUN_COLS[5] = { 0xDFCFBC, 0xB6A998, 0x908576, 0x6B6256, 0x4C463D };
	constexpr std::uint32_t CUSTOM_COLS[5] = { 0xD9D1C0, 0xBFB6A4, 0xA59C8A, 0x8B8271, 0x716958 };  // neutral bone, for authors' art
	constexpr std::uint32_t O_STONE = 0x8B8679, O_FACE = 0x4A463F, O_INK = 0x17191C, O_TRAGIC = 0x7C838B, O_SURF = 0xEEF6F8;

	// the part of a polygon at or below the line y = a_y (Sutherland-Hodgman against one edge): the "filled to a level" clips
	Poly ClipBelow(const Poly& a_p, double a_y)
	{
		Poly out;
		const size_t n = a_p.size();
		for (size_t i = 0; i < n; ++i) {
			const auto& p = a_p[i];
			const auto& q = a_p[(i + 1) % n];
			const bool pin = p.second >= a_y, qin = q.second >= a_y;
			if (pin) {
				out.push_back(p);
			}
			if (pin != qin) {
				const double t = (a_y - p.second) / (q.second - p.second);
				out.emplace_back(p.first + (q.first - p.first) * t, a_y);
			}
		}
		return out;
	}

	// a shape in its own frame, placed: scaled, turned a_deg degrees, moved to (a_x, a_y) (SVG translate rotate scale)
	Poly Placed(const Poly& a_p, double a_x, double a_y, double a_deg, double a_sc)
	{
		const double a = a_deg * std::numbers::pi / 180.0, c = std::cos(a), s = std::sin(a);
		Poly out;
		out.reserve(a_p.size());
		for (const auto& [x, y] : a_p) {
			const double px = x * a_sc, py = y * a_sc;
			out.emplace_back(a_x + px * c - py * s, a_y + px * s + py * c);
		}
		return out;
	}

	// an open line drawn dashed (on, off), along its length
	void DashedPath(GFx& g, const Poly& a_p, double a_on, double a_off, bool a_closed)
	{
		Poly p = a_p;
		if (a_closed && !p.empty()) {
			p.push_back(p.front());
		}
		double along = 0.0;
		for (size_t i = 1; i < p.size(); ++i) {
			const double dx = p[i].first - p[i - 1].first, dy = p[i].second - p[i - 1].second;
			const double len = std::sqrt(dx * dx + dy * dy);
			if (std::fmod(along, a_on + a_off) < a_on) {
				MoveTo(g, p[i - 1].first, p[i - 1].second);
				LineTo(g, p[i].first, p[i].second);
			}
			along += len;
		}
	}

	// which of the design's four looks this stage is (the designs have four; a mod may have more or fewer stages)
	int LookOf(int a_st, int a_n) { return a_n <= 1 ? 0 : static_cast<int>(std::lround(3.0 * a_st / (a_n - 1))); }

	// ---- bladder: a dam holding back the water; the leak grows each stage (hairline crack and drip, open crack and
	// jet, big crack with spray and water lapping over the top)
	void DrawBladderTile(GFx& g, double a_pos, int a_st, int a_n, bool a_cap = true)
	{
		g.Invoke("clear", nullptr, nullptr, 0);
		const int n = std::max(1, a_n), st = std::clamp(a_st, 0, n - 1), look = LookOf(st, n);
		DrawTileFrame(g, st == n - 1 && n > 1);
		const double ox = -12, top = -26, bot = 26;
		const double lt = top + 9, lb = bot - 2;  // RANGE['bladder']
		const double fy = lt + std::clamp(a_pos, 0.0, 1.0) * (lb - lt);
		const double b = bot - 1, wl = ox - 35, t = top + 12;
		const double xt0 = ox - 8, xt1 = ox + 6, xb0 = ox - 14, xb1 = ox + 18;
		const auto wx = [&](double y) { return y > t ? xt0 + (xb0 - xt0) * std::max(0.0, y - t) / (b - t) : xt0; };
		const std::uint32_t col = RampColor(BLADDER_COLS, st, n), light = BLADDER_COLS[0];

		// the reservoir: it fills as the need grows (the bar's pointer goes down, the water comes up), a wavy surface of
		// 5 px humps
		const double yy = std::max(lb - std::clamp(a_pos, 0.0, 1.0) * (lb - lt), t - 3);
		const int humps = std::max(0, static_cast<int>((wx(yy) - wl) / 5.0));
		Poly surf{ { wl, yy } };
		for (int k = 0; k < humps; ++k) {
			const double x0 = wl + k * 5.0;
			QuadPoints(surf, x0, yy, x0 + 2.5, yy + (k % 2 == 0 ? -2.6 : 2.6), x0 + 5.0, yy, 4);
		}
		Poly water{ { wl, b } };
		water.insert(water.end(), surf.begin(), surf.end());
		water.emplace_back(std::max(wx(yy), xt0), yy);
		water.emplace_back(xb0, b);
		NoLine(g);
		Fill(g, col);
		PolyPath(g, water);
		EndFill(g);
		Line(g, 1.8, O_SURF);
		MoveTo(g, surf[0].first, surf[0].second);
		for (size_t i = 1; i < surf.size(); ++i) {
			LineTo(g, surf[i].first, surf[i].second);
		}
		NoLine(g);

		// the wall: a stone trapezoid with a curved downstream face
		Poly wall{ { xt0, t }, { xt1, t } };
		QuadPoints(wall, xt1, t, xt1 + 6, (t + b) / 2, xb1, b, 8);
		wall.emplace_back(xb0, b);
		Fill(g, O_STONE);
		PolyPath(g, wall);
		EndFill(g);
		Edged(g, [&]() { PolyPath(g, wall); });
		Line(g, 1.3, 0x000000, 55.0);
		for (int k = 1; k <= 2; ++k) {
			const double y = t + (b - t) * k / 3.0;
			MoveTo(g, wx(y) + 1, y);
			LineTo(g, xt1 + (xb1 - xt1) * k / 3.0 + 3, y);
		}
		NoLine(g);
		Edged(g, [&]() { MoveTo(g, wl, b + 1); LineTo(g, ox + 22, b + 1); }, 1.8);

		// the leak
		const double cx = ox + 12, cy = t + (b - t) * 0.55;
		if (look == 1) {
			Line(g, 1.6, 0x111111);
			MoveTo(g, cx - 8, cy - 6);
			LineTo(g, cx - 5, cy - 3);
			LineTo(g, cx - 6, cy);
			NoLine(g);
			Line(g, 1.0, 0x000000);
			Fill(g, light);
			CirclePath(g, cx + 3, cy + 4, 1.8);
			EndFill(g);
			NoLine(g);
		}
		if (look >= 2) {
			Line(g, look == 2 ? 2.2 : 2.8, 0x111111);
			MoveTo(g, cx - 12, cy - 9);
			LineTo(g, cx - 9, cy - 5);
			LineTo(g, cx - 11, cy - 2);
			LineTo(g, cx - 7, cy + 1);
			LineTo(g, cx - 3, cy + 1);
			const double L = look == 2 ? 9.0 : 14.0, w = look == 2 ? 2.8 : 4.0;
			const auto jet = [&]() { MoveTo(g, cx, cy); CurveTo(g, cx + L * 0.7, cy - 3, cx + L, b - 1); };
			Line(g, w + T_EDGE * 2.0, 0x000000, 70.0);
			jet();
			Line(g, w, light);
			jet();
			NoLine(g);
		}
		if (look >= 3) {
			for (const auto& [dx, dy] : { std::pair{ 5.0, -6.0 }, std::pair{ 10.0, -3.0 }, std::pair{ 13.0, -8.0 } }) {
				Line(g, 1.0, 0x000000);
				Fill(g, light);
				CirclePath(g, cx + dx, cy + dy, 1.7);
				EndFill(g);
			}
			Line(g, 2.0, light);  // lapping over the top
			MoveTo(g, xt0 + 2, t - 1);
			CurveTo(g, xt0 + 5, t - 5, xt0 + 8, t - 2);
			NoLine(g);
		}
		DrawTileBar(g, lt, lb, BLADDER_COLS, n, fy, -1, a_cap);
	}

	// ---- fun: comedy (bone, draining with fun) and tragedy (cold iron-grey) masks; from the third look the tragedy
	// mask comes to the front, at the worst only it is left
	void DrawMask(GFx& g, double a_x, double a_y, double a_deg, bool a_happy, std::uint32_t a_col, double a_fyLocal, double a_sc,
		double a_op)
	{
		constexpr double w = 15, h = 19;
		Poly face{ { -w, -h + 3 } };
		QuadPoints(face, -w, -h + 3, 0, -h - 3, w, -h + 3, 8);
		QuadPoints(face, w, -h + 3, w + 1, h - 6, 0, h, 10);
		QuadPoints(face, 0, h, -w - 1, h - 6, -w, -h + 3, 10);
		const double al = a_op * 100.0;
		const auto P = [&](const Poly& a_p) { return Placed(a_p, a_x, a_y, a_deg, a_sc); };
		NoLine(g);
		Fill(g, O_FACE, al);
		PolyPath(g, P(face));
		EndFill(g);
		const Poly lit = ClipBelow(face, a_fyLocal);  // the fill, up to the level (in the mask's own frame)
		if (lit.size() >= 3) {
			Fill(g, a_col, al);
			PolyPath(g, P(lit));
			EndFill(g);
		}
		Line(g, (T_SW + T_EDGE * 2.0) * a_sc, 0x000000, 70.0 * a_op);
		PolyPath(g, P(face));
		Line(g, T_SW * a_sc, T_BONE, 90.0 * a_op);
		PolyPath(g, P(face));
		// eyes and mouth
		Line(g, 2.6 * a_sc, O_INK, al);
		for (double d : { -1.0, 1.0 }) {
			Poly eye{ { d * 10, a_happy ? -5.0 : -8.0 } };
			QuadPoints(eye, d * 10, a_happy ? -5.0 : -8.0, d * 6, a_happy ? -11.0 : -2.0, d * 2, a_happy ? -5.0 : -8.0, 6);
			const Poly e = P(eye);
			MoveTo(g, e[0].first, e[0].second);
			for (size_t i = 1; i < e.size(); ++i) {
				LineTo(g, e[i].first, e[i].second);
			}
		}
		Poly mouth{ { -9, a_happy ? 4.0 : 12.0 } };
		if (a_happy) {
			QuadPoints(mouth, -9, 4, 0, 15, 9, 4, 8);
			QuadPoints(mouth, 9, 4, 0, 9, -9, 4, 8);
		} else {
			QuadPoints(mouth, -9, 12, 0, 2, 9, 12, 8);
			QuadPoints(mouth, 9, 12, 0, 7, -9, 12, 8);
		}
		Line(g, 1.0 * a_sc, T_BONE, 70.0 * a_op);
		Fill(g, O_INK, al);
		PolyPath(g, P(mouth));
		EndFill(g);
		NoLine(g);
	}
	void DrawFunTile(GFx& g, double a_pos, int a_st, int a_n, bool a_cap = true)
	{
		g.Invoke("clear", nullptr, nullptr, 0);
		const int n = std::max(1, a_n), st = std::clamp(a_st, 0, n - 1), look = LookOf(st, n);
		DrawTileFrame(g, st == n - 1 && n > 1);
		const double ox = -12, top = -26, bot = 26;
		const double lt = top + 4, lb = bot - 2;  // RANGE['fun']
		const double fy = lt + std::clamp(a_pos, 0.0, 1.0) * (lb - lt);
		const std::uint32_t col = RampColor(FUN_COLS, st, n);
		if (look >= 3) {
			DrawMask(g, ox - 2, top + 24, 6, false, O_TRAGIC, -30, 1.2, 1.0);
		} else if (look < 2) {
			DrawMask(g, ox + 11, top + 28, 14, false, O_TRAGIC, -30, 0.78, 0.55);
			DrawMask(g, ox - 6, top + 23, -12, true, col, fy - (top + 23), 1.12, 1.0);
		} else {
			DrawMask(g, ox - 13, top + 19, -14, true, col, fy - (top + 19), 0.78, 0.55);
			DrawMask(g, ox + 5, top + 26, 10, false, O_TRAGIC, -30, 1.12, 1.0);
		}
		DrawTileBar(g, lt, lb, FUN_COLS, n, fy, -1, a_cap);
	}

	// ---- social: two people (head and shoulders), the warmth draining as you get lonely; at the worst stage the
	// second person is only a dashed outline (gone)
	void DrawSocialTile(GFx& g, double a_pos, int a_st, int a_n, bool a_cap = true)
	{
		g.Invoke("clear", nullptr, nullptr, 0);
		const int n = std::max(1, a_n), st = std::clamp(a_st, 0, n - 1), look = LookOf(st, n);
		DrawTileFrame(g, st == n - 1 && n > 1);
		const double ox = -12, top = -26, bot = 26;
		const double lt = top + 4, lb = bot - 2;
		const double fy = lt + std::clamp(a_pos, 0.0, 1.0) * (lb - lt);
		const std::uint32_t col = RampColor(SOCIAL_COLS, st, n);
		const double b = bot, yy = std::max(fy, top);
		const auto bust = [&](double x, double hy, double r, double sh) {
			Poly body{ { x - sh, b } };
			QuadPoints(body, x - sh, b, x - sh, hy + r + 4, x, hy + r + 3, 8);
			QuadPoints(body, x, hy + r + 3, x + sh, hy + r + 4, x + sh, b, 8);
			return std::pair{ body, CirclePoly(x, hy, r, 28) };
		};
		const auto [body1, head1] = bust(ox - 9, top + 12, 8.5, 15);
		const auto [body2, head2] = bust(ox + 11, top + 16, 7.5, 13);
		const auto person = [&](const Poly& a_body, const Poly& a_head, bool a_fill) {
			NoLine(g);
			for (const Poly* p : { &a_body, &a_head }) {
				Fill(g, T_INSIDE);
				PolyPath(g, *p);
				EndFill(g);
				if (a_fill) {
					const Poly lit = ClipBelow(*p, yy);
					if (lit.size() >= 3) {
						Fill(g, col);
						PolyPath(g, lit);
						EndFill(g);
					}
				}
			}
			Edged(g, [&]() { PolyPath(g, a_body); });
			Edged(g, [&]() { PolyPath(g, a_head); });
			NoLine(g);
		};
		if (look >= 3) {
			Line(g, 1.4, T_BONE, 60.0);
			DashedPath(g, body2, 2.5, 2.5, true);
			DashedPath(g, head2, 2.5, 2.5, true);
			NoLine(g);
			person(body1, head1, false);
		} else {
			person(body2, head2, true);
			person(body1, head1, true);
		}
		DrawTileBar(g, lt, lb, SOCIAL_COLS, n, fy, -1, a_cap);
	}

	// ---- an author's art: our frame and bar; the picture itself is a loaded clip over this (SyncIcons). a_missing:
	// the picture can't be loaded, so a plain placeholder shows where it would be
	void DrawIconTile(GFx& g, double a_pos, int a_st, int a_n, bool a_cap, bool a_missing)
	{
		g.Invoke("clear", nullptr, nullptr, 0);
		const int n = std::max(1, a_n), st = std::clamp(a_st, 0, n - 1);
		DrawTileFrame(g, st == n - 1 && n > 1);
		const double top = -26, bot = 26;
		const double lt = top + 5, lb = bot - 2;
		const double fy = lt + std::clamp(a_pos, 0.0, 1.0) * (lb - lt);
		if (a_missing) {  // a dashed square with a question mark
			Line(g, 1.4, T_BONE, 60.0);
			DashedPath(g, Poly{ { -44, -24 }, { 4, -24 }, { 4, 24 }, { -44, 24 } }, 3.0, 3.0, true);
			Line(g, 2.4, T_BONE, 80.0);
			MoveTo(g, -26, -7);
			CurveTo(g, -26, -14, -20, -14);
			CurveTo(g, -14, -14, -14, -8);
			CurveTo(g, -14, -3, -20, 0);
			LineTo(g, -20, 5);
			NoLine(g);
			Fill(g, T_BONE, 80.0);
			CirclePath(g, -20, 11, 1.6);
			EndFill(g);
		}
		DrawTileBar(g, lt, lb, CUSTOM_COLS, n, fy, -1, a_cap);
	}
