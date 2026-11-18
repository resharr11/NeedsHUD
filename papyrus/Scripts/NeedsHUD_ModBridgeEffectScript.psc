Scriptname NeedsHUD_ModBridgeEffectScript extends ActiveMagicEffect
{NeedsHUD: copies the values other needs mods keep only inside their Papyrus scripts (SunHelm's drink count, iNeed's
needs) into NeedsHUD's own globals once a second. The C++ plugin must never read script properties itself (doing it
crashed a save load in 1.0.2), so this script does it and the plugin reads only the globals. Runs from the hidden
ability NeedsHUD_ModBridge, which the plugin gives the player when SunHelm or iNeed is installed. A mod that isn't
installed is skipped, so its bridge script is never loaded.}

Event OnEffectStart(Actor akTarget, Actor akCaster)
	RegisterForSingleUpdate(1.0)
EndEvent

Event OnUpdate()
	; which mods are installed: the plugin writes it into NeedsHUD_BridgeMods after every load (1 = SunHelm, 2 = iNeed,
	; added together), so a mod installed or removed mid-playthrough is followed
	GlobalVariable mods = Game.GetFormFromFile(0x80D, "NeedsHUD.esp") as GlobalVariable
	int which = 0
	if mods
		which = mods.GetValueInt()
	endif
	if which % 2 == 1
		NeedsHUD_SunHelmBridge.Copy()
	endif
	if (which / 2) % 2 == 1
		NeedsHUD_INeedBridge.Copy()
	endif
	RegisterForSingleUpdate(1.0)
EndEvent
