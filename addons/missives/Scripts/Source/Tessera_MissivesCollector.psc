Scriptname Tessera_MissivesCollector extends ReferenceAlias
Event OnDeath(Actor killer)
    GetOwningQuest().SetStage(105)
EndEvent
