// ---- the player snapshot (1.0.3): every game value the HUD needs about the player, copied on the game's MAIN thread.
// The ticks draw on the UI thread (AddUITask); reading the player's live effect list or spell list from there races with
// the main thread (an effect ending while the HUD walks the list = a freed node = a crash; found by the 1.0.3 code
// review after a player's crash). So a main-thread task (CaptureSnapshot, queued by the ticker before each UI tick)
// copies what the tiles need into plain data, and the UI tick reads only that copy (SnapView). Form pointers to spells
// and effects are kept only for comparing; anything read from a spell (its type, its name) is copied while still on
// the main thread, because a spell made at runtime (a potion) can be freed later.

	struct EffSnap
	{
		const RE::MagicItem*     spell = nullptr;  // compare only, never dereference on the UI thread
		RE::FormID               spellID = 0;
		const RE::EffectSetting* mgef = nullptr;   // the effect's base record (data records, kept for the whole session)
		bool                     dispelled = false;
		bool                     inactive = false;
		bool                     disease = false;  // the spell is of type Disease
		bool                     isSpell = false;  // the spell is a SpellItem (not a potion, enchantment or ingredient)
		float                    duration = 0.0f;
		float                    elapsed = 0.0f;
		std::string              spellName;        // filled for diseases and staged abilities only (the disease tile)
	};

	struct PlayerSnap
	{
		bool                                ok = false;  // false until the first capture after a load
		std::vector<EffSnap>                effects;
		std::vector<const RE::SpellItem*>   hasSpells;   // which of the watched spells the player has (HasSpell)
		double                              warmth = 0.0;
		bool                                combat = false;
		double                              weight = -1.0;  // the player's body weight slider (RND's body tile)
		bool                                loaded3D = false;  // the player's 3D is loaded (in the world, not at a menu)
		bool                                iNeedRunning = false;  // iNeed's quest runs (quest flags are live game state)
		std::vector<std::pair<const RE::TESGlobal*, float>> globals;  // the watched globals' values (GV)
		double                              daysPassed = -1.0;  // the calendar's days passed (-1 = no calendar)
	};

	bool INeedQuestRunning();  // ColdGauge.inl (main thread only)

	std::mutex                         g_snapLock;
	PlayerSnap                         g_snap;          // written by the main thread, under g_snapLock
	PlayerSnap                         g_view;          // the UI thread's copy for this tick (UI thread only)
	std::vector<const RE::SpellItem*>  g_spellWatch;    // spells the tiles ask about (HasSpell); grows as they ask
	std::mutex                         g_spellWatchLock;
	// abilities the disease tile treats as diseases (iNeed's and config files' stage abilities; filled at kDataLoaded by
	// DiseaseTile.inl / ConfigLoader.inl, before any tick, then only read)
	std::vector<const RE::MagicItem*>  g_stagedAbilities;
	// global variables the tiles read (GV); grows as they ask, under g_globalWatchLock
	std::vector<const RE::TESGlobal*>  g_globalWatch;
	std::mutex                         g_globalWatchLock;
	// the game's main thread (set at kDataLoaded and by every capture) and the thread running the UI ticks
	std::atomic<std::thread::id>       g_mainThreadId{};
	std::atomic<std::thread::id>       g_uiTickThreadId{};

	// main thread: copy the player's state
	void CaptureSnapshot()
	{
		g_mainThreadId = std::this_thread::get_id();
		PlayerSnap s;
		// global variables and the calendar are copied even without a player (they are game state, not the player's)
		{
			std::lock_guard lock(g_globalWatchLock);
			s.globals.reserve(g_globalWatch.size());
			for (auto g : g_globalWatch) {
				s.globals.emplace_back(g, g->value);
			}
		}
		if (const auto cal = RE::Calendar::GetSingleton()) {
			s.daysPassed = static_cast<double>(cal->GetDaysPassed());
		}
		const auto player = RE::PlayerCharacter::GetSingleton();
		if (!player) {
			std::lock_guard lock(g_snapLock);
			g_snap = std::move(s);  // ok stays false: no player data
			return;
		}
		if (auto list = player->GetActiveEffectList()) {
			for (auto ae : *list) {
				if (!ae) {
					continue;
				}
				EffSnap e;
				e.spell = ae->spell;
				e.spellID = ae->spell ? ae->spell->GetFormID() : 0;
				e.mgef = ae->GetBaseObject();
				e.dispelled = ae->flags.any(RE::ActiveEffect::Flag::kDispelled);
				e.inactive = ae->flags.any(RE::ActiveEffect::Flag::kInactive);
				e.duration = ae->duration;
				e.elapsed = ae->elapsedSeconds;
				if (ae->spell) {
					e.disease = ae->spell->GetSpellType() == RE::MagicSystem::SpellType::kDisease;
					auto asSpell = ae->spell->As<RE::SpellItem>();
					e.isSpell = asSpell != nullptr;
					const bool staged = std::find(g_stagedAbilities.begin(), g_stagedAbilities.end(), ae->spell) != g_stagedAbilities.end();
					if (e.disease || staged) {
						const char* n = ae->spell->GetName();
						e.spellName = n ? n : "";
					}
				}
				s.effects.push_back(std::move(e));
			}
		}
		{
			std::lock_guard lock(g_spellWatchLock);
			for (auto sp : g_spellWatch) {
				if (sp && player->HasSpell(const_cast<RE::SpellItem*>(sp))) {
					s.hasSpells.push_back(sp);
				}
			}
		}
		s.warmth = static_cast<double>(player->GetWarmthRating());
		s.combat = player->IsInCombat();
		if (auto base = player->GetActorBase()) {
			s.weight = static_cast<double>(base->weight);
		}
		s.loaded3D = player->Is3DLoaded();
		s.iNeedRunning = INeedQuestRunning();
		s.ok = true;
		std::lock_guard lock(g_snapLock);
		g_snap = std::move(s);
	}

	// after a load starts: nothing from the previous game may be shown
	void ClearSnapshot()
	{
		std::lock_guard lock(g_snapLock);
		g_snap = PlayerSnap{};
	}

	// UI thread, at the start of each tick: take this tick's copy
	void TakeSnapshotView()
	{
		g_uiTickThreadId = std::this_thread::get_id();
		std::lock_guard lock(g_snapLock);
		g_view = g_snap;
	}

	// every global the HUD reads is found through this at kDataLoaded (main thread), which puts it on the watch list,
	// so the first capture already copies it (1.0.4 review: registering on first ask meant one live read off the main
	// thread). Main thread only.
	RE::TESGlobal* LookupGlobal(RE::TESDataHandler* a_data, RE::FormID a_id, std::string_view a_file)
	{
		auto g = a_data ? a_data->LookupForm<RE::TESGlobal>(a_id, a_file) : nullptr;
		if (g) {
			std::lock_guard lock(g_globalWatchLock);
			if (std::find(g_globalWatch.begin(), g_globalWatch.end(), g) == g_globalWatch.end()) {
				g_globalWatch.push_back(g);
			}
		}
		return g;
	}

	// a global variable's value, safe from any thread (1.0.4 review: the UI ticks read globals live). On the main thread
	// it is the live value; elsewhere the value from the last main-thread capture (the UI tick's own copy, or the newest
	// capture under its lock for any other thread). Off the main thread it never reads the live value: a global not in
	// the copy (not found through LookupGlobal, or no capture since a load) reads 0 and is added to the watch list.
	float GV(const RE::TESGlobal* a_g)
	{
		if (!a_g) {
			return 0.0f;
		}
		const auto me = std::this_thread::get_id();
		if (me == g_mainThreadId.load()) {
			return a_g->value;
		}
		auto find = [a_g](const PlayerSnap& a_s, float& a_out) {
			for (const auto& [g, v] : a_s.globals) {
				if (g == a_g) {
					a_out = v;
					return true;
				}
			}
			return false;
		};
		float v = 0.0f;
		if (me == g_uiTickThreadId.load()) {
			if (find(g_view, v)) {
				return v;
			}
		} else {
			std::lock_guard lock(g_snapLock);
			if (find(g_snap, v)) {
				return v;
			}
		}
		std::lock_guard lock(g_globalWatchLock);
		if (std::find(g_globalWatch.begin(), g_globalWatch.end(), a_g) == g_globalWatch.end()) {
			g_globalWatch.push_back(a_g);
			logger::warn("Global {:08X} was read before it was registered: it reads 0 until the next copy", a_g->GetFormID());
		}
		return 0.0f;
	}

	// the calendar's days passed, safe from any thread (same rule as GV): live on the main thread, the copy elsewhere;
	// -1 when there is none (no calendar, or no capture yet since a load)
	double DaysPassed()
	{
		const auto me = std::this_thread::get_id();
		if (me == g_mainThreadId.load()) {
			const auto cal = RE::Calendar::GetSingleton();
			return cal ? static_cast<double>(cal->GetDaysPassed()) : -1.0;
		}
		if (me == g_uiTickThreadId.load()) {
			return g_view.daysPassed;
		}
		std::lock_guard lock(g_snapLock);
		return g_snap.daysPassed;
	}

	// UI thread: does the player have this spell (as of the last capture)? A spell asked about for the first time is
	// added to the watch list and reads "no" until the next capture (one tick, a tenth of a second)
	bool SnapHasSpell(const RE::SpellItem* a_spell)
	{
		if (!a_spell) {
			return false;
		}
		if (std::find(g_view.hasSpells.begin(), g_view.hasSpells.end(), a_spell) != g_view.hasSpells.end()) {
			return true;
		}
		std::lock_guard lock(g_spellWatchLock);
		if (std::find(g_spellWatch.begin(), g_spellWatch.end(), a_spell) == g_spellWatch.end()) {
			g_spellWatch.push_back(a_spell);
		}
		return false;
	}

	// UI thread: an effect of this base record runs on the player
	bool SnapHasEffect(const RE::EffectSetting* a_mgef)
	{
		if (!a_mgef) {
			return false;
		}
		return std::any_of(g_view.effects.begin(), g_view.effects.end(),
			[&](const EffSnap& e) { return e.mgef == a_mgef && !e.dispelled; });
	}
