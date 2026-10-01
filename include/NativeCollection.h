#pragma once
#include "Campaign.h"
#include "TournamentPresentation.h"
#include "VendorDistribution.h"
#include "RuleCulture.h"
#include "WorldAccess.h"
#include "WorldIdentity.h"
#include "Development.h"
#include "DevelopmentTournaments.h"
#include "Opponents.h"
#include "Presentation.h"
#include "DeckPresentation.h"
#include "SavedMatch.h"
namespace campaign {
inline constexpr const char* plugin="Tessera TCG.esp";
inline ttcg::CollectionSave saved;
inline ttcg::SavedMatch savedMatch;
inline RE::TESBoundObject* goldForm{};
inline RE::TESObjectBOOK* albumForm{};
inline RE::TESBoundObject* packForm{};
inline RE::TESBoundObject* tournamentPackForm{};
inline bool saveValid=true;
inline ttcg::DevelopmentSettings development;
inline RE::TESGlobal* progression{};
inline RE::TESGlobal *erandurGlobal{},*collegeGlobal{};
inline RE::TESFaction* collegeFaction{};
inline RE::TESQuest* collegeSuspension{};
inline RE::TESFaction* guildFaction{};
inline RE::TESQuest *guildBan{},*brandSheiArrest{};
inline RE::TESGlobal* guildGlobal{};
inline RE::TESGlobal* valdrGlobal{};
inline std::array<RE::TESGlobal*,ttcg::opponents.size()> opponentGlobals{};
inline bool practice=false,competitiveMatch=false,requestAnswered=false;
inline unsigned ruleRequest=0,sessionRules=0,tournamentID=0;
inline unsigned tournamentRegistration=0;
// High word: tournament ID; low word: letter form. An acknowledgement must apply
// to the event actually queued, even when another event uses the same note.
inline std::atomic<std::uint64_t> tournamentLetterReady{0},tournamentLetterInFlight{0};
inline std::atomic<std::uint64_t> tournamentLetterSentAt{0};
inline std::uint64_t tournamentCourierClock(){return std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::steady_clock::now().time_since_epoch()).count();}
inline ttcg::GameDate gameDate(){auto c=RE::Calendar::GetSingleton();return c?ttcg::GameDate{int(c->GetYear()),int(c->GetMonth()),int(c->GetDay()),int(c->GetHour())}:ttcg::GameDate{};}
inline unsigned gameHour() {auto c=RE::Calendar::GetSingleton();return c?static_cast<unsigned>(std::clamp(c->GetDaysPassed()*24.0f,0.0f,99999900.0f)):0;}
inline ttcg::Hand rivalDeck{};
inline bool canStake=false;
inline std::string notice, receipt, cultureNotice;
inline bool cultureGame=false;
inline std::vector<ttcg::CardID> revealed;
inline std::string revealKind;
inline std::uint64_t revealID=0;
inline ttcg::Stock owned;
inline int physicalCount(RE::TESObjectREFR* actor,RE::TESBoundObject* form) {
 if(!actor||!form) return 0;
 auto items=actor->GetInventoryCounts([&](RE::TESBoundObject& object){ return &object==form; });
 auto it=items.find(form); return it==items.end()?0:std::max(0,it->second);
}
inline int physicalChange(RE::TESObjectREFR* actor,RE::TESBoundObject* form,int amount) {
 if(!actor||!form||!amount) return 0;
 const int before=physicalCount(actor,form);
 if(amount>0) actor->AddObjectToContainer(form,nullptr,amount,nullptr);
 else actor->RemoveItem(form,std::min(before,-amount),RE::ITEM_REMOVE_REASON::kRemove,nullptr,nullptr);
 return physicalCount(actor,form)-before;
}
inline bool authoredKey(std::uint32_t id){return ttcg::identityMaster(id)!=nullptr;}
inline std::uint32_t persistentID(RE::TESForm* form){
 if(!form)return 0;const auto file=form->GetFile(0);
 return file?ttcg::authoredIdentity(file->GetFilename(),form->GetLocalFormID(),form->GetFormID()):form->GetFormID();
}
template<class T> T* resolveID(std::uint32_t id){
 if(!id)return nullptr;
 const auto master=ttcg::identityMaster(id);
 return master?RE::TESDataHandler::GetSingleton()->LookupForm<T>(id&0xFFFFFFu,master):RE::TESForm::LookupByID<T>(id);
}
inline unsigned tournamentVenue(){
 auto player=RE::PlayerCharacter::GetSingleton();if(!player)return 0;
 constexpr unsigned cells[]{0x1605E,0x3A184,0x16A0E,0x138CE,0x13A7F,0x16BDF,0x13814,0x16789,0x16DFE,0xDB017EC0};
 const auto cell=persistentID(player->GetParentCell());for(unsigned n=0;n<10;++n)if(cell==cells[n])return n+1;
 return 0;
}
inline int profileIndex(RE::Actor* actor) {return actor?ttcg::opponentIndex(persistentID(actor->GetActorBase())):-1;}
inline const ttcg::OpponentProfile* profile(RE::Actor* actor) { const int i=profileIndex(actor);return i<0?nullptr:&ttcg::opponents[i]; }
inline std::uint32_t ledgerKey(RE::TESObjectREFR* actor) {
 if(!actor)return 0;const auto* p=profile(actor->As<RE::Actor>());return ttcg::opponentLedgerKey(p?p->base:0,persistentID(actor));
}
inline std::uint32_t locationHold(RE::Actor* actor,unsigned fallback=ttcg::whiterunHold){
 auto ref=actor?actor:RE::PlayerCharacter::GetSingleton();
 constexpr std::array<RE::FormID,9> locations{0x16772,0x1676F,0x16770,0x1676E,0x1676D,0x1676C,0x1676B,0x1676A,0x16769};
 auto location=ref?ref->GetCurrentLocation():nullptr;
 for(unsigned depth=0;location&&depth<32;++depth,location=location->parentLoc){
   if(persistentID(location)==0xDB016E2A)return ttcg::solstheimRegion;
   for(unsigned n=0;n<locations.size();++n)if(location->GetFormID()==locations[n])return n+1;
 }
 if(ref&&persistentID(ref->GetWorldspace())==0xDB000800)return ttcg::solstheimRegion;
 return fallback;
}
inline std::uint32_t actorHold(RE::Actor* actor,unsigned fallback=ttcg::whiterunHold){
 if(const auto* p=profile(actor))return p->hold;
 return locationHold(actor,fallback);
}
inline const char* namedPlace(RE::BGSLocation* location){
 for(unsigned depth=0;location&&depth<32;++depth,location=location->parentLoc){
  const char* name=location->GetFullName();if(name&&*name)return name;
 }
 return nullptr;
}
inline const char* childPlace(std::uint32_t base,unsigned hold){
 const char* found=nullptr;
 if(auto* lists=RE::ProcessLists::GetSingleton())lists->ForAllActors([&](RE::Actor* actor){
  if(found||!actor||persistentID(actor->GetActorBase())!=base)return RE::BSContainer::ForEachResult::kContinue;
  if(auto* name=namedPlace(actor->GetEditorLocation()))found=name;
  else if(auto* name=namedPlace(actor->GetCurrentLocation()))found=name;
  else if(auto* cell=actor->GetParentCell()){const char* name=cell->GetFullName();if(name&&*name)found=name;}
  return found?RE::BSContainer::ForEachResult::kStop:RE::BSContainer::ForEachResult::kContinue;
 });
 return found?found:ttcg::holdName(hold);
}
inline ttcg::HoldTerms terms(RE::Actor* actor) {
 const auto i=profileIndex(actor);
 const auto terms=i>=0&&ttcg::opponents[i].traveller?ttcg::opponentTerms(saved,i):ttcg::holdTerms(saved,actorHold(actor));
 auto selected=development.terms(terms);
 if(i>=0&&ttcg::opponents[i].traveller)selected.rules&=~ttcg::Reverse;
 return selected;
}
inline bool unlocked(int i) {
 return development.unlockedPlayer(saved,i);
}
inline bool isChild(RE::Actor* actor) {return actor&&actor->IsChild();}
inline bool cultureEnabled(RE::Actor* actor){return !tournamentID&&development.recordResults&&development.rules<0&&!practice&&!isChild(actor)&&ttcg::cultureOpponent(saved,profileIndex(actor));}
inline bool hasAlbum() {return development.albumAccess(saved);}
inline bool actorReadyForCards(RE::Actor* actor){return actor&&!actor->IsDead()&&!actor->IsDisabled()&&!actor->IsInCombat()&&!actor->GetCurrentScene()&&!actor->IsHostileToActor(RE::PlayerCharacter::GetSingleton());}
inline bool resumingWith(RE::Actor* actor){return savedMatch.present&&!savedMatch.tournament&&actor&&persistentID(actor)==savedMatch.opponent;}
inline bool canChallenge(RE::Actor* actor,bool allowResume=true) {if(!hasAlbum())return false;if(allowResume&&resumingWith(actor))return actorReadyForCards(actor);const int i=profileIndex(actor);if(tournamentID){const auto* e=ttcg::tournament(saved,tournamentID);return e&&i>=0&&ttcg::tournamentRival(*e)==ttcg::opponents[i].base;}if(!actorReadyForCards(actor))return false;return isChild(actor)||(i>=0?unlocked(i)&&ttcg::opponentHasCards(saved,i)&&ttcg::encounterReady(saved,i,gameHour())&&(development.unlockAllPlayers||ttcg::challengeReputationMet(saved,i)):actor&&development.allowAnyNPC);}
inline int fixedWager() {if(tournamentID)return 0;return development.fixedWager(practice,canStake);}
inline int maxWager(RE::Actor* actor) {if(tournamentID)return 0;const auto* p=profile(actor);return development.maxWager(practice,canStake,p?p->wager:0);}
inline ttcg::Stock inventory(RE::TESObjectREFR* actor) {
 if(!actor) return {};
 ttcg::Stock result;
 if(actor==RE::PlayerCharacter::GetSingleton()) result=saved.player;
 else if(auto it=saved.opponents.find(ledgerKey(actor));it!=saved.opponents.end()) result=it->second;
 result[0]=physicalCount(actor,goldForm); return result;
}
struct Bank {
 std::array<RE::TESObjectREFR*,2> actors{};
 std::uint32_t rival=0;
 bool removedOpponent=false;
 explicit Bank(std::uint32_t opponent,std::uint32_t recoveryLedger=0):actors{RE::PlayerCharacter::GetSingleton(),resolveID<RE::TESObjectREFR>(opponent)},rival(actors[1]?ledgerKey(actors[1]):recoveryLedger),removedOpponent(!actors[1]&&recoveryLedger) {}
 int count(int p,ttcg::CardID id) {
   if(!id) return physicalCount(actors[p],goldForm);
   if(p==0) return ttcg::count(saved.player,id);
   auto it=saved.opponents.find(rival); return it==saved.opponents.end()?0:ttcg::count(it->second,id);
 }
 int change(int p,ttcg::CardID id,int amount) {
   // Canceling a removed reference still restores its virtual card collection.
   // Its physical purse has ceased to exist; do not strand the player's refund
   // behind gold which can no longer be returned to that actor.
   if(p==1&&!id&&removedOpponent&&amount>0)return amount;
   if(!id) return physicalChange(actors[p],goldForm,amount);
   if(ttcg::cardIndex(id)<0||(p==1&&!rival)) return 0;
   auto& stock=p==0?saved.player:saved.opponents[rival];
   const int moved=std::clamp(amount,-ttcg::count(stock,id),1000000-ttcg::count(stock,id));
   stock[id]+=moved; if(p==0&&moved>0){saved.discovered[id]=1;saved.discovered[ttcg::baseCardID(id)]=1;}
   return moved;
 }
};
inline bool available() { return saveValid&&goldForm&&packForm&&tournamentPackForm&&progression&&albumForm; }
inline void initForms() {
 erandurGlobal=RE::TESDataHandler::GetSingleton()->LookupForm<RE::TESGlobal>(0xD20,plugin);
 guildGlobal=RE::TESDataHandler::GetSingleton()->LookupForm<RE::TESGlobal>(0xB40,plugin);
 guildFaction=RE::TESForm::LookupByID<RE::TESFaction>(0x29DA9);
 guildBan=RE::TESForm::LookupByID<RE::TESQuest>(0xB03A2);
 brandSheiArrest=RE::TESForm::LookupByID<RE::TESQuest>(0x4EF25);
 collegeGlobal=RE::TESDataHandler::GetSingleton()->LookupForm<RE::TESGlobal>(0xF20,plugin);
 collegeFaction=RE::TESForm::LookupByID<RE::TESFaction>(0x1F259);
 collegeSuspension=RE::TESForm::LookupByID<RE::TESQuest>(0x5B5DC);
 valdrGlobal=RE::TESDataHandler::GetSingleton()->LookupForm<RE::TESGlobal>(0xC20,plugin);
 albumForm=RE::TESDataHandler::GetSingleton()->LookupForm<RE::TESObjectBOOK>(0x870,plugin);
 for(std::size_t i=0;i<ttcg::opponents.size();++i){opponentGlobals[i]=RE::TESDataHandler::GetSingleton()->LookupForm<RE::TESGlobal>(ttcg::opponents[i].knownGlobal,plugin);}
 progression=RE::TESDataHandler::GetSingleton()->LookupForm<RE::TESGlobal>(0x850,plugin);
 goldForm=RE::TESForm::LookupByID<RE::TESBoundObject>(0xF);
 packForm=RE::TESDataHandler::GetSingleton()->LookupForm<RE::TESObjectMISC>(0x840,plugin);
 tournamentPackForm=RE::TESDataHandler::GetSingleton()->LookupForm<RE::TESObjectMISC>(0xFD6,plugin);
 vendors::initialize(plugin);
 SKSE::log::info("Album forms available: {}",available());
}
inline void seedOpponent(RE::Actor* actor);
inline void syncProgression() {
 auto player=RE::PlayerCharacter::GetSingleton();
 saved.guildMember=player&&guildFaction&&guildBan&&player->IsInFaction(guildFaction)&&!guildBan->IsRunning();
 saved.brandSheiJailed=brandSheiArrest&&brandSheiArrest->IsRunning()&&brandSheiArrest->GetCurrentStageID()==20;
 if(guildGlobal)guildGlobal->value=saved.guildMember?1.0f:0.0f;
 saved.collegeMember=player&&collegeFaction&&collegeSuspension&&player->IsInFaction(collegeFaction)&&!collegeSuspension->IsRunning();
 if(collegeGlobal)collegeGlobal->value=saved.collegeMember?1.0f:0.0f;
 auto stage=[](std::uint32_t id){const auto q=resolveID<RE::TESQuest>(id);return q?unsigned(q->GetCurrentStageID()):0u;};
 auto running=[](std::uint32_t id){const auto q=resolveID<RE::TESQuest>(id);return q&&q->IsRunning();};
 auto completed=[](std::uint32_t id){const auto q=resolveID<RE::TESQuest>(id);return q&&q->IsCompleted();};
 const auto orcFriends=resolveID<RE::TESFaction>(0x24029),builders=resolveID<RE::TESFaction>(0xDB01C4E5);
 const auto race=player?persistentID(player->GetRace()):0;
 const bool orcWelcome=player&&(race==0x13747||race==0xA82B9||(orcFriends&&player->IsInFaction(orcFriends)));
 ttcg::refreshWorldAccess(saved,{stage(0x23B6C),stage(0x76B4A),running(0xD2CAF)&&stage(0x1F7A3)<200,running(0x21550),stage(0xDB017F90),stage(0xDB01CAF1),stage(0xDB01AAB0),orcWelcome,completed(0x25F3E),completed(0xDB018B15)});
 saved.tournamentBlocked.clear();
 for(int i=0;i<static_cast<int>(ttcg::opponents.size());++i){const auto& p=ttcg::opponents[i];bool present=false,dead=false;
   for(auto id:{p.reference,p.alternateReference})if(id){auto actor=resolveID<RE::Actor>(id);present|=actor&&!actor->IsDead()&&!actor->IsDisabled();dead|=actor&&actor->IsDead();
    if(actor&&p.dialogueFaction)if(auto faction=RE::TESDataHandler::GetSingleton()->LookupForm<RE::TESFaction>(p.dialogueFaction,plugin);faction&&!actor->IsInFaction(faction))actor->AddToFaction(faction,0);
    if(actor&&builders&&actor->IsInFaction(builders))saved.accessBlocked.insert(p.base);
    if(actor&&(p.base==0x13653||p.base==0x13657)&&locationHold(actor,0)==ttcg::haafingarHold)saved.tournamentBlocked.insert(p.base);
   }
   ttcg::progress(saved,i).unavailable=!present;
   if(dead&&!present)saved.deadPlayers.insert(p.base);else saved.deadPlayers.erase(p.base);
 }
 if(valdrGlobal&&valdrGlobal->value>0)saved.valdrRescued=true;
 if(erandurGlobal)saved.erandurReady=erandurGlobal->value>0;
 if(progression)progression->value=hasAlbum()?2.0f:0.0f;
 const auto hour=gameHour();
 for(int i=0;i<static_cast<int>(ttcg::opponents.size());++i){
   const auto& p=ttcg::opponents[i];const auto restock=ttcg::progress(saved,i).restockAt;
   // Refresh due stock before dialogue conditions are evaluated; never seed an
   // unseen collection just to decide whether its owner is willing to play.
   if(hasAlbum()&&unlocked(i)&&!saved.contract.pending()&&restock&&hour>=restock) {
     auto actor=resolveID<RE::Actor>(p.reference);
     if((!actor||actor->IsDisabled())&&p.alternateReference)actor=resolveID<RE::Actor>(p.alternateReference);
     if(actor)seedOpponent(actor);
   }
   const bool freeToTalk=actorReadyForCards(resolveID<RE::Actor>(p.reference))||(p.alternateReference&&actorReadyForCards(resolveID<RE::Actor>(p.alternateReference)));
   const bool resuming=savedMatch.present&&!savedMatch.tournament&&(savedMatch.opponent==p.reference||savedMatch.opponent==p.alternateReference);
   if(opponentGlobals[i])opponentGlobals[i]->value=hasAlbum()&&(resuming||unlocked(i))&&freeToTalk?
     (resuming||(ttcg::opponentHasCards(saved,i)&&ttcg::encounterReady(saved,i,hour)&&(development.unlockAllPlayers||ttcg::willingToPlay(saved,i,hour)))?1.0f:2.0f):0.0f;
 }
}
inline unsigned tournamentLetterID(RE::TESObjectBOOK* book){
 if(!book)return 0;const auto data=RE::TESDataHandler::GetSingleton();if(!data)return 0;
 // Match the loaded base forms directly, including ESL load indices and book
 // overrides. Receipt recognition must not depend on source-file metadata.
 for(unsigned id=0xF80;id<0xF80+30;++id)if(data->LookupForm<RE::TESObjectBOOK>(id,plugin)==book)return id;
 return 0;
}
inline void acknowledgeTournamentCourier(std::uint64_t request){
 auto* book=RE::TESForm::LookupByID<RE::TESObjectBOOK>(unsigned(request));const auto letter=tournamentLetterID(book);
 if(!letter)return;
 if(auto id=ttcg::recordTournamentInvitation(saved,letter,false,gameHour(),unsigned(request>>32)))SKSE::log::info("Tournament {} invitation queued with courier",id);
 tournamentLetterInFlight.compare_exchange_strong(request,0);
}
inline void recoverTournamentLetters(){
 auto player=RE::PlayerCharacter::GetSingleton();auto container=resolveID<RE::TESObjectREFR>(0x39FB9);auto courier=resolveID<RE::TESObjectREFR>(0x39FB7);
 if(!player||!container||!courier)return;
 auto data=RE::TESDataHandler::GetSingleton();std::set<unsigned> letters;
 for(const auto& e:saved.tournaments.events)if(e.invitation==1||e.invitation==2)letters.insert(ttcg::tournamentLetter(e));
 for(auto id:letters)if(auto book=data->LookupForm<RE::TESObjectBOOK>(id,plugin)){
  const auto carried=physicalCount(player,book),copies=physicalCount(container,book)+physicalCount(courier,book);
  for(auto event:ttcg::reconcileTournamentInvitations(saved,id,carried,copies,gameHour()))
   SKSE::log::info("Tournament {} invitation recovered from player inventory",event);
 }
 saved.tournaments.developmentToken=0;
}
inline void ensureAlbum();
inline void updateTournaments(){
 if(!available()||saved.contract.pending())return;
 recoverTournamentLetters();
 ttcg::refreshTournamentRewards(saved);
 const auto hour=gameHour();const bool hadAlbum=hasAlbum();
 for(auto id:ttcg::updateDevelopmentTournaments(saved,development,hour,tournamentVenue()))if(const auto* e=ttcg::tournament(saved,id))
  SKSE::log::info("Development {} {} in {}: {}",ttcg::tournamentCircuitName(e->circuit),id,ttcg::holdName(e->hold),e->circuit?"awaiting courier":"ready at inn");
 if(!hadAlbum&&hasAlbum())ensureAlbum();
 if(progression&&hasAlbum())progression->value=2.0f;
 ttcg::tickTournaments(saved,hour,actorHold(RE::PlayerCharacter::GetSingleton(),0),development.tournamentSwitches());
 // Use the vanilla courier holding container; never add the letter to the player.
 auto data=RE::TESDataHandler::GetSingleton();
 for(unsigned hold=1;hold<=10;++hold)if(auto global=data->LookupForm<RE::TESGlobal>(0xFCC+hold-1,plugin)){
  global->value=std::any_of(saved.tournaments.events.begin(),saved.tournaments.events.end(),[&](const auto& e){return e.hold==hold&&ttcg::tournamentEntryAvailable(e,hour)&&!ttcg::tournamentEntryActive(saved);})?1.0f:0.0f;
 }
 if(auto ready=tournamentLetterReady.load()){
  const auto* e=ttcg::tournament(saved,unsigned(ready>>32));if(!e||!e->circuit||e->invitation!=1||e->awarded)tournamentLetterReady=0;
 }
 if(auto request=tournamentLetterInFlight.load()){
  const auto* e=ttcg::tournament(saved,unsigned(request>>32));
  if(!e||e->invitation!=1||e->awarded)tournamentLetterInFlight.compare_exchange_strong(request,0);
  else if(tournamentCourierClock()-tournamentLetterSentAt.load()>30000){
   SKSE::log::warn("Tournament {} courier request was not acknowledged; retrying",e->id);
   tournamentLetterInFlight.compare_exchange_strong(request,0);
  }
 }
 for(auto& e:saved.tournaments.events)if(e.circuit&&e.invitation==1&&!e.awarded){
  auto book=data->LookupForm<RE::TESObjectBOOK>(ttcg::tournamentLetter(e),plugin);if(!book)continue;
  if(tournamentLetterInFlight.load())continue;
  if(!tournamentLetterReady.load())tournamentLetterReady.store((std::uint64_t(e.id)<<32)|book->GetFormID());
 }
 for(auto& [base,amount]:saved.tournaments.npcGold)if(amount){int i=ttcg::opponentIndex(base);if(i>=0){auto actor=resolveID<RE::Actor>(ttcg::opponents[i].reference);if(actor)amount-=std::clamp(physicalChange(actor,goldForm,int(amount)),0,int(amount));}}
 ttcg::deliverTournamentPacks(saved,[](unsigned kind,int amount){return physicalChange(RE::PlayerCharacter::GetSingleton(),kind?tournamentPackForm:packForm,amount);});
 if(saved.goldCredit>0){Bank bank(0);saved.goldCredit-=std::clamp(bank.change(0,0,saved.goldCredit),0,saved.goldCredit);}
}
inline void acceptChallenge(RE::Actor* actor) {
 if(!actor||resumingWith(actor))return;const int i=profileIndex(actor);
 if(i>=0)ttcg::acceptOpponentChallenge(saved,i);
 else if(isChild(actor)){const auto base=persistentID(actor->GetActorBase());saved.players[base].known=true;saved.childHolds[base]=actorHold(actor);}
}
inline void recordGame(RE::Actor* actor,int winner,bool completed=true) {
 if(tournamentID){if(completed)ttcg::recordTournamentMatch(saved,tournamentID,winner,development.recordResults);else ttcg::withdrawTournament(saved,tournamentID);syncProgression();updateTournaments();return;}
 const int i=profileIndex(actor);if(!development.recordResults||isChild(actor)||i<0)return;
 const auto& c=saved.contract;
 const bool staked=competitiveMatch&&c.held&&(c.trade!=ttcg::TradeNone||c.wager>0);
 if(!staked){syncProgression();return;}
 if(completed&&cultureGame)cultureNotice=ttcg::cultureChangeText(ttcg::finishRuleCulture(saved,i,winner,sessionRules,gameHour()));
 else ttcg::abandonRuleCulture(saved,gameHour());
 cultureGame=false;
 ttcg::recordCampaignResult(saved,i,winner,true,sessionRules,static_cast<std::uint32_t>(std::chrono::steady_clock::now().time_since_epoch().count()),true);
 ttcg::finishTravellerEncounter(saved,i,gameHour());
 syncProgression();
}
inline void finish(int winner,unsigned selection=~0u,std::uint32_t recoveryLedger=0) {
 auto& c=saved.contract;if(!c.pending())return;
 if(selection==~0u)selection=c.automaticChoice(winner);
 if(c.held){
   receipt.clear();const auto owners=c.transfers(winner,selection);int won=0,lost=0;
   for(int i=0;i<10;++i)if(owners[i]!=i/5){if(owners[i]==0)++won;else ++lost;}
   if(won)receipt=std::format("Won {} card{}",won,won==1?"":"s");
   if(lost){if(!receipt.empty())receipt+=" · ";receipt+=std::format("Lost {} card{}",lost,lost==1?"":"s");}
   if(c.wager){if(!receipt.empty())receipt+=" · ";receipt+=winner<0?"Wager returned":std::format("{}{} gold",winner==0?"+":"−",c.wager);}
 }
 Bank bank(c.opponent,recoveryLedger);
 if(!c.settleSelection(bank,winner,selection))notice="Inventory transfer pending.";
 else{ttcg::retainOwnedDecks(saved);notice.clear();syncProgression();SKSE::log::info("Match stakes settled: outcome {}, selection {}",winner,selection);}
}
inline void ensureAlbum() {
 auto player=RE::PlayerCharacter::GetSingleton();
 if((saved.starter||saved.developmentCardsGranted||saved.developmentFoilsGranted)&&albumForm&&player&&physicalCount(player,albumForm)==0)physicalChange(player,albumForm,1);
}
inline bool claimAlbum() {
 if(!available()||!ttcg::claimAlbum(saved,physicalCount(RE::PlayerCharacter::GetSingleton(),albumForm)>0))return false;
 syncProgression();
 SKSE::log::info("Album acquired: starter collection granted");
 return true;
}
inline bool buyAlbum(RE::Actor* actor) {
 if(!available()||!vendors::albumEligible(actor)||hasAlbum())return false;
 claimAlbum();if(hasAlbum())return false;
 Bank bank(0);
 if(!ttcg::buyAlbum(saved,bank))return false;
 ensureAlbum();syncProgression();
 SKSE::log::info("Album purchased from {} ({:08X}); starter collection granted",actor->GetName(),actor->GetFormID());
 return true;
}
inline void removeLegacyAlbumStock() {
 if(!available())return;
 const auto changed=vendors::removeLegacyAlbums([](RE::TESObjectREFR* ref){return physicalCount(ref,albumForm);},
   [](RE::TESObjectREFR* ref,int amount){return physicalChange(ref,albumForm,amount);});
 if(changed)SKSE::log::info("Removed legacy album stock from {} merchant inventories",changed);
}
inline std::string savedMatchName(){auto actor=resolveID<RE::Actor>(savedMatch.opponent);return actor?actor->GetName():"your opponent";}
inline void resolveSavedMatch(bool forfeit) {
 if(!savedMatch.present)return;
 const auto table=savedMatch; savedMatch={};
 const auto previousTournament=tournamentID;
 tournamentID=table.tournament;competitiveMatch=table.competitive;cultureGame=table.culture;sessionRules=table.table.rules;
 if(!table.table.finished()){
  if(forfeit){recordGame(resolveID<RE::Actor>(table.opponent),1,false);saved.contract.forfeit();}
  else {
   if(table.tournament)if(auto* event=ttcg::tournament(saved,table.tournament)){
    if(!ttcg::tournamentOpen(*event,gameHour()))ttcg::withdrawTournament(saved,table.tournament);
    else {event->matchNode=0;syncProgression();ttcg::resolveTournament(saved,*event);}
   }
   if(table.culture)ttcg::abandonRuleCulture(saved,gameHour());
  }
 }
 if(saved.contract.pending())finish(saved.contract.completed?saved.contract.outcome:-1,~0u,table.ledger);
 tournamentID=previousTournament;cultureGame=false;
}
inline bool recoverSavedMatch() {
 if(!savedMatch.present)return false;
 const auto* event=ttcg::tournament(saved,savedMatch.tournament);
 if(savedMatch.tournament&&!savedMatch.table.finished()&&(!event||!ttcg::tournamentOpen(*event,gameHour())||event->eliminated||event->awarded||!event->matchNode||event->matchNode!=ttcg::tournamentNode(*event))){
  resolveSavedMatch(false);notice="The tournament has ended.";return false;
 }
 auto actor=resolveID<RE::Actor>(savedMatch.opponent);
 if(!actor||actor->IsDead()||actor->IsDisabled()){
  resolveSavedMatch(false);notice="Your interrupted game has been canceled.";return false;
 }
 return true;
}
inline void recover() {
 if(!available()) return;
 removeLegacyAlbumStock();
 if(!saved.economyRandom)saved.economyRandom=static_cast<std::uint32_t>(std::chrono::steady_clock::now().time_since_epoch().count())|1u;
 claimAlbum();
 if(saved.goldCredit) { Bank bank(0); saved.goldCredit-=bank.change(0,0,saved.goldCredit); }
 syncProgression();
 if(recoverSavedMatch()){
  // A committed choice may still have outstanding native inventory writes.
  // Retry those credits without replacing a held, unchosen card reward.
  if(saved.contract.paying)finish(saved.contract.outcome);
  return;
 }
 if(!saved.contract.pending()) {
   development.grantFoils(saved);
 if(development.grantCards(saved)){ensureAlbum();SKSE::log::info("Development: granted all {} playable cards",ttcg::cards.size());}
   ensureAlbum();ttcg::retainOwnedDecks(saved);updateTournaments();return;
 }
 auto& c=saved.contract;
 // Legacy/malformed saves without a resumable table still recover their escrow.
 // Finished legacy games keep their result and use a deterministic card choice.
 int winner=c.outcome>=-1?c.outcome:-1;
 finish(winner);
 development.grantFoils(saved);
 if(development.grantCards(saved)){ensureAlbum();SKSE::log::info("Development: granted all {} playable cards",ttcg::cards.size());}
 claimAlbum();ensureAlbum();updateTournaments();
}
inline void seedOpponent(RE::Actor* actor) {
 const int i=profileIndex(actor);
 if(i<0){
   if(actor&&!isChild(actor)&&development.allowAnyNPC)saved.opponents.try_emplace(ledgerKey(actor),ttcg::childStock(persistentID(actor->GetActorBase())));
   return;
 }
 const auto id=ledgerKey(actor);
 const bool first=!saved.opponents.contains(id);
 const auto restock=ttcg::progress(saved,i).restockAt;const bool due=restock&&gameHour()>=restock;
 ttcg::replenishOpponent(saved,i,id,gameHour());
 if(first||due){Bank bank(persistentID(actor));const int purse=bank.count(1,0);if(purse<ttcg::opponents[i].wager*2)bank.change(1,0,ttcg::opponents[i].wager*2-purse);}
}
inline bool prepare(RE::Actor* actor,bool resetTerms=false) {
 notice.clear();receipt.clear();revealed.clear();revealKind.clear();
 if(!available()){notice="Card data unavailable.";return false;}
 recover();
 if(savedMatch.present){
  if(actor){notice="Finish your game with "+savedMatchName()+" first.";return false;}
  owned=inventory(RE::PlayerCharacter::GetSingleton());rivalDeck={};canStake=false;return true;
 }
 if(saved.contract.pending()||saved.goldCredit){notice="Transfer pending.";return false;}
 seedOpponent(actor);
 const int i=profileIndex(actor);
 if(resetTerms){ttcg::abandonRuleCulture(saved,gameHour());cultureGame=false;cultureNotice.clear();requestAnswered=false;sessionRules=terms(actor).rules;ruleRequest=development.rules>=0||!isChild(actor)?0:ttcg::childRequestedRule(saved,persistentID(actor->GetActorBase()),gameHour()/24,sessionRules);}
 owned=inventory(RE::PlayerCharacter::GetSingleton());auto rival=inventory(actor);
 practice=isChild(actor);
 if(!actor){canStake=false;rivalDeck={};if(ttcg::playableCount(owned)<5)notice="You need five cards.";ttcg::retainOwnedDecks(saved);return true;}
 if(isChild(actor))rival=ttcg::childStock(persistentID(actor->GetActorBase()));
 canStake=!isChild(actor)&&(profile(actor)||development.allowAnyNPC)&&ttcg::playableCount(owned)>=5&&ttcg::playableCount(rival)>=5;
 if(ttcg::playableCount(owned)<5)notice="You need five cards.";
 else if(ttcg::playableCount(ttcg::playableOpponentStock(rival,i))<5){notice="They don't have enough cards for a game.";return false;}
 const auto base=actor->GetActorBase()?persistentID(actor->GetActorBase()):0;
 const auto found=saved.players.find(base);const ttcg::PlayerRecord record=found==saved.players.end()?ttcg::PlayerRecord{}:found->second;
 const auto games=record.competitive.wins+record.competitive.losses+record.competitive.draws;
 const auto handSeed=practice?static_cast<std::uint32_t>(std::chrono::steady_clock::now().time_since_epoch().count()):persistentID(actor)^(games*2654435761u);
 if(tournamentID){auto* e=ttcg::tournament(saved,tournamentID);if(!e)return false;sessionRules=e->rules;practice=false;canStake=false;ruleRequest=0;}
 const auto* event=ttcg::tournament(saved,tournamentID);const auto node=event?ttcg::tournamentNode(*event):0;
 if(event&&!node){notice="This tournament has no game to prepare.";return false;}
 const auto seed=event?ttcg::tournamentHash(event->seed^node^(event->wins[node-8][0]+event->wins[node-8][1])*2654435761u^event->replays):handSeed;
 rivalDeck=ttcg::opponentHand(rival,i,seed,sessionRules);
 if(!ttcg::validHand(rivalDeck,rival)){notice="They don't have enough cards for a game.";return false;}
 if(saved.culture.pending.kind&&practice){ttcg::abandonRuleCulture(saved,gameHour());sessionRules=terms(actor).rules;notice="Rule challenge canceled.";}
 if(!tournamentID&&cultureEnabled(actor))ttcg::prepareRuleSpread(saved,i,gameHour());
 ttcg::retainOwnedDecks(saved);return true;
}
inline bool openPack(std::uint32_t seed) {
 if(!available()||saved.contract.pending()||saved.goldCredit||ttcg::pendingTournamentFoils(saved)) return false;
 auto cards=ttcg::packCards(seed);
 if(cards.size()!=5u)return false;
 const auto randomBefore=saved.economyRandom;
 for(auto& id:cards)id=ttcg::mintCard(saved,id);
 for(auto id:cards) if(ttcg::count(saved.player,id)>=1000000){saved.economyRandom=randomBefore;return false;}
 if(physicalChange(RE::PlayerCharacter::GetSingleton(),packForm,-1)!=-1){saved.economyRandom=randomBefore;return false;}
 Bank bank(0); for(auto id:cards) bank.change(0,id,1);
 revealed=std::move(cards);revealKind="standard";++revealID;
 ttcg::saveActiveDeck(saved);
 return true;
}
inline bool openTournamentPack(){
 if(!available()||saved.goldCredit)return false;
 auto cards=ttcg::openTournamentPack(saved,[]{return physicalChange(RE::PlayerCharacter::GetSingleton(),tournamentPackForm,-1)==-1;});
 if(cards.empty())return false;
 revealed=std::move(cards);revealKind="tournament";++revealID;return true;
}
inline std::string json(RE::Actor* actor,bool=true,bool conceal=false) {
 const auto* event=ttcg::tournament(saved,tournamentID);const auto displayHold=event?event->hold:actorHold(actor);
 auto stock=inventory(RE::PlayerCharacter::GetSingleton()); auto rival=inventory(actor);
 std::string out="{\"owned\":[";
 for(std::size_t i=0;i<ttcg::cards.size()*2;++i) { if(i) out+=','; out+=std::to_string(ttcg::count(stock,ttcg::displayCardID(static_cast<int>(i)))); }
 ttcg::ensureSavedDecks(saved);
 auto visibleDeck=saved.deck;
 out+="],\"deck\":[";
 for(int h=0;h<5;++h) { if(h) out+=','; out+=std::to_string(ttcg::displayIndex(visibleDeck[h])); }
 out+="],"+ttcg::savedDeckFields(saved);
 out+=",\"rivalDeck\":[";
 for(int h=0;h<5;++h) { if(h) out+=','; out+=conceal?"-2":std::to_string(ttcg::displayIndex(rivalDeck[h])); }
 auto& c=saved.contract;
 out+=std::format("],\"gold\":{},\"rivalGold\":{},\"canStake\":{},\"loaner\":false,\"staked\":{},\"reward\":{},\"wager\":{},\"notice\":{},\"receipt\":{},\"prizes\":[",
   ttcg::count(stock,0),ttcg::count(rival,0),canStake?"true":"false",c.pending()&&(c.rule()||c.wager)?"true":"false",c.choicePending()?"true":"false",c.wager,ttcg::quote(notice),ttcg::quote(receipt));
 for(int h=0;h<5;++h) { if(h) out+=','; out+=conceal?"-2":std::to_string(ttcg::displayIndex(c.hands[1][h])); }
 out+=std::format("],\"outcome\":{},\"choice\":{},\"paying\":{}}}",c.outcome,c.rule()==ttcg::TradeOne&&c.outcome==1?ttcg::prizeChoice(c.hands[0]):-1,c.paying?"true":"false");
 out.pop_back();
 out+=",\"trade\":"+std::to_string(c.rule())+",\"required\":"+std::to_string(c.required(c.outcome))+",\"selectedMask\":"+std::to_string(c.held?c.automaticChoice(c.outcome):c.selected)+",\"transferOwners\":[";
 const auto destinations=c.transfers(c.outcome,c.held?c.automaticChoice(c.outcome):c.selected);
 for(unsigned n=0;n<10;++n){if(n)out+=',';out+=std::to_string(destinations[n]);}out+="]";
 out+=",\"known\":[";
 for(std::size_t i=0;i<ttcg::cards.size()*2;++i) { if(i) out+=','; out+=ttcg::count(saved.discovered,ttcg::displayCardID(static_cast<int>(i)))?"true":"false"; }
 out+=std::format("],\"packs\":[{},{}],\"revealID\":{},\"revealKind\":{},\"revealed\":[",physicalCount(RE::PlayerCharacter::GetSingleton(),packForm),physicalCount(RE::PlayerCharacter::GetSingleton(),tournamentPackForm),revealID,ttcg::quote(revealKind));
 for(std::size_t i=0;i<revealed.size();++i) { if(i) out+=','; out+=std::to_string(ttcg::displayIndex(revealed[i])); }
 out+="],\"roster\":[";
 bool first=true;
 for(std::size_t i=0;i<ttcg::opponents.size();++i) {
   if(!development.showsPlayer(saved,i))continue;
   if(!first)out+=',';first=false;const auto& p=ttcg::opponents[i];const auto& r=ttcg::progress(saved,i).competitive;
   out+=std::format("{{\"name\":{},\"location\":{},\"skill\":{},\"wins\":{},\"losses\":{},\"draws\":{},\"unlocked\":{}}}",ttcg::quote(p.name),ttcg::quote(p.location),ttcg::quote(p.skillName),r.wins,r.losses,r.draws,unlocked(i)?"true":"false");
   out.pop_back();out+=",\"holdID\":"+std::to_string(p.traveller?11:p.hold)+",\"hold\":"+ttcg::quote(p.traveller?"Travellers":ttcg::holdName(p.hold))+",\"traveller\":"+(p.traveller?"true":"false")+"}";
   out.pop_back();out+=",\"base\":"+std::to_string(p.base)+",\"friendlyOnly\":"+std::string("false")+"}";

 }
 // Children are revealed by accepted challenges, without recording their games.
 for(const auto& [base,record]:saved.players)if(ttcg::opponentIndex(base)<0&&record.known){
   auto npc=resolveID<RE::TESNPC>(base);if(!npc||!npc->GetRace()||!npc->GetRace()->IsChildRace())continue;
   if(!first)out+=',';first=false;const auto& r=record.competitive;
   const auto hold=saved.childHolds.contains(base)?saved.childHolds.at(base):1;
   out+=std::format("{{\"name\":{},\"location\":{},\"skill\":\"Beginner\",\"wins\":{},\"losses\":{},\"draws\":{},\"unlocked\":true}}",ttcg::quote(npc->GetName()),ttcg::quote(childPlace(base,hold)),r.wins,r.losses,r.draws);
   out.pop_back();out+=",\"holdID\":"+std::to_string(hold)+",\"hold\":"+ttcg::quote(ttcg::holdName(hold))+"}";
   out.pop_back();out+=",\"base\":"+std::to_string(base)+",\"friendlyOnly\":true}";
 }
 const auto* p=profile(actor);const int i=profileIndex(actor);
 const auto playStock=isChild(actor)?ttcg::childStock(actor?persistentID(actor->GetActorBase()):0):rival;
 const std::string playReason=ttcg::cardsPreventingPlay(stock,playStock,i);
 const bool ready=actor&&canChallenge(actor,false)&&playReason.empty();
 out+=std::format("],\"introduced\":{},\"unlocked\":{},\"skill\":{},\"fixedRules\":{},\"fixedTrade\":{},\"maxWager\":{},\"tutorial\":false}}",saved.starter?"true":"false",saved.starter?"true":"false",ttcg::quote(development.skillName(p?p->skillName:isChild(actor)?"Beginner":"")),sessionRules,!practice&&!tournamentID?terms(actor).trade:0,maxWager(actor));
 out.pop_back();out+=std::format(",\"canPlay\":{},\"practice\":{},\"ruleRequest\":{},\"hold\":{},\"objective\":{}}}",
 ready?"true":"false",practice?"true":"false",requestAnswered?0:ruleRequest,ttcg::quote(p&&p->traveller?"":ttcg::holdName(displayHold)),ttcg::quote(ttcg::reputationObjective(saved,displayHold)));

 out.pop_back();out+=",\"playUnavailableReason\":"+ttcg::quote(playReason.empty()&&!ready?(p&&p->traveller&&!ttcg::encounterReady(saved,i,gameHour())?"M'aiq has played enough for now.":"They are unavailable for another game."):playReason);
 out+=",\"fixedWager\":"+std::to_string(fixedWager());
 const auto cultureHold=actorHold(actor),hour=gameHour();
 const bool cultureAvailable=cultureEnabled(actor);
 out+=",\"culture\":"+ttcg::cultureJson(saved,i,cultureHold,hour,cultureAvailable,cultureNotice);
 out+=",\"holdID\":"+std::to_string(displayHold)+",\"regions\":[";
 bool regionFirst=true;
 for(const auto& h:ttcg::holds){if(!regionFirst)out+=',';regionFirst=false;const auto terms=ttcg::holdTerms(saved,h.id);
 const auto rep=ttcg::localReputation(saved,h.id);
 out+=std::format("{{\"id\":{},\"name\":{},\"rules\":{},\"trade\":{},\"reputationTitle\":{},\"wins\":{}}}",h.id,ttcg::quote(h.name),terms.rules,terms.trade,ttcg::quote(rep.title()),rep.wins);}
 for(int n=0;n<int(ttcg::opponents.size());++n)if(ttcg::opponents[n].traveller&&development.showsPlayer(saved,n)){out+=",{\"id\":11,\"name\":\"Travellers\",\"travellers\":true}";break;}
 out+="],\"ruleLesson\":"+std::to_string(tournamentID?0:development.ruleLesson)+",\"tournamentID\":"+std::to_string(tournamentID)+",\"tournaments\":"+ttcg::tournamentsJson(saved,gameHour(),tournamentVenue(),tournamentRegistration,gameDate(),RE::PlayerCharacter::GetSingleton()->GetName(),actorHold(RE::PlayerCharacter::GetSingleton(),0))+"}";
 return out;
}
inline void save(SKSE::SerializationInterface* serial) {
 if(!saveValid) return;
 auto data=ttcg::encodeCollection(saved);
 if(!serial->WriteRecord(0x434F4C4C,ttcg::collectionFormat,data.data(),static_cast<std::uint32_t>(data.size()*4))) SKSE::log::error("Could not save collection state");
 if(savedMatch.present){const auto table=ttcg::encodeMatch(savedMatch);if(!serial->WriteRecord(0x4D415443,ttcg::savedMatchFormat,table.data(),static_cast<std::uint32_t>(table.size()*4)))SKSE::log::error("Could not save interrupted match");}
}
inline void revert(SKSE::SerializationInterface*) {saved={};savedMatch={};tournamentID=0;tournamentRegistration=0;tournamentLetterReady=0;tournamentLetterInFlight=0;cultureGame=false;cultureNotice.clear();saveValid=true;notice.clear();receipt.clear();revealed.clear();revealKind.clear();revealID=0;practice=false;competitiveMatch=false;requestAnswered=false;ruleRequest=0;sessionRules=0;}
inline void load(SKSE::SerializationInterface* serial) {
 revert(serial);
 std::uint32_t type,version,length;
 while(serial->GetNextRecordInfo(type,version,length)) {
   if(type==0x4D415443){
    if(version!=ttcg::savedMatchFormat||length!=65*4){SKSE::log::warn("Unsupported saved match; returning its stakes");continue;}
    std::vector<std::uint32_t> data(length/4);
    if(serial->ReadRecordData(data.data(),length)!=length||!ttcg::decodeMatch(data,savedMatch)){savedMatch={};SKSE::log::warn("Invalid saved match; returning its stakes");}
    continue;
   }
   if(type!=0x434F4C4C) continue;
   if((version!=20&&version!=21&&version!=26&&version!=27&&version!=ttcg::collectionFormat)||length%4||length>1048576) { saveValid=false; continue; }
   std::vector<std::uint32_t> data(length/4);
   if(serial->ReadRecordData(data.data(),length)!=length||!ttcg::decodeCollection(data,saved,version)) { saveValid=false; continue; }
   for(auto* id:{&saved.contract.opponent}) if(*id&&!authoredKey(*id)) {
     RE::FormID resolved=0; if(serial->ResolveFormID(*id,resolved)) *id=resolved;
     else { *id=0; if(saved.contract.pending()) saveValid=false; }
   }
 }
 std::map<std::uint32_t,ttcg::Stock> opponents;
 for(auto& [old,stock]:saved.opponents) { RE::FormID resolved=0; if(authoredKey(old)?(resolved=old,true):serial->ResolveFormID(old,resolved)) opponents[resolved]=std::move(stock); else saveValid=false; }
 saved.opponents=std::move(opponents);
 std::map<std::uint32_t,ttcg::MerchantStock> merchants;
 for(auto& [old,stock]:saved.merchants){RE::FormID resolved=0;if(authoredKey(old)?(resolved=old,true):serial->ResolveFormID(old,resolved))merchants[resolved]=std::move(stock);else saveValid=false;}
 saved.merchants=std::move(merchants);
 std::map<std::uint32_t,ttcg::PlayerRecord> players;
 for(auto& [old,record]:saved.players){
   if(ttcg::opponentIndex(old)>=0||authoredKey(old)){players[old]=record;continue;}
   RE::FormID resolved=0;if(serial->ResolveFormID(old,resolved))players[resolved]=record;
 }
 saved.players=std::move(players);
 std::map<std::uint32_t,unsigned> childHolds;
 for(auto [old,hold]:saved.childHolds){if(ttcg::opponentIndex(old)>=0||authoredKey(old)){childHolds[old]=hold;continue;}RE::FormID resolved=0;if(serial->ResolveFormID(old,resolved))childHolds[resolved]=hold;}
 saved.childHolds=std::move(childHolds);
 for(auto* id:{&savedMatch.opponent,&savedMatch.ledger,&savedMatch.waitPackage})if(*id&&!authoredKey(*id)){
  RE::FormID resolved=0;if(serial->ResolveFormID(*id,resolved))*id=resolved;else *id=0;
 }
 if(savedMatch.present&&(!savedMatch.opponent||!ttcg::matchContractValid(savedMatch,saved.contract))){savedMatch={};SKSE::log::warn("Saved match does not match its stakes; recovering collection");}
 for(auto& e:saved.tournaments.events)if(!savedMatch.present||savedMatch.tournament!=e.id||savedMatch.table.finished())e.matchNode=0;
 // An interrupted final keeps its prize screen. Acquisition stays locked until
 // it is resolved; deliberately leaving still forfeits unused foil upgrades.
 if(!savedMatch.present||!savedMatch.tournament)ttcg::forfeitTournamentFoils(saved);
 if(!savedMatch.present||!savedMatch.culture)ttcg::abandonRuleCulture(saved,0);
 syncProgression();
 SKSE::log::info("Collection loaded: valid {}, starter {}, pending {}",saveValid,saved.starter,saved.contract.pending());
}
}
