Scriptname TTCG_Opponents Hidden
; Cached availability, refreshed by Tessera and by Refresh().
; -1: not registered, 0: unavailable, 1: ready, 2: declines, 3: insufficient cards.
Int Function GetAvailability(Actor opponent) Global Native
Bool Function IsRegistered(Actor opponent) Global Native
; These requests run on the game thread. See TTCG.log for rejected requests.
Function AddPlayer(Actor opponent) Global Native
Function SetEnabled(Actor opponent, Bool enabled) Global Native
Function Refresh(Actor opponent) Global Native
; grantId must be a permanent namespace:id. Repeated requests do not mint copies.
Function GiveCard(Actor opponent, String cardId, String grantId, Int quantity = 1, Bool foil = False) Global Native
; Query after the queued request has run. True also means a unique was already owned.
Bool Function HasReceivedGrant(Actor opponent, String grantId) Global Native
