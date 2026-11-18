// ---- the Needs group (2026-11-08, his rule): the need tiles in one row, placed together from one corner with one
// position and size (his order: hunger, thirst, tiredness, then the thermometer; thirst comes with other needs mods).
// Each tile can be hidden; the row closes up so there are no gaps. Its own clip in the HUD, its own settings page.

	// the group's options (only the UI task touches g_needsOpt); new ones from the settings page wait in
	// g_needsPending until the next tick takes them, like the thermometer's (Settings.h)
	NeedsHUD::NeedsOptions g_needsOpt;
	NeedsHUD::NeedsOptions g_needsPending;
	std::mutex             g_needsLock;
	std::atomic_bool       g_needsChanged{ false };

	constexpr const char* NEEDS_CLIP = "NeedsHUD_Needs";
	constexpr double H_CROWN = 18.0;                               // the spike crowns' reach, mockup px
	// size (2026-11-08, his "default 1X icons are too big", decided with game-UI guidance): Skyrim's HUD is small and
	// quiet and the thermometer is the star, so at 1.00x a tile is 0.65 of the mockup (about 78 x 55 px at 1080p);
	// it reads by its bar, level and red frame, not by detail (peripheral vision). The text matches the thermometer's
	// (his "the font is too big, even compared to the thermometer"): the stage name as big as the thermometer's
	// (22.5 px at 1080p, its 15 HUD units), the value line as its warmth line (18 px); smaller sizes shrink the text
	// down to 18 px and no further (Xbox Accessibility Guideline 101's PC minimum)
	constexpr double TILE_SCALE = 0.65;
	constexpr double NAME_PX = 22.5, TEXT_PX_MIN = 18.0;           // 1080p px
	constexpr double H_TEXT_Y = TILE_H / 2 + 6;                    // the stage name's top, below the tile
	constexpr double TILE_BOX_W = TILE_W + 2 * H_CROWN;            // a tile with its spike crowns (the row's step = this + spacing)
	// hunger, thirst, tiredness, disease, frostbite, vitality, dirt (his row order), drunk, body; then the config-only ones:
	// bladder, fun, social (2026-11-09) and up to eight custom needs from other authors' files (kinds 12..19)
	constexpr int    TILE_COUNT = 20;

	struct TileState
	{
		bool   made = false;
		int    shownStage = -2;
		int    shownBonus = -1;
		double shownV = -1.0;
		int    shownNumber = -1;  // the raw value last written
		int    shownCount = -1;   // the stage count last drawn
		std::string shownLabel;   // the name last written (a disease can change name at the same stage)
		int    shownAffl = -2;    // the affliction badge last drawn
		double shownTarget = -2.0;  // Last Seed's vitality target last drawn
		GFx    clip, art, nameSh, name, numSh, num;
		// an author's art (config Icons): one loaded clip per picture, over the drawing
		std::string      iconKey;     // the pictures loaded (their paths, joined)
		std::vector<GFx> icons;
		int              iconWait = 0;    // ticks the shown picture has had to load
		bool             iconMissing = false;
		std::vector<bool> iconFitted;
		int              shownIcon = -2;  // which picture shows
	};

	struct NeedsGroupState
	{
		bool      failed = false;  // the HUD refused our clip: stop trying
		int       shown = -1;      // alpha last set: 1 shown, 0 hidden, -1 not yet (or must be set again)
		AutoShow  autoShow;        // when to show (combat, stage changes): the whole row shows when any tile's stage changes
		double    shownAlpha = -1.0;
		int       layout = -1;     // which tiles were laid out (bit per tile)
		long long sourceKey = -1;  // which needs mod they were laid out for
		double    placedFor = -1.0;  // visible frame width the position was worked out for
		double    nameF = 30.0;      // the stage name's size inside a tile clip (tile units)
		TileState tiles[TILE_COUNT];
	};
	NeedsGroupState g_needs;

	// an author's pictures for a tile (config Icons): loaded once into clips over the drawing, the one for this stage shown.
	// .swf: drawn on a 128 x 128 canvas, shown at half size in the art area; .png / .jpg: fitted into the same 64 x 64 area. A picture that hasn't loaded after 3 s counts as missing (placeholder).
	// a .png picture (2026-11-09; his test: the game's loader read a .png but not a .jpg or a .dds): loaded by the game's own loader and handed to the HUD's image loader,
	// the way the book menu shows its pictures (BSScaleformExternalTexture + BSScaleformImageLoader::AddTexture), then shown
	// as "img://<path>". A bare "img://" without this gave stripes: the HUD had never been given the picture. Each picture is
	// loaded once and kept for the whole game (never released: they are a few small icons). Returns "" when the game refused it.
	// a .png's size in pixels from its header (IHDR: width and height, big-endian, at bytes 16..23); 0 x 0 if unreadable.
	// Read through Data (Mod Organizer's virtual folder shows the mod's file there too).
	std::pair<double, double> PngSize(const std::string& a_path)
	{
		std::ifstream f("Data/" + a_path, std::ios::binary);
		unsigned char b[24] = {};
		if (!f.read(reinterpret_cast<char*>(b), 24) || b[1] != 'P' || b[2] != 'N' || b[3] != 'G') {
			return { 0.0, 0.0 };
		}
		const auto be = [&](int i) { return static_cast<double>((b[i] << 24) | (b[i + 1] << 16) | (b[i + 2] << 8) | b[i + 3]); };
		return { be(16), be(20) };
	}

	std::string RegisterImage(const std::string& a_path)
	{
		static std::unordered_map<std::string, std::string> done;  // path -> url ("" = refused)
		if (const auto it = done.find(a_path); it != done.end()) {
			return it->second;
		}
		std::string url;
		const auto mgr = RE::BSScaleformManager::GetSingleton();
		const auto loader = mgr ? mgr->imageLoader.get() : nullptr;
		if (!loader) {
			logger::info("Icon {}: the HUD's image loader isn't there", a_path);
		} else {
			for (const std::string& p : { a_path, "Data/" + a_path }) {
				auto tex = new RE::BSScaleformExternalTexture();  // kept on purpose (the HUD points at it)
				const bool loaded = tex->LoadPNG(p.c_str());
				const bool added = loaded && loader->AddTexture(*tex);
				logger::info("Icon {}: the game's loader {} \"{}\", the HUD {}", a_path, loaded ? "read" : "could not read", p,
					added ? "took it" : (loaded ? "refused it" : "-"));
				if (added) {
					const char* fp = tex->filePath.c_str();
					url = std::string("img://") + ((fp && *fp) ? fp : p.c_str());
					break;  // kept: the HUD now points at it
				}
				delete tex;  // not taken: let it go
			}
		}
		done[a_path] = url;
		return url;
	}

	void SyncIcons(RE::GFxMovieView* a_movie, TileState& a_t, const std::vector<std::string>& a_icons, int a_st)
	{
		std::string key;
		for (const auto& i : a_icons) {
			key += i + "|";
		}
		if (key != a_t.iconKey) {
			for (auto& c : a_t.icons) {
				if (c.IsDisplayObject()) {
					c.Invoke("removeMovieClip", nullptr, nullptr, 0);
				}
			}
			a_t.icons.clear();
			a_t.iconFitted.clear();
			a_t.iconKey = key;
			a_t.iconWait = a_icons.empty() ? -1 : 0;
			a_t.iconMissing = false;
			a_t.shownIcon = -2;
			for (size_t k = 0; k < a_icons.size(); ++k) {
				const std::string name = std::format("icon{}{}", k, a_t.iconKey.size());  // a new name for a new set
				GFx c = ChildClip(a_movie, a_t.clip, name.c_str(), 20.0 + k);
				std::string path = a_icons[k];
				std::ranges::replace(path, '\\', '/');
				const std::string low = cfg::Lower(path);
				// .swf, .png or .jpg (his pick 2026-11-09, .png/.jpg on trial): loaded the same way, from Interface;
				// anything else is never loaded and shows the placeholder (the config log says why)
				if (IconFileOk(low)) {
					// a .swf loads as a movie (from Interface); a picture first goes through the game's own loader (RegisterImage)
					const std::string url = low.ends_with(".swf") ? (low.starts_with("interface/") ? path.substr(10) : path) : RegisterImage(path);
					if (!url.empty()) {
						const GFx arg = MakeString(a_movie, url);
						c.Invoke("loadMovie", nullptr, &arg, 1);
					}
				}
				// the canvas rule for a .swf: 128 x 128, shown at 64 x 64 centred in the art area; a .png stays at its own
				// size until it has loaded (so _width reads its true pixels), then it is fitted
				const bool swf = low.ends_with(".swf");
				c.SetMember("_x", GFx(swf ? -50.0 : 0.0));
				c.SetMember("_y", GFx(swf ? -32.0 : 0.0));
				c.SetMember("_xscale", GFx(swf ? 50.0 : 100.0));
				c.SetMember("_yscale", GFx(swf ? 50.0 : 100.0));
				c.SetMember("_visible", GFx(false));
				a_t.icons.push_back(c);
				a_t.iconFitted.push_back(false);
			}
		}
		if (a_t.icons.empty()) {
			return;
		}
		const int shown = a_t.icons.size() == 1 ? 0 : std::clamp(a_st, 0, static_cast<int>(a_t.icons.size()) - 1);
		// still loading: fit what has arrived (an image: by its own size), give up after 3 s
		if (a_t.iconWait >= 0) {
			++a_t.iconWait;
			bool all = true;
			for (size_t k = 0; k < a_t.icons.size(); ++k) {
				if (a_t.iconFitted[k]) {
					continue;
				}
				GFx w, h;
				auto& c = a_t.icons[k];
				if (c.GetMember("_width", &w) && w.IsNumber() && w.GetNumber() > 0.0 && c.GetMember("_height", &h) && h.IsNumber()) {
					if (!cfg::Lower(a_icons[k]).ends_with(".swf")) {
						// a .png: the HUD reports every registered picture as 64 x 64 whatever its size (his test 2026-11-09:
						// 128 x 128 and 160 x 120 both read 64 x 64), so the real size comes from the file and the picture is
						// fitted into the 64 x 64 area keeping its shape
						const auto [pw, ph] = PngSize(a_icons[k]);
						const double big = std::max(pw, ph);
						const double dw = big > 0.0 ? 64.0 * pw / big : 64.0, dh = big > 0.0 ? 64.0 * ph / big : 64.0;
						c.SetMember("_xscale", GFx(100.0 * dw / w.GetNumber()));
						c.SetMember("_yscale", GFx(100.0 * dh / h.GetNumber()));
						c.SetMember("_x", GFx(-18.0 - dw / 2.0));
						c.SetMember("_y", GFx(-dh / 2.0));
						logger::info("Icon {} loaded ({} x {} px in the file, shown {:.0f} x {:.0f})", a_icons[k], pw, ph, dw, dh);
					}
					a_t.iconFitted[k] = true;
				} else {
					all = false;
				}
			}
			if (all) {
				a_t.iconWait = -1;
			} else if (a_t.iconWait > 30) {
				for (size_t k = 0; k < a_t.icons.size(); ++k) {
					if (!a_t.iconFitted[k]) {
						logger::info("Icon {} did not load (wrong path, or a file type the game can't read): a placeholder shows instead", a_icons[k]);
					}
				}
				a_t.iconWait = -1;
			}
		}
		a_t.iconMissing = !a_t.iconFitted[shown] && a_t.iconWait < 0;
		if (shown != a_t.shownIcon || a_t.iconMissing) {
			a_t.shownIcon = shown;
			for (size_t k = 0; k < a_t.icons.size(); ++k) {
				a_t.icons[k].SetMember("_visible", GFx(static_cast<int>(k) == shown && a_t.iconFitted[k]));
			}
		}
		if (a_t.iconWait < 0 && a_t.iconFitted[shown]) {
			a_t.icons[shown].SetMember("_visible", GFx(true));
		}
	}

	// a stage name in the HUD font (one size for all; the row's spacing makes room for the longest)
	void SetLabel(RE::GFxMovieView* a_movie, TileState& a_t, const std::string& a_text, double a_nameF)
	{
		const double len = static_cast<double>(a_text.size());
		(void)len;
		const double size = a_nameF;  // one size for every name: the row is spaced by the widest, so none overlaps
		for (GFx* f : { &a_t.nameSh, &a_t.name }) {
			SetText(a_movie, *f, a_text);
			if (f->IsDisplayObject()) {
				GFx format;
				a_movie->CreateObject(&format, "TextFormat");
				format.SetMember("size", GFx(size));
				f->Invoke("setTextFormat", nullptr, &format, 1);
				f->SetMember("_y", GFx(H_TEXT_Y + (a_nameF - size) * 0.5 + (f == &a_t.nameSh ? 2.0 : 0.0)));
			}
		}
	}

	// ---- the sources (2026-11-08, his "make our mod flexible, we will use their names and number of stages"): one
	// reading per need from whichever needs mod runs it; the tiles draw what it says, with as many equal blocks as it
	// has stages and its own stage names ----
	struct NeedRead
	{
		bool                     ok = false;
		double                   value = 0.0;   // the mod's raw value (the value line)
		std::vector<double>      bounds;        // n + 1 stage limits, the last one the need's max
		std::vector<std::string> names;         // n stage names (the mod's own)
		std::vector<std::string> extra;         // other names it can show (bonuses), for the row's spacing
		std::vector<std::string> icons;         // a config file's own art for this tile (empty: our drawing)
		int                      st = 0;
		int                      bonus = 0;     // the drawing's bonus look: hunger 1 = well fed; tiredness 1-3 rested, REFRESHING
		std::string              label;
		const char*              word = "";     // the value line: "Hunger 120"
		std::string              line2;         // the disease tile's second line instead ("Severe, stage 2 of 3")
		bool                     urgent = false;  // always counts as the worst stage (Sanguinare Vampiris)
		int                      affl = -1;       // Survival's affliction on this need (0 Weakened, 1 Addled), -1 none: the badge
		double                   shownValue = -1.0;  // the value line's number when it isn't 'value' (Last Seed's vitality), -1 none
		double                   target = -1.0;      // Last Seed's vitality target (0..160), -1 none
		bool                     bloody = false;     // Dirt and Blood's bloodstains (the dirt tile)
		bool                     noCap = false;      // the bar has no red bad-end cap (drunk with SunHelm/iNeed: nothing comes after Drunk)
	};

	int StageIn(double a_v, const std::vector<double>& a_b)
	{
		const int n = static_cast<int>(a_b.size()) - 1;
		int i = 0;
		while (i < n - 1 && a_v >= a_b[i + 1]) {
			++i;
		}
		return i;
	}
	// 0..1 down the bar: each stage one equal block, the value piecewise inside its block (a last stage with no width,
	// like SunHelm's, which stops the value at its own limit, counts as full)
	double PosIn(double a_v, const std::vector<double>& a_b)
	{
		const int n = static_cast<int>(a_b.size()) - 1;
		const int i = StageIn(a_v, a_b);
		const double w = a_b[i + 1] - a_b[i];
		const double f = w > 0.0 ? std::clamp((a_v - a_b[i]) / w, 0.0, 1.0) : 1.0;
		return (i + f) / n;
	}

	// a spell with a running effect on the player (a cast spell such as SunHelm's drunk spell)
	bool SpellRunning(const RE::SpellItem* a_spell)
	{
		if (!a_spell) {
			return false;
		}
		for (const auto& e : g_view.effects) {  // the main-thread snapshot (Snapshot.inl)
			if (e.spell == a_spell && !e.dispelled && !e.inactive) {
				return true;
			}
		}
		return false;
	}

	NeedRead ReadNeedCore(int a_kind)  // 0 hunger, 1 thirst, 2 tiredness, 3 disease, 4 frostbite, 5 vitality, 6 dirt, 7 drunk, 8 body
	{
		NeedRead r;
		const auto player = RE::PlayerCharacter::GetSingleton();
		if (a_kind == 7) {  // drunkenness (his "add both", 2026-11-09): only while there is drink in you; each mod's own words
			if (RndOn() && g_rndDrunk) {
				static constexpr const char* RND_DRUNK[4] = { "Sober", "Dizzy", "Drunk", "Wasted" };
				static constexpr double DEF[5] = { 0, 30, 60, 90, 150 };
				r.value = GV(g_rndDrunk);
				if (r.value <= 0.0) {
					return r;
				}
				double last = 0.0;
				for (int k = 0; k < 4; ++k) {
					last = std::max(last, g_rndDrunkLv[k] ? static_cast<double>(GV(g_rndDrunkLv[k])) : DEF[k]);
					r.bounds.push_back(k == 0 ? 0.0 : last);
					r.names.push_back(RND_DRUNK[k]);
				}
				// the bar ends where RND blacks you out (RND_Stumbling: Level04 - 15 = 135 of its 150 cap), not at the cap the
				// points never reach: the bottom of the bar = you pass out (his question 2026-11-09), like Last Seed's 100
				const double cap = g_rndDrunkLv[4] ? static_cast<double>(GV(g_rndDrunkLv[4])) : DEF[4];
				r.bounds.push_back(std::max(last + 1.0, cap - 15.0));
			} else if (LastSeedOn() && g_lsDrunk && !(g_lsDrunkOn && GV(g_lsDrunkOn) < 2.0f)) {
				r.value = GV(g_lsDrunk);
				if (r.value <= 0.0) {
					return r;
				}
				r.bounds = { 0, 20, 40, 60, 80, 100 };  // at 100 you pass out
				r.names = { "Sober", "Relaxed", "Tipsy", "Drunk", "Very Drunk" };
			} else if (SunHelmOn() && g_shMainQuest && g_shDrunkSpell) {
				// SunHelm (his "both", 2026-11-09): drinks toward its limit, then Drunk while its drunk spell runs
				const bool   drunk = SpellRunning(g_shDrunkSpell);
				const double count = BridgeValue(g_bridgeSHDrinks);  // copied by the Papyrus bridge (1.0.3)
				const double limit = std::max(1.0, g_shNumDrinks ? static_cast<double>(GV(g_shNumDrinks)) : 3.0);
				if (!drunk && count <= 0.0) {
					return r;
				}
				r.bounds = { 0.0, limit, limit + 1.0 };
				r.names = { "Tipsy", "Drunk" };
				r.noCap = true;  // his question 2026-11-09: the cap read as a 3rd stage; these mods have no pass-out after Drunk
				r.value = drunk ? limit + 0.5 : std::min(count, limit - 0.01);
				r.shownValue = drunk ? limit : std::max(0.0, count);
				r.ok = true;
				r.word = "Drinks";
				r.st = StageIn(r.value, r.bounds);
				r.label = r.names[r.st];
				return r;
			} else if (INeedOn() && g_inDrunkSpell) {
				// iNeed (his "both"): only Drunk while its drunk spell is on. Its drink counter (DrunkCount) is a plain script
				// variable, which neither C++ (our rule) nor a Papyrus bridge can read, so 1.0.3 dropped the Tipsy count
				const bool   drunk = SnapHasSpell(g_inDrunkSpell);
				if (!drunk) {
					return r;
				}
				const double limit = 1.0;
				r.bounds = { 0.0, limit, limit + 1.0 };
				r.names = { "Tipsy", "Drunk" };
				r.noCap = true;  // his question 2026-11-09: the cap read as a 3rd stage; these mods have no pass-out after Drunk
				r.value = limit + 0.5;
				r.ok = true;
				r.word = "";  // no drink count to show: the value line stays empty
				r.st = StageIn(r.value, r.bounds);
				r.label = r.names[r.st];
				return r;
			} else {
				return r;
			}
			r.ok = true;
			r.word = "Drunk";
			r.st = StageIn(r.value, r.bounds);
			r.label = r.names[r.st];
			return r;
		}
		if (a_kind == 8) {  // RND's body weight (his "add both"): the body slider 0..100, top of the bar = Muscular
			if (!RndOn() || !g_rndWeightOn || GV(g_rndWeightOn) < 1.0f || g_view.weight < 0.0) {
				return r;
			}
			const double w = std::clamp(g_view.weight, 0.0, 100.0);  // read on the main thread (Snapshot.inl)
			r.ok = true;
			r.value = 100.0 - w;  // as "how far from Muscular", so the bar runs top down like the other tiles
			r.shownValue = w;
			r.bounds = { 0, 20, 40, 60, 80, 100 };
			r.names = { "Muscular", "Trained", "Wiry", "Slender", "Skinny" };
			r.word = "Body";
			// RND's own bands: Muscular above 80, Trained above 60 ... (its upper limits belong to the band below)
			r.st = std::clamp(static_cast<int>(std::ceil((100.0 - w) / 20.0)) - 1, 0, 4);
			r.label = r.names[r.st];
			return r;
		}
		if (a_kind == 6 && g_dnbDirt[0]) {  // Dirt and Blood: five levels (no hours), each with a bloody version
			const auto d = ReadDirtAndBlood();
			if (d.ok) {
				static constexpr const char* DNB_NAMES[5] = { "Clean", "Slightly Dirty", "Somewhat Dirty", "Quite Dirty", "Covered in Dirt" };
				r.ok = true;
				r.st = d.level;
				r.value = d.level + 0.5;
				r.shownValue = d.level;  // the value line: "Dirt 2"
				r.bounds = { 0, 1, 2, 3, 4, 5 };
				r.names.assign(std::begin(DNB_NAMES), std::end(DNB_NAMES));
				r.extra = { "Bloodstains", "Large Bloodstains", "Quite Bloody", "Covered in Blood" };
				r.bloody = d.bloody;
				// its own words when bloody ("Large Bloodstains"); the clean state is "Recently washed" in the mod: Clean here
				r.label = (d.bloody && !d.name.empty()) ? d.name : r.names[r.st];
				r.word = "Dirt";
				return r;
			}
		}
		if (a_kind == 6 && BathingInSkyrimOn()) {  // Bathing in Skyrim: 0..100% dirt, its five tiers and names
			static constexpr double DEF[5] = { 0.0, 0.2, 0.6, 0.98, 1.0 };
			r.ok = true;
			r.value = std::clamp(static_cast<double>(GV(g_bisDirt)), 0.0, 1.0) * 100.0;
			double last = 0.0;
			for (int i = 0; i < 5; ++i) {
				last = std::max(last, (g_bisLimits[i] ? static_cast<double>(GV(g_bisLimits[i])) : DEF[i]) * 100.0);
				r.bounds.push_back(i == 0 ? 0.0 : last);
				r.names.push_back(g_bisNames[i]);
			}
			r.bounds.push_back(std::max(last, 100.0));  // the last tier (Filthy, at 100%) is a full block
			r.word = "Dirt";  // "Dirt 45": percent
			r.st = StageIn(r.value, r.bounds);
			r.label = r.names[r.st];
			return r;
		}
		if (a_kind == 6) {  // Keep It Clean's dirt (his picks 2026-11-09): hours since the last wash, its four stages
			const auto d = ReadDirt();
			if (!d.ok) {
				return r;
			}
			r.ok = true;
			r.value = d.hours;
			r.bounds.assign(std::begin(d.bounds), std::end(d.bounds));
			r.names = { "Clean", "Not Fresh", "Dirty", "Very Dirty" };
			r.word = "Unwashed";  // "Unwashed 12": hours since the last wash
			r.st = StageIn(r.value, r.bounds);
			r.label = r.names[r.st];
			return r;
		}
		if (a_kind == 5) {  // Last Seed's Vitality (his call 2026-11-09): 0..160, higher = better; drawn top = good like every tile
			if (!LastSeedNeedOn(3)) {
				return r;
			}
			static constexpr const char* VIT_NAMES[8] = { "Peak Condition", "Vigorous", "Very Healthy", "Healthy", "Drained", "Weakened", "Ruined", "Dying" };
			const double v = std::clamp(static_cast<double>(GV(g_lsValue[3])), 0.0, 160.0);
			r.ok = true;
			r.value = 160.0 - v;  // as "how far from peak", so the stages run like the other tiles
			r.shownValue = v;
			r.target = g_lsVitalityTarget ? std::clamp(static_cast<double>(GV(g_lsVitalityTarget)), 0.0, 160.0) : -1.0;
			r.bounds = { 0, 20, 40, 60, 80, 100, 120, 140, 160 };
			r.names.assign(std::begin(VIT_NAMES), std::end(VIT_NAMES));
			r.word = "Vitality";
			r.st = StageIn(r.value, r.bounds);
			r.label = r.names[r.st];
			return r;
		}
		if (a_kind == 4) {  // Frostfall's frostbite (his idea 2026-11-09): one block per frostbitten body part, filled to the count
			const auto f = ReadFrostbite();
			if (!f.ok) {
				return r;
			}
			r.ok = true;
			r.st = f.parts - 1;
			r.value = f.parts - 0.5;
			r.bounds = { 0, 1, 2, 3, 4 };
			r.names.assign(4, "Frostbite");
			r.label = "Frostbite";
			r.line2 = f.line2;
			return r;
		}
		if (a_kind == 3) {  // the disease tile: shown only while diseased; one block per stage, filled to its stage
			const auto d = ReadDisease();
			if (!d.ok) {
				return r;
			}
			r.ok = true;
			r.st = d.st;
			r.value = d.pos;
			r.urgent = d.vampire;
			r.bonus = d.vampire ? 1 : 0;  // the blood-red look
			for (int i = 0; i <= d.n; ++i) {
				r.bounds.push_back(i);
			}
			r.names.assign(d.n, d.label);
			r.extra = DiseaseNames();
			r.label = d.label;
			r.line2 = d.line2;
			return r;
		}
		if (SunHelmOn()) {
			// SunHelm (its _SHHungerSystem / _SHThirstSystem / _SHFatigueSystem stages; the first stage is its own good state)
			RE::TESGlobal* v = a_kind == 0 ? g_shHunger : a_kind == 1 ? g_shThirst : g_shFatigue;
			RE::TESGlobal* off = a_kind == 0 ? g_shHungerOff : a_kind == 1 ? g_shThirstOff : g_shFatigueOff;
			if (!GlobalOn(v, off)) {
				return r;
			}
			r.ok = true;
			r.value = GV(v);
			if (a_kind == 0) {
				r.bounds = { 0, 40, 80, 140, 240, 360, 360 };
				r.names = { "Well Fed", "Satisfied", "Peckish", "Hungry", "Ravenous", "Starving" };
				r.word = "Hunger";
			} else if (a_kind == 1) {
				r.bounds = { 0, 40, 60, 120, 180, 300, 300 };
				r.names = { "Quenched", "Sated", "Thirsty", "Parched", "Dehydrated", "Severely Dehydrated" };
				r.word = "Thirst";
			} else {
				r.bounds = { 0, 80, 160, 240, 360, 480, 480 };
				r.names = { "Well Rested", "Rested", "Slightly Tired", "Tired", "Weary", "Exhausted" };
				r.word = "Fatigue";
			}
			r.st = StageIn(r.value, r.bounds);
			r.bonus = (r.st == 0 && a_kind != 1) ? (a_kind == 0 ? 1 : 2) : 0;  // the well-fed / well-rested look
			r.label = r.names[r.st];
			return r;
		}
		if (LastSeedOn() && !SunHelmOn()) {
			// Last Seed (read 2026-11-09 from its _Seed_AttributeSystem): 0..120, rising, six stages of 20, its own names;
			// its first stage is its good state (Full / Quenched / Rested), drawn with the well-fed / rested look
			if (!LastSeedNeedOn(a_kind)) {
				return r;
			}
			static constexpr const char* LS_NAMES[3][6] = {
				{ "Full", "Content", "Hungry", "Very Hungry", "Ravenous", "Starving" },
				{ "Quenched", "Refreshed", "Thirsty", "Very Thirsty", "Parched", "Severely Parched" },
				{ "Rested", "Sharp", "Tired", "Very Tired", "Exhausted", "Severely Exhausted" } };
			static constexpr const char* LS_RESTED[5] = { "Rested", "Well Rested", "Lover's Comfort", "Father's Love", "Mother's Love" };
			r.ok = true;
			r.value = GV(g_lsValue[a_kind]);
			r.bounds = { 0, 20, 40, 60, 80, 100, 120 };
			r.names.assign(std::begin(LS_NAMES[a_kind]), std::end(LS_NAMES[a_kind]));
			r.word = a_kind == 0 ? "Hunger" : a_kind == 1 ? "Thirst" : "Fatigue";
			r.st = StageIn(r.value, r.bounds);
			r.label = r.names[r.st];
			if (a_kind == 0) {
				r.bonus = r.st == 0 ? 1 : 0;
			} else if (a_kind == 2) {
				r.extra.assign(std::begin(LS_RESTED), std::end(LS_RESTED));
				int best = -1;  // its sleep bonuses, the best one showing (the tile's rested looks: 1 rested, 2 well rested, 3 lover's)
				for (int i = 4; i >= 0 && player; --i) {
					if (g_lsRested[i] && SnapHasSpell(g_lsRested[i]) && (best < 0 || i >= 2)) {
						best = i;
					}
				}
				if (best >= 0) {
					r.bonus = best == 0 ? 1 : best == 1 ? 2 : 3;
					r.label = LS_RESTED[best];
				}
			}
			return r;
		}
		if (INeedOn() && !SunHelmOn() && !LastSeedOn()) {
			// iNeed (read 2026-11-09): six stages from its penalty global, its own words; the pointer moves inside the
			// first three with the need's value (0..110), the last three (at 0) grow with time, so it sits in their middle
			static constexpr const char* IN_NAMES[3][6] = {
				{ "Well Sated", "Sated", "A Bit Hungry", "Mildly Hungry", "Moderately Hungry", "Severely Hungry" },
				{ "Well Hydrated", "Hydrated", "A Bit Thirsty", "Mildly Thirsty", "Moderately Thirsty", "Severely Thirsty" },
				{ "Well Rested", "Rested", "A Bit Fatigued", "Mildly Fatigued", "Moderately Fatigued", "Severely Fatigued" } };
			// its values come through the Papyrus bridge (NeedsHUD_INeedBridge: HungerState/ThirstState/FatigueState, and the
			// rates, 0 = that need switched off in its menu)
			if (!g_inPenalty[a_kind] || INeedRate(a_kind) == 0.0) {
				return r;  // that need is switched off in iNeed's menu (or unreadable: the penalty decides alone)
			}
			// the first three stages from the value itself (iNeed's own widget does the same; its penalty global only
			// changes on its hourly update, so right after loading it can still say "fine" at 36, seen 2026-11-09);
			// the penalty decides "well sated" (its buff) and the mild/moderate/severe stages at zero
			const double state = INeedState(a_kind);  // copied by the Papyrus bridge (1.0.3), -1 until it has run
			const int pen = static_cast<int>(std::lround(GV(g_inPenalty[a_kind])));
			int st = std::clamp(pen + 2, 0, 5);
			if (state >= 0.0) {
				st = state <= 0.0 ? std::clamp(pen + 2, 3, 5) : state <= 50.0 ? 2 : (pen == -2 ? 0 : 1);
			}
			double frac = 0.5;
			if (state >= 0.0 && st == 1) {
				frac = std::clamp((95.0 - state) / 45.0, 0.0, 0.999);
			} else if (state >= 0.0 && st == 2) {
				frac = std::clamp((50.0 - state) / 50.0, 0.0, 0.999);
			}
			r.ok = true;
			r.value = st + frac;
			r.shownValue = state >= 0.0 ? state : -1.0;
			r.bounds = { 0, 1, 2, 3, 4, 5, 6 };
			r.names.assign(std::begin(IN_NAMES[a_kind]), std::end(IN_NAMES[a_kind]));
			r.word = a_kind == 0 ? "Hunger" : a_kind == 1 ? "Thirst" : "Fatigue";
			r.st = st;
			r.label = r.names[st];
			r.bonus = st == 0 ? (a_kind == 0 ? 1 : a_kind == 2 ? 2 : 0) : 0;  // the well-fed / well-rested looks
			return r;
		}
		if (RndOn() && !SunHelmOn() && !LastSeedOn() && !INeedOn()) {
			// Realistic Needs and Diseases (read 2026-11-09): points rising = worse, its level globals as limits (its MCM
			// can't move them, but read anyway), capped at the last level: that stage is a full block; its own names
			static constexpr const char* RND_NAMES[3][6] = {
				{ "Overindulged", "Satiated", "Peckish", "Hungry", "Very Hungry", "Starving" },
				{ "Quenched", "Slightly Thirsty", "Thirsty", "Very Thirsty", "Dehydrated", "" },
				{ "Not Tired", "Slightly Tired", "Tired", "Very Tired", "Exhausted", "" } };
			static constexpr double DEF[3][6] = { { 0, 5, 30, 60, 120, 240 }, { 0, 40, 80, 160, 240, 0 }, { 0, 30, 60, 120, 240, 0 } };
			const int n = a_kind == 0 ? 6 : 5;
			if (!g_rndPoints[a_kind]) {
				return r;
			}
			r.ok = true;
			r.value = GV(g_rndPoints[a_kind]);
			double last = 0.0;
			for (int k = 0; k < n; ++k) {
				last = std::max(last, g_rndLevels[a_kind][k] ? static_cast<double>(GV(g_rndLevels[a_kind][k])) : DEF[a_kind][k]);
				r.bounds.push_back(k == 0 ? 0.0 : last);
				r.names.push_back(RND_NAMES[a_kind][k]);
			}
			r.bounds.push_back(last);  // the cap: the last stage is full
			r.word = a_kind == 0 ? "Hunger" : a_kind == 1 ? "Thirst" : "Fatigue";
			r.st = StageIn(r.value, r.bounds);
			r.label = r.names[r.st];
			r.bonus = (a_kind == 0 && r.st == 0) ? 1 : 0;  // Overindulged: the well-fed look
			return r;
		}
		// Survival Mode (vanilla, or Survival Mode Improved: same values, its own stage limits); it has no thirst
		const bool survival = g_survivalOn && GV(g_survivalOn) >= 1.0f;
		if (!survival || a_kind == 1) {
			return r;
		}
		if (g_starfrost && a_kind != 1 && (a_kind == 0 ? g_hungerValue : g_tiredValue)) {
			// Starfrost (his call 2026-11-08): only its named stages count, with its own names; its two "neutral"
			// stages (no effect, no name) become one block named in our words. Limits: SMI's stages 3-5 and the max
			// (where Starfrost's need stops); with its vanilla-hunger module the same rule follows the restored scale
			const auto& sb = a_kind == 0 ? g_hungerBoundsSmi : g_tiredBoundsSmi;
			r.ok = true;
			r.value = a_kind == 0 ? GV(g_hungerValue) : GV(g_tiredValue);
			if (sb[0] >= 0.0 && sb[2] > 0.0) {
				r.bounds = { 0.0, sb[2], sb[3], sb[4], sb[5] };
			} else {
				r.bounds = a_kind == 0 ? std::vector<double>{ 0, 20, 60, 120, 120 } : std::vector<double>{ 0, 300, 450, 600, 600 };
			}
			r.st = StageIn(r.value, r.bounds);
			if (a_kind == 0) {
				r.names = { "Not Hungry", "Hungry", "Very Hungry", "Starving" };
				r.extra.assign(std::begin(SF_FED_NAMES), std::end(SF_FED_NAMES));
				r.word = "Hunger";
				int fed = 0;  // its meal bonuses: the best one showing
				for (int i = 2; i >= 0 && player; --i) {
					if (g_sfFed[i] && SnapHasSpell(g_sfFed[i])) {
						fed = i + 1;
						break;
					}
				}
				r.bonus = fed;  // 1-3: the well-fed look; the number keeps the name in step
				r.label = fed ? SF_FED_NAMES[fed - 1] : r.names[r.st];
			} else {
				r.names = { "Not Tired", "Tired", "Very Tired", "Exhausted" };
				r.extra.assign(std::begin(RESTED_NAMES) + 1, std::begin(RESTED_NAMES) + 4);
				r.word = "Tiredness";
				r.bonus = RestedBonus();  // Starfrost keeps Skyrim's Rested, Well Rested, Lover's Comfort
				r.label = r.bonus ? RESTED_NAMES[r.bonus] : r.names[r.st];
			}
			return r;
		}
		if (a_kind == 0 && g_hungerValue) {
			r.ok = true;
			r.value = GV(g_hungerValue);
			r.bounds.assign(std::begin(HUNGER_BOUNDS), std::end(HUNGER_BOUNDS));
			if (g_hungerBoundsSmi[0] >= 0.0 && g_hungerBoundsSmi[1] > 0.0) {
				for (int i = 1; i < 6; ++i) {  // its limits and its max (468, not 1000)
					r.bounds[i] = g_hungerBoundsSmi[i];
				}
			}
			r.names.assign(std::begin(HUNGER_NAMES), std::end(HUNGER_NAMES));
			r.extra = { "Well Fed" };
			r.word = "Hunger";
			r.st = StageIn(r.value, r.bounds);
			r.bonus = SnapHasSpell(g_wellFed) ? 1 : 0;
			r.label = r.bonus ? "Well Fed" : r.names[r.st];
		} else if (a_kind == 2 && g_tiredValue) {
			r.ok = true;
			r.value = GV(g_tiredValue);
			double b[6];
			TiredBounds(b);
			r.bounds.assign(std::begin(b), std::end(b));
			r.names.assign(std::begin(TIRED_NAMES), std::end(TIRED_NAMES));
			r.extra.assign(std::begin(RESTED_NAMES) + 1, std::end(RESTED_NAMES));
			r.word = "Tiredness";
			r.st = StageIn(r.value, r.bounds);
			r.bonus = RestedBonus();
			if (r.bonus == 0 && r.st == 1 && FcRefreshing()) {
				r.bonus = REFRESHING;  // Free Cooking's drink counters Drained
			}
			r.label = r.bonus ? RESTED_NAMES[r.bonus] : r.names[r.st];
		}
		return r;
	}

	// Survival's afflictions show on their need's tile whichever needs mod runs it (his rule 2026-11-09: "it should show
	// right? even if it's an edge case": e.g. an Addled left over after switching from Survival to SunHelm)
	// a tile from another author's config file (ConfigLoader.inl): their value, stages, names and art
	NeedRead ConfigNeedRead(const CfgTile& a_t, int a_kind)
	{
		NeedRead r;
		const CfgScale s = ConfigScale(a_t);
		if (!s.ok || (a_kind == CFG_DRUNK && s.value <= 0.0)) {  // the drink tile only while there is drink in you
			return r;
		}
		static constexpr const char* WORDS[CFG_CUSTOM0] = { "Hunger", "Thirst", "Fatigue", "", "", "", "Dirt", "Drunk", "", "Bladder", "Fun", "Social" };
		r.ok = true;
		r.value = s.value;
		r.bounds = s.bounds;
		r.names = a_t.names;
		r.st = std::clamp(s.st, 0, static_cast<int>(a_t.names.size()) - 1);
		r.label = a_t.names[r.st];
		r.shownValue = std::max(0.0, s.raw);
		r.word = !a_t.word.empty() ? a_t.word.c_str() : a_kind >= CFG_CUSTOM0 ? a_t.displayName.c_str() : WORDS[a_kind];
		r.bonus = (a_t.bonusFirst && r.st == 0) ? (a_kind == CFG_HUNGER ? 1 : a_kind == CFG_TIRED ? 2 : 0) : 0;
		r.noCap = !a_t.cap;
		r.icons = a_t.icons;
		return r;
	}

	NeedRead ReadNeed(int a_kind)
	{
		// a config file whose mod runs wins the tile; bladder, fun, social and custom needs come only from config files
		NeedRead r;
		if (const CfgTile* ct = ConfigTileFor(a_kind)) {
			r = ConfigNeedRead(*ct, a_kind);
		} else if (a_kind < CFG_BLADDER) {
			r = ReadNeedCore(a_kind);
		}
		if (r.ok && (a_kind == 0 || a_kind == 2)) {
			r.affl = a_kind == 0 ? (Afflicted(0) ? 0 : -1) : (Afflicted(1) ? 1 : -1);
		}
		return r;
	}

	// which tiles a mod in this load order can feed (the settings page lists only those; 2026-11-10, his look at the page).
	// By the plugins that are loaded, not by what runs now: a needs mod switched off in this save is still listed, so it
	// can be switched back on. Found once at kDataLoaded, after the config files
	std::array<std::atomic_bool, CFG_CUSTOM0> g_tileAvail{};
	std::atomic_bool g_tileAvailKnown{ false };
	// the thermometer's wetness bar has a source (Frostfall, or a config file's [Cold] Wetness): the settings page shows
	// "Show wetness" only then (his find 2026-11-12: it showed without Frostfall)
	std::atomic_bool g_wetnessAvail{ false };
	void FindAvailableTiles(RE::TESDataHandler* a_data)
	{
		const auto on = [&](const char* a_file) { return ModActive(a_data, a_file) != nullptr; };
		const bool survival = on(SURVIVAL_FILE), sunhelm = on(SUNHELM_FILE), lastSeed = on(LASTSEED_FILE), ineed = on(INEED_FILE), rnd = on(RND_FILE);
		bool avail[CFG_CUSTOM0] = {};
		avail[0] = avail[2] = survival || sunhelm || lastSeed || ineed || rnd;  // hunger, tiredness
		avail[1] = sunhelm || lastSeed || ineed || rnd;                         // thirst (Survival Mode has none)
		avail[3] = true;                                                        // diseases are in the base game
		avail[4] = on(FF_FILE);                                                 // Frostfall's frostbite
		avail[5] = lastSeed;                                                    // Last Seed's Vitality
		avail[6] = on(KIC_FILE) || on(DNB_FILE) || on(BIS_FILE);                // Keep It Clean, Dirt and Blood, Bathing in Skyrim
		avail[7] = rnd || lastSeed || sunhelm || ineed;                         // drunk
		avail[8] = rnd;                                                         // RND's body weight
		bool wet = on(FF_FILE);
		for (const auto& m : g_cfgMods) {  // a config file in use (its plugin loaded) feeds its tiles; bladder, fun, social only so
			for (const auto& t : m.tiles) {
				if (t.kind >= 0 && t.kind < CFG_CUSTOM0) {
					avail[t.kind] = true;
				}
			}
			wet = wet || m.cold.wetness.Ok();
		}
		g_wetnessAvail = wet;
		std::string list;
		for (int i = 0; i < CFG_CUSTOM0; ++i) {
			g_tileAvail[i] = avail[i];
			if (avail[i]) {
				list += (list.empty() ? "" : ", ") + std::to_string(i);
			}
		}
		g_tileAvailKnown = true;
		logger::info("Settings page lists the tiles a loaded mod can feed: {}; wetness {}", list, wet ? "listed" : "not listed (no Frostfall)");
	}

	void NeedsGaugeTick()
	{
		auto& n = g_needs;
		if (n.failed) {
			return;
		}
		static int seenLoad = 0;
		if (const int load = g_loadCount.load(); load != seenLoad) {
			seenLoad = load;
			for (auto& t : n.tiles) {
				t.shownV = -1.0;  // the bars jump to the loaded save's values instead of easing from the old ones
			}
			n.autoShow = AutoShow{};  // "only on change" compares with the loaded game, not the one before (1.0.3)
		}
		auto ui = RE::UI::GetSingleton();
		auto hud = ui ? ui->GetMenu(RE::HUDMenu::MENU_NAME) : nullptr;
		RE::GFxMovieView* movie = hud ? hud->uiMovie.get() : nullptr;
		GFx root;
		if (!movie || !movie->GetVariable(&root, "_root") || !root.IsDisplayObject()) {
			return;
		}
		GFx box;
		if (!root.GetMember(NEEDS_CLIP, &box) || !box.IsDisplayObject()) {
			// first time, or the HUD movie was rebuilt: start over
			GFx depth;
			root.Invoke("getNextHighestDepth", &depth, nullptr, 0);
			const GFx args[2]{ MakeString(movie, NEEDS_CLIP), depth.IsNumber() ? depth.GetNumber() : 9001.0 };
			root.Invoke("createEmptyMovieClip", nullptr, args, 2);
			if (!root.GetMember(NEEDS_CLIP, &box) || !box.IsDisplayObject()) {
				logger::error("Could not add the needs group to the HUD");
				n.failed = true;
				return;
			}
			n = NeedsGroupState{};
			JoinHudModes(movie, box, "Needs group");
			logger::info("Needs group added to the HUD");
		}

		// new options from the settings page: lay out, place, size and redraw everything again (the text fields are
		// made with their size, so they are removed and made anew)
		if (g_needsChanged.exchange(false)) {
			{
				std::scoped_lock lock(g_needsLock);
				g_needsOpt = g_needsPending;
			}
			for (auto& t : n.tiles) {
				if (t.clip.IsDisplayObject()) {
					for (const char* name : { "nameSh", "name", "numSh", "num" }) {
						GFx field;
						if (t.clip.GetMember(name, &field) && field.IsDisplayObject()) {
							field.Invoke("removeTextField", nullptr, nullptr, 0);
						}
					}
				}
				t.shownStage = -2;
				t.shownNumber = -1;
			}
			n.placedFor = -1.0;
			n.layout = -1;
			n.shown = -1;
		}

		// which tiles show: the group and the tile switched on, and a needs mod giving that need
		const auto& o = g_needsOpt;
		// a tile switched off (or the whole group) isn't read at all: no disease scan or script reads for nothing
		bool asked[TILE_COUNT] = { o.showHunger, o.showThirst, o.showTiredness, o.showDisease, o.showFrostbite, o.showVitality,
			o.showDirt, o.showDrunk, o.showBody, o.showBladder, o.showFun, o.showSocial };
		for (int i = 0; i < CFG_CUSTOM_MAX; ++i) {  // custom needs: shown unless the player switched that one off
			asked[CFG_CUSTOM0 + i] = i < static_cast<int>(g_cfgCustom.size()) && !NeedsHUD::NeedHidden(o, g_cfgCustom[i].first);
		}
		NeedRead reads[TILE_COUNT];
		for (int i = 0; i < TILE_COUNT; ++i) {
			if (o.enabled && asked[i]) {
				reads[i] = ReadNeed(i);
			}
		}
		if (!(o.enabled && asked[3])) {
			VampireChangeDay(false, 0.0);  // disease tile off: no vampirism clock (it restarts if the tile comes back on)
		}
		bool want[TILE_COUNT];
		long long cfgMask = 0;  // which tiles a config file feeds now (their names differ from the built-in ones)
		for (int i = 0; i < TILE_COUNT; ++i) {
			want[i] = o.enabled && asked[i] && reads[i].ok;
			if (want[i] && ConfigTileFor(i)) {
				cfgMask |= 1LL << i;
			}
		}
		// the source (or its stage count) changing lays the row out again: the names to measure change
		const long long sourceKey = (SunHelmOn() ? 1 : LastSeedOn() ? 3 : INeedOn() ? 4 : RndOn() ? 5 : (g_starfrost ? 2 : 0)) + (cfgMask << 4);
		int layout = 0, count = 0;
		for (int i = 0; i < TILE_COUNT; ++i) {
			if (want[i]) {
				layout |= 1 << i;
				++count;
			}
		}
		const bool on = count > 0;
		if (!on) {
			if (n.shown != 0) {
				n.shown = 0;
				box.SetMember("_alpha", GFx(0.0));
			}
			n.autoShow.alpha = -1.0;
			n.shownAlpha = -1.0;
			return;
		}
		n.shown = 1;  // the alpha itself is set after the tiles, once their stages are known (when to show)

		// the tiles' clips (made once; a hidden one is only made see-through)
		static constexpr const char* TILE_CLIPS[TILE_COUNT] = { "hunger", "thirst", "tired", "disease", "frostbite", "vitality", "dirt", "drunk",
			"body", "bladder", "fun", "social", "need0", "need1", "need2", "need3", "need4", "need5", "need6", "need7" };
		for (int i = 0; i < TILE_COUNT; ++i) {
			auto& t = n.tiles[i];
			if (!t.made) {
				t.clip = ChildClip(movie, box, TILE_CLIPS[i], 1.0 + i);
				t.art = t.clip.IsDisplayObject() ? ChildClip(movie, t.clip, "art", 1.0) : GFx();
				t.made = t.art.IsDisplayObject();
				if (!t.made) {
					return;
				}
			}
		}

		// lay out and place the row: from the chosen screen corner, offset by the set percents of the visible screen,
		// to the edge of a box round the tiles, their spike crowns and their text; always kept fully on screen
		const auto rect = movie->GetVisibleFrameRect();
		const double width = rect.right - rect.left, height = rect.bottom - rect.top;
		const double tileScale = TILE_SCALE * o.scale;  // mockup px -> 1080p px
		const double k = MOCK_TO_HUD * tileScale;
		const double nameF = std::max(TEXT_PX_MIN, NAME_PX * o.scale) / tileScale;          // in tile units
		const double numF = std::max(TEXT_PX_MIN, NAME_PX * 0.8 * o.scale) / tileScale;
		if (width != n.placedFor || layout != n.layout || sourceKey != n.sourceKey) {
			n.placedFor = width;
			n.layout = layout;
			n.sourceKey = sourceKey;
			n.nameF = nameF;
			// the text fields first (made with their size): each tile is as wide as the wider of its frame and the
			// widest name it can ever show (measured in the HUD's own font), so names never overlap and the row never
			// jumps when a name changes; each tile keeps its own width (a long disease name, "Crippling Bone Break
			// Fever", must not spread the whole row)
			const double fw = nameF * 30.0;  // wide enough for the longest names (SunHelm's Severely Dehydrated was cut, 2026-11-08)
			double widths[TILE_COUNT]{};
			double widest = TILE_BOX_W;
			for (int i = 0; i < TILE_COUNT; ++i) {
				auto& t = n.tiles[i];
				t.clip.SetMember("_alpha", GFx(want[i] ? 100.0 : 0.0));
				if (!want[i]) {
					continue;
				}
				double tileW = TILE_BOX_W;
				t.nameSh = TextField(movie, t.clip, "nameSh", 10.0, -fw / 2 + 2, H_TEXT_Y + 2, fw, nameF, 0x000000);
				t.name = TextField(movie, t.clip, "name", 11.0, -fw / 2, H_TEXT_Y, fw, nameF, LABEL);  // the thermometer's colour
				if (o.numbers || i == 3 || i == 4) {  // the disease and frostbite tiles always have their second line
					const double ny = H_TEXT_Y + nameF * 1.5;
					t.numSh = TextField(movie, t.clip, "numSh", 12.0, -fw / 2 + 2, ny + 2, fw, numF, 0x000000);
					t.num = TextField(movie, t.clip, "num", 13.0, -fw / 2, ny, fw, numF, 0xB8B2A6);
				}
				std::vector<std::string> names = reads[i].names;
				names.insert(names.end(), reads[i].extra.begin(), reads[i].extra.end());
				const auto measure = [&](GFx& a_field) {
					GFx tw;
					if (a_field.IsDisplayObject() && a_field.GetMember("textWidth", &tw) && tw.IsNumber()) {
						tileW = std::max(tileW, tw.GetNumber() + 8.0);  // + the shadow and a little air
					}
				};
				for (const auto& text : names) {
					SetLabel(movie, t, text, nameF);
					measure(t.name);
				}
				if (o.numbers || i == 3 || i == 4) {  // the value line (smaller, but long)
					if (i == 4) {
						SetText(movie, t.num, tr::F("{} \xC2\xB7 {} min", tr::T(std::string("Hands")) + ", " + tr::T(std::string("Feet")) + ", " + tr::T(std::string("Head")), 5));
						measure(t.num);
					}
					if (i == 3) {
						SetText(movie, t.num, tr::F("Vampirism in {} hours", 23));
						measure(t.num);
					}
					SetText(movie, t.num, i == 3 ? tr::F("{} {} of {}", tr::T(std::string("Crippling, stage")), 3, 3) : i == 4 ? tr::F("{} \xC2\xB7 {} min", tr::T(std::string("All four")), 5) : tr::T(std::string(reads[i].word)) + " 1000");
					measure(t.num);
				}
				widths[i] = tileW;
				widest = std::max(widest, tileW);
				t.shownStage = -2;  // the real name goes back on below
				t.shownNumber = -1;
			}

			// one tile's box: as wide as the widest of frame or name, from its top to the bottom of its text; the
			// tiles follow each other across (a row) or down (a column), spacing apart
			const double tileBelow = H_TEXT_Y + nameF * 1.5 + (o.numbers || want[3] || want[4] ? numF * 1.6 : nameF * 0.2);
			const double tileTall = TILE_H / 2 + tileBelow;
			const double gap = std::clamp(o.spacing, NeedsHUD::SPACING_MIN, NeedsHUD::SPACING_MAX);
			const double step = tileTall + gap;  // a column's step
			// line break (his ask 2026-11-09): after perLine tiles a new row (or column) starts, growing away from the
			// screen corner; a short last line hugs the corner's side. Every tile's centre is placed from the block's top left
			const int per = o.perLine > 0 ? o.perLine : count;
			std::vector<std::vector<int>> lines;
			for (int i = 0; i < TILE_COUNT; ++i) {
				if (want[i]) {
					if (lines.empty() || static_cast<int>(lines.back().size()) >= per) {
						lines.emplace_back();
					}
					lines.back().push_back(i);
				}
			}
			const int  L = static_cast<int>(lines.size());
			const int  anchor = std::clamp(o.anchor, 0, 3);
			const bool fromRight = anchor == 1 || anchor == 3, fromBottom = anchor >= 2;
			std::vector<double> span(L, 0.0);  // a row's width, or a column's width (its widest tile)
			size_t longest = 0;
			for (int l = 0; l < L; ++l) {
				longest = std::max(longest, lines[l].size());
				for (size_t j = 0; j < lines[l].size(); ++j) {
					const double w = widths[lines[l][j]];
					span[l] = o.vertical ? std::max(span[l], w) : span[l] + w + (j ? gap : 0.0);
				}
			}
			double blockW = 0.0, blockH = 0.0;
			if (o.vertical) {
				for (double w : span) {
					blockW += w;
				}
				blockW += (L - 1) * gap;
				blockH = (static_cast<double>(longest) - 1) * step + tileTall;
			} else {
				blockW = *std::max_element(span.begin(), span.end());
				blockH = L * tileTall + (L - 1) * gap;
			}
			const double rowW = blockW * k, tall = blockH * k;
			double left = fromRight ? rect.right - width * o.posX / 100.0 - rowW : rect.left + width * o.posX / 100.0;
			double top = fromBottom ? rect.bottom - height * o.posY / 100.0 - tall : rect.top + height * o.posY / 100.0;
			left = std::clamp(left, rect.left + 6.0, std::max(rect.left + 6.0, rect.right - 6.0 - rowW));
			top = std::clamp(top, rect.top + 6.0, std::max(rect.top + 6.0, rect.bottom - 6.0 - tall));
			box.SetMember("_x", GFx(left));
			box.SetMember("_y", GFx(top));
			box.SetMember("_xscale", GFx(k * 100.0));
			box.SetMember("_yscale", GFx(k * 100.0));
			double colX = 0.0;  // columns: the next column's left edge, in the order they stand on screen
			for (int v = 0; v < L; ++v) {
				// the first line sits at the corner; the next ones grow away from it
				const int l = o.vertical ? (fromRight ? L - 1 - v : v) : v;
				const auto& line = lines[l];
				if (o.vertical) {
					const double down = fromBottom ? (static_cast<double>(longest) - line.size()) * step : 0.0;
					for (size_t j = 0; j < line.size(); ++j) {
						auto& t = n.tiles[line[j]];
						t.clip.SetMember("_x", GFx(colX + span[l] / 2));
						t.clip.SetMember("_y", GFx(down + j * step + TILE_H / 2));
					}
					colX += span[l] + gap;
				} else {
					const int    row = fromBottom ? L - 1 - l : l;
					double       x = fromRight ? blockW - span[l] : 0.0;
					const double y = row * (tileTall + gap) + TILE_H / 2;
					for (int i : line) {
						auto& t = n.tiles[i];
						t.clip.SetMember("_x", GFx(x + widths[i] / 2));
						t.clip.SetMember("_y", GFx(y));
						x += widths[i] + gap;
					}
				}
			}
		}

		// each shown tile: ease like the mercury (a fifth of the way each tick), redraw on change
		// every shown tile's stage and bonus (and which tiles show), one entry each (1.0.4 review: packed into one 64-bit number,
		// the first tiles fell off once eleven or more showed, so their changes never brought the group up)
		std::vector<std::uint64_t> stagesKey{ static_cast<std::uint64_t>(layout) };
		bool      anyWorst = false;
		std::vector<int> levels;  // onlyWorse: each shown tile's stage, higher = worse (a lost bonus counts as worse too)
		for (int i = 0; i < TILE_COUNT; ++i) {
			if (!want[i]) {
				continue;
			}
			auto& t = n.tiles[i];
			const NeedRead& r = reads[i];
			const int nst = static_cast<int>(r.names.size());
			if (o.numbers || i == 3 || i == 4) {
				// the value line; the disease tile shows its stage there instead ("Severe, stage 2 of 3"; none with one stage)
				const int whole = (i == 3 || i == 4) ? static_cast<int>(std::hash<std::string>{}(r.line2) & 0x7FFFFFFF) : static_cast<int>(std::lround(r.shownValue >= 0.0 ? r.shownValue : r.value));
				if (whole != t.shownNumber) {
					t.shownNumber = whole;
					const std::string text = (i == 3 || i == 4) ? r.line2 : (*r.word ? std::format("{} {}", tr::T(std::string(r.word)), whole) : std::string());
					SetText(movie, t.numSh, text);
					SetText(movie, t.num, text);
				}
			}
			const double target = PosIn(r.value, r.bounds);  // 0..1 down the bar
			double v = t.shownV < 0.0 ? target : t.shownV + (target - t.shownV) * 0.2;
			if (std::abs(target - v) < 0.0005) {
				v = target;
			}
			const int st = r.st, bonus = r.bonus;
			stagesKey.push_back(static_cast<std::uint64_t>(i));
			stagesKey.push_back(static_cast<std::uint64_t>(st));
			stagesKey.push_back(static_cast<std::uint64_t>(bonus));
			levels.push_back(st * 8 + (7 - std::clamp(bonus, 0, 7)));
			anyWorst = anyWorst || (st == nst - 1 && bonus == 0 && i != 8) || r.urgent;  // body: no band is a warning
			if (!r.icons.empty() && t.iconWait >= 0 && t.iconWait < 31) {
				t.shownStage = -2;  // the picture is still loading: check it again next tick
			}
			if (std::abs(v - t.shownV) < 0.00025 && st == t.shownStage && bonus == t.shownBonus && nst == t.shownCount && r.label == t.shownLabel && r.affl == t.shownAffl && std::abs(r.target - t.shownTarget) < 0.5) {
				continue;
			}
			const bool labelChanged = st != t.shownStage || bonus != t.shownBonus || nst != t.shownCount || r.label != t.shownLabel;
			t.shownLabel = r.label;
			t.shownAffl = r.affl;
			t.shownTarget = r.target;
			t.shownV = v;
			t.shownStage = st;
			t.shownBonus = bonus;
			t.shownCount = nst;
			SyncIcons(movie, t, r.icons, st);
			if (!r.icons.empty()) {
				DrawIconTile(t.art, v, st, nst, !r.noCap, t.iconMissing);  // an author's own art over our frame and bar
			} else if (i >= CFG_CUSTOM0) {
				DrawIconTile(t.art, v, st, nst, !r.noCap, true);  // a custom need without art: the placeholder
			} else if (i == CFG_BLADDER) {
				DrawBladderTile(t.art, v, st, nst, !r.noCap);
			} else if (i == CFG_FUN) {
				DrawFunTile(t.art, v, st, nst, !r.noCap);
			} else if (i == CFG_SOCIAL) {
				DrawSocialTile(t.art, v, st, nst, !r.noCap);
			} else if (i == 0) {
				DrawHungerTile(t.art, v, st, nst, bonus > 0);
			} else if (i == 1) {
				DrawThirstTile(t.art, v, st, nst);
			} else if (i == 2) {
				DrawTiredTile(t.art, v, st, nst, bonus);
			} else if (i == 3) {
				DrawDiseaseTile(t.art, v, st, nst, r.urgent);
			} else if (i == 4) {
				DrawFrostbiteTile(t.art, st, 0);  // his pick: the hand, one frosted finger per part
			} else if (i == 5) {
				DrawVitalityTile(t.art, 160.0 * (1.0 - v), r.target, 0);  // his pick: the anatomical heart
			} else if (i == 6) {
				DrawDirtTile(t.art, v, st, nst, r.bloody);  // the approved tunic (blood-red stains with Dirt and Blood)
			} else if (i == 7) {
				DrawDrunkTile(t.art, v, st, nst, !r.noCap);  // the tipping tankard (approved 2026-11-09)
			} else {
				DrawBodyTile(t.art, v, st, nst);  // the torso (approved 2026-11-09)
			}
			if (r.affl >= 0) {
				DrawAfflictionBadge(t.art, -TILE_W / 2 + 2, -TILE_H / 2 + 2, 13.0, r.affl);  // on the frame's top left corner
			}
			if (labelChanged) {
				SetLabel(movie, t, r.label, n.nameF);
			}
		}
		// when to show: the whole row, on any tile's stage change
		const double alpha = AutoShowAlpha(n.autoShow, o, stagesKey, anyWorst, NowMs(), levels);
		if (alpha != n.shownAlpha) {
			n.shownAlpha = alpha;
			box.SetMember("_alpha", GFx(alpha));
		}
	}
