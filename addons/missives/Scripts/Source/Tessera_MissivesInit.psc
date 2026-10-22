Scriptname Tessera_MissivesInit extends Quest
FormList Property Boards Auto
FormList Property Jobs Auto

Event OnInit()
    Setup()
EndEvent

Function Setup()
    RegisterForModEvent("Tessera_CollectionChanged", "OnCollectionChanged")
    Int i = 0
    While i < Jobs.GetSize()
        FormList quests = Boards.GetAt(i) as FormList
        Quest job = Jobs.GetAt(i) as Quest
        If quests && job && !quests.HasForm(job)
            quests.AddForm(job)
        EndIf
        i += 1
    EndWhile
    awaitingLoad = !Tessera_Collection.IsReady()
    RestoreJobs()
EndFunction

Function RestoreJobs()
    If !Tessera_Collection.IsReady()
        Return
    EndIf
    Int i = 0
    While i < Jobs.GetSize()
        (Jobs.GetAt(i) as Tessera_MissivesQuest).Restore()
        i += 1
    EndWhile
EndFunction

Event OnCollectionChanged(String eventName, String status, Float revision, Form sender)
    ; Only restore when the API becomes ready, not on every card change.
    If status == "ready" && awaitingLoad
        awaitingLoad = False
        RestoreJobs()
    ElseIf status == "unavailable"
        awaitingLoad = True
    EndIf
EndEvent
Bool awaitingLoad = True
