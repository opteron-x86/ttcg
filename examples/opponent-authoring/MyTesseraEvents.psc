Scriptname MyTesseraEvents extends Quest
Actor Property MyOpponent Auto

Event OnInit()
    RegisterForModEvent("TTCG_MatchCompleted", "OnTesseraMatch")
EndEvent

Event OnTesseraMatch(String eventName, String details, Float winner, Form sender)
    If sender != MyOpponent
        Return
    EndIf
    ; winner: -1 draw, 0 player won, 1 NPC won.
    ; Add your quest response here. Re-register after loading if your quest needs it.
EndEvent
