Scriptname NeedsHUD_SunHelmBridge Hidden
{NeedsHUD: SunHelm's drink count (_SunHelmMain._SHDrinksConsumed) into the global NeedsHUD_SHDrinks. Called only when
SunHelmSurvival.esp is loaded. -1 = not known (its quest isn't running).}

Function Copy() global
	GlobalVariable drinks = Game.GetFormFromFile(0x803, "NeedsHUD.esp") as GlobalVariable
	_SunHelmMain main = Game.GetFormFromFile(0xD61, "SunHelmSurvival.esp") as _SunHelmMain
	if !drinks
		return
	endif
	if main && main.IsRunning()
		drinks.SetValue(main._SHDrinksConsumed as float)
	else
		drinks.SetValue(-1.0)
	endif
EndFunction
