// ---- the hunger tile (2026-11-08): the approved NeedsHUD needs design (mockups\needshud_mockups_2026-11-04.zip,
// g4.py / final3: o_stew + small_gauge + frame_box_iron), drawn live from C++ like the thermometer.
// A dark rounded tile with a worn-iron frame and rivets; the stew bowl on the left (the stew level is the gauge,
// a spoon leans out, chunks in the stew, steam when Well Fed); a small bar of the five stage blocks on the right with
// a pointer at your hunger; the stage name under it (the thermometer's text style). At Starving the frame turns red with the red spike
// crowns on both sides (the same warning shape as the red thermometer).
// Survival's own values: Survival_HungerNeedValue (0 fed .. 1000), stages Satisfied <160, Peckish <340, Hungry <520,
// Famished <770, Starving; Well Fed = Survival's bonus spell Survival_HungerStage0 (its "hasBonus").
// Stage blocks are all the same size (his rule for every gauge); the value maps piecewise inside each block.
// All coordinates are the mockup's 1080p pixels around the tile centre; the clip is scaled to the HUD.

	constexpr double HUNGER_BOUNDS[6] = { 0.0, 160.0, 340.0, 520.0, 770.0, 1000.0 };
	constexpr const char* HUNGER_NAMES[5] = { "Satisfied", "Peckish", "Hungry", "Famished", "Starving" };  // the thermometer's case
	constexpr std::uint32_t HUNGER_COLS[5] = { 0xDDC2A8, 0xBAA28B, 0x98836F, 0x796757, 0x5F5041 };  // ramp(70, 18, ...)
	constexpr std::uint32_t T_BONE = 0xCFC6B2, T_IRON = 0x8F8A80, T_RED = 0xE0705A, T_INSIDE = 0x2B2620;
	constexpr double T_SW = 2.0, T_EDGE = 2.4;          // main stroke, dark separation outside it (edge_and_line)
	constexpr double MOCK_TO_HUD = 720.0 / 1080.0;      // mockup pixels are 1080p; the HUD stage is 720 high
	constexpr double TILE_W = 120.0, TILE_H = 84.0;

	// the stage (0..4) of a value, from a need's six stage bounds (the last one is the need's max)
	int StageOf(double a_v, const double (&a_bounds)[6])
	{
		for (int i = 0; i < 5; ++i) {
			if (a_v < a_bounds[i + 1]) {
				return i;
			}
		}
		return 4;
	}
	// 0..1 down the bar: each stage one fifth (equal blocks), the value piecewise inside its block
	double BarPos(double a_v, const double (&a_bounds)[6])
	{
		a_v = std::clamp(a_v, 0.0, a_bounds[5]);
		const int i = StageOf(a_v, a_bounds);
		const double f = (a_v - a_bounds[i]) / std::max(1.0, a_bounds[i + 1] - a_bounds[i]);
		return (i + std::clamp(f, 0.0, 1.0)) / 5.0;
	}
	int    HungerStage(double a_v) { return StageOf(a_v, HUNGER_BOUNDS); }
	double HungerBarPos(double a_v) { return BarPos(a_v, HUNGER_BOUNDS); }

	// any number of stages (his rule 2026-11-08: each mod's own stage count): stage i of n takes its colour from the
	// approved five-colour ramp, spread evenly from its first colour to its last (n = 5 gives the ramp itself)
	std::uint32_t RampColor(const std::uint32_t (&a_cols)[5], int a_i, int a_n)
	{
		if (a_n <= 1) {
			return a_cols[0];
		}
		const double t = std::clamp(static_cast<double>(a_i) / (a_n - 1), 0.0, 1.0) * 4.0;
		const int    k = std::min(3, static_cast<int>(t));
		const double f = t - k;
		const auto ch = [&](int a_shift) {
			const double c0 = (a_cols[k] >> a_shift) & 0xFF, c1 = (a_cols[k + 1] >> a_shift) & 0xFF;
			return static_cast<std::uint32_t>(std::lround(c0 + (c1 - c0) * f)) << a_shift;
		};
		return ch(16) | ch(8) | ch(0);
	}

	// a stroke drawn twice: dark and wide first (reads on snow), then the bright line (edge_and_line)
	template <class PathFn>
	void Edged(GFx& g, PathFn a_path, double a_w = T_SW, std::uint32_t a_col = T_BONE, double a_alpha = 90.0)
	{
		Line(g, a_w + T_EDGE * 2.0, 0x000000, 70.0);
		a_path();
		Line(g, a_w, a_col, a_alpha);
		a_path();
	}

	// the red three-spike warning crown on a side of the frame (ornament at the worst stage); a_d = -1 left, +1 right
	void SpikeCrown(GFx& g, double x, double y, double d)
	{
		const double pts[7][2] = { { x, y - 11 }, { x + d * 9, y - 8 }, { x + d * 5, y - 3 }, { x + d * 17, y },
			{ x + d * 5, y + 3 }, { x + d * 9, y + 8 }, { x, y + 11 } };
		const auto path = [&]() {
			MoveTo(g, pts[0][0], pts[0][1]);
			for (int i = 1; i < 7; ++i) {
				LineTo(g, pts[i][0], pts[i][1]);
			}
		};
		Line(g, T_SW + T_EDGE * 2.0, 0x000000, 70.0);
		Fill(g, 0x000000, 70.0);
		path();
		EndFill(g);
		Line(g, 1.2, T_RED);
		Fill(g, T_RED);
		path();
		EndFill(g);
	}

	// one quadratic curve sampled (the bowl's sides), for the stew clip
	void QuadPoints(std::vector<std::pair<double, double>>& a_out, double x0, double y0, double cx, double cy, double x1, double y1, int n)
	{
		for (int i = 1; i <= n; ++i) {
			const double t = static_cast<double>(i) / n, u = 1.0 - t;
			a_out.emplace_back(u * u * x0 + 2 * u * t * cx + t * t * x1, u * u * y0 + 2 * u * t * cy + t * t * y1);
		}
	}
	// the part of a closed polygon at or below y = a_y (one Sutherland-Hodgman edge)
	std::vector<std::pair<double, double>> BelowY(const std::vector<std::pair<double, double>>& a_poly, double a_y)
	{
		std::vector<std::pair<double, double>> out;
		const size_t n = a_poly.size();
		for (size_t i = 0; i < n; ++i) {
			const auto& p = a_poly[i];
			const auto& q = a_poly[(i + 1) % n];
			const bool pin = p.second >= a_y, qin = q.second >= a_y;
			if (pin) {
				out.push_back(p);
			}
			if (pin != qin) {
				const double t = (a_y - p.second) / (q.second - p.second);
				out.emplace_back(p.first + t * (q.first - p.first), a_y);
			}
		}
		return out;
	}

	// the tile's frame (frame_box_iron): dark see-through backing, dark edge, iron line (red at the worst stage), faint
	// bone inner line, rivets, and the red spike crowns at the worst stage; around the tile centre (0, 0)
	void DrawTileFrame(GFx& g, bool worst)
	{
		const double cx = 0.0, cy = 0.0;
		const double x0 = cx - TILE_W / 2, y0 = cy - TILE_H / 2;
		FillRect(g, x0, y0, TILE_W, TILE_H, 6.0, 0x0D0D0C, 62.0);
		Line(g, 1.6 + T_EDGE * 2.0, 0x000000, 70.0);
		RoundRectPath(g, x0, y0, TILE_W, TILE_H, 6.0);
		Line(g, 1.6, worst ? T_RED : T_IRON);
		RoundRectPath(g, x0, y0, TILE_W, TILE_H, 6.0);
		Line(g, 1.0, T_BONE, 25.0);
		RoundRectPath(g, x0 + 3, y0 + 3, TILE_W - 6, TILE_H - 6, 4.0);
		NoLine(g);
		for (const auto& [px, py] : { std::pair{ x0 + 5, y0 + 5 }, std::pair{ x0 + TILE_W - 5, y0 + 5 }, std::pair{ x0 + 5, y0 + TILE_H - 5 },
				 std::pair{ x0 + TILE_W - 5, y0 + TILE_H - 5 } }) {
			Fill(g, T_IRON);
			CirclePath(g, px, py, 1.3);
			EndFill(g);
		}
		if (worst) {
			SpikeCrown(g, x0, cy, -1.0);
			SpikeCrown(g, x0 + TILE_W, cy, 1.0);
		}
	}

	constexpr std::uint32_t T_BRONZE = 0xC59867;  // Free Cooking's Refreshing (lch 66, 34, 72)

	// the stage bar (vband, 9 px wide, blocks a_cols from lt to lb) with the red cap at the bad end, and the pointer
	// (chevron) at fy; a_hollow: a block drawn dark with a bronze outline (the stage Refreshing counters), -1 none
	void DrawTileBar(GFx& g, double lt, double lb, const std::uint32_t (&a_cols)[5], int a_n, double fy, int a_hollow = -1, bool a_cap = true)
	{
		const double cx = 0.0;
		const double bx = cx + 24, w = 9.0;
		NoLine(g);
		Fill(g, 0x000000, 70.0);
		RoundRectPath(g, bx - 1.5, lt - 1.5, w + 3, lb - lt + 3, 1.0);
		EndFill(g);
		const int n = std::max(1, a_n);
		for (int i = 0; i < n; ++i) {
			const double a = lt + (lb - lt) * i / n, z = lt + (lb - lt) * (i + 1) / n;
			FillRect(g, bx, a, w, z - a, 0.0, RampColor(a_cols, i, n));
			if (i) {
				Line(g, 1.8, 0x000000);
				MoveTo(g, bx - 2, a);
				LineTo(g, bx + w + 2, a);
				NoLine(g);
			}
		}
		if (a_cap) {  // the bad end of a need's scale (the disease tile has none: every block is already bad, his call 2026-11-08)
			FillRect(g, bx - 1.5, lb, w + 3, 6, 0.0, 0x000000, 80.0);
			FillRect(g, bx, lb + 1, w, 4, 0.0, T_RED);
		}
		if (a_hollow >= 0 && a_hollow < n) {
			const double a = lt + (lb - lt) * a_hollow / n, z = lt + (lb - lt) * (a_hollow + 1) / n;
			FillRect(g, bx, a, w, z - a, 0.0, 0x2A2724);
			Line(g, 1.4, T_BRONZE);
			RoundRectPath(g, bx - 0.8, a, w + 1.6, z - a, 0.0);
			NoLine(g);
		}
		const double px = bx + 14;
		const auto chevron = [&]() {
			MoveTo(g, px + 15, fy - 8);
			LineTo(g, px, fy);
			LineTo(g, px + 15, fy + 8);
			LineTo(g, px + 10, fy);
			LineTo(g, px + 15, fy - 8);
		};
		Line(g, T_EDGE * 2.0, 0x000000, 75.0);
		Fill(g, 0x000000, 75.0);
		chevron();
		EndFill(g);
		NoLine(g);
		Fill(g, T_BONE);
		chevron();
		EndFill(g);
	}

	// a_pos: 0..1 down the bar (the stew's level), a_st of a_n stages, a_fed: the well-fed look (full, steaming)
	void DrawHungerTile(GFx& g, double a_pos, int a_st, int a_n, bool a_fed)
	{
		g.Invoke("clear", nullptr, nullptr, 0);
		const int  st = std::clamp(a_st, 0, std::max(0, a_n - 1));
		const bool worst = !a_fed && st == a_n - 1;
		const double cx = 0.0, cy = 0.0;

		DrawTileFrame(g, worst);

		// the stew bowl (o_stew)
		const double ox = cx - 12, top = cy - 26, bot = cy + 26;
		const double lt = top + 19, lb = bot - 3;               // RANGE['hunger']: where the stew level moves
		const double v = a_fed ? 0.0 : std::clamp(a_pos, 0.0, 1.0);
		const double fy = lt + v * (lb - lt);                   // the stew's surface (and the pointer)
		const double b = bot - 2, rimY = top + 20, R = 23;
		// spoon handle, behind the bowl
		Line(g, 3.0 + T_EDGE * 2.0, 0x000000, 70.0);
		MoveTo(g, ox + 4, rimY + 4);
		LineTo(g, ox + 18, top + 2);
		Line(g, 3.0, 0x8A6440);
		MoveTo(g, ox + 4, rimY + 4);
		LineTo(g, ox + 18, top + 2);
		// the bowl's inside
		const auto bowlPath = [&]() {
			MoveTo(g, ox - R, rimY);
			CurveTo(g, ox - R + 1, b, ox, b);
			CurveTo(g, ox + R - 1, b, ox + R, rimY);
			LineTo(g, ox - R, rimY);
		};
		NoLine(g);
		Fill(g, T_INSIDE);
		bowlPath();
		EndFill(g);
		// the stew: the bowl's shape below the surface
		const double yy = std::max(fy, rimY - 2);
		std::vector<std::pair<double, double>> bowl{ { ox - R, rimY } };
		QuadPoints(bowl, ox - R, rimY, ox - R + 1, b, ox, b, 12);
		QuadPoints(bowl, ox, b, ox + R - 1, b, ox + R, rimY, 12);
		const auto stew = BelowY(bowl, yy);
		if (stew.size() >= 3) {
			Fill(g, RampColor(HUNGER_COLS, a_fed ? 0 : st, a_n));
			MoveTo(g, stew[0].first, stew[0].second);
			for (size_t i = 1; i < stew.size(); ++i) {
				LineTo(g, stew[i].first, stew[i].second);
			}
			LineTo(g, stew[0].first, stew[0].second);
			EndFill(g);
			if (v < 0.7) {  // chunks in the stew
				for (const auto& [dx, dy, r] : { std::tuple{ -8.0, 4.0, 2.2 }, std::tuple{ 6.0, 6.0, 1.8 }, std::tuple{ -1.0, 9.0, 1.6 } }) {
					if (yy + dy < b - 4) {
						Fill(g, T_INSIDE, 45.0);
						CirclePath(g, ox + dx, yy + dy, r);
						EndFill(g);
					}
				}
			}
		}
		Edged(g, bowlPath);
		Edged(g, [&]() { EllipsePath(g, ox, rimY, R + 1, 3, 0.0); }, 1.8);
		Edged(g, [&]() {
			MoveTo(g, ox - 9, b);
			LineTo(g, ox - 11, b + 3);
			LineTo(g, ox + 11, b + 3);
			LineTo(g, ox + 9, b);
		}, 1.8);
		if (a_fed) {  // steam: two little S wisps over the rim
			for (const double dx : { -8.0, 2.0 }) {
				Edged(g, [&]() {
					const double sx = ox + dx, sy = rimY - 5;
					MoveTo(g, sx, sy);  // the mockup's "c-4,-4 4,-7 0,-11" as two quadratic pieces
					CurveTo(g, sx - 4, sy - 3, sx, sy - 5.5);
					CurveTo(g, sx + 4, sy - 8, sx, sy - 11);
				}, 1.6);
			}
		}

		DrawTileBar(g, lt, lb, HUNGER_COLS, a_n, fy);
	}
