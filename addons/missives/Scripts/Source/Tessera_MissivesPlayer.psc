Scriptname Tessera_MissivesPlayer extends ReferenceAlias
Event OnPlayerLoadGame()
    (GetOwningQuest() as Tessera_MissivesInit).Setup()
EndEvent
