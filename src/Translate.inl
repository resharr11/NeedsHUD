	// ---- translation hook (2026-11-09, his call: NeedsHUD ships English only and helps translators) ----
	// Skyrim's own translation file format: Interface/Translations/NeedsHUD_<LANGUAGE>.txt, UTF-16 LE with BOM, one
	// "$NHUD_English text<TAB>Translated text" per line, picked by the game's language (sLanguage in Skyrim.ini). The
	// English file we ship lists every phrase NeedsHUD shows. A missing file or line shows the English text, so a
	// translation can be partial. Patterns keep {} where a number or word goes ("Warmth {}"); a translation may move
	// the {} but must keep the same count, or the English pattern is used.
	namespace tr
	{
		inline std::unordered_map<std::string, std::string> g_map;
		// every key starts with $NHUD_ (the game loads these files into one shared table for all mods: the prefix keeps
		// "$Hunger" from replacing another mod's text of the same name)
		inline const std::string KEY_PREFIX = "$NHUD_";
		inline std::string g_lang = "ENGLISH";
		inline std::string g_override;  // sLanguage= in NeedsHUD.ini (kept when the menu rewrites the file)

		inline std::string Utf16ToUtf8(const std::u16string& a_in)
		{
			std::string out;
			for (size_t i = 0; i < a_in.size(); ++i) {
				std::uint32_t c = a_in[i];
				if (c >= 0xD800 && c <= 0xDBFF && i + 1 < a_in.size() && a_in[i + 1] >= 0xDC00 && a_in[i + 1] <= 0xDFFF) {
					c = 0x10000 + ((c - 0xD800) << 10) + (a_in[i + 1] - 0xDC00);
					++i;
				}
				if (c < 0x80) {
					out += static_cast<char>(c);
				} else if (c < 0x800) {
					out += static_cast<char>(0xC0 | (c >> 6));
					out += static_cast<char>(0x80 | (c & 0x3F));
				} else if (c < 0x10000) {
					out += static_cast<char>(0xE0 | (c >> 12));
					out += static_cast<char>(0x80 | ((c >> 6) & 0x3F));
					out += static_cast<char>(0x80 | (c & 0x3F));
				} else {
					out += static_cast<char>(0xF0 | (c >> 18));
					out += static_cast<char>(0x80 | ((c >> 12) & 0x3F));
					out += static_cast<char>(0x80 | ((c >> 6) & 0x3F));
					out += static_cast<char>(0x80 | (c & 0x3F));
				}
			}
			return out;
		}

		// the game's language, as the translation files name it (ENGLISH, GERMAN, RUSSIAN...)
		inline std::string GameLanguage()
		{
			std::string lang = "ENGLISH";
			if (auto ini = RE::INISettingCollection::GetSingleton()) {
				if (auto s = ini->GetSetting("sLanguage:General")) {
					if (const char* v = s->GetString(); v && *v) {
						lang = v;
					}
				}
			}
			for (auto& ch : lang) {
				ch = static_cast<char>(std::toupper(static_cast<unsigned char>(ch)));
			}
			return lang;
		}

		// reads the file for the game's language once (kDataLoaded); English needs no lookup, so it isn't read
		inline void Load()
		{
			g_lang = GameLanguage();
			// for translators (and our tests): "sLanguage=GERMAN" under [General] in NeedsHUD.ini shows that file without
			// switching the whole game's language
			if (std::ifstream ini(IniToRead()); ini) {
				for (std::string line; std::getline(ini, line);) {
					if (!line.empty() && line.back() == '\r') {
						line.pop_back();
					}
					if (line.rfind("sLanguage=", 0) == 0 && line.size() > 10) {
						g_lang = line.substr(10);
						for (auto& ch : g_lang) {
							ch = static_cast<char>(std::toupper(static_cast<unsigned char>(ch)));
						}
						g_override = g_lang;
						logger::info("Translation: NeedsHUD.ini asks for {}", g_lang);
					}
				}
			}
			g_map.clear();
			if (g_lang == "ENGLISH") {
				logger::info("Translation: the game is in English, NeedsHUD's own text is shown as written");
				return;
			}
			const std::string path = "Data/Interface/Translations/NeedsHUD_" + g_lang + ".txt";
			std::ifstream f(path, std::ios::binary);
			if (!f) {
				logger::info("Translation: no {} found, NeedsHUD's own text stays in English", path.substr(5));
				return;
			}
			const std::string raw((std::istreambuf_iterator<char>(f)), std::istreambuf_iterator<char>());
			std::string text;
			if (raw.size() >= 2 && static_cast<unsigned char>(raw[0]) == 0xFF && static_cast<unsigned char>(raw[1]) == 0xFE) {
				std::u16string u;
				for (size_t i = 2; i + 1 < raw.size(); i += 2) {
					u += static_cast<char16_t>(static_cast<unsigned char>(raw[i]) | (static_cast<unsigned char>(raw[i + 1]) << 8));
				}
				text = Utf16ToUtf8(u);
			} else {  // saved as UTF-8 by mistake: read it anyway (skipping a UTF-8 BOM)
				text = raw.size() >= 3 && raw.compare(0, 3, "\xEF\xBB\xBF") == 0 ? raw.substr(3) : raw;
			}
			int lines = 0, used = 0;
			size_t pos = 0;
			while (pos < text.size()) {
				size_t end = text.find('\n', pos);
				if (end == std::string::npos) {
					end = text.size();
				}
				std::string line = text.substr(pos, end - pos);
				pos = end + 1;
				if (!line.empty() && line.back() == '\r') {
					line.pop_back();
				}
				const size_t tab = line.find('\t');
				if (line.rfind(KEY_PREFIX, 0) != 0 || tab == std::string::npos) {
					continue;
				}
				++lines;
				const std::string key = line.substr(KEY_PREFIX.size(), tab - KEY_PREFIX.size()), value = line.substr(tab + 1);
				if (!value.empty()) {
					g_map[key] = value;
					++used;
				}
			}
			logger::info("Translation: {} read, {} of {} lines translated", path.substr(5), used, lines);
		}

		// a phrase as the player should see it (the English when there is no translation)
		inline std::string T(const std::string& a_en)
		{
			const auto it = g_map.find(a_en);
			return it != g_map.end() ? it->second : a_en;
		}
		inline const char* T(const char* a_en)
		{
			if (g_map.empty() || !a_en) {
				return a_en;
			}
			const auto it = g_map.find(a_en);
			return it != g_map.end() ? it->second.c_str() : a_en;
		}

		// a pattern with {} filled in; a translation with broken braces falls back to the English pattern
		template <class... A>
		std::string F(const char* a_pattern, const A&... a_args)
		{
			const char* p = T(a_pattern);
			try {
				return std::vformat(p, std::make_format_args(a_args...));
			} catch (...) {
				return std::vformat(a_pattern, std::make_format_args(a_args...));
			}
		}
	}

