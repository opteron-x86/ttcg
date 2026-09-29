#pragma once
#include "Tournaments.h"
#include <charconv>
#include <string>
namespace ttcg {
struct DevelopmentSettings {
 bool enabled=false,addAllFoils=false,addAllCards=false,showAllPlayers=false;
 bool unlockAllPlayers=false,revealAllCards=false,allowAnyNPC=false;
 bool recordResults=true;
 bool regularTournaments=false,invitationalTournaments=false,mastersTournaments=false;
 unsigned tournamentHold=1;
 int ruleLesson=0;
 int rules=-1,tradeRule=-1,wager=-1,opponentSkill=-1;
 inline static constexpr std::array<std::pair<const char*,bool DevelopmentSettings::*>,8> switches{{
  {"RegularTournaments",&DevelopmentSettings::regularTournaments},{"InvitationalTournaments",&DevelopmentSettings::invitationalTournaments},
  {"MastersTournaments",&DevelopmentSettings::mastersTournaments},{"ShowAllPlayers",&DevelopmentSettings::showAllPlayers},
  {"UnlockAllPlayers",&DevelopmentSettings::unlockAllPlayers},{"RevealAllCards",&DevelopmentSettings::revealAllCards},
  {"AllowAnyNPC",&DevelopmentSettings::allowAnyNPC},{"RecordResults",&DevelopmentSettings::recordResults}
 }};
 bool set(std::string_view key,int value) {
   if(!enabled)return false;
   for(auto [name,field]:switches)if(key==name){if(value!=0&&value!=1)return false;this->*field=value;return true;}
   if(key=="TournamentHold"&&value>=1&&value<=10)tournamentHold=value;
   else if(key=="RuleLesson"&&value>=0&&value<=4)ruleLesson=value;
   else if(key=="Rules"&&(value==-1||(value>=0&&validRules(value))))rules=value;
   else if(key=="TradeRule"&&value>=-1&&value<=4)tradeRule=value;
   else if(key=="Wager"&&(value==-1||value==0||value==5||value==10||value==25||value==50||value==100||value==200))wager=value;
   else if(key=="OpponentSkill"&&value>=-1&&value<=3)opponentSkill=value;
   else return false;
   return true;
 }
 template<class Persist> bool setPersisted(std::string_view key,int value,Persist persist) {
   auto next=*this;if(!next.set(key,value)||!persist(key,value))return false;*this=next;return true;
 }
 template<class Read> static DevelopmentSettings load(Read read) {
   auto integer=[&](const char* key,int fallback) {
     auto text=read(key);const auto first=text.find_first_not_of(" \t\r\n"),last=text.find_last_not_of(" \t\r\n");
     if(first==std::string::npos)return fallback;
     int value=0;const char* end=text.data()+last+1;
     auto result=std::from_chars(text.data()+first,end,value);
     return result.ec==std::errc{}&&result.ptr==end?value:fallback;
   };
   DevelopmentSettings d;d.enabled=integer("Enabled",0)==1;if(!d.enabled)return d;
   d.addAllFoils=integer("AddAllFoils",0)==1;d.addAllCards=integer("AddAllCards",0)==1;
   for(auto [key,field]:switches)d.set(key,integer(key,key==std::string_view("RecordResults")?1:0));
   const auto hold=integer("TournamentHold",1);if(hold>=1&&hold<=10)d.tournamentHold=hold;
   auto lesson=integer("RuleLesson",0);if(lesson>=0&&lesson<=4)d.ruleLesson=lesson;
   auto value=integer("Rules",-1);if(value>=0&&validRules(value))d.rules=value;
   value=integer("TradeRule",-1);if(value>=0&&value<=4)d.tradeRule=value;
   value=integer("Wager",-1);if(value==0||value==5||value==10||value==25||value==50||value==100||value==200)d.wager=value;
   value=integer("OpponentSkill",-1);if(value>=0&&value<=3)d.opponentSkill=value;
   return d;
 }
 std::array<bool,3> tournamentSwitches() const {
   return {enabled&&regularTournaments,enabled&&invitationalTournaments,enabled&&mastersTournaments};
 }
 bool showsPlayer(const CollectionSave& s,int i) const {return showAllPlayers||progress(s,i).known;}
 bool unlockedPlayer(const CollectionSave& s,int i) const {
   return i>=0&&i<static_cast<int>(opponents.size())&&!s.accessBlocked.contains(opponents[i].base)&&(!opponents[i].college||s.collegeMember)&&(!opponents[i].guild||s.guildMember)&&(opponents[i].base!=0x1334F||!s.brandSheiJailed)&&(unlockedOpponent(s,i)||(unlockAllPlayers&&!progress(s,i).unavailable));
 }
 const char* skillName(const char* normal) const {
   static constexpr const char* names[]={"Beginner","Regular","Experienced","Expert"};
   return opponentSkill<0?normal:names[opponentSkill];
 }
 HoldTerms terms(HoldTerms normal) const {
   if(rules>=0)normal.rules=rules;if(tradeRule>=0)normal.trade=tradeRule;
   return normal;
 }
 int fixedWager(bool practice,bool canStake) const {return practice||!canStake?0:wager;}
 int maxWager(bool practice,bool canStake,int normal) const {
   const int fixed=fixedWager(practice,canStake);return fixed>=0?fixed:normal;
 }
 bool albumAccess(const CollectionSave& s) const {return s.starter||s.developmentCardsGranted||s.developmentFoilsGranted||unlockAllPlayers;}
 // Explicit buttons refill missing copies, including cards previously lost or
 // sold. Automatic INI grants below remain one-time, discovery-aware grants.
 int addMissingCards(CollectionSave& s,bool foil) const {
   if(!enabled||s.contract.pending()||s.goldCredit||pendingTournamentFoils(s))return -1;
   int added=0;
   for(const auto& c:cards){const auto id=c.form|(foil?foilFlag:0);if(!count(s.player,id)){s.player[id]=1;++added;}s.discovered[c.form]=1;s.discovered[id]=1;}
   (foil?s.developmentFoilsGranted:s.developmentCardsGranted)=true;return added;
 }
 bool grantFoils(CollectionSave& s) const {
   if(!enabled||!addAllFoils||s.contract.pending()||s.goldCredit)return false;
   auto missing=[&](const Card& c){const auto foil=c.form|foilFlag;return !count(s.player,foil)&&!count(s.discovered,foil);};
   if(s.developmentFoilsGranted){
     bool any=false;for(const auto& c:cards)if(missing(c)){++s.player[c.form|foilFlag];s.discovered[c.form|foilFlag]=1;s.discovered[c.form]=1;any=true;}
     return any;
   }
   for(const auto& c:cards)if(count(s.player,c.form|foilFlag)>=1000000)return false;
   for(const auto& c:cards){++s.player[c.form|foilFlag];s.discovered[c.form|foilFlag]=1;s.discovered[c.form]=1;}
   s.developmentFoilsGranted=true;return true;
 }
 bool grantCards(CollectionSave& s) const {
   if(!enabled||!addAllCards||s.contract.pending()||s.goldCredit)return false;
   if(!s.developmentCardsGranted){
     if(!grantDevelopmentCards(s,enabled,-1))return false;
     s.developmentCardsGranted=true;return true;
   }
   bool any=false;for(const auto& c:cards)if(!count(s.player,c.form)&&!count(s.discovered,c.form)){++s.player[c.form];s.discovered[c.form]=1;any=true;}
   return any;
 }
};
}
