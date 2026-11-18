// ---- config files from other authors (2026-11-09, his pick: config files, no Papyrus or C++ API; guide "NeedsHUD Patch
// Guide for Mod Authors"). Every Data/SKSE/Plugins/NeedsHUD/*.ini describes where one needs mod keeps its values;
// NeedsHUD reads them like its built-in mods. A file whose mod is running wins over built-in support for the same tile.
// Read once at kDataLoaded (forms looked up then, never per tick); values are read each tick (globals). Script values
// ("property:" / "variable:") were dropped in 1.0.3: C++ must never read Papyrus script values (rule for all his plugins);
// authors copy such a value into a global from their own script instead (see the patch guide).

	int  StageIn(double a_v, const std::vector<double>& a_b);  // NeedsGroup.inl
	bool SpellRunning(const RE::SpellItem* a_spell);           // NeedsGroup.inl

	// tile kinds fed by config files (the needs row's own numbering, NeedsGroup.inl)
	constexpr int CFG_HUNGER = 0, CFG_THIRST = 1, CFG_TIRED = 2, CFG_DIRT = 6, CFG_DRUNK = 7, CFG_BLADDER = 9, CFG_FUN = 10,
				  CFG_SOCIAL = 11, CFG_CUSTOM0 = 12, CFG_CUSTOM_MAX = 8;  // custom needs take kinds 12..19

	// one value: a number or a global (this plugin's or "Other.esp|0x...")
	struct CfgValue
	{
		enum class Kind { None, Number, Global } kind = Kind::None;
		double         number = 0.0;
		RE::TESGlobal* global = nullptr;

		bool   Ok() const { return kind == Kind::Number || (kind == Kind::Global && global); }
		double Get() const
		{
			switch (kind) {
			case Kind::Number: return number;
			case Kind::Global: return global ? static_cast<double>(GV(global)) : 0.0;
			default: return 0.0;
			}
		}
	};

	struct CfgTile
	{
		bool                     present = false;
		int                      kind = -1;
		std::string              id;            // custom needs: the id after "Need:"
		std::string              displayName;   // custom needs: the name in the menu
		CfgValue                 value;
		bool                     falling = false;  // higher = better
		std::vector<CfgValue>    stages;           // n + 1 limits, lowest first, the last = the maximum
		std::vector<std::string> names;            // n names, best first
		std::vector<RE::SpellItem*> stageSpells;   // instead of a value: one spell per stage, best first
		CfgValue                 enabled;          // a switch: shown while >= enabledMin
		double                   enabledMin = 1.0;
		bool                     bonusFirst = false;
		std::string              word;             // the value line's word
		bool                     cap = true;       // drunk: the red end (nothing after the last stage: no)
		std::vector<std::string> icons;            // the author's art (one per stage, or one for all)
		// the thermometer's extras
		CfgValue                 target, nearHeat, inColdWater, wetness, wetnessMax;
		std::vector<std::string> wetNames;
	};

	struct CfgMod
	{
		std::string           file;     // the config file's name (for the log)
		std::string           name;     // [Mod] Name
		std::string           plugin;   // the plugin that is loaded (Plugin or the first loaded AltPlugin)
		CfgValue              running;
		double                runningMin = 1.0;
		std::vector<CfgTile>  tiles;    // need tiles, dirt, drunk, bladder/fun/social, custom needs
		CfgTile               cold;     // the thermometer ([Cold])
		bool                  Running() const { return !running.Ok() || running.Get() >= runningMin; }
	};
	std::vector<CfgMod> g_cfgMods;

	// [ColdProtection] (2026-11-15, his design for Cooking Skill Overhaul's Spicy, open to any mod): a buff that makes every
	// place count some cold stages warmer. StageShift = how many stages right now (0 = none); FreezingWaterSafe >= 1 while
	// the buff keeps freezing water harmless. The thermometer recolours its blocks from them (ColdGauge.inl DrawStrip)
	// and drops the water look. Read whatever the file's Running says: the globals themselves say 0 when nothing runs.
	std::vector<CfgValue> g_cfgColdShift, g_cfgWaterSafe;
	int ColdProtectionShift()
	{
		int shift = 0;
		for (const auto& v : g_cfgColdShift) {
			shift = std::max(shift, static_cast<int>(std::lround(v.Get())));
		}
		return shift;
	}
	bool ColdProtectionWaterSafe()
	{
		return std::ranges::any_of(g_cfgWaterSafe, [](const CfgValue& v) { return v.Get() >= 1.0; });
	}

	// the custom needs, for the menu (id, name); fixed after kDataLoaded
	std::vector<std::pair<std::string, std::string>> g_cfgCustom;

	// the picture files a tile can show (path in lower case): .swf and .png (his test 2026-11-09: the game's own picture
	// loader read a .png; it could not read a .jpg or a .dds, and a .dds without it showed as stripes)
	inline bool IconFileOk(const std::string& a_low)
	{
		return a_low.ends_with(".swf") || a_low.ends_with(".png");
	}

	namespace cfg
	{
		std::string Trim(std::string a_s)
		{
			const auto b = a_s.find_first_not_of(" \t\r\n");
			if (b == std::string::npos) {
				return {};
			}
			const auto e = a_s.find_last_not_of(" \t\r\n");
			return a_s.substr(b, e - b + 1);
		}
		std::string Lower(std::string a_s)
		{
			std::ranges::transform(a_s, a_s.begin(), [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
			return a_s;
		}
		std::vector<std::string> Split(const std::string& a_s, char a_sep)
		{
			std::vector<std::string> out;
			std::string cur;
			for (char c : a_s) {
				if (c == a_sep) {
					out.push_back(Trim(cur));
					cur.clear();
				} else {
					cur += c;
				}
			}
			out.push_back(Trim(cur));
			std::erase_if(out, [](const std::string& s) { return s.empty(); });
			return out;
		}
		bool ParseId(const std::string& a_s, RE::FormID& a_id)
		{
			try {
				size_t used = 0;
				const unsigned long v = std::stoul(a_s, &used, 16);
				if (used != a_s.size() || v > 0xFFFFFF) {
					return false;  // a load order byte (0x2C000802) or junk
				}
				a_id = static_cast<RE::FormID>(v);
				return true;
			} catch (...) {
				return false;
			}
		}
		// "0x000802" (this plugin) or "Other.esp|0x000802": the plugin and the local ID
		bool ParseForm(const std::string& a_s, const std::string& a_plugin, std::string& a_file, RE::FormID& a_id)
		{
			const auto bar = a_s.find('|');
			a_file = bar == std::string::npos ? a_plugin : Trim(a_s.substr(0, bar));
			const std::string id = Trim(bar == std::string::npos ? a_s : a_s.substr(bar + 1));
			return id.size() > 2 && (id[0] == '0') && (id[1] == 'x' || id[1] == 'X') && ParseId(id.substr(2), a_id);
		}

		struct Ctx
		{
			RE::TESDataHandler* data;
			const std::string*  plugin;
			std::string         where;  // "file [Section] Key" for the log
			int                 problems = 0;
		};
		void Bad(Ctx& a_c, const std::string& a_why)
		{
			++a_c.problems;
			logger::info("  {}: {}", a_c.where, a_why);
		}

		CfgValue Value(Ctx& a_c, const std::string& a_s)
		{
			CfgValue v;
			const std::string low = Lower(a_s);
			if (low.starts_with("property:") || low.starts_with("variable:")) {
				// dropped in 1.0.3: NeedsHUD no longer reads script values (they crashed a save load)
				Bad(a_c, "script values are no longer read (" + a_s + "); copy the value into a global from your own script "
					"and give that global's form ID here (see the patch guide)");
				return v;
			}
			if (low.find("0x") != std::string::npos) {
				std::string file;
				RE::FormID  id = 0;
				if (!ParseForm(a_s, *a_c.plugin, file, id)) {
					Bad(a_c, "the form ID can't be read (" + a_s + "); write it without the load order byte");
					return v;
				}
				v.kind = CfgValue::Kind::Global;
				v.global = LookupGlobal(a_c.data, id, file);
				if (!v.global) {
					Bad(a_c, std::format("no global {:06X} in {} (missing, or another record type)", id, file));
				}
				return v;
			}
			try {
				size_t used = 0;
				v.number = std::stod(a_s, &used);
				if (used == a_s.size()) {
					v.kind = CfgValue::Kind::Number;
					return v;
				}
			} catch (...) {
			}
			Bad(a_c, "not a number or a global's form ID (" + a_s + ")");
			return v;
		}

		RE::SpellItem* Spell(Ctx& a_c, const std::string& a_s)
		{
			std::string file;
			RE::FormID  id = 0;
			if (!ParseForm(a_s, *a_c.plugin, file, id)) {
				Bad(a_c, "the spell's form ID can't be read (" + a_s + ")");
				return nullptr;
			}
			auto sp = a_c.data->LookupForm<RE::SpellItem>(id, file);
			if (!sp) {
				Bad(a_c, std::format("no spell {:06X} in {}", id, file));
			}
			return sp;
		}

		int TileKind(const std::string& a_section)
		{
			static const std::pair<const char*, int> KINDS[] = { { "hunger", CFG_HUNGER }, { "thirst", CFG_THIRST },
				{ "tiredness", CFG_TIRED }, { "dirt", CFG_DIRT }, { "drunk", CFG_DRUNK }, { "bladder", CFG_BLADDER },
				{ "fun", CFG_FUN }, { "social", CFG_SOCIAL } };
			const std::string low = Lower(a_section);
			for (const auto& [n, k] : KINDS) {
				if (low == n) {
					return k;
				}
			}
			return -1;
		}

		// one disease family: its stage spells and the spells that stay beside them (counted as one disease)
		struct CfgDisease
		{
			std::string name;
			std::vector<RE::SpellItem*> stages, carriers;
		};
	}

	// the stage words of config diseases live here for good (DiseaseForm keeps a pointer)
	std::deque<std::string>              g_cfgWordText;
	std::deque<std::vector<const char*>> g_cfgWords;

	void LoadOneConfig(RE::TESDataHandler* a_data, const std::filesystem::path& a_path, int& a_group)
	{
		const std::string fileName = a_path.filename().string();
		std::ifstream in(a_path);
		if (!in) {
			logger::info("Config {}: can't be opened", fileName);
			return;
		}
		// sections in order, each a list of (key, value)
		std::vector<std::pair<std::string, std::vector<std::pair<std::string, std::string>>>> sections;
		std::string line;
		while (std::getline(in, line)) {
			if (const auto sc = line.find(';'); sc != std::string::npos) {
				line = line.substr(0, sc);  // comments, also after a value
			}
			line = cfg::Trim(line);
			if (line.empty()) {
				continue;
			}
			if (line.front() == '[' && line.back() == ']') {
				sections.push_back({ cfg::Trim(line.substr(1, line.size() - 2)), {} });
				continue;
			}
			const auto eq = line.find('=');
			if (eq == std::string::npos || sections.empty()) {
				continue;
			}
			sections.back().second.push_back({ cfg::Trim(line.substr(0, eq)), cfg::Trim(line.substr(eq + 1)) });
		}
		const auto get = [](const auto& a_keys, const char* a_key) -> std::string {
			for (const auto& [k, v] : a_keys) {
				if (cfg::Lower(k) == cfg::Lower(a_key)) {
					return v;
				}
			}
			return {};
		};

		CfgMod m;
		m.file = fileName;
		const auto modSec = std::ranges::find_if(sections, [](const auto& s) { return cfg::Lower(s.first) == "mod"; });
		if (modSec == sections.end()) {
			logger::info("Config {}: no [Mod] section, skipped", fileName);
			return;
		}
		m.name = get(modSec->second, "Name");
		if (m.name.empty()) {
			m.name = a_path.stem().string();
		}
		// the plugin: Plugin, else the first loaded AltPlugin (a merged or renamed release with the same IDs)
		std::vector<std::string> plugins{ get(modSec->second, "Plugin") };
		for (const auto& alt : cfg::Split(get(modSec->second, "AltPlugin"), ',')) {
			plugins.push_back(alt);
		}
		for (const auto& p : plugins) {
			if (!p.empty() && ModActive(a_data, p.c_str())) {
				m.plugin = p;
				break;
			}
		}
		if (m.plugin.empty()) {
			logger::info("Config {} ({}): plugin {} not loaded, skipped", fileName, m.name, plugins[0].empty() ? "(none given)" : plugins[0]);
			return;
		}
		cfg::Ctx c{ a_data, &m.plugin, {} };
		if (const auto r = get(modSec->second, "Running"); !r.empty()) {
			c.where = fileName + " [Mod] Running";
			m.running = cfg::Value(c, r);
		}
		if (const auto r = get(modSec->second, "RunningMin"); !r.empty()) {
			try { m.runningMin = std::stod(r); } catch (...) { c.where = fileName + " [Mod] RunningMin"; cfg::Bad(c, "not a number"); }
		}

		std::vector<cfg::CfgDisease> diseases;
		std::vector<std::string>     diseaseWords;
		for (const auto& [section, keys] : sections) {
			const std::string low = cfg::Lower(section);
			if (low == "mod") {
				continue;
			}
			if (low == "coldprotection") {
				if (const auto v = get(keys, "StageShift"); !v.empty()) {
					c.where = fileName + " [ColdProtection] StageShift";
					if (auto val = cfg::Value(c, v); val.Ok()) {
						g_cfgColdShift.push_back(val);
					}
				}
				if (const auto v = get(keys, "FreezingWaterSafe"); !v.empty()) {
					c.where = fileName + " [ColdProtection] FreezingWaterSafe";
					if (auto val = cfg::Value(c, v); val.Ok()) {
						g_cfgWaterSafe.push_back(val);
					}
				}
				continue;
			}
			if (low == "disease") {
				diseaseWords = cfg::Split(get(keys, "Words"), ',');
				for (const auto& [k, v] : keys) {
					const std::string kl = cfg::Lower(k);
					if (kl == "words" || kl.ends_with(".carriers")) {
						continue;
					}
					c.where = fileName + " [Disease] " + k;
					cfg::CfgDisease d;
					d.name = k;
					for (const auto& s : cfg::Split(v, ',')) {
						if (auto sp = cfg::Spell(c, s)) {
							d.stages.push_back(sp);
						}
					}
					c.where = fileName + " [Disease] " + k + ".Carriers";
					for (const auto& s : cfg::Split(get(keys, (k + ".Carriers").c_str()), ',')) {
						if (auto sp = cfg::Spell(c, s)) {
							d.carriers.push_back(sp);
						}
					}
					if (!d.stages.empty()) {
						diseases.push_back(std::move(d));
					}
				}
				continue;
			}
			CfgTile t;
			const bool isCold = low == "cold";
			if (low.starts_with("need:")) {
				t.id = cfg::Trim(section.substr(5));
				const int used = static_cast<int>(g_cfgCustom.size());
				if (t.id.empty() || used >= CFG_CUSTOM_MAX) {
					logger::info("  {} [{}]: {}", fileName, section, t.id.empty() ? "a custom need needs an id" : "too many custom needs (8 at most)");
					continue;
				}
				const auto known = std::ranges::find_if(g_cfgCustom, [&](const auto& p) { return cfg::Lower(p.first) == cfg::Lower(t.id); });
				t.kind = known != g_cfgCustom.end() ? CFG_CUSTOM0 + static_cast<int>(known - g_cfgCustom.begin()) : CFG_CUSTOM0 + used;
				t.displayName = get(keys, "Name");
				if (t.displayName.empty()) {
					t.displayName = t.id;
				}
				if (known == g_cfgCustom.end()) {
					g_cfgCustom.push_back({ t.id, t.displayName });
				}
			} else if (!isCold) {
				t.kind = cfg::TileKind(section);
				if (t.kind < 0) {
					logger::info("  {} [{}]: unknown section, skipped", fileName, section);
					continue;
				}
			}
			const auto key = [&](const char* a_k) {
				c.where = fileName + " [" + section + "] " + a_k;
				return get(keys, a_k);
			};
			if (const auto s = key("Value"); !s.empty()) {
				t.value = cfg::Value(c, s);
			}
			if (const auto s = key("Direction"); !s.empty()) {
				const std::string d = cfg::Lower(s);
				if (d == "falling") {
					t.falling = true;
				} else if (d != "rising") {
					cfg::Bad(c, "write rising or falling");
				}
			}
			for (const auto& s : cfg::Split(key("Stages"), ',')) {
				c.where = fileName + " [" + section + "] Stages";
				t.stages.push_back(cfg::Value(c, s));
			}
			t.names = cfg::Split(key("Names"), ',');
			for (const auto& s : cfg::Split(key("StageSpells"), ',')) {
				c.where = fileName + " [" + section + "] StageSpells";
				if (auto sp = cfg::Spell(c, s)) {
					t.stageSpells.push_back(sp);
				}
			}
			if (const auto s = key("Enabled"); !s.empty()) {
				t.enabled = cfg::Value(c, s);
			}
			if (const auto s = key("EnabledMin"); !s.empty()) {
				try { t.enabledMin = std::stod(s); } catch (...) { cfg::Bad(c, "not a number"); }
			}
			t.bonusFirst = cfg::Lower(key("Bonus")) == "first";
			t.word = key("Word");
			t.cap = cfg::Lower(key("Cap")) != "no";
			t.icons = cfg::Split(key("Icons"), ',');
			if (const auto s = key("Icon"); !s.empty() && t.icons.empty()) {
				t.icons = { s };
			}
			for (const auto& i : t.icons) {  // pictures a tile can show (IconFileOk)
				if (!IconFileOk(cfg::Lower(i))) {
					c.where = fileName + " [" + section + "] Icons";
					cfg::Bad(c, std::format("{} is not a .swf or .png (the game can't read a .jpg or .dds here; save it as .png): a placeholder shows instead", i));
				}
			}
			if (isCold) {
				if (const auto s = key("Target"); !s.empty()) {
					t.target = cfg::Value(c, s);
				}
				if (const auto s = key("NearHeat"); !s.empty()) {
					t.nearHeat = cfg::Value(c, s);
				}
				if (const auto s = key("InColdWater"); !s.empty()) {
					t.inColdWater = cfg::Value(c, s);
				}
				if (const auto s = key("Wetness"); !s.empty()) {
					t.wetness = cfg::Value(c, s);
				}
				if (const auto s = key("WetnessMax"); !s.empty()) {
					t.wetnessMax = cfg::Value(c, s);
				}
				t.wetNames = cfg::Split(key("WetnessNames"), ',');
			}
			// the shape must hold together, or the tile is left out
			c.where = fileName + " [" + section + "]";
			const bool bySpells = !t.stageSpells.empty();
			if (bySpells) {
				if (t.names.size() != t.stageSpells.size()) {
					cfg::Bad(c, std::format("{} stage spells but {} names: give one name per spell", t.stageSpells.size(), t.names.size()));
					continue;
				}
				if (isCold && (t.names.size() < 3 || t.names.size() > 6)) {
					cfg::Bad(c, "the thermometer takes three to six stages: left out");
					continue;
				}
				// the place arrow is read on the Stages scale: with stage spells and no Stages there is no scale for it
				if (isCold && t.target.Ok() && t.stages.size() != t.names.size() + 1) {
					cfg::Bad(c, "Target needs Stages (one number more than Names) to place the arrow: the arrow is left out");
					t.target = {};
				}
				if (isCold && t.target.Ok() && std::ranges::any_of(t.stages, [](const CfgValue& v) { return !v.Ok(); })) {
					cfg::Bad(c, "a stage limit can't be read, so Target can't be placed: the arrow is left out");
					t.target = {};
				}
			} else {
				if (!t.value.Ok()) {
					cfg::Bad(c, "no usable Value (or StageSpells): left out");
					continue;
				}
				if (t.names.empty() || t.stages.size() != t.names.size() + 1) {
					cfg::Bad(c, std::format("{} stage limits for {} names: Stages needs one number more than Names (the last is the maximum)",
								t.stages.size(), t.names.size()));
					continue;
				}
				if (std::ranges::any_of(t.stages, [](const CfgValue& v) { return !v.Ok(); })) {
					cfg::Bad(c, "a stage limit can't be read: left out");
					continue;
				}
				if (isCold && (t.names.size() < 3 || t.names.size() > 6)) {
					cfg::Bad(c, "the thermometer takes three to six stages: left out");
					continue;
				}
			}
			t.present = true;
			if (isCold) {
				m.cold = std::move(t);
			} else {
				m.tiles.push_back(std::move(t));
			}
		}

		// diseases: each family counts as one, its stages in this file's words (like Oregon and RND, DiseaseTile.inl)
		if (!diseases.empty()) {
			g_cfgWords.emplace_back();
			auto& words = g_cfgWords.back();
			for (const auto& w : diseaseWords) {
				g_cfgWordText.push_back(w + ", stage");
				words.push_back(g_cfgWordText.back().c_str());
			}
			// the list gets its full length now, before any family keeps its address: growing it later would move it and
			// leave the earlier families pointing at freed memory. At least 3 words (the tile reads up to the third)
			std::size_t longest = 3;
			for (const auto& d : diseases) {
				longest = (std::max)(longest, d.stages.size());
			}
			while (words.size() < longest) {
				g_cfgWordText.push_back("Stage");
				words.push_back(g_cfgWordText.back().c_str());
			}
			for (const auto& d : diseases) {
				++a_group;
				const int n = static_cast<int>(d.stages.size());
				std::vector<RE::SpellItem*> mine = d.stages;
				mine.insert(mine.end(), d.carriers.begin(), d.carriers.end());
				// the same spells in the built-in table (a vanilla disease, an older entry) give way to this family
				std::erase_if(g_diseaseForms, [&](const DiseaseForm& f) { return std::ranges::find(mine, f.spell) != mine.end(); });
				for (int st = 0; st < n; ++st) {
					DiseaseForm f{ d.stages[st], st, false, d.name };
					f.lsStages = n;
					f.words = words.data();
					f.group = a_group;
					g_diseaseForms.push_back(f);
					if (d.stages[st]->GetSpellType() != RE::MagicSystem::SpellType::kDisease) {
						g_stagedAbilities.push_back(d.stages[st]);  // an ability per stage (like iNeed) counts as a disease too
					}
				}
				for (auto sp : d.carriers) {
					DiseaseForm f{ sp, 0, false, d.name };
					f.lsStages = n;
					f.words = words.data();
					f.group = a_group;
					f.orBase = true;
					g_diseaseForms.push_back(f);
				}
			}
		}

		std::string what;
		for (const auto& t : m.tiles) {
			what += (what.empty() ? "" : ", ") + (t.kind >= CFG_CUSTOM0 ? "need " + t.id : std::string(
				t.kind == CFG_HUNGER ? "hunger" : t.kind == CFG_THIRST ? "thirst" : t.kind == CFG_TIRED ? "tiredness" :
				t.kind == CFG_DIRT ? "dirt" : t.kind == CFG_DRUNK ? "drunk" : t.kind == CFG_BLADDER ? "bladder" :
				t.kind == CFG_FUN ? "fun" : "social"));
		}
		if (m.cold.present) {
			what += (what.empty() ? "" : ", ") + std::string("cold");
		}
		if (!diseases.empty()) {
			what += (what.empty() ? "" : ", ") + std::format("{} disease(s)", diseases.size());
		}
		logger::info("Config {} ({}, {}): {}; {} problem(s)", fileName, m.name, m.plugin, what.empty() ? "nothing usable" : what, c.problems);
		if (!m.tiles.empty() || m.cold.present) {
			g_cfgMods.push_back(std::move(m));
		}
	}

	// every config file, in name order (the first running file wins a tile)
	void LoadNeedsConfigs(RE::TESDataHandler* a_data)
	{
		const std::filesystem::path dir{ "Data/SKSE/Plugins/NeedsHUD" };
		std::error_code ec;
		if (!a_data || !std::filesystem::is_directory(dir, ec)) {
			logger::info("Config files: none (no Data/SKSE/Plugins/NeedsHUD folder)");
			return;
		}
		std::vector<std::filesystem::path> files;
		for (const auto& e : std::filesystem::directory_iterator(dir, ec)) {
			if (e.is_regular_file() && cfg::Lower(e.path().extension().string()) == ".ini") {
				files.push_back(e.path());
			}
		}
		std::ranges::sort(files);
		int group = 400;  // after the built-in disease groups (Oregon 1+, RND 101+, Last Seed 201+, iNeed 301+)
		for (const auto& f : files) {
			LoadOneConfig(a_data, f, group);
		}
		logger::info("Config files: {} read, {} in use", files.size(), g_cfgMods.size());
	}

	// the tile a running config mod gives this kind (first in file order), or nullptr
	const CfgTile* ConfigTileFor(int a_kind)
	{
		for (const auto& m : g_cfgMods) {
			if (!m.Running()) {
				continue;
			}
			for (const auto& t : m.tiles) {
				if (t.kind == a_kind && !(t.enabled.Ok() && t.enabled.Get() < t.enabledMin)) {
					return &t;
				}
			}
		}
		return nullptr;
	}
	const CfgTile* ConfigCold()
	{
		for (const auto& m : g_cfgMods) {
			if (m.Running() && m.cold.present && !(m.cold.enabled.Ok() && m.cold.enabled.Get() < m.cold.enabledMin)) {
				return &m.cold;
			}
		}
		return nullptr;
	}

	// a config tile's value on a rising scale (0 = best), its limits likewise, so the stage logic is the same either way
	struct CfgScale
	{
		double              value = 0.0, raw = 0.0;
		std::vector<double> bounds;  // n + 1, rising
		int                 st = 0;
		bool                ok = false;
	};
	CfgScale ConfigScale(const CfgTile& a_t)
	{
		CfgScale s;
		const int n = static_cast<int>(a_t.names.size());
		if (!a_t.stageSpells.empty()) {
			// one spell per stage: the worst one running is the stage
			int st = -1;
			for (int i = 0; i < static_cast<int>(a_t.stageSpells.size()); ++i) {
				if (SpellRunning(a_t.stageSpells[i])) {
					st = i;
				}
			}
			if (st < 0) {
				return s;
			}
			for (int i = 0; i <= n; ++i) {
				s.bounds.push_back(i);
			}
			s.st = st;
			s.value = st + 0.5;
			s.raw = st;
			s.ok = true;
			return s;
		}
		std::vector<double> b;
		for (const auto& v : a_t.stages) {
			b.push_back(v.Get());
		}
		for (size_t i = 1; i < b.size(); ++i) {
			b[i] = std::max(b[i], b[i - 1]);  // limits never go backwards (an MCM can move one past the next)
		}
		s.raw = a_t.value.Get();
		const double lo = b.front(), hi = b.back();
		if (a_t.falling) {
			// higher = better: flip the scale, so the best stage (the top limits) comes first
			s.value = hi - s.raw;
			for (auto it = b.rbegin(); it != b.rend(); ++it) {
				s.bounds.push_back(hi - *it);
			}
		} else {
			s.value = s.raw - lo;
			for (double x : b) {
				s.bounds.push_back(x - lo);
			}
		}
		s.value = std::clamp(s.value, 0.0, s.bounds.back());
		s.st = StageIn(s.value, s.bounds);
		s.ok = true;
		return s;
	}

	// the thermometer from a config file (ColdGauge.inl's ColdCfgRead): its stages as the gauge's blocks, the value
	// piecewise inside each, the place arrow on the same scale
	bool ConfigColdRead(ColdCfgRead& a_out)
	{
		const CfgTile* t = ConfigCold();
		if (!t) {
			return false;
		}
		const CfgScale s = ConfigScale(*t);
		if (!s.ok) {
			return false;
		}
		const int n = static_cast<int>(t->names.size());
		const auto pos = [&](double a_rising) {
			const double v = std::clamp(a_rising, 0.0, s.bounds.back());
			const int    i = StageIn(v, s.bounds);
			const double w = s.bounds[i + 1] - s.bounds[i];
			return (i + (w > 0.0 ? std::clamp((v - s.bounds[i]) / w, 0.0, 1.0) : 1.0)) / n;
		};
		// the place arrow on the Stages numbers (1.0.4 review: with stage spells the mercury's scale is the stage count,
		// 0..n, so a numeric Target placed on it landed at the wrong spot). Same limits as a value-driven gauge: never
		// going backwards, flipped when higher is better.
		const auto arrowPos = [&](double a_raw) {
			std::vector<double> b;
			for (const auto& v : t->stages) {
				b.push_back(v.Get());
			}
			for (size_t i = 1; i < b.size(); ++i) {
				b[i] = std::max(b[i], b[i - 1]);
			}
			const double lo = b.front(), hi = b.back();
			std::vector<double> bounds;
			double v = 0.0;
			if (t->falling) {
				v = hi - a_raw;
				for (auto it = b.rbegin(); it != b.rend(); ++it) {
					bounds.push_back(hi - *it);
				}
			} else {
				v = a_raw - lo;
				for (double x : b) {
					bounds.push_back(x - lo);
				}
			}
			v = std::clamp(v, 0.0, bounds.back());
			const int    i = StageIn(v, bounds);
			const double w = bounds[i + 1] - bounds[i];
			return (i + (w > 0.0 ? std::clamp((v - bounds[i]) / w, 0.0, 1.0) : 1.0)) / n;
		};
		a_out.n = n;
		a_out.idx = s.st;
		a_out.target = pos(s.value);
		a_out.raw = s.raw;
		a_out.names = t->names;
		a_out.word = t->word.empty() ? std::string("Cold") : t->word;
		a_out.nearHeat = t->nearHeat.Ok() && t->nearHeat.Get() > 0.0;
		a_out.inWater = t->inColdWater.Ok() && t->inColdWater.Get() > 0.0;
		a_out.arrow = a_out.nearHeat ? 0.0 : (t->target.Ok() && t->stages.size() == static_cast<size_t>(n) + 1) ? arrowPos(t->target.Get()) : -1.0;
		// wetness: cut evenly into three, drawn on Frostfall's bar (dry / Damp / Wet / Drenched)
		a_out.wetV = -1.0;
		if (t->wetness.Ok()) {
			const double mx = t->wetnessMax.Ok() ? t->wetnessMax.Get() : 1.0;
			a_out.wetRaw = t->wetness.Get();
			const double f = mx > 0.0 ? std::clamp(a_out.wetRaw / mx, 0.0, 1.0) : 0.0;
			if (f > 0.0) {
				const int    lv = std::min(2, static_cast<int>(f * 3.0));
				const double in = f * 3.0 - lv;
				a_out.wetV = FF_WET_BOUNDS[lv] + in * (FF_WET_BOUNDS[lv + 1] - FF_WET_BOUNDS[lv]) + 0.001;
			} else {
				a_out.wetV = 0.0;
			}
			a_out.wetNames = t->wetNames;
		}
		return true;
	}
