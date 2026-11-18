// ---- hit by fire, third way: a soft particle fire (his "go on", 2026-11-05, after the full research on how real
// fire behaves). The vector-outline fire had hit its limit (hard edges, droplet-like bits). Real fire is not a
// fountain of separate drops (Zukoski 1985; McCaffrey 1979; Xia & Zhang 2018; Krivokorytov 2012): a steady,
// connected body low down, and rising swirls that swell its top into a bulge, squeeze a neck under it and let the
// piece go; that piece is one big shape that rises and burns out fast, getting dimmer as it cools (no top edge).
// Edges are soft because soot thins out gradually, so a bright, fairly firm flame with a wide faint glow round it.
// One shared moving air field bends everything together (correlated, not random per particle).
//
// Built from soft radial-gradient blobs (Scaleform in Skyrim can't blur a clip): FireBlobs only works out where
// every blob is for a time t (no state, so any frame rate gives the same fire); the gauge draws each blob as a
// sprite clip made once and then only moved, scaled, turned and faded. Colours by pass, drawn in this order so
// all blobs of one colour merge into one mass: soft glow, yellow-orange body, orange inside, red-pink low down,
// and the thin bright strands on top. Zelda's look (his benchmark): yellow-orange top and rim, red-pink low.
//
// Numbers for the moving parts are from the research's VFX mapping (art-directed, not lab values): a new tongue
// about 7 times a second, 3-5 alive at once, each 0.38-0.6 s, growing wide, then tall, then thin; it is held to
// the body for its first ~45%, then lets go and rises; main pulse about 2 Hz (the real 4 Hz needs a faster
// redraw than we have to look smooth).

	enum FirePass : int { FP_GLOW = 0, FP_BODY, FP_ORANGE, FP_RED, FP_STRAND, FP_COUNT };
	// the fire is drawn twice: fully behind the gauge, and see-through in front of it, so the gauge burns inside the
	// fire instead of the fire sitting on top of it (his note, 2026-11-05: "over the object, not coming from it")
	constexpr double FIRE_FRONT = 0.42;

	struct FirePassLook
	{
		std::uint32_t rgb;
		double        firm;    // where the blob starts to fade (0..1 of its radius): higher = firmer edge
		int           most;    // how many blobs of this pass at most (the sprite pool)
	};
	constexpr FirePassLook FIRE_PASSES[FP_COUNT]{
		{ 0xFF8A3A, 0.00, 12 },  // the wide faint glow
		{ 0xFFAA3A, 0.68, 110 },  // yellow-orange: the flame itself
		{ 0xFF7428, 0.15, 56 },  // orange inside
		{ 0xF04650, 0.20, 12 },  // red-pink low down
		{ 0xFFC848, 0.55, 22 },  // thin bright strands
	};

	struct FireBlob
	{
		int    pass;
		double x, y;    // centre, gauge units (y down)
		double rx, ry;  // half width across, half length along its direction
		double rot;     // degrees, clockwise: 0 = its length points straight down/up
		double alpha;   // 0..1
	};

	inline double FxHash(double a_n) { return FxFrac(std::sin(a_n * 12.9898 + 4.1414) * 43758.5453); }

	// a piecewise-linear curve through six evenly spaced points (u 0..1)
	inline double FxCurve6(const double (&a_k)[6], double a_u)
	{
		const double f = std::clamp(a_u, 0.0, 1.0) * 5.0;
		const int    i = std::min(static_cast<int>(f), 4);
		return a_k[i] + (a_k[i + 1] - a_k[i]) * (f - i);
	}

	// the shared moving air: a side push that travels upward (crests move to lower y as time goes on), stronger
	// higher up
	inline double FireAirX(double a_y, double a_t)
	{
		constexpr double TAU = 2.0 * std::numbers::pi;
		const double k = 0.35 + 0.65 * (1.0 - FxSmooth(-20.0, 170.0, a_y));
		return k * (3.2 * std::sin(0.045 * a_y + TAU * 1.05 * a_t) + 1.7 * std::sin(0.11 * a_y + TAU * 1.9 * a_t + 1.3) +
		               0.8 * std::sin(0.23 * a_y + TAU * 3.1 * a_t + 2.1));
	}

	// the wind (his note, 2026-11-05: Zelda's tendrils bend as if the fire meets the wind): a gusty side wind,
	// -1..1, that turns every second or so. A tongue bends downwind more toward its tip, and each point feels the
	// wind of a moment ago (the gas that is there now left the base earlier), so a turning gust whips through the
	// tongue as an S and the tip trails behind
	inline double FireWind(double a_t)
	{
		constexpr double TAU = 2.0 * std::numbers::pi;
		return 0.55 * std::sin(TAU * 0.37 * a_t) + 0.33 * std::sin(TAU * 0.83 * a_t + 1.2) + 0.17 * std::sin(TAU * 1.9 * a_t + 0.4);
	}

	// the body's half width at height y (gauge units): round, widest over the upper part (his reference)
	inline double FireBodyHalfW(double a_y)
	{
		return 26.0 + 8.0 * std::exp(-std::pow((a_y - 62.0) / 52.0, 2.0)) - 12.0 * (1.0 - FxSmooth(-2.0, 40.0, a_y));
	}
	constexpr double FIRE_CX = -5.5;  // the middle of tube + scale

	// height fade: the higher it rises, the more it has cooled (no top edge)
	inline double FireCool(double a_y) { return 1.0 - FxSmooth(-10.0, -80.0, a_y); }

	void FireBlobs(double a_s, double a_t, std::vector<FireBlob>& a_out)
	{
		a_out.clear();
		if (a_s <= 0.0) {
			return;
		}
		constexpr double TAU = 2.0 * std::numbers::pi;
		// growing in and dying down: the fire's top sinks toward the base (a fire dies down, it doesn't turn
		// see-through: fading every blob together shows their overlaps), and it only fades right at the end
		const double drop = (1.0 - std::min(a_s, 1.0)) * 110.0;
		const double sz = 0.6 + 0.4 * std::min(a_s, 1.0);
		const double al = std::min(1.0, a_s * 4.0);

		// ---- the body: nine rows up the gauge, two overlapping blobs each (left and right, each on its own
		// rhythm, so the outline is never a stack of rings), breathing, with bulges travelling up about twice a second
		// the rows rise all the time (hot gas rising, the 2026-11-05 AI reviews: a body that only pulses reads as a
		// candle): each row climbs one slot every 0.45 s; the top one swells and fades out as it leaves, a new one
		// comes in at the bottom (hidden in the red glow)
		constexpr int    K = 7;
		constexpr double SLOT = 20.0, CLIMB = SLOT / 0.45;
		const double scroll = a_t * CLIMB / SLOT;
		const double slide = FxFrac(scroll);
		for (int k = 0; k < K; ++k) {
			const double row = std::floor(scroll) + K - k;  // the row's own identity, kept as it climbs
			const double y0 = 8.0 + (k - slide) * SLOT;
			const double leave = 1.0 - FxSmooth(-6.0, 10.0, y0);  // 0 while in the body, 1 once gone above it
			const double bulge = std::pow(std::max(0.0, std::sin(0.05 * y0 + TAU * 2.0 * a_t + 0.6)), 2.0);
			const double yb = y0 - 3.0 * bulge;
			const double hw = FireBodyHalfW(yb) * (1.0 + 0.14 * bulge * (1.0 - FxSmooth(60.0, 170.0, yb))) * sz;
			// one side swells while the other draws in, then they swap (ChatGPT's review: both edges in step read as stacked beads)
			const double swap = 3.0 * std::sin(TAU * 0.7 * a_t + 0.035 * y0) + 2.0 * std::sin(TAU * 1.6 * a_t - 0.05 * y0 + 1.0);
			const double air = swap + FireAirX(yb, a_t) * 0.55 + 7.0 * FireWind(a_t - 0.002 * (190.0 - yb)) * (1.0 - FxSmooth(0.0, 170.0, yb));
			const double low = FxSmooth(50.0, 112.0, yb);  // the flames sit on the upper part; below, the gauge only glows red-hot (his rule)
			double top = FxSmooth(drop - 14.0, drop + 14.0, yb) * (1.0 - leave);  // dying down; leaving at the top
			a_out.push_back({ FP_GLOW, FIRE_CX + air, yb, hw * 1.35, 30.0 * sz, 0.0, 0.16 * al * top * (1.0 - low) });
			// the core of the row: one wide blob in the middle (hidden inside the mass)
			{
				const double ph = row * 1.7;
				const double breathe = 1.0 + 0.06 * std::sin(TAU * 0.9 * a_t + ph);
				a_out.push_back({ FP_BODY, FIRE_CX + air, yb, hw * (0.72 - 0.3 * leave) * breathe, 26.0 * sz * breathe * (1.0 - 0.3 * leave), 0.0,
					0.97 * al * top * (1.0 - low) });
			}
			// the outline: tall narrow licks along both edges, tilted up and out, each on its own rhythm (round blobs
			// on the edge read as a row of semicircles: his note, 2026-11-05)
			for (int side = -1; side <= 1; side += 2) {
				for (int e = 0; e < 2; ++e) {
					const double id = row * 7.3 + side * 2.1 + e * 3.9;
					const double ph = id * 1.618;
					const double flick = 0.5 + 0.5 * std::sin(TAU * (1.6 + 0.9 * FxHash(id)) * a_t + ph);  // reaches out and draws back
					const double y = yb - 6.0 + e * 12.0 + 4.0 * (FxHash(id + 0.5) - 0.5);
					const double reach = (0.6 + 0.08 * flick) * (0.55 + 0.45 * FxSmooth(-12.0, 30.0, y0));  // close together on top
					const double x = FIRE_CX + air + side * hw * reach;
					const double rx = hw * (0.27 + 0.05 * flick) * (1.0 - 0.4 * leave) * (0.6 + 0.4 * FxSmooth(-12.0, 30.0, y0));  // smaller on top (a crown of points read as horns)
					const double ry = (18.0 + 5.0 * flick) * sz * (1.0 - 0.3 * leave);
					a_out.push_back({ FP_BODY, x, y, rx, ry, side * (6.0 + 6.0 * flick) * FxSmooth(-12.0, 40.0, y0), 0.97 * al * top * (1.0 - low) });
				}
			}
			a_out.push_back({ FP_ORANGE, FIRE_CX + air, yb + 5.0, hw * 0.72, 30.0 * sz, 0.0, 0.78 * al * top * FxSmooth(4.0, 70.0, yb) * (1.0 - FxSmooth(72.0, 108.0, yb)) });
			if (yb > 30.0) {
				a_out.push_back({ FP_RED, FIRE_CX + air, yb + 8.0, hw * (0.62 + 0.18 * FxSmooth(40.0, 150.0, yb)), 24.0 * sz, 0.0, 0.7 * al * FxSmooth(30.0, 80.0, yb) * FxSmooth(drop - 20.0, drop + 10.0, yb) * (1.0 - FxSmooth(84.0, 112.0, yb)) });
			}
		}

		// ---- tongues: each one a chain of linked blobs (they move together, like one swirl), born every ~0.14 s
		constexpr double EVERY = 0.1;
		constexpr int    M = 10;  // blobs per tongue
		const auto tongue = [&](double a_id, double a_u, double a_rootX, double a_rootY, double a_len, double a_w, double a_lean,
								double a_curl, double a_wind, int a_pass, double a_firmAlpha, bool a_inner = false, double a_hook = 0.0) {
			static constexpr double LEN_K[6]{ 0.7, 0.9, 1.3, 1.5, 1.2, 0.6 };   // tall over life
			static constexpr double WID_K[6]{ 0.8, 1.15, 1.0, 0.8, 0.6, 0.35 };  // wide, then thinner, never a needle
			static constexpr double ALP_K[6]{ 0.6, 1.0, 1.0, 1.0, 0.9, 0.0 };  // solid; fades only at the very end
			const double L = a_len * FxCurve6(LEN_K, a_u) * sz;
			const double W = a_w * FxCurve6(WID_K, a_u) * sz;
			const double A = FxCurve6(ALP_K, a_u) * al * a_firmAlpha;
			if (A <= 0.01 || L < 1.0) {
				return;
			}
			// held to the body early, then let go: the root lifts away (fast at first) and the neck pinches to nothing
			const double let = FxSmooth(0.3, 1.0, a_u);
			const double lift = a_len * sz * 0.35 * let;  // its mass rises a little, then it sinks back in (no flying drops)
			const double neck = 0.4 * FxSmooth(0.45, 0.8, a_u);
			const double ph = a_id * 2.399;
			double px = a_rootX, py = a_rootY - lift;
			const double seg = L / (M - 1);
			double prevX = px, prevY = py;
			for (int j = 0; j < M; ++j) {
				const double t = static_cast<double>(j) / (M - 1);
				if (j > 0) {
					// leans out at the root, turns up with height (buoyancy), curls on its own slow rhythm
					const double a = a_lean * (1.0 - 0.85 * t) + a_curl * std::sin(TAU * (0.9 * a_t + 0.8 * t) + ph) * t +
					                 a_wind * 0.6 * FireWind(a_t - 0.22 * t) * t +
					                 a_hook * std::sin(std::numbers::pi * std::clamp(a_u * 1.15, 0.0, 1.0)) * FxSmooth(0.55, 1.0, t);  // a brief hook over at the top, at its peak (ChatGPT's read of Zelda)
					px += std::sin(a) * seg;
					py -= std::cos(a) * seg;
				}
				const double ax = px + FireAirX(py, a_t) * (0.25 + 0.75 * t);  // the shared air bends it, the tip most
				double prof = (0.38 + 0.62 * std::pow(std::max(1.0 - t, 0.0), 0.9)) * (0.95 + 0.35 * std::exp(-std::pow((t - 0.2) / 0.2, 2.0)));  // a soft, rounded tip that fades, not a point
				prof *= 1.0 - neck * (1.0 - FxSmooth(0.0, 0.5, t));            // the neck: a loose piece thins to a point below
				const double cool = FireCool(py);
				const double rx = W * prof * (0.45 + 0.55 * cool);  // cooling high up: it thins rather than turning see-through (see-through chains show bands)
				if (rx < 2.5) {  // no hair-thin bits
					prevX = ax;
					prevY = py;
					continue;
				}
				const double dx = ax - prevX, dy = py - prevY;
				const double rot = j > 0 ? std::atan2(-dx, dy) * 180.0 / std::numbers::pi + 180.0 : 0.0;
				const double ry = seg * 1.3 + rx * 0.3;
				double fade = A * std::min(1.0, cool * 2.0) * (1.0 - 0.3 * t * t);
				// a chain of see-through blobs shows stripes where they overlap, so a tongue stays solid and mostly
				// fades by thinning (its width curve); only the last of it turns see-through
				fade = std::min(1.0, fade * 1.7);
				a_out.push_back({ a_pass, ax, py, rx, ry, rot, fade });
				if (a_inner && t < 0.45 && neck < 0.5) {
					a_out.push_back({ FP_ORANGE, ax, py + 2.0, rx * 0.55, ry * 0.8, rot, fade * 0.5 * FxSmooth(18.0, 70.0, py) });
				}
				prevX = ax;
				prevY = py;
			}
		};
		const double now = a_t / EVERY;
		for (int back = 0; back < 8; ++back) {
			const double id = std::floor(now) - back;
			const double born = (id + 0.6 * FxHash(id)) * EVERY;
			const double life = 0.22 + 0.14 * FxHash(id + 0.31);
			const double u = (a_t - born) / life;
			if (u < 0.0 || u >= 1.0) {
				continue;
			}
			const double kind = FxHash(id + 0.77);
			if (kind < 0.3) {
				// out of the top dome: the big ones (every few) and medium ones; off-centre ones sit lower and lean out
				const double big = FxHash(id + 0.53) < 0.4 ? 1.0 : 0.0;
				const double off = (FxHash(id + 0.19) * 2.0 - 1.0) * 24.0;
				const double rootY = 12.0 + 0.03 * off * off + drop;
				// Zelda's tendril: short and thick at the root, bends at an elbow and its tip curls over, the way the
				// wind blows (or outward when there is little wind)
				const double w0 = FireWind(born);
				const double dir = FxHash(id + 0.61) < 0.4 ? (w0 > 0.0 ? 1.0 : -1.0) : (FxHash(id + 0.83) < 0.5 ? -1.0 : 1.0);
				const double hook = dir * (0.75 + 0.55 * FxHash(id + 0.29));  // the tip's turn, 40-75 degrees
				tongue(id, u, FIRE_CX + off, rootY, big ? 22.0 : 14.0, big ? 17.0 : 13.0, 0.022 * off + dir * 0.25, 0.08, 0.6, FP_BODY, 1.0, big > 0.0, hook);
			}
		}

		// ---- the thin bright strands (Zelda's curling yellow wisps): one at a time out of the top, long and narrow,
		// curling more, held then let go like the tongues
		constexpr double STRAND_EVERY = 1.6;
		for (int back = 0; back < 2; ++back) {
			const double id = std::floor(a_t / STRAND_EVERY) - back;
			const double born = (id + 0.3 * FxHash(id + 5.0)) * STRAND_EVERY;
			const double u = (a_t - born) / 0.4;
			if (u < 0.0 || u >= 1.0) {
				continue;
			}
			const double off = (FxHash(id + 5.5) * 2.0 - 1.0) * 20.0;
			// a strand always hooks over to one side (Zelda's curl), on top of what the wind does to it
			const double w0 = FireWind(born);
			const double dir = FxHash(id + 5.2) < 0.4 ? (w0 > 0.0 ? 1.0 : -1.0) : (FxHash(id + 5.4) < 0.5 ? -1.0 : 1.0);
			const double hook = dir * (0.9 + 0.5 * FxHash(id + 5.9));
			tongue(id + 100.0, u, FIRE_CX + off, -2.0 + 0.04 * off * off + drop, 24.0, 12.0, 0.03 * off + dir * 0.2, 0.08, 0.6, FP_BODY, 1.0, false, hook);
		}
	}
