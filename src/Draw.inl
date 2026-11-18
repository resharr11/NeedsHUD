// Drawing helpers: thin wrappers over the ActionScript 2 drawing API of a MovieClip in the HUD movie
// (lineStyle / beginFill / moveTo / lineTo / curveTo / endFill), the same calls TrueHUD makes from C++.
// Colours are 0xRRGGBB, alpha is 0-100 (ActionScript 2), sizes are in the clip's own units.
// Everything here runs on the UI thread (SKSE AddUITask).

	using GFx = RE::GFxValue;

	void Line(GFx& a_clip, double a_width, std::uint32_t a_rgb, double a_alpha = 100.0)
	{
		const GFx args[3]{ a_width, static_cast<double>(a_rgb), a_alpha };
		a_clip.Invoke("lineStyle", nullptr, args, 3);
	}
	void NoLine(GFx& a_clip)
	{
		a_clip.Invoke("lineStyle", nullptr, nullptr, 0);
	}
	void Fill(GFx& a_clip, std::uint32_t a_rgb, double a_alpha = 100.0)
	{
		const GFx args[2]{ static_cast<double>(a_rgb), a_alpha };
		a_clip.Invoke("beginFill", nullptr, args, 2);
	}
	void EndFill(GFx& a_clip)
	{
		a_clip.Invoke("endFill", nullptr, nullptr, 0);
	}
	void MoveTo(GFx& a_clip, double a_x, double a_y)
	{
		const GFx args[2]{ a_x, a_y };
		a_clip.Invoke("moveTo", nullptr, args, 2);
	}
	void LineTo(GFx& a_clip, double a_x, double a_y)
	{
		const GFx args[2]{ a_x, a_y };
		a_clip.Invoke("lineTo", nullptr, args, 2);
	}
	void CurveTo(GFx& a_clip, double a_cx, double a_cy, double a_x, double a_y)
	{
		const GFx args[4]{ a_cx, a_cy, a_x, a_y };
		a_clip.Invoke("curveTo", nullptr, args, 4);
	}

	// the outline of a rounded rectangle (the caller sets the line and fill first)
	void RoundRectPath(GFx& a_clip, double a_x, double a_y, double a_w, double a_h, double a_r)
	{
		a_r = std::min({ a_r, a_w / 2.0, a_h / 2.0 });
		const double r = a_x + a_w, b = a_y + a_h;
		MoveTo(a_clip, a_x + a_r, a_y);
		LineTo(a_clip, r - a_r, a_y);
		CurveTo(a_clip, r, a_y, r, a_y + a_r);
		LineTo(a_clip, r, b - a_r);
		CurveTo(a_clip, r, b, r - a_r, b);
		LineTo(a_clip, a_x + a_r, b);
		CurveTo(a_clip, a_x, b, a_x, b - a_r);
		LineTo(a_clip, a_x, a_y + a_r);
		CurveTo(a_clip, a_x, a_y, a_x + a_r, a_y);
	}

	// a circle from 8 quadratic curves
	void CirclePath(GFx& a_clip, double a_cx, double a_cy, double a_r)
	{
		constexpr double step = std::numbers::pi / 4.0;
		const double cr = a_r / std::cos(step / 2.0);  // control points sit outside the circle
		MoveTo(a_clip, a_cx + a_r, a_cy);
		for (int i = 1; i <= 8; ++i) {
			const double a = i * step, m = a - step / 2.0;
			CurveTo(a_clip, a_cx + cr * std::cos(m), a_cy + cr * std::sin(m), a_cx + a_r * std::cos(a), a_cy + a_r * std::sin(a));
		}
	}

	void FillRect(GFx& a_clip, double a_x, double a_y, double a_w, double a_h, double a_r, std::uint32_t a_rgb, double a_alpha = 100.0)
	{
		NoLine(a_clip);
		Fill(a_clip, a_rgb, a_alpha);
		RoundRectPath(a_clip, a_x, a_y, a_w, a_h, a_r);
		EndFill(a_clip);
	}

	// a stroke with a dark edge under it, so a bright line still reads on snow (the mockups' edge_and_line)
	void EdgedRect(GFx& a_clip, double a_x, double a_y, double a_w, double a_h, double a_r, double a_width, std::uint32_t a_rgb)
	{
		Line(a_clip, a_width + 2.4, 0x000000, 70.0);
		RoundRectPath(a_clip, a_x, a_y, a_w, a_h, a_r);
		Line(a_clip, a_width, a_rgb);
		RoundRectPath(a_clip, a_x, a_y, a_w, a_h, a_r);
	}
	void EdgedCircle(GFx& a_clip, double a_cx, double a_cy, double a_r, double a_width, std::uint32_t a_rgb)
	{
		Line(a_clip, a_width + 2.4, 0x000000, 70.0);
		CirclePath(a_clip, a_cx, a_cy, a_r);
		Line(a_clip, a_width, a_rgb);
		CirclePath(a_clip, a_cx, a_cy, a_r);
	}

	// ---- text: a TextField in the game's own HUD font, with a dark copy under it as its shadow ----

	GFx MakeString(RE::GFxMovieView* a_movie, const std::string& a_text)
	{
		GFx value;
		a_movie->CreateString(&value, a_text.c_str());
		return value;
	}

	// creates (or finds) a text field named a_name inside a_parent, styled once
	GFx TextField(RE::GFxMovieView* a_movie, GFx& a_parent, const char* a_name, double a_depth, double a_x, double a_y, double a_w,
		double a_size, std::uint32_t a_rgb)
	{
		GFx field;
		if (a_parent.GetMember(a_name, &field) && field.IsDisplayObject()) {
			return field;
		}
		const GFx args[6]{ MakeString(a_movie, a_name), a_depth, a_x, a_y, a_w, a_size * 1.6 };
		a_parent.Invoke("createTextField", nullptr, args, 6);
		if (!a_parent.GetMember(a_name, &field) || !field.IsDisplayObject()) {
			return field;
		}
		field.SetMember("embedFonts", GFx(true));
		field.SetMember("selectable", GFx(false));
		GFx format;
		a_movie->CreateObject(&format, "TextFormat");
		format.SetMember("font", MakeString(a_movie, "$EverywhereMediumFont"));  // the HUD's own font (gfxfontlib.swf)
		format.SetMember("size", GFx(a_size));
		format.SetMember("color", GFx(static_cast<double>(a_rgb)));
		format.SetMember("align", MakeString(a_movie, "center"));
		format.SetMember("letterSpacing", GFx(0.8));
		field.Invoke("setNewTextFormat", nullptr, &format, 1);
		return field;
	}

	void SetText(RE::GFxMovieView* a_movie, GFx& a_field, const std::string& a_text)
	{
		if (a_field.IsDisplayObject()) {
			a_field.SetMember("text", MakeString(a_movie, tr::T(a_text)));  // every HUD text goes through the translation hook
		}
	}

	// ---- curves and gradients ----

	// continues the current path along a circle from angle a0 to a1 (radians, y down; any direction), in pieces of
	// at most 45 degrees, each one quadratic curve (the current point must already be at angle a0)
	void ArcTo(GFx& a_clip, double a_cx, double a_cy, double a_r, double a_a0, double a_a1)
	{
		const int    n = std::max(1, static_cast<int>(std::ceil(std::abs(a_a1 - a_a0) / (std::numbers::pi / 4.0))));
		const double step = (a_a1 - a_a0) / n;
		const double cr = a_r / std::cos(step / 2.0);
		for (int i = 1; i <= n; ++i) {
			const double a = a_a0 + step * i, m = a - step / 2.0;
			CurveTo(a_clip, a_cx + cr * std::cos(m), a_cy + cr * std::sin(m), a_cx + a_r * std::cos(a), a_cy + a_r * std::sin(a));
		}
	}

	// an ellipse turned by a_rot radians, from 8 quadratic curves
	void EllipsePath(GFx& a_clip, double a_cx, double a_cy, double a_rx, double a_ry, double a_rot)
	{
		const auto pt = [&](double a, double s) {
			const double x = a_rx * s * std::cos(a), y = a_ry * s * std::sin(a);
			return std::pair{ a_cx + x * std::cos(a_rot) - y * std::sin(a_rot), a_cy + x * std::sin(a_rot) + y * std::cos(a_rot) };
		};
		constexpr double step = std::numbers::pi / 4.0;
		const double ctl = 1.0 / std::cos(step / 2.0);
		auto [x0, y0] = pt(0.0, 1.0);
		MoveTo(a_clip, x0, y0);
		for (int i = 1; i <= 8; ++i) {
			auto [cx, cy] = pt(step * i - step / 2.0, ctl);
			auto [x, y] = pt(step * i, 1.0);
			CurveTo(a_clip, cx, cy, x, y);
		}
	}

	struct Stop
	{
		std::uint32_t rgb;
		double        alpha;  // 0-100
		double        at;     // 0-1 along the gradient
	};

	// beginGradientFill with a "box" matrix: a linear gradient runs across the box's width, turned by a_rot
	// (pi/2 = top to bottom); a radial one fills the box (circle or ellipse)
	void GradFill(RE::GFxMovieView* a_movie, GFx& a_clip, bool a_radial, std::initializer_list<Stop> a_stops, double a_x, double a_y,
		double a_w, double a_h, double a_rot = 0.0)
	{
		GFx colors, alphas, ratios, matrix;
		a_movie->CreateArray(&colors);
		a_movie->CreateArray(&alphas);
		a_movie->CreateArray(&ratios);
		for (const auto& s : a_stops) {
			colors.PushBack(GFx(static_cast<double>(s.rgb)));
			alphas.PushBack(GFx(s.alpha));
			ratios.PushBack(GFx(std::clamp(s.at, 0.0, 1.0) * 255.0));
		}
		a_movie->CreateObject(&matrix);
		matrix.SetMember("matrixType", MakeString(a_movie, "box"));
		matrix.SetMember("x", GFx(a_x));
		matrix.SetMember("y", GFx(a_y));
		matrix.SetMember("w", GFx(a_w));
		matrix.SetMember("h", GFx(a_h));
		matrix.SetMember("r", GFx(a_rot));
		const GFx args[5]{ MakeString(a_movie, a_radial ? "radial" : "linear"), colors, alphas, ratios, matrix };
		a_clip.Invoke("beginGradientFill", nullptr, args, 5);
	}

	// ---- the affliction badge (his pick 2026-11-08: option A, a small round badge on the need's own gauge; "player just
	// need to see that something is weird": the symbol only has to read as "something's off", the details are in the
	// game's active effects). Survival's afflictions: 0 Weakened (hunger: a broken blade), 1 Addled (tiredness: a
	// spiral), 2 Frostbitten (cold: a snowflake). Dark red-brown disc with the warning red ring, bone symbol.
	void DrawAfflictionBadge(GFx& g, double cx, double cy, double r, int a_kind)
	{
		constexpr std::uint32_t RING = 0xE0705A, DISC = 0x3A221E, MARK = 0xECE7DB;
		NoLine(g);
		Fill(g, 0x000000, 80.0);
		CirclePath(g, cx, cy, r + r * 0.22);
		EndFill(g);
		Line(g, r * 0.18, RING);
		Fill(g, DISC);
		CirclePath(g, cx, cy, r);
		EndFill(g);
		const double s = r / 17.0;  // the mockup's symbol was drawn for r = 17
		Line(g, 3.6 * s, MARK);
		if (a_kind == 0) {  // a blade snapped in two, and its crossguard
			MoveTo(g, cx - 9 * s, cy + 9 * s);
			LineTo(g, cx - 1 * s, cy + 1 * s);
			MoveTo(g, cx + 3 * s, cy - 3 * s);
			LineTo(g, cx + 9 * s, cy - 9 * s);
			Line(g, 3.0 * s, MARK);
			MoveTo(g, cx - 12 * s, cy + 4 * s);
			LineTo(g, cx - 4 * s, cy + 12 * s);
		} else if (a_kind == 1) {  // a spiral (a dizzy, addled head)
			Line(g, 3.0 * s, MARK);
			for (int i = 1; i < 48; ++i) {
				const double t = i * 0.2, rr = t * 1.6 * s;
				const double x = cx + std::cos(t) * rr, y = cy + std::sin(t) * rr;
				if (i == 1) {
					MoveTo(g, x, y);
				} else {
					LineTo(g, x, y);
				}
			}
		} else {  // a snowflake: three crossing strokes
			Line(g, 3.0 * s, MARK);
			for (int a = 0; a < 3; ++a) {
				const double t = a * std::numbers::pi / 3.0, dx = std::cos(t) * 11 * s, dy = std::sin(t) * 11 * s;
				MoveTo(g, cx - dx, cy - dy);
				LineTo(g, cx + dx, cy + dy);
			}
		}
		NoLine(g);
	}
