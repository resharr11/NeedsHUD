// Hit by fire / hit by frost: flavour drawn around the thermometer when Survival Mode's own rule says the player was
// hit by fire or frost (Survival_PlayerMagicHitInfo: a magic effect with MagicDamageFire / MagicDamageFrost, or one
// of Survival_FrostbitePoisonEffects). Flavour only: the liquid, the stage name and the arrow always stay true.
// Looks (his Zelda reference videos, 2026-11-05): fire = FireFx.inl (soft particle fire, drawn as sprites by the gauge), frost = DrawIceCase (both described there;
// DrawIce is the earlier ice shell, kept unused). a_s: 0..1 how strong (they grow in fast and fade or thaw away).

	// the gauge's outline, clockwise from the top tip (y down): the ornament, the tube's right side, round the bulb,
	// up the tube's left side to the scale, round the scale, back to the ornament
	std::vector<std::pair<double, double>> GaugeOutline()
	{
		std::vector<std::pair<double, double>> pts;
		const auto seg = [&](double x0, double y0, double x1, double y1, double step) {
			const int n = std::max(1, static_cast<int>(std::ceil(std::hypot(x1 - x0, y1 - y0) / step)));
			for (int i = 0; i < n; ++i) {
				pts.emplace_back(x0 + (x1 - x0) * i / n, y0 + (y1 - y0) * i / n);
			}
		};
		constexpr double R = 19.3, RX = 15.2, LX = -15.4, SX = -26.4, ST = 26.4, SB = 182.8;
		const double aR = std::atan2(-std::sqrt(R * R - RX * RX), RX);                 // where the right side meets the bulb
		const double aL = std::atan2(-std::sqrt(R * R - LX * LX), LX) + 2.0 * std::numbers::pi;
		seg(0.0, -1.0, RX, 18.8, 9.0);
		seg(RX, 18.8, RX, BULB_Y + R * std::sin(aR), 13.0);
		for (int i = 0; i < 7; ++i) {                                                    // round the bulb
			const double a = aR + (aL - aR) * i / 7.0;
			pts.emplace_back(R * std::cos(a), BULB_Y + R * std::sin(a));
		}
		seg(LX, BULB_Y + R * std::sin(aL), LX, SB, 9.0);
		seg(LX, SB, SX, SB, 11.0);
		seg(SX, SB, SX, ST, 14.0);
		seg(SX, ST, LX, ST, 11.0);
		seg(LX, ST, LX, 18.8, 8.0);
		seg(LX, 18.8, 0.0, -1.0, 9.0);
		return pts;
	}

	// over the frame, never inside it: the ice shell, its crack and the icicles
	void DrawIce(GFx& g, double a_s)
	{
		if (a_s <= 0.0) {
			return;
		}
		static const auto outline = GaugeOutline();
		const std::size_t n = outline.size();
		std::vector<std::pair<double, double>> outer(n);
		for (std::size_t i = 0; i < n; ++i) {
			const auto [x0, y0] = outline[(i + n - 1) % n];
			const auto [x1, y1] = outline[(i + 1) % n];
			double nx = y1 - y0, ny = -(x1 - x0);  // outward for a clockwise outline with y down
			const double l = std::hypot(nx, ny);
			nx /= l;
			ny /= l;
			const auto [x, y] = outline[i];
			const double h = std::fmod(std::sin(i * 12.9898) * 43758.5453, 1.0);  // a fixed jitter per point
			double t = 3.6 + 4.4 * std::abs(h);
			if (y < 46.0 && x > 0.0) {
				t *= 1.9;  // the thick shoulder, upper right
			}
			outer[i] = { x + nx * t * a_s, y + ny * t * a_s };
		}
		// a dark edge under the shell so it reads on snow, then the facets: faces turned to the upper left (the
		// light) pale, the rest alternating body and dark
		Line(g, 2.2, 0x1E2A32, 45.0 * a_s);
		MoveTo(g, outer[0].first, outer[0].second);
		for (std::size_t i = 1; i <= n; ++i) {
			LineTo(g, outer[i % n].first, outer[i % n].second);
		}
		NoLine(g);
		for (std::size_t i = 0; i < n; ++i) {
			const std::size_t j = (i + 1) % n;
			const double fx = (outer[i].first + outer[j].first) / 2 - (outline[i].first + outline[j].first) / 2;
			const double fy = (outer[i].second + outer[j].second) / 2 - (outline[i].second + outline[j].second) / 2;
			const double lit = (-0.7 * fx - 0.7 * fy) / std::max(std::hypot(fx, fy), 0.001);
			const double hj = std::abs(std::fmod(std::sin(i * 78.233) * 12543.53, 1.0));
			const std::uint32_t rgb = lit > 0.3 ? (hj > 0.5 ? 0xC4DDE7 : 0xAACBD8) : lit < -0.3 ? (hj > 0.5 ? 0x4C6878 : 0x5A7686) : (hj > 0.5 ? 0x8DB0C0 : 0x7FA2B3);
			Fill(g, rgb, 88.0 * std::min(1.0, a_s * 1.5));
			MoveTo(g, outline[i].first, outline[i].second);
			LineTo(g, outline[j].first, outline[j].second);
			LineTo(g, outer[j].first, outer[j].second);
			LineTo(g, outer[i].first, outer[i].second);
			LineTo(g, outline[i].first, outline[i].second);
			EndFill(g);
		}
		// the cold off-white edge along the outside
		Line(g, 0.9, 0xE6F2F7, 85.0 * a_s);
		MoveTo(g, outer[0].first, outer[0].second);
		for (std::size_t i = 1; i <= n; ++i) {
			LineTo(g, outer[i % n].first, outer[i % n].second);
		}
		// one crack across the shoulder
		Line(g, 0.8, 0x22323C, 85.0 * a_s);
		MoveTo(g, 15.4, 30.0);
		LineTo(g, 18.0, 33.5);
		LineTo(g, 17.0, 37.0);
		LineTo(g, 20.0, 40.5);
		NoLine(g);
		// icicles hanging under the scale and the bulb
		constexpr double ICI[3][3]{ { -22.5, 182.8, 7.5 }, { -18.0, 182.8, 4.5 }, { 6.0, BULB_Y + 19.3, 5.5 } };
		for (const auto& c : ICI) {
			const double len = c[2] * a_s;
			Fill(g, 0x9CC0CE, 90.0 * std::min(1.0, a_s * 1.5));
			MoveTo(g, c[0] - 1.7, c[1] - 0.2);
			LineTo(g, c[0] + 1.7, c[1] - 0.2);
			LineTo(g, c[0] + 0.2, c[1] + len);
			LineTo(g, c[0] - 1.7, c[1] - 0.2);
			EndFill(g);
			Line(g, 0.7, 0xE6F2F7, 80.0 * a_s);
			MoveTo(g, c[0] - 1.5, c[1]);
			LineTo(g, c[0] + 0.1, c[1] + len * 0.9);
			NoLine(g);
		}
	}

	// ---- fire: the soft particle fire (FireFx.inl) over the gauge, which itself glows red-hot under it
	double FxSmooth(double a_e0, double a_e1, double a_x)
	{
		const double t = std::clamp((a_x - a_e0) / (a_e1 - a_e0), 0.0, 1.0);
		return t * t * (3.0 - 2.0 * t);
	}
	double FxFrac(double a_x) { return a_x - std::floor(a_x); }

	// the gauge glowing red-hot under the fire (drawn once in its own clip; the clip's alpha follows the fire)
	void DrawFireTint(RE::GFxMovieView* a_movie, GFx& g)
	{
		NoLine(g);
		GradFill(a_movie, g, false, { { 0xFFA040, 72.0, 0.0 }, { 0xFF7A3C, 68.0, 0.3 }, { 0xFF5048, 55.0, 0.45 }, { 0xF04450, 52.0, 0.7 }, { 0xEE3A4C, 62.0, 1.0 } },
			-145.0, -45.0, 290.0, 290.0, std::numbers::pi / 2.0);  // a turned linear gradient runs over the box's width: y -45 .. 245;
		// glowing hot all the way up (it burns inside the fire): orange high up, red-hot low down
		static const auto shape = GaugeOutline();  // the gauge's real outline (not the gap under the scale)
		MoveTo(g, shape[0].first, shape[0].second);
		for (std::size_t i = 1; i <= shape.size(); ++i) {
			LineTo(g, shape[i % shape.size()].first, shape[i % shape.size()].second);
		}
		EndFill(g);
	}

	#include "FireFx.inl"

	// one blob sprite, drawn once: a soft round gradient of radius 50 in the pass's colour, full in the middle,
	// fading out from "firm" to the edge (the sprite is then scaled to the blob's size)
	void DrawFireSprite(RE::GFxMovieView* a_movie, GFx& g, int a_pass)
	{
		const auto& P = FIRE_PASSES[a_pass];
		const double f = P.firm;
		NoLine(g);
		GradFill(a_movie, g, true,
			{ { P.rgb, 100.0, 0.0 }, { P.rgb, 100.0, f }, { P.rgb, 62.0, f + (1.0 - f) * 0.35 }, { P.rgb, 22.0, f + (1.0 - f) * 0.7 },
				{ P.rgb, 0.0, 1.0 } },
			-50.0, -50.0, 100.0, 100.0);
		CirclePath(g, 0.0, 0.0, 50.0);
		EndFill(g);
	}

	// ---- frost, Zelda's way (his reference video of the BotW gauge frozen, 2026-11-05): the gauge sealed in a clean
	// ice crystal: a long hexagon a little bigger than the gauge with small notched corners like a snowflake, a crisp
	// white edge (a faint dark line under it so it reads on snow), an inner bevel, a pale frost wash over the face
	// with the readout faintly showing, two glints. Light and clean, no dark facets. It snaps on and off.
	void DrawIceCase(GFx& g, double a_s)
	{
		if (a_s <= 0.0) {
			return;
		}
		// the hexagon, clockwise from the top, round the scale, tube and bulb
		constexpr double V[6][2]{ { -3.0, -15.0 }, { 29.5, 6.0 }, { 30.0, 208.0 }, { 0.0, 236.0 }, { -32.5, 196.0 }, { -32.5, 12.0 } };
		const double k = 0.94 + 0.06 * std::min(a_s, 1.0);  // snaps in from a touch smaller
		constexpr double CX = -5.0, CY = 110.0;
		const auto at = [&](double x, double y, double inset) {
			const double dx = x - CX, dy = y - CY, l = std::max(std::hypot(dx, dy * 0.35), 0.001);
			return std::pair{ CX + dx * k - dx / l * inset, CY + dy * k - dy * 0.35 / l * inset };
		};
		// each corner gets a small notch: in along one side, a short cut inward, back out along the other
		const auto path = [&](double inset) {
			std::vector<std::pair<double, double>> p;
			for (int i = 0; i < 6; ++i) {
				const auto& a = V[(i + 5) % 6];
				const auto& v = V[i];
				const auto& b = V[(i + 1) % 6];
				const double la = std::hypot(a[0] - v[0], a[1] - v[1]), lb = std::hypot(b[0] - v[0], b[1] - v[1]);
				const double cut = 3.2;
				const double ax = v[0] + (a[0] - v[0]) / la * cut, ay = v[1] + (a[1] - v[1]) / la * cut;
				const double bx = v[0] + (b[0] - v[0]) / lb * cut, by = v[1] + (b[1] - v[1]) / lb * cut;
				const double mx = (ax + bx) / 2 + (CX - v[0]) * 0.012, my = (ay + by) / 2 + (CY - v[1]) * 0.012;
				p.push_back(at(ax, ay, inset));
				p.push_back(at(mx, my, inset));
				p.push_back(at(bx, by, inset));
			}
			return p;
		};
		const auto trace = [&](const std::vector<std::pair<double, double>>& p) {
			MoveTo(g, p[0].first, p[0].second);
			for (std::size_t i = 1; i <= p.size(); ++i) {
				LineTo(g, p[i % p.size()].first, p[i % p.size()].second);
			}
		};
		const double a = std::min(1.0, a_s);
		const auto outer = path(0.0), inner = path(3.6);
		// the frost wash over the face
		NoLine(g);
		Fill(g, 0xE6F3FA, 58.0 * a);
		trace(outer);
		EndFill(g);
		// a lighter band just inside the edge (the bevel)
		Line(g, 1.3, 0xFFFFFF, 45.0 * a);
		trace(inner);
		// the crisp edge over a faint dark line
		Line(g, 3.6, 0x24343E, 38.0 * a);
		trace(outer);
		Line(g, 1.9, 0xFFFFFF, 96.0 * a);
		trace(outer);
		NoLine(g);
		// two glints, upper left (where the light comes from)
		Fill(g, 0xFFFFFF, 40.0 * a);
		const auto quad = [&](double x0, double y0, double x1, double y1, double w) {
			const auto p0 = at(x0, y0, 0.0), p1 = at(x1, y1, 0.0);
			MoveTo(g, p0.first, p0.second);
			LineTo(g, p0.first + w, p0.second);
			LineTo(g, p1.first + w, p1.second);
			LineTo(g, p1.first, p1.second);
			LineTo(g, p0.first, p0.second);
		};
		quad(-8.5, 62.0, -2.0, 44.0, 2.4);   // on the tube's dark upper part, clear of the scale's colours
		quad(-8.5, 80.0, 1.0, 54.0, 1.3);
		EndFill(g);
	}
