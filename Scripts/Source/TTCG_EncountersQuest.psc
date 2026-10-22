Scriptname TTCG_EncountersQuest extends Quest
; Retained for saves made before master approaches were retired. Never used.
Bool pathing = false
Event OnInit()
    RegisterForSingleUpdate(30.0)
EndEvent
Event OnUpdate()
    Quest rescue = Game.GetFormFromFile(0x376EE, "Skyrim.esm") as Quest
    GlobalVariable rescued = Game.GetFormFromFile(0xC20, "Tessera TCG.esp") as GlobalVariable
    if rescue && rescued
        if rescue.GetStageDone(60) || rescue.GetStageDone(100)
            rescued.SetValue(1.0)
        endif
    endif
    Quest nightmare = Game.GetFormFromFile(0x242AF, "Skyrim.esm") as Quest
    GlobalVariable destroyed = Game.GetFormFromFile(0x1052ED, "Skyrim.esm") as GlobalVariable
    GlobalVariable ready = Game.GetFormFromFile(0xD20, "Tessera TCG.esp") as GlobalVariable
    if nightmare && destroyed && ready
        if nightmare.GetStageDone(200) && destroyed.GetValue() > 0
            ready.SetValue(1.0)
        else
            ready.SetValue(0.0)
        endif
    endif
    Book invitation = TTCG_Native.NextTournamentInvitation()
    if invitation
        WICourierScript courier = Game.GetForm(0x39F82) as WICourierScript
        if courier
            courier.AddItemToContainer(invitation, 1)
            TTCG_Native.TournamentInvitation(invitation, false)
        endif
    endif
    TTCG_Native.BackgroundUpdate()
    ; Fallback upkeep; native interaction and actor-load events refresh on demand.
    RegisterForSingleUpdate(30.0)
EndEvent
