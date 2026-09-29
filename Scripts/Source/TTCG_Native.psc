Scriptname TTCG_Native Hidden
Function BeginMatch(Actor opponent) Global Native
Function BuyAlbum(Actor merchant) Global Native
Function OpenCardShop(Actor merchant) Global Native
Function OpenTournaments(Actor host) Global Native
Function OpenAlbum() Global Native


Actor Function NextMaster() Global Native
Function MasterArrived(Actor challenger) Global Native
Function MasterResponse(Actor challenger, Bool accepted) Global Native

Function RegisterTournament(Actor host) Global Native
Book Function NextTournamentInvitation() Global Native
Function TournamentInvitation(Book letter, Bool delivered) Global Native
