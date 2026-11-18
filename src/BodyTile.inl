// ---- the body tile (2026-11-09, his "add both" after RND's widgets): Realistic Needs and Diseases' body weight, the
// character's own body slider (0..100) that RND moves with what you eat: Skinny, Slender, Wiry, Trained, Muscular (top
// of the bar = Muscular, the well fed end). A bare torso seen from the front that broadens stage by stage: ribs show
// when Skinny, a lean line when Wiry, chest and belly lines when Trained, a broad chest with ab lines when Muscular.
// Not a need: every band has its trade-offs, so neutral colours, no red cap and no red frame. Coordinates are the
// mockup's 1080p px around the tile centre.

	constexpr std::uint32_t BODY_COLS[5] = { 0xD8C6AE, 0xC7B39A, 0xB6A187, 0xA48F76, 0x927E66 };  // even steps, no warning tone
	constexpr std::uint32_t B_SKIN = 0xC9A27E, B_SKIN_DARK = 0x8E6B4F, B_LINE = 0x5A4130, B_CLOTH = 0x6B5A44;

	// a_pos: 0..1 down the bar (0 = Muscular), a_st of a_n stages from the top (Muscular) down (Skinny)
	void DrawBodyTile(GFx& g, double a_pos, int a_st, int a_n)
	{
		g.Invoke("clear", nullptr, nullptr, 0);
		const int n = std::max(1, a_n);
		const int st = std::clamp(a_st, 0, n - 1);
		DrawTileFrame(g, false);

		const double top = -26, bot = 26;
		const double lt = top + 5, lb = bot - 2;
		const double fy = lt + std::clamp(a_pos, 0.0, 1.0) * (lb - lt);
		// 0 skinny .. 4 muscular, spread over the mod's stage count
		const int look = n <= 1 ? 2 : 4 - static_cast<int>(std::lround(4.0 * st / (n - 1)));
		const double k = look / 4.0;

		const double ox = -13;
		const double sh = 9.0 + 11.0 * k;   // half the shoulder width
		const double ws = 6.0 + 4.5 * k;    // half the waist
		const double ny = -14.0;            // neck base
		const double wy = 21.0;             // waist (the belt)
		const double headR = 6.2;

		// the torso: neck, sloped shoulders, arms cut short at the upper arm, the chest tapering to the waist
		const double arm = 3.0 + 4.5 * k;   // upper arm thickness
		// the right half from the neck down to the waist (offsets from ox), then mirrored for the left
		Poly half{ { 3.2, ny - 3 } };
		QuadPoints(half, 3.2, ny - 3, 4.5, ny, sh - 2, ny + 1.5 - k, 6);                // trapezius
		QuadPoints(half, sh - 2, ny + 1.5 - k, sh + 2.5, ny + 2.5, sh + 1.5, ny + 9, 6);  // shoulder cap
		half.emplace_back(sh + 1.0, ny + 14);                                           // the arm cut short
		half.emplace_back(sh + 1.0 - arm, ny + 14.5);
		QuadPoints(half, sh + 1.0 - arm, ny + 14.5, sh - arm - 0.5 * k, ny + 24, ws, wy, 8);  // the side down to the waist
		Poly torso;
		for (const auto& [x, y] : half) {
			torso.emplace_back(ox + x, y);
		}
		for (auto it = half.rbegin(); it != half.rend(); ++it) {
			torso.emplace_back(ox - it->first, it->second);
		}

		NoLine(g);
		Fill(g, B_SKIN);
		PolyPath(g, torso);
		EndFill(g);
		// shade down the left side
		Fill(g, B_SKIN_DARK, 45.0);
		{
			Poly sd{ { ox - sh - 1.0, ny + 9 }, { ox - sh + 4, ny + 9 }, { ox - ws + 3, wy }, { ox - ws, wy } };
			PolyPath(g, sd);
		}
		EndFill(g);
		// trousers' waistband under the belt line
		FillRect(g, ox - ws - 0.5, wy - 0.5, 2 * ws + 1, 5.5, 1.0, B_CLOTH);

		// the head
		Fill(g, B_SKIN);
		CirclePath(g, ox, ny - 3 - headR + 0.5, headR);
		EndFill(g);

		Edged(g, [&]() { PolyPath(g, torso); }, 1.6);
		Edged(g, [&]() { CirclePath(g, ox, ny - 3 - headR + 0.5, headR); }, 1.6);
		NoLine(g);

		// the body's lines
		const auto stroke = [&](double a_w, auto a_path) {
			Line(g, a_w, B_LINE, 85.0);
			a_path();
			NoLine(g);
		};
		if (look <= 1) {  // ribs showing: short arcs down both sides (three when skinny, two when slender) and a thin sternum
			stroke(1.0, [&]() {
				const int nr = look == 0 ? 3 : 2;
				for (int i = 0; i < nr; ++i) {
					const double y = ny + 9 + i * 3.6;
					MoveTo(g, ox - 1.5, y - 1);
					CurveTo(g, ox - 4, y + 1.5, ox - ws - 0.5, y + 0.5);
					MoveTo(g, ox + 1.5, y - 1);
					CurveTo(g, ox + 4, y + 1.5, ox + ws + 0.5, y + 0.5);
				}
				MoveTo(g, ox, ny + 5);
				LineTo(g, ox, ny + 9 + nr * 3.6);
			});
		}
		if (look == 2) {  // wiry: a lean centre line and a hint of the chest
			stroke(1.0, [&]() {
				MoveTo(g, ox, ny + 6);
				LineTo(g, ox, wy - 3);
				MoveTo(g, ox - 8, ny + 9);
				CurveTo(g, ox - 4, ny + 11, ox - 1, ny + 9);
				MoveTo(g, ox + 8, ny + 9);
				CurveTo(g, ox + 4, ny + 11, ox + 1, ny + 9);
			});
		}
		if (look >= 3) {  // trained / muscular: the chest's lower curve, the centre line, belly lines (two rows when muscular)
			const double cw = 7.0 + 3.0 * k;
			stroke(look == 4 ? 1.4 : 1.2, [&]() {
				MoveTo(g, ox - cw, ny + 6);
				CurveTo(g, ox - cw * 0.6, ny + 12.5, ox - 0.5, ny + 9.5);
				MoveTo(g, ox + cw, ny + 6);
				CurveTo(g, ox + cw * 0.6, ny + 12.5, ox + 0.5, ny + 9.5);
				MoveTo(g, ox, ny + 9.5);
				LineTo(g, ox, wy - 2);
				const int rows = look == 4 ? 2 : 1;
				for (int r = 0; r < rows; ++r) {
					const double y = ny + 17 + r * 5.0;
					MoveTo(g, ox - 4.5, y);
					LineTo(g, ox - 1.2, y + 0.5);
					MoveTo(g, ox + 4.5, y);
					LineTo(g, ox + 1.2, y + 0.5);
				}
			});
		}

		DrawTileBar(g, lt, lb, BODY_COLS, n, fy, -1, false);
	}
