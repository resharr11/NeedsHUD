// ---- the disease tile (2026-11-08, his idea "a you are diseased widget"): shown only while the player has a disease,
// after tiredness in the Needs row. Icon: a plague doctor's beaked mask, three-quarters view (his pick: "the plague
// doctor is cute", "in an oblique perspective"), in Skyrim's materials; its lenses glow sicker green with the stage.
// Stages (read from the files 2026-11-08, UESP Skyrim:Disease): Survival Mode moves most diseases from normal to
// Severe to Crippling after 24 hours untreated (trap versions too); SMI and Starfrost reuse those records (Starfrost
// adds Severe and Crippling Food Poisoning); without Survival a disease has one stage. Several at once: the worst
// shows, with "+N" for the others (his pick). The afflictions (Weakened, Addled, Frostbitten) are not diseases: they
// show on their need's tile (his pick).

	// every block already sick (his rule 2026-11-08: "you have a disease or you don't", so no pale "fine" block like
	// hunger's first): bile yellow-green at stage 1 down to dark rotten olive
	constexpr std::uint32_t DISEASE_COLS[5] = { 0xA9B04E, 0x939C44, 0x7D883B, 0x687433, 0x54602B };
	// Sanguinare Vampiris (his rule 2026-11-08: "the most urgent warning. cure or you are forced on a different
	// playstyle"; it counts days, not stages): blood-red bar and lenses, red frame from the first day
	constexpr std::uint32_t VAMPIRE_COLS[5] = { 0xB0484A, 0x9A3B3E, 0x833033, 0x6D2629, 0x581D20 };
	constexpr std::uint32_t T_BOIL = 0x6E3B2E, T_SWEAT = 0xD8E8EE, T_LEATHER_DK = 0x4A3B2E, T_HAT = 0x2B2724;

	// the same mask turned three-quarters toward the viewer (his ask 2026-11-08: "an oblique perspective"): both lenses
	// show (the far one smaller, half behind the beak), the beak comes forward and down to the left, the hat brim tilts
	void DrawPlagueMask34(GFx& g, double ox, double oy, int st, int n, const std::uint32_t (&a_cols)[5])
	{
		const auto head = [&]() { EllipsePath(g, ox + 5, oy + 2, 15, 16, 0.0); };
		const auto beak = [&]() {
			MoveTo(g, ox - 4, oy + 1);
			CurveTo(g, ox - 18, oy + 3, ox - 33, oy + 22);  // upper edge, out and down to the tip
			CurveTo(g, ox - 14, oy + 18, ox + 6, oy + 11);  // lower edge back under the near lens
			CurveTo(g, ox + 2, oy + 4, ox - 4, oy + 1);
		};
		const auto crown = [&]() {
			MoveTo(g, ox - 6, oy - 15);
			LineTo(g, ox - 4, oy - 32);
			CurveTo(g, ox + 6, oy - 35, ox + 16, oy - 31);
			LineTo(g, ox + 17, oy - 15);
			LineTo(g, ox - 6, oy - 15);
		};
		const auto brim = [&]() { EllipsePath(g, ox + 5, oy - 14, 25, 5.2, -0.10); };
		const auto lens = [&](double lx, double ly, double r, bool glint) {
			Line(g, 2.0 + T_EDGE * 2, 0x000000, 70.0);
			CirclePath(g, lx, ly, r);
			Line(g, 2.0, T_IRON);
			Fill(g, RampColor(a_cols, st, n));
			CirclePath(g, lx, ly, r);
			EndFill(g);
			NoLine(g);
			if (glint) {
				Fill(g, 0xFFFFFF, 70.0);
				CirclePath(g, lx - r * 0.3, ly - r * 0.32, r * 0.24);
				EndFill(g);
			}
		};

		// the head, then the far lens (behind the beak), the beak, the near lens, the hat on top
		Edged(g, head);
		Fill(g, T_LEATHER_DK);
		NoLine(g);
		head();
		EndFill(g);
		lens(ox - 9, oy - 5, 4.4, false);
		Line(g, T_SW + T_EDGE * 2, 0x000000, 70.0);
		beak();
		Line(g, T_SW, T_BONE);
		Fill(g, T_LEATHER_DK);
		beak();
		EndFill(g);
		NoLine(g);
		// the beak's ridge (the light catches its top) and a stitched seam underneath
		Line(g, 1.6, 0x9C7A56, 85.0);
		MoveTo(g, ox - 3, oy + 4);
		CurveTo(g, ox - 16, oy + 6, ox - 29, oy + 19);
		Line(g, 1.0, 0x6A5038, 90.0);
		MoveTo(g, ox + 2, oy + 10);
		CurveTo(g, ox - 12, oy + 15, ox - 27, oy + 20);
		NoLine(g);
		// the strap round to the back of the head, with a rivet
		Line(g, 2.0, 0x8A6A4A);
		MoveTo(g, ox + 14, oy - 3);
		CurveTo(g, ox + 18, oy - 2, ox + 20, oy - 5);
		NoLine(g);
		Fill(g, T_IRON);
		CirclePath(g, ox + 18, oy - 3, 1.5);
		EndFill(g);
		lens(ox + 8, oy - 3, 6.4, true);
		Line(g, T_SW + T_EDGE * 2, 0x000000, 70.0);
		crown();
		brim();
		Line(g, T_SW, T_BONE);
		Fill(g, T_HAT);
		brim();
		EndFill(g);
		Fill(g, T_HAT);
		crown();
		EndFill(g);
		NoLine(g);
		// a band round the crown
		Line(g, 2.0, 0x5A4632);
		MoveTo(g, ox - 5.5, oy - 19);
		CurveTo(g, ox + 6, oy - 21, ox + 16.8, oy - 19);
		NoLine(g);
	}

	// a_pos: 0..1 down the bar, a_st of a_n disease stages
	void DrawDiseaseTile(GFx& g, double a_pos, int a_st, int a_n, bool a_vampire = false)
	{
		const auto& cols = a_vampire ? VAMPIRE_COLS : DISEASE_COLS;
		g.Invoke("clear", nullptr, nullptr, 0);
		const int n = std::max(1, a_n);
		const int st = std::clamp(a_st, 0, n - 1);
		DrawTileFrame(g, a_vampire || (st == n - 1 && n > 1));
		const double cx = 0.0, cy = 0.0;
		const double ox = cx - 12, top = cy - 26, bot = cy + 26;
		const double lt = top + 2, lb = bot - 1;
		const double fy = lt + std::clamp(a_pos, 0.0, 1.0) * (lb - lt);
		DrawPlagueMask34(g, ox + 6, cy + 5, st, n, cols);
		DrawTileBar(g, lt, lb, cols, n, fy, -1, false);
	}

#ifndef NH_DRAW_ONLY  // (the drawing test harness renders the tile without the game)
	// ---- which diseases there are and their stages (form IDs read from the files 2026-11-08) ----
	struct DiseaseForm
	{
		RE::SpellItem* spell = nullptr;
		int            stage = 0;      // 0 normal, 1 Severe, 2 Crippling
		bool           progresses = false;  // a normal stage that Survival moves on to Severe (with Survival on)
		std::string    name;                // the disease's own (normal-stage) name, from the game (any language)
		bool           vampire = false;     // Sanguinare Vampiris: 3 days, then vampirism
		bool           shProgresses = false;  // a normal stage that SunHelm's disease module moves on (while SunHelm runs)
		bool           sunhelm = false;     // SunHelm's stage words: Acute, Severe (Survival's: Severe, Crippling)
		int            lsStages = 0;        // a mod's own staged diseases (Last Seed 5, iNeed 4): how many stages, 0 = none
		const char* const* words = nullptr;  // that mod's stage words (nullptr: Last Seed's)
		int            group = 0;           // Oregon: its family (1..), so a disease and its symptoms count as one
		bool           orBase = false;      // Oregon: the disease itself (its symptoms are separate abilities)
		bool           healsAlone = false;  // Oregon: symptoms left without the disease heal by themselves ("healing")
	};
	constexpr const char* OREGON_FILE = "Oregon - Diseases of Skyrim.esp";
	// SunHelm's optional disease module (read 2026-11-09 from SunHelmDiseases.esp): every vanilla disease (creature and
	// trap) plus 7 from Oblivion get Acute and Severe stages, one more each 24 h uncured (_SHDiseaseProgressionScript)
	constexpr const char* SH_DISEASES_FILE = "SunHelmDiseases.esp";
	bool           g_shDiseasesLoaded = false;
	RE::TESGlobal* g_shDiseasesOn = nullptr;  // _SHDiseasesEnabled (SunHelmSurvival.esp 0x5D19F3), its MCM switch
	bool SunHelmDiseasesActive()
	{
		return g_shDiseasesLoaded && SunHelmOn() && (!g_shDiseasesOn || GV(g_shDiseasesOn) >= 1.0f);
	}
	enum : int { F_BONEBREAKFEVER, F_BROWNROT, F_GREENSPORE, F_GUTWORM, F_RATTLES, F_ROCKJOINT, F_BRAINROT, F_ATAXIA,
		F_WITBANE, F_DROOPS, F_FOODPOISONING, F_SHAKES, F_SWAMPFEVER, F_WITHER, F_DAMPWORM, F_CHILLS, F_FEEBLELIMB,
		F_ASTRALVAPORS, F_COUNT };
	// each family's normal-stage spell (its name heads the tile); a second place to look when the first mod is absent
	// (SunHelm without Survival: Bone Break Fever and Brain Rot come from Skyrim.esm)
	struct FamilyBase { RE::FormID id; const char* file; RE::FormID id2 = 0; const char* file2 = nullptr; };
	constexpr FamilyBase FAMILY_BASE[F_COUNT] = { { 0x000830, SURVIVAL_FILE, 0x0B877E, "Skyrim.esm" },
		{ 0x000914, SURVIVAL_FILE }, { 0x000912, SURVIVAL_FILE }, { 0x000915, SURVIVAL_FILE }, { 0x0B8781, "Skyrim.esm" },
		{ 0x0B8782, "Skyrim.esm" }, { 0x00082E, SURVIVAL_FILE, 0x0B877F, "Skyrim.esm" }, { 0x0B877C, "Skyrim.esm" },
		{ 0x0B8783, "Skyrim.esm" }, { 0x0285C1, "Dragonborn.esm" }, { 0x000918, SURVIVAL_FILE },
		{ 0x000838, SH_DISEASES_FILE }, { 0x000840, SH_DISEASES_FILE }, { 0x000839, SH_DISEASES_FILE },
		{ 0x00083A, SH_DISEASES_FILE }, { 0x00083D, SH_DISEASES_FILE }, { 0x000837, SH_DISEASES_FILE },
		{ 0x000849, SH_DISEASES_FILE } };
	std::vector<DiseaseForm> g_diseaseForms;
	// g_stagedAbilities (Snapshot.inl): iNeed's disease stages are abilities, counted as diseases too

	void FindDiseases(RE::TESDataHandler* a_data)
	{
		g_diseaseForms.clear();
		// family: the disease's normal-stage spell, whose name heads the tile at every stage ("Brown Rot", then
		// "Severe, stage 2 of 3" under it; his pick 2026-11-08, so the row keeps room only for the base names)
		struct Entry { RE::FormID id; const char* file; int stage; bool progresses; int family = -1; };
		static constexpr Entry ENTRIES[] = {
			// Skyrim's own (creature, then trap versions); with Survival on they progress (the trap ones too: they
			// carry Survival's progression script), Bone Break Fever and Brain Rot by swapping to Survival's copies
			{ 0x0B877C, "Skyrim.esm", 0, true },  // Ataxia
			{ 0x0B877E, "Skyrim.esm", 0, true },  // Bone Break Fever
			{ 0x0B877F, "Skyrim.esm", 0, true },  // Brain Rot
			{ 0x0B8781, "Skyrim.esm", 0, true },  // Rattles
			{ 0x0B8782, "Skyrim.esm", 0, true },  // Rockjoint
			{ 0x0B8783, "Skyrim.esm", 0, true },  // Witbane
			{ 0x10A24A, "Skyrim.esm", 0, true },
			{ 0x10A24C, "Skyrim.esm", 0, true },
			{ 0x10A24D, "Skyrim.esm", 0, true },
			{ 0x10A24E, "Skyrim.esm", 0, true },
			{ 0x10A24F, "Skyrim.esm", 0, true },
			{ 0x10A250, "Skyrim.esm", 0, true },
			{ 0x0B8780, "Skyrim.esm", 0, false },     // Sanguinare Vampiris (turns into vampirism, no Severe stage)
			{ 0x0037E9, "Dawnguard.esm", 0, false },  // Sanguinare Vampiris (trap)
			{ 0x0285C1, "Dragonborn.esm", 0, true },  // Droops
			{ 0x01FF2E, "Dragonborn.esm", 0, false }, // Black Heart Blight (not catchable, but a disease)
			// Survival Mode's new diseases and stages
			{ 0x000912, SURVIVAL_FILE, 0, true },   // Greenspore
			{ 0x000914, SURVIVAL_FILE, 0, true },   // Brown Rot
			{ 0x000915, SURVIVAL_FILE, 0, true },   // Gutworm
			{ 0x00082E, SURVIVAL_FILE, 0, true },   // Brain Rot (Survival's)
			{ 0x000830, SURVIVAL_FILE, 0, true },   // Bone Break Fever (Survival's)
			{ 0x000918, SURVIVAL_FILE, 0, false },  // Food Poisoning (3 days; Starfrost makes it progress, below)
			{ 0x000984, SURVIVAL_FILE, 1, false, F_BONEBREAKFEVER }, { 0x000985, SURVIVAL_FILE, 2, false, F_BONEBREAKFEVER },  // Bone Break Fever
			{ 0x00098B, SURVIVAL_FILE, 1, false, F_BROWNROT }, { 0x000986, SURVIVAL_FILE, 2, false, F_BROWNROT },  // Brown Rot
			{ 0x000987, SURVIVAL_FILE, 1, false, F_GREENSPORE }, { 0x000988, SURVIVAL_FILE, 2, false, F_GREENSPORE },  // Greenspore
			{ 0x000989, SURVIVAL_FILE, 1, false, F_GUTWORM }, { 0x00098A, SURVIVAL_FILE, 2, false, F_GUTWORM },  // Gutworm
			{ 0x00098C, SURVIVAL_FILE, 1, false, F_RATTLES }, { 0x00098D, SURVIVAL_FILE, 2, false, F_RATTLES },  // Rattles
			{ 0x00098E, SURVIVAL_FILE, 1, false, F_ROCKJOINT }, { 0x00098F, SURVIVAL_FILE, 2, false, F_ROCKJOINT },  // Rockjoint
			{ 0x000990, SURVIVAL_FILE, 1, false, F_BRAINROT }, { 0x000991, SURVIVAL_FILE, 2, false, F_BRAINROT },  // Brain Rot
			{ 0x000992, SURVIVAL_FILE, 1, false, F_ATAXIA }, { 0x000993, SURVIVAL_FILE, 2, false, F_ATAXIA },  // Ataxia
			{ 0x000994, SURVIVAL_FILE, 1, false, F_WITBANE }, { 0x000995, SURVIVAL_FILE, 2, false, F_WITBANE },  // Witbane
			{ 0x000996, SURVIVAL_FILE, 1, false, F_DROOPS }, { 0x000997, SURVIVAL_FILE, 2, false, F_DROOPS },  // Droops
			// Starfrost: Severe and Crippling Food Poisoning, and an old Brown Rot it keeps
			{ 0x000841, STARFROST_FILE, 1, false, F_FOODPOISONING }, { 0x000842, STARFROST_FILE, 2, false, F_FOODPOISONING },
			{ 0x00001D, STARFROST_FILE, 0, true },
			// SunHelm: its own Food Poisoning (one stage); with its disease module, 7 Oblivion diseases and Acute/Severe stages
			{ 0x6410BF, SUNHELM_FILE, 0, false },
			{ 0x000837, SH_DISEASES_FILE, 0, false }, { 0x000838, SH_DISEASES_FILE, 0, false },  // Feeble Limb, Shakes
			{ 0x000839, SH_DISEASES_FILE, 0, false }, { 0x00083A, SH_DISEASES_FILE, 0, false },  // Wither, Dampworm
			{ 0x00083D, SH_DISEASES_FILE, 0, false }, { 0x000840, SH_DISEASES_FILE, 0, false },  // Chills, Swamp Fever
			{ 0x000849, SH_DISEASES_FILE, 0, false },                                             // Astral Vapors
			{ 0x00083C, SH_DISEASES_FILE, 1, false, F_CHILLS }, { 0x00083B, SH_DISEASES_FILE, 2, false, F_CHILLS },
			{ 0x00083E, SH_DISEASES_FILE, 1, false, F_SHAKES }, { 0x00083F, SH_DISEASES_FILE, 2, false, F_SHAKES },
			{ 0x000841, SH_DISEASES_FILE, 1, false, F_SWAMPFEVER }, { 0x000842, SH_DISEASES_FILE, 2, false, F_SWAMPFEVER },
			{ 0x000843, SH_DISEASES_FILE, 1, false, F_WITHER }, { 0x000844, SH_DISEASES_FILE, 2, false, F_WITHER },
			{ 0x000845, SH_DISEASES_FILE, 1, false, F_DAMPWORM }, { 0x000846, SH_DISEASES_FILE, 2, false, F_DAMPWORM },
			{ 0x000847, SH_DISEASES_FILE, 1, false, F_FEEBLELIMB }, { 0x000848, SH_DISEASES_FILE, 2, false, F_FEEBLELIMB },
			{ 0x00084A, SH_DISEASES_FILE, 1, false, F_ASTRALVAPORS }, { 0x00084B, SH_DISEASES_FILE, 2, false, F_ASTRALVAPORS },
			{ 0x00084C, SH_DISEASES_FILE, 1, false, F_ATAXIA }, { 0x00084D, SH_DISEASES_FILE, 2, false, F_ATAXIA },
			{ 0x00084E, SH_DISEASES_FILE, 1, false, F_BONEBREAKFEVER }, { 0x00084F, SH_DISEASES_FILE, 2, false, F_BONEBREAKFEVER },
			{ 0x000850, SH_DISEASES_FILE, 1, false, F_BRAINROT }, { 0x000851, SH_DISEASES_FILE, 2, false, F_BRAINROT },
			{ 0x000852, SH_DISEASES_FILE, 1, false, F_DROOPS }, { 0x000853, SH_DISEASES_FILE, 2, false, F_DROOPS },
			{ 0x000854, SH_DISEASES_FILE, 1, false, F_RATTLES }, { 0x000855, SH_DISEASES_FILE, 2, false, F_RATTLES },
			{ 0x000856, SH_DISEASES_FILE, 1, false, F_ROCKJOINT }, { 0x000857, SH_DISEASES_FILE, 2, false, F_ROCKJOINT },
			{ 0x000858, SH_DISEASES_FILE, 1, false, F_WITBANE }, { 0x000859, SH_DISEASES_FILE, 2, false, F_WITBANE },
		};
		g_shDiseasesLoaded = ModActive(a_data, SH_DISEASES_FILE) != nullptr;
		g_shDiseasesOn = ModActive(a_data, SUNHELM_FILE) ? LookupGlobal(a_data, 0x5D19F3, SUNHELM_FILE) : nullptr;
		int found = 0;
		RE::SpellItem* base[F_COUNT]{};
		for (int f = 0; f < F_COUNT; ++f) {
			if (ModActive(a_data, FAMILY_BASE[f].file)) {
				base[f] = a_data->LookupForm<RE::SpellItem>(FAMILY_BASE[f].id, FAMILY_BASE[f].file);
			}
			if (!base[f] && FAMILY_BASE[f].file2 && ModActive(a_data, FAMILY_BASE[f].file2)) {
				base[f] = a_data->LookupForm<RE::SpellItem>(FAMILY_BASE[f].id2, FAMILY_BASE[f].file2);
			}
		}
		for (const auto& e : ENTRIES) {
			if (!ModActive(a_data, e.file)) {
				continue;
			}
			if (auto spell = a_data->LookupForm<RE::SpellItem>(e.id, e.file)) {
				bool prog = e.progresses;
				if (e.id == 0x000918 && std::string_view(e.file) == SURVIVAL_FILE && ModActive(a_data, STARFROST_FILE)) {
					prog = true;
				}
				const char* name = (e.family >= 0 && base[e.family]) ? base[e.family]->GetName() : spell->GetName();
				const bool vampire = (e.id == 0x0B8780 && std::string_view(e.file) == "Skyrim.esm") ||
				                     (e.id == 0x0037E9 && std::string_view(e.file) == "Dawnguard.esm");
				const bool fromSH = std::string_view(e.file) == SH_DISEASES_FILE;
				// SunHelm's module moves on every vanilla normal stage that Survival does, and its own new ones
				const bool shProg = g_shDiseasesLoaded && e.stage == 0 && (fromSH || (e.progresses && (std::string_view(e.file) == "Skyrim.esm" || std::string_view(e.file) == "Dragonborn.esm")));
				g_diseaseForms.push_back({ spell, e.stage, prog, (name && *name) ? name : "Diseased", vampire, shProg, fromSH });
				++found;
			}
		}
		// Last Seed (read 2026-11-09 from LastSeed.esp): it swaps each vanilla disease for its own five stages, Mild,
		// Moderate, Severe, Crippling, Deadly (type Disease; one step a day, it can also get better and is cured at 0),
		// and has skooma withdrawal in three. Named by the vanilla disease (Last Seed's stage spells say "Mild Ataxia")
		if (ModActive(a_data, LASTSEED_FILE)) {
			// all 34 families of five stages (generated 2026-11-09 from LastSeed.esp: every Disease-type spell named
			// Mild/Moderate/Severe/Crippling/Deadly <name>); the name comes from the family's base spell (the vanilla
			// disease, or Last Seed's own carrier spell), else from the Mild stage without its first word
			// the base spell (the caught disease: Last Seed's _Seed_Disease runs on it) STAYS on the player next to the one stage
			// spell (_Seed_Disease.modDisease swaps only the stages; the base goes when cured), like its trap version: both
			// are type Disease, so they are grouped with the stages (counted once, not "Ataxia +1")
			struct LsFamily { RE::FormID stages[5]; RE::FormID nameId; const char* nameFile; RE::FormID trap; const char* trapFile; };
			static constexpr LsFamily LS[] = {
				{ { 0x02AE11, 0x02AE12, 0x02AE13, 0x171D82, 0x171D83 }, 0x0B877C, "Skyrim.esm", 0x10A24A, "Skyrim.esm" },  // Ataxia
				{ { 0x02AE14, 0x02AE15, 0x02AE16, 0x171D84, 0x171D85 }, 0x0B877E, "Skyrim.esm", 0x10A24C, "Skyrim.esm" },  // Bone Break Fever
				{ { 0x02AE17, 0x02AE18, 0x02AE19, 0x171D86, 0x171D87 }, 0x0B877F, "Skyrim.esm", 0x10A24D, "Skyrim.esm" },  // Brain Rot
				{ { 0x02AE1A, 0x02AE1B, 0x02AE1C, 0x171D88, 0x171D89 }, 0x0B8781, "Skyrim.esm", 0x10A24E, "Skyrim.esm" },  // Rattles
				{ { 0x02AE1D, 0x02AE1E, 0x02AE1F, 0x171D8A, 0x171D8B }, 0x0B8782, "Skyrim.esm", 0x10A24F, "Skyrim.esm" },  // Rockjoint
				{ { 0x02AE20, 0x02AE21, 0x02AE22, 0x171D8C, 0x171D8D }, 0x0B8783, "Skyrim.esm", 0x10A250, "Skyrim.esm" },  // Witbane
				{ { 0x02AE23, 0x02AE24, 0x02AE25, 0x171D8E, 0x171D8F }, 0x0285C1, "Dragonborn.esm", 0x02AE0F, LASTSEED_FILE },  // Droops
				{ { 0x2E387A, 0x2E387B, 0x2E387C, 0x2E387D, 0x2E387E }, 0, nullptr, 0x2BB03C, LASTSEED_FILE },  // Skooma Withdrawal
				{ { 0x32A6E5, 0x32A6E6, 0x32A6E7, 0x32A6E8, 0x32A6E9 }, 0x01FF2E, "Dragonborn.esm", 0x32A6E4, LASTSEED_FILE },  // Black Heart Blight
				{ { 0x59E548, 0x59E549, 0x59E54A, 0x59E54B, 0x59E54C }, 0x59E54D, LASTSEED_FILE, 0x59E54E, LASTSEED_FILE },  // Brown Rot
				{ { 0x59E558, 0x59E559, 0x59E55A, 0x59E55B, 0x59E55C }, 0x59E557, LASTSEED_FILE, 0x59E55D, LASTSEED_FILE },  // Stomach Rot
				{ { 0x59E55F, 0x59E560, 0x59E561, 0x59E562, 0x59E563 }, 0x59E55E, LASTSEED_FILE, 0x59E564, LASTSEED_FILE },  // Swamp Fever
				{ { 0x5C1C93, 0x5C1C94, 0x5C1C95, 0x5C1C96, 0x5C1C97 }, 0x5C1C92, LASTSEED_FILE, 0x5C1C98, LASTSEED_FILE },  // Greenspore
				{ { 0x5C1CA2, 0x5C1CA3, 0x5C1CA4, 0x5C1CA5, 0x5C1CA6 }, 0x5C1CA8, LASTSEED_FILE, 0x5C1CA7, LASTSEED_FILE },  // Shakes
				{ { 0x5C1CAF, 0x5C1CB0, 0x5C1CB1, 0x5C1CB2, 0x5C1CB3 }, 0x5C1CAE, LASTSEED_FILE, 0x5C1CB4, LASTSEED_FILE },  // Collywobbles
				{ { 0x5C1CBA, 0x5C1CBB, 0x5C1CBC, 0x5C1CBD, 0x5C1CBE }, 0x5C1CC0, LASTSEED_FILE, 0x5C1CBF, LASTSEED_FILE },  // Feeble Limb
				{ { 0x5C1CC6, 0x5C1CC7, 0x5C1CC8, 0x5C1CC9, 0x5C1CCA }, 0x5C1CCC, LASTSEED_FILE, 0x5C1CCB, LASTSEED_FILE },  // Astral Vapors
				{ { 0x5C1CD2, 0x5C1CD3, 0x5C1CD4, 0x5C1CD5, 0x5C1CD6 }, 0x5C1CD8, LASTSEED_FILE, 0x5C1CD7, LASTSEED_FILE },  // Yellow Tick
				{ { 0x5C1CDE, 0x5C1CDF, 0x5C1CE0, 0x5C1CE1, 0x5C1CE2 }, 0x5C1CE4, LASTSEED_FILE, 0x5C1CE3, LASTSEED_FILE },  // Witless Pox
				{ { 0x5C1CEA, 0x5C1CEB, 0x5C1CEC, 0x5C1CED, 0x5C1CEE }, 0x5C1CF0, LASTSEED_FILE, 0x5C1CEF, LASTSEED_FILE },  // Tunnel Cough
				{ { 0x5C1CF6, 0x5C1CF7, 0x5C1CF8, 0x5C1CF9, 0x5C1CFA }, 0x5C1CFC, LASTSEED_FILE, 0x5C1CFB, LASTSEED_FILE },  // Wither
				{ { 0x5C1D02, 0x5C1D03, 0x5C1D04, 0x5C1D05, 0x5C1D06 }, 0x5C1D08, LASTSEED_FILE, 0x5C1D07, LASTSEED_FILE },  // Chanthrax Blight
				{ { 0x5C1D0E, 0x5C1D0F, 0x5C1D10, 0x5C1D11, 0x5C1D12 }, 0x5C1D14, LASTSEED_FILE, 0x5C1D13, LASTSEED_FILE },  // Helljoint
				{ { 0x5C1D1A, 0x5C1D1B, 0x5C1D1C, 0x5C1D1D, 0x5C1D1E }, 0x5C1D20, LASTSEED_FILE, 0x5C1D1F, LASTSEED_FILE },  // Serpiginous Dementia
				{ { 0x5C1D21, 0x5C1D22, 0x5C1D23, 0x5C1D24, 0x5C1D25 }, 0x5C1D27, LASTSEED_FILE, 0x5C1D26, LASTSEED_FILE },  // Blood Lung
				{ { 0x5C1D32, 0x5C1D33, 0x5C1D34, 0x5C1D35, 0x5C1D36 }, 0x5C1D38, LASTSEED_FILE, 0x5C1D37, LASTSEED_FILE },  // Rotbone
				{ { 0x5C1D3E, 0x5C1D3F, 0x5C1D40, 0x5C1D41, 0x5C1D42 }, 0x5C1D44, LASTSEED_FILE, 0x5C1D43, LASTSEED_FILE },  // Ash Woe Blight
				{ { 0x5C1D4A, 0x5C1D4B, 0x5C1D4C, 0x5C1D4D, 0x5C1D4E }, 0x5C1D50, LASTSEED_FILE, 0x5C1D4F, LASTSEED_FILE },  // Ticklebritch
				{ { 0x5C1D51, 0x5C1D52, 0x5C1D53, 0x5C1D54, 0x5C1D55 }, 0x5C1D57, LASTSEED_FILE, 0x5C1D56, LASTSEED_FILE },  // Ash-Chancre
				{ { 0x5C1D62, 0x5C1D63, 0x5C1D64, 0x5C1D65, 0x5C1D66 }, 0x5C1D68, LASTSEED_FILE, 0x5C1D67, LASTSEED_FILE },  // Red Rage
				{ { 0x5C1D6E, 0x5C1D6F, 0x5C1D70, 0x5C1D71, 0x5C1D72 }, 0x5C1D74, LASTSEED_FILE, 0x5C1D73, LASTSEED_FILE },  // Dampworm
				{ { 0x5C1D7A, 0x5C1D7B, 0x5C1D7C, 0x5C1D7D, 0x5C1D7E }, 0x5C1D80, LASTSEED_FILE, 0x5C1D7F, LASTSEED_FILE },  // Scalon Sunburn
				{ { 0x5C1D86, 0x5C1D87, 0x5C1D88, 0x5C1D89, 0x5C1D8A }, 0x5C1D8C, LASTSEED_FILE, 0x5C1D8B, LASTSEED_FILE },  // Chills
				{ { 0x5C6E8E, 0x5C6E8F, 0x5C6E90, 0x5C6E91, 0x5C6E92 }, 0x5C6E94, LASTSEED_FILE, 0x5C6E93, LASTSEED_FILE },  // Rust Chancre
			};
			int lsFound = 0, lsGroup = 200;  // after Oregon's and RND's groups
			for (const auto& f : LS) {
				++lsGroup;
				std::string name;
				if (f.nameFile && ModActive(a_data, f.nameFile)) {
					if (auto b = a_data->LookupForm<RE::SpellItem>(f.nameId, f.nameFile); b && b->GetName() && *b->GetName()) {
						name = b->GetName();
					}
				}
				for (int st = 0; st < 5; ++st) {
					if (auto spell = a_data->LookupForm<RE::SpellItem>(f.stages[st], LASTSEED_FILE)) {
						if (name.empty()) {
							const std::string mild = spell->GetName() ? spell->GetName() : "";
							const auto sp = mild.find(' ');
							name = sp == std::string::npos ? mild : mild.substr(sp + 1);
						}
						DiseaseForm d{ spell, st, false, name.empty() ? std::string("Diseased") : name };
						d.lsStages = 5;
						d.group = lsGroup;
						g_diseaseForms.push_back(d);
						++lsFound;
					}
				}
				std::vector<RE::SpellItem*> bases;
				if (f.nameId && f.nameFile && ModActive(a_data, f.nameFile)) {
					if (auto b = a_data->LookupForm<RE::SpellItem>(f.nameId, f.nameFile)) {
						bases.push_back(b);
					}
				}
				if (auto t = a_data->LookupForm<RE::SpellItem>(f.trap, f.trapFile)) {
					bases.push_back(t);
				}
				// the vanilla entry for the same spell (one stage, no group) gives way to this one
				std::erase_if(g_diseaseForms, [&](const DiseaseForm& d) { return d.group != lsGroup && std::find(bases.begin(), bases.end(), d.spell) != bases.end(); });
				for (auto b : bases) {
					DiseaseForm d{ b, 0, false, name.empty() ? std::string("Diseased") : name };
					d.lsStages = 5;
					d.group = lsGroup;
					d.orBase = true;
					g_diseaseForms.push_back(d);
				}
			}
			found += lsFound;
			logger::info("Diseases: {} of Last Seed's staged disease spells found", lsFound);
		}
		// iNeed (read 2026-11-09 from iNeed.esp): with its disease option on it turns a caught vanilla disease (and its
		// own Stomach Rot) into four ABILITIES, Mild, Moderate, Severe, Deathly (_SNDis*Spell_1-4; not type Disease)
		g_stagedAbilities.clear();
		if (ModActive(a_data, INEED_FILE)) {
			static constexpr const char* IN_WORDS[4] = { "Mild, stage", "Moderate, stage", "Severe, stage", "Deathly, stage" };
			// the caught disease itself STAYS on the player next to the ability (iNeed only adds the ability, _SNDisease);
			// a cure removes it and the abilities then step down one at a time (DiseaseCuring): grouped with its stages
			// and shown as "healing" like Oregon (checked 2026-11-09 in _sndisease.psc)
			struct InFamily { RE::FormID stages[4]; RE::FormID bases[2]; const char* baseFile; };
			static constexpr InFamily IN[] = { { { 0x030B46, 0x030B48, 0x030B4A, 0x030B4C }, { 0x0B877C, 0x10A24A }, "Skyrim.esm" },  // Ataxia
				{ { 0x030B4F, 0x030B51, 0x030B53, 0x030B55 }, { 0x01FF2E, 0 }, "Dragonborn.esm" },  // Black-Heart Blight
				{ { 0x0310BA, 0x0310BC, 0x0310BE, 0x0310C0 }, { 0x0B877E, 0x10A24C }, "Skyrim.esm" },  // Bone Break Fever
				{ { 0x0310C5, 0x0310C7, 0x0310C9, 0x0310CB }, { 0x0B877F, 0x10A24D }, "Skyrim.esm" },  // Brain Rot
				{ { 0x0310D5, 0x0310D7, 0x0310D9, 0x0310DB }, { 0x0285C1, 0 }, "Dragonborn.esm" },  // Droops
				{ { 0x0310DF, 0x0310E1, 0x0310E3, 0x0310E5 }, { 0x0B8781, 0x10A24E }, "Skyrim.esm" },  // Rattles
				{ { 0x0310EA, 0x0310EC, 0x0310EE, 0x0310F0 }, { 0x0B8782, 0x10A24F }, "Skyrim.esm" },  // Rockjoint
				{ { 0x0310FA, 0x0310FC, 0x0310FE, 0x031662 }, { 0x0B8783, 0x10A250 }, "Skyrim.esm" },  // Witbane
				{ { 0x031665, 0x031667, 0x031669, 0x03166B }, { 0x03B2D2, 0 }, INEED_FILE } };  // Stomach Rot (its own, from salmon)
			int inFound = 0, inGroup = 300;
			for (const auto& f : IN) {
				++inGroup;
				std::string name;
				for (int st = 0; st < 4; ++st) {
					if (auto spell = a_data->LookupForm<RE::SpellItem>(f.stages[st], INEED_FILE)) {
						if (name.empty()) {  // "Mild Brain Rot" -> "Brain Rot"
							const std::string mild = spell->GetName() ? spell->GetName() : "";
							const auto sp = mild.find(' ');
							name = sp == std::string::npos ? mild : mild.substr(sp + 1);
						}
						DiseaseForm d{ spell, st, false, name.empty() ? std::string("Diseased") : name };
						d.lsStages = 4;
						d.words = IN_WORDS;
						d.group = inGroup;
						d.healsAlone = f.baseFile != INEED_FILE;  // its Stomach Rot starts at the ability, with no disease
						g_diseaseForms.push_back(d);
						g_stagedAbilities.push_back(spell);
						++inFound;
					}
				}
				std::vector<RE::SpellItem*> bases;
				for (RE::FormID id : f.bases) {
					if (id && ModActive(a_data, f.baseFile)) {
						if (auto b = a_data->LookupForm<RE::SpellItem>(id, f.baseFile)) {
							bases.push_back(b);
						}
					}
				}
				std::erase_if(g_diseaseForms, [&](const DiseaseForm& d) { return d.group != inGroup && std::find(bases.begin(), bases.end(), d.spell) != bases.end(); });
				for (auto b : bases) {
					DiseaseForm d{ b, 0, false, name.empty() ? std::string("Diseased") : name };
					d.lsStages = 4;
					d.words = IN_WORDS;
					d.group = inGroup;
					d.orBase = true;
					d.healsAlone = f.baseFile != INEED_FILE;
					g_diseaseForms.push_back(d);
				}
			}
			found += inFound;
			logger::info("Diseases: {} of iNeed's staged disease abilities found", inFound);
		}
		// Oregon - Diseases of Skyrim (read 2026-11-09 from its esp and OREGON_SymptomManager_Script): 28 diseases of its
		// own (type Disease; the vanilla ones become applicator spells). While one runs, its symptoms are ABILITIES in three
		// stages, Mild, Severe, Crippling (Fatal for four), one more by chance each game hour; cured, the symptoms stay
		// and heal one stage at a time by themselves
		if (ModActive(a_data, OREGON_FILE)) {
			static constexpr const char* OR_WORDS[3] = { "Mild, stage", "Severe, stage", "Crippling, stage" };
			static constexpr const char* OR_WORDS_FATAL[3] = { "Mild, stage", "Severe, stage", "Fatal, stage" };
			struct OrFamily { RE::FormID base; RE::FormID stages[3]; bool fatal; };
			static constexpr OrFamily OREGON[] = {
				{ 0x02913E, { 0x029138, 0x02913A, 0x02913C }, false },  // Astral Vapors
				{ 0x029108, { 0x02910C, 0x02910E, 0x029110 }, false },  // Ataxia
				{ 0x00AA15, { 0x00AA0B, 0x00AA0D, 0x00AA0F }, false },  // Blood Lung
				{ 0x00FB20, { 0x00FB22, 0x00FB24, 0x00FB26 }, false },  // Bone Break Fever
				{ 0x00FB2E, { 0x00FB30, 0x00FB32, 0x00FB34 }, false },  // Brain Rot
				{ 0x01EE84, { 0x01EE86, 0x01EE88, 0x01EE8A }, false },  // Brown Rot
				{ 0x03336A, { 0x03336C, 0x03336E, 0x033370 }, false },  // Caliron's Curse
				{ 0x033363, { 0x03335C, 0x03335E, 0x033360 }, false },  // Cannibal's Prion
				{ 0x02910A, { 0x029112, 0x029114, 0x029116 }, false },  // Chrondiasis
				{ 0x033380, { 0x03337A, 0x03337C, 0x03337E }, false },  // Consumption
				{ 0x01EEAD, { 0x01EEAF, 0x01EEB1, 0x01EEB3 }, false },  // Dampworm
				{ 0x0384B5, { 0x0384AF, 0x0384B1, 0x0384B3 }, false },  // Dancing Plague
				{ 0x019D73, { 0x019D75, 0x019D77, 0x019D79 }, false },  // Droops
				{ 0x0290ED, { 0x0290EF, 0x0290F1, 0x0290F3 }, true },  // Dysentery
				{ 0x023FD1, { 0x023FD3, 0x023FD5, 0x023FD7 }, true },  // Greenspore
				{ 0x023FBD, { 0x023FC1, 0x023FC3, 0x023FC5 }, false },  // Helljoint
				{ 0x00FB3C, { 0x00FB3E, 0x00FB40, 0x00FB42 }, true },  // Rabies
				{ 0x01EEA4, { 0x01EE98, 0x01EE9A, 0x01EE9C }, false },  // Rattles
				{ 0x02912C, { 0x02912E, 0x029130, 0x029132 }, false },  // Red Rage
				{ 0x023FBF, { 0x023FC7, 0x023FC9, 0x023FCB }, false },  // Rust Chancre
				{ 0x019D64, { 0x019D52, 0x019D54, 0x019D56 }, false },  // Shakes
				{ 0x02E250, { 0x02E24A, 0x02E24C, 0x02E24E }, true },  // Stone Blood
				{ 0x0384A8, { 0x0384A2, 0x0384A4, 0x0384A6 }, false },  // Swamp Fever
				{ 0x038495, { 0x038497, 0x038499, 0x03849B }, false },  // Ticklebritch
				{ 0x038483, { 0x038485, 0x038487, 0x038489 }, false },  // Tunnel Cough
				{ 0x01EEA6, { 0x01EE9E, 0x01EEA0, 0x01EEA2 }, false },  // Wither
				{ 0x019D66, { 0x019D58, 0x019D5A, 0x019D5C }, false },  // Witless Pox
				{ 0x019D68, { 0x019D5E, 0x019D60, 0x019D62 }, false },  // Yellow Tick
			};
			int orFound = 0, group = 0;
			for (const auto& f : OREGON) {
				++group;
				const auto words = f.fatal ? OR_WORDS_FATAL : OR_WORDS;
				std::string name;
				auto baseSpell = a_data->LookupForm<RE::SpellItem>(f.base, OREGON_FILE);
				if (baseSpell && baseSpell->GetName() && *baseSpell->GetName()) {
					name = baseSpell->GetName();
				}
				for (int st = 0; st < 3 && name.empty(); ++st) {  // "Rattles Symptoms (Mild)" -> "Rattles"
					if (auto s = a_data->LookupForm<RE::SpellItem>(f.stages[st], OREGON_FILE); s && s->GetName()) {
						const std::string full = s->GetName();
						name = full.substr(0, full.find(" Symptoms"));
					}
				}
				if (name.empty()) {
					name = "Diseased";
				}
				if (baseSpell) {
					DiseaseForm d{ baseSpell, 0, false, name };
					d.lsStages = 3;
					d.words = words;
					d.group = group;
					d.orBase = true;
					d.healsAlone = true;
					g_diseaseForms.push_back(d);
					++orFound;
				}
				for (int st = 0; st < 3; ++st) {
					if (auto spell = a_data->LookupForm<RE::SpellItem>(f.stages[st], OREGON_FILE)) {
						DiseaseForm d{ spell, st, false, name };
						d.lsStages = 3;
						d.words = words;
						d.group = group;
						d.healsAlone = true;
						g_diseaseForms.push_back(d);
						g_stagedAbilities.push_back(spell);
						++orFound;
					}
				}
			}
			found += orFound;
			logger::info("Diseases: {} of Oregon's disease and symptom spells found", orFound);
		}
		// Realistic Needs and Diseases (read 2026-11-09 from its esp and RND_ProgDisease): it rewrites six vanilla diseases
		// (their creature and trap spells) and adds three stage spells of its own, also type Disease and with the same name:
		// stage 1 at once, 2 after 6 h, 3 after 12 h; from a day on, each sleep may take the top stage off (the disease
		// itself goes last). Grouped like Oregon so a disease and its stages count as one, not "Ataxia +3"
		if (ModActive(a_data, RND_FILE)) {
			static constexpr const char* RND_WORDS[3] = { "Mild, stage", "Severe, stage", "Crippling, stage" };
			struct RndFamily { RE::FormID base, trap; RE::FormID stages[3]; };
			static constexpr RndFamily RND_DIS[] = {
				{ 0x0B877C, 0x10A24A, { 0x010678, 0x00A530, 0x00A534 } },  // Ataxia
				{ 0x0B877E, 0x10A24C, { 0x01067A, 0x00F623, 0x00F625 } },  // Bone Break Fever
				{ 0x0B877F, 0x10A24D, { 0x01067C, 0x00F62D, 0x00F62F } },  // Brain Rot
				{ 0x0B8781, 0x10A24E, { 0x01067E, 0x00F61C, 0x00F61E } },  // Rattles
				{ 0x0B8782, 0x10A24F, { 0x010680, 0x00F0AE, 0x00F0AF } },  // Rock Joint
				{ 0x0B8783, 0x10A250, { 0x010682, 0x00F0B4, 0x00F0B2 } },  // Witbane
			};
			int rndFound = 0, group = 100;  // after Oregon's groups
			for (const auto& f : RND_DIS) {
				++group;
				std::string name;
				std::vector<RE::SpellItem*> bases;
				for (RE::FormID id : { f.base, f.trap }) {
					if (auto sp = a_data->LookupForm<RE::SpellItem>(id, "Skyrim.esm")) {
						bases.push_back(sp);
						if (name.empty() && sp->GetName() && *sp->GetName()) {
							name = sp->GetName();
						}
					}
				}
				name = name.empty() ? "Diseased" : name;
				// the vanilla entry for the same spell (one stage, no group) gives way to this one
				std::erase_if(g_diseaseForms, [&](const DiseaseForm& d) { return std::find(bases.begin(), bases.end(), d.spell) != bases.end(); });
				for (auto sp : bases) {
					DiseaseForm d{ sp, 0, false, name };
					d.lsStages = 3;
					d.words = RND_WORDS;
					d.group = group;
					d.orBase = true;
					g_diseaseForms.push_back(d);
					++rndFound;
				}
				for (int st = 0; st < 3; ++st) {
					if (auto spell = a_data->LookupForm<RE::SpellItem>(f.stages[st], RND_FILE)) {
						DiseaseForm d{ spell, st, false, name };
						d.lsStages = 3;
						d.words = RND_WORDS;
						d.group = group;
						g_diseaseForms.push_back(d);
						++rndFound;
					}
				}
			}
			found += rndFound;
			logger::info("Diseases: {} of 30 Realistic Needs and Diseases disease and stage spells found", rndFound);
		}
		logger::info("Diseases: {} known disease spells found{}", found, g_shDiseasesLoaded ? " (SunHelm's disease module loaded)" : "");
	}

	struct DiseaseRead
	{
		bool        ok = false;
		int         st = 0, n = 1, others = 0;
		std::string label;  // the disease's name (+N for the others)
		std::string line2;  // "Severe, stage 2 of 3" (empty with one stage)
		double      pos = 0.0;       // 0..n along the bar
		bool        vampire = false;  // the tile turns blood red and stays red (always the most urgent)
	};

	// the day the player turns: Skyrim's own countdown is a property of the disease's effect script
	// (VampireDiseaseEffectScript.VampireChangeTimer = the day caught + 3), which C++ must never read (rule for all his
	// plugins; 1.0.3). So NeedsHUD keeps its own clock: the global NeedsHUD_VampireSeenDay (NeedsHUD.esp 0x80A) holds the
	// day we first saw Sanguinare on the player (the tile checks 10 times a second, so that is the day caught), and the
	// save keeps it like any global. -1 = not caught. Players who update while already infected start counting at the update
	// The global is only written on the main thread (a task; 1.0.3 code review); until that task has run, the value
	// asked for is used (g_vampWant), so the tile never waits on it.
	std::atomic<float> g_vampWant{ -2.0f };  // the value queued for the global, -2 = nothing queued
	std::atomic_int    g_vampWantLoad{ -1 };  // the load it was queued in (a queued value never carries into another game)
	void SetVampireSeenDay(float a_day)
	{
		const int load = g_loadCount.load();
		g_vampWantLoad = load;
		if (g_vampWant.exchange(a_day) == a_day) {
			return;  // already queued
		}
		SKSE::GetTaskInterface()->AddTask([a_day, load]() {
			if (g_vampireSeenDay && load == g_loadCount.load()) {  // never into a game loaded since it was queued
				g_vampireSeenDay->value = a_day;
				return;  // the tile drops the queued value once its copy of the global shows it (VampireChangeDay)
			}
			float expected = a_day;
			g_vampWant.compare_exchange_strong(expected, -2.0f);  // not written (another game): nothing is pending
		});
	}
	double VampireChangeDay(bool a_infected, double a_now)
	{
		if (!g_vampireSeenDay) {
			return -1.0;
		}
		// the queued value counts until the tile's copy of the global shows it (1.0.4 review: dropping it as soon as the
		// write ran let a tick holding an older copy queue the clock again, a few game seconds later)
		float       want = g_vampWant.load();
		const float copy = GV(g_vampireSeenDay);
		if (want > -2.0f && (copy == want || g_vampWantLoad.load() != g_loadCount.load()) &&
			g_vampWant.compare_exchange_strong(want, -2.0f)) {
			want = -2.0f;
		}
		const float seen = want > -2.0f ? want : copy;
		if (!a_infected) {
			if (seen >= 0.0f) {
				SetVampireSeenDay(-1.0f);  // cured (or turned): the next infection starts a new clock
			}
			return -1.0;
		}
		if (seen < 0.0f) {
			SetVampireSeenDay(static_cast<float>(a_now));
			return a_now + 3.0;
		}
		return seen + 3.0;
	}

	DiseaseRead ReadDisease()
	{
		DiseaseRead r;
		if (!g_view.ok) {
			return r;
		}
		const bool survival = g_survivalOn && GV(g_survivalOn) >= 1.0f;
		// a disease counts only while one of its effects still runs on the player (his test 2026-11-08: after a Cure Disease
		// potion the game's Active Effects list was empty, yet HasSpell still said yes for Sanguinare and Ataxia).
		// Read from the main-thread snapshot (Snapshot.inl): type and name were copied there
		std::vector<const RE::MagicItem*> live;
		std::vector<const EffSnap*>       liveFx;  // the first effect of each, for the name of a disease we don't know
		for (const auto& e : g_view.effects) {
			if (e.spell && !e.dispelled &&
				(e.disease || std::find(g_stagedAbilities.begin(), g_stagedAbilities.end(), e.spell) != g_stagedAbilities.end()) &&
				std::find(live.begin(), live.end(), e.spell) == live.end()) {  // each disease once (a spell has several effects)
				live.push_back(e.spell);
				liveFx.push_back(&e);
			}
		}
		const DiseaseForm* worst = nullptr;
		int count = 0;
		std::vector<int> groups;  // Oregon: a disease and its symptoms are one
		for (const auto& d : g_diseaseForms) {
			if (std::find(live.begin(), live.end(), d.spell) == live.end()) {
				continue;
			}
			if (d.group == 0 || std::find(groups.begin(), groups.end(), d.group) == groups.end()) {
				++count;
				if (d.group != 0) {
					groups.push_back(d.group);
				}
			}
			if (!worst || (d.vampire && !worst->vampire) || (!worst->vampire && d.stage > worst->stage)) {
				worst = &d;
			}
		}
		// any other disease the game runs on the player (his pick 2026-11-09: a general rule for mods we haven't read):
		// shown with the game's name and one stage; a known disease at a later stage, or Sanguinare, still comes first
		static DiseaseForm unknown;
		RE::FormID         unknownID = 0;
		for (std::size_t k = 0; k < live.size(); ++k) {
			const auto* fx = liveFx[k];
			const bool  known = std::any_of(g_diseaseForms.begin(), g_diseaseForms.end(), [&](const DiseaseForm& d) { return d.spell == live[k]; });
			if (known || !fx->isSpell) {
				continue;  // in our table (handled above)
			}
			++count;
			if (!worst) {
				unknown = DiseaseForm{};
				unknown.spell = static_cast<RE::SpellItem*>(const_cast<RE::MagicItem*>(live[k]));  // compared only, never read here
				unknown.name = !fx->spellName.empty() ? fx->spellName : "Diseased";
				unknownID = fx->spellID;
				worst = &unknown;
			}
		}
		static RE::FormID lastLogged = 0;  // the log names each new worst disease once (form ID for console tests)
		const RE::FormID worstID = !worst ? 0 : (worst == &unknown ? unknownID : worst->spell->GetFormID());
		if (worstID != lastLogged) {
			lastLogged = worstID;
			if (worst) {
				logger::info("Disease tile: {} ({:08X}), stage {}, {} disease(s)", worst->name, worstID, worst->stage + 1, count);
			} else {
				logger::info("Disease tile: no disease");
			}
		}
		if (!worst || !worst->vampire) {
			VampireChangeDay(false, 0.0);  // no Sanguinare: clear our clock (Sanguinare always comes first when present)
		}
		if (!worst) {
			return r;
		}
		r.ok = true;
		r.others = count - 1;
		r.st = worst->stage;
		// three stages when it can get worse (or already has); one when it can't
		const bool shDiseases = SunHelmDiseasesActive();
		r.n = worst->lsStages > 0 ? worst->lsStages : (worst->stage > 0 || (worst->progresses && survival) || (worst->shProgresses && shDiseases)) ? 3 : 1;
		r.label = worst->name;
		if (r.others > 0) {
			r.label += std::format(" +{}", r.others);
		}
		// each mod's own stage words: Survival normal / Severe / Crippling, SunHelm normal / Acute / Severe
		static constexpr const char* SEVERITY[3] = { "Stage", "Severe, stage", "Crippling, stage" };
		static constexpr const char* SEVERITY_SH[3] = { "Stage", "Acute, stage", "Severe, stage" };
		const bool shWords = worst->sunhelm || (shDiseases && !survival);
		static constexpr const char* SEVERITY_LS[5] = { "Mild, stage", "Moderate, stage", "Severe, stage", "Crippling, stage", "Deadly, stage" };
		// Oregon: symptoms left after the disease itself is cured heal by themselves, a stage at a time ("Severe, healing")
		const bool orHealing = worst->healsAlone && !std::any_of(g_diseaseForms.begin(), g_diseaseForms.end(), [&](const DiseaseForm& d) {
			return d.group == worst->group && d.orBase && std::find(live.begin(), live.end(), d.spell) != live.end();
		});
		if (orHealing) {
			const std::string w = worst->words[std::clamp(r.st, 0, 2)];
			r.line2 = tr::F("{}, healing", tr::T(w.substr(0, w.find(','))));
		} else if (worst->lsStages > 0) {
			r.line2 = tr::F("{} {} of {}", tr::T(std::string((worst->words ? worst->words : SEVERITY_LS)[std::clamp(r.st, 0, r.n - 1)])), r.st + 1, r.n);
		} else if (r.n > 1) {
			r.line2 = tr::F("{} {} of {}", tr::T(std::string((shWords ? SEVERITY_SH : SEVERITY)[std::clamp(r.st, 0, 2)])), r.st + 1, r.n);
		}
		r.pos = r.st + 0.999;
		if (worst->vampire) {
			// three day blocks, the pointer moving with the hours; the line counts down to the change
			r.vampire = true;
			r.n = 3;
			const double now = std::max(0.0, DaysPassed());
			double change = VampireChangeDay(true, now);
			if (change < 0.0) {
				// NeedsHUD.esp missing (no clock global): count from when this session first saw it
				static double seenSince = -1.0;
				static int    seenLoad = -1;
				if (seenLoad != g_loadCount.load()) {
					seenLoad = g_loadCount.load();
					seenSince = -1.0;
				}
				seenSince = seenSince < 0.0 ? now : seenSince;
				change = seenSince + 3.0;
			}
			const double left = std::max(0.0, change - now);
			r.pos = std::clamp(3.0 - left, 0.0, 2.999);
			r.st = static_cast<int>(r.pos);
			const int hours = static_cast<int>(std::ceil(left * 24.0));
			r.line2 = left <= 0.0      ? std::string("Turning now")
			        : hours == 1      ? tr::F("Vampirism in {} hour", hours)
			        : hours < 24      ? tr::F("Vampirism in {} hours", hours)
			        : std::lround(left) == 1 ? tr::F("Vampirism in {} day", 1)
			                          : tr::F("Vampirism in {} days", static_cast<int>(std::lround(left)));
		}
		return r;
	}

	// every name the tile can show, for the row's spacing
	std::vector<std::string> DiseaseNames()
	{
		std::vector<std::string> v;
		for (const auto& d : g_diseaseForms) {
			v.push_back(d.name + " +2");
		}
		return v;
	}
#endif
