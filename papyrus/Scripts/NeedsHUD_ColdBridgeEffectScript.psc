Scriptname NeedsHUD_ColdBridgeEffectScript extends ActiveMagicEffect
{NeedsHUD: copies Survival Mode's cold level for the player's current place (Survival_NeedCold.currentColdLevel: region,
night and weather together) into the global NeedsHUD_PlaceColdLevel twice a second, for the thermometer's place
arrow. The C++ plugin must not read script properties itself, so this tiny script does it. Runs from the hidden
ability NeedsHUD_ColdBridge, which the plugin gives the player. -1 = Survival Mode isn't there or isn't running.}

GlobalVariable placeCold

Event OnEffectStart(Actor akTarget, Actor akCaster)
	placeCold = Game.GetFormFromFile(0x800, "NeedsHUD.esp") as GlobalVariable
	RegisterForSingleUpdate(0.5)
EndEvent

Event OnUpdate()
	if !placeCold
		placeCold = Game.GetFormFromFile(0x800, "NeedsHUD.esp") as GlobalVariable
	endif
	Survival_NeedCold cold = Game.GetFormFromFile(0x896, "ccQDRSSE001-SurvivalMode.esl") as Survival_NeedCold
	if placeCold
		if cold && cold.IsRunning()
			placeCold.SetValue(cold.currentColdLevel as float)
		else
			placeCold.SetValue(-1.0)
		endif
	endif
	RegisterForSingleUpdate(0.5)
EndEvent
