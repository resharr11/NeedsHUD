Scriptname NeedsHUD_INeedBridge Hidden
{NeedsHUD: iNeed's needs (_SNQuestScript HungerState/ThirstState/FatigueState, 0..110, higher = better) and their rates
(0 = that need is switched off in iNeed's menu) into NeedsHUD's globals NeedsHUD_INState0-2 and NeedsHUD_INRate0-2.
Called only when iNeed.esp is loaded. -1 = not known (its quest isn't running).}

Function Copy() global
	_SNQuestScript q = Game.GetFormFromFile(0xD62, "iNeed.esp") as _SNQuestScript
	if !q || !q.IsRunning()
		int id = 0x804
		while id <= 0x809
			SetG(id, -1.0)
			id += 1
		endwhile
		return
	endif
	SetG(0x804, q.HungerState)
	SetG(0x805, q.ThirstState)
	SetG(0x806, q.FatigueState)
	SetG(0x807, q.HungerRate)
	SetG(0x808, q.ThirstRate)
	SetG(0x809, q.FatigueRate)
EndFunction

Function SetG(int id, float value) global
	GlobalVariable g = Game.GetFormFromFile(id, "NeedsHUD.esp") as GlobalVariable
	if g
		g.SetValue(value)
	endif
EndFunction
