Scriptname Tessera_CompletionistPlayer extends ReferenceAlias
Event OnPlayerLoadGame()
    (GetOwningQuest() as Tessera_CompletionistQuest).Setup()
EndEvent
