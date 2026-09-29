#pragma once
#include "Match.h"
#include "CardArt.h"
#include <format>
#include <string>
#include <string_view>
namespace ttcg {
inline std::string quote(std::string_view text) {
 std::string out="\"";
 for(unsigned char c:text) {
   if(c=='"'||c=='\\') { out+='\\'; out+=static_cast<char>(c); }
   else if(c<32) out+=std::format("\\u{:04x}",c);
   else out+=static_cast<char>(c);
 }
 return out+'"';
}
inline bool concealRivalHand(const Match& match,bool active){return !active||(!(match.rules&Open)&&!match.finished());}
inline std::string captureLabel(unsigned rule){
 if(rule==CaptureStage::Combo)return "Combo";
 std::string text;
 if(rule&SameWall)text="Same Wall";else if(rule&Same)text="Same";
 for(auto bit:{Plus,FallenAce,Legion,Decimation})if(rule&bit){if(!text.empty())text+=" / ";text+=ruleName(bit);}
 return text;
}
inline std::string snapshotJson(const ttcg::Match& match, std::uint64_t session, std::uint64_t revision, const std::string& opponentName, bool active, bool thinking, const Result& lastResult, bool settling, bool musicEnabled=true, bool musicAvailable=false) {
 auto score=match.score();

 std::string data=std::format("{{\"session\":{},\"revision\":{},\"phase\":{},\"opponent\":{},\"turn\":{},\"thinking\":{},\"score\":[{},{}],\"rule\":{},\"cards\":[",
   session,revision,quote(!active?"ready":match.finished()?"result":"match"),quote(opponentName),match.turn,thinking?"true":"false",score[0],score[1],quote(captureLabel((lastResult.same?Same:0u)|(lastResult.sameWall?SameWall:0u)|(lastResult.plus?Plus:0u)|(lastResult.fallenAce?FallenAce:0u))));
 for(std::size_t i=0;i<ttcg::cards.size()*2;++i) {
   auto& c=ttcg::cards[i%ttcg::cards.size()];
   if(i) data+=',';
   data+=std::format("{{\"name\":{},\"art\":{},\"sides\":[{},{},{},{}],\"tier\":{},\"focus\":{}}}",quote(c.name),quote(cardImages[i%cards.size()].art),c.sides[0],c.sides[1],c.sides[2],c.sides[3],c.tier,quote(c.focus));
   data.pop_back();data+=",\"thumbnail\":"+quote(cardImages[i%cards.size()].thumbnail)+"}";
   data.pop_back(); data+=std::format(",\"id\":{},\"cardID\":{},\"rarity\":{},\"groups\":{},\"subtypes\":{},\"affinities\":{}}}",quote(c.id),c.form|(i>=cards.size()?0x80000000u:0u),quote(c.rarity),c.groups,c.subtypes,c.affinities);
   data.pop_back();data+=std::format(",\"baseIndex\":{},\"foil\":{}",i%cards.size(),i>=cards.size()?"true":"false");data+=std::string(",\"unique\":")+(c.unique?"true":"false")+"}";
 }
 data+="],\"cardBack\":"+quote(cardBack)+",\"cardBackID\":"+quote(cardBackID)+",\"cardBackVersion\":"+std::to_string(cardBackVersion)+",\"cardBackMissing\":"+(cardBackID!=cardBackPreference?"true":"false")+",\"cardBacks\":[";
 for(std::size_t i=0;i<cardBacks.size();++i){if(i)data+=',';const auto& b=cardBacks[i];data+="{\"id\":"+quote(b.id)+",\"name\":"+quote(b.name)+",\"art\":"+quote(b.art)+",\"custom\":"+(b.custom?"true":"false")+"}";}
 data+="],\"tiles\":[";
 for(int i=0;i<9;++i){if(i)data+=',';data+=std::to_string(match.tiles[i]);}
 data+="],\"modifiers\":[";
 for(int i=0;i<9;++i){if(i)data+=',';data+=std::to_string(match.modifier(match.board[i].card,i));}
 data+="],\"activeGroup\":"+quote(match.activeGroup<0?"":creatureGroups[match.activeGroup])+",\"groupModifier\":"+std::to_string(match.activeGroup<0?0:match.rules&Legion?1:-1)+",\"handModifiers\":[";
 for(int p=0;p<2;++p){if(p)data+=',';data+='[';for(int h=0;h<5;++h){if(h)data+=',';data+=std::to_string(p==1&&(!active||!match.known(p,h,0))?0:match.groupModifier(match.hands[p][h]));}data+=']';}
 data+="],\"hands\":[";
 for(int p=0;p<2;++p) { if(p) data+=','; data+='[';
   for(int h=0;h<5;++h) { if(h) data+=','; data+=match.used[p][h]?"null":((p==1&&(!active||!match.known(p,h,0)))?"-2":std::to_string(match.hands[p][h]+(match.foil(p,h)?int(cards.size()):0))); }
   data+=']';
 }
 data+="],\"fullHands\":[";
 for(int p=0;p<2;++p) { if(p) data+=','; data+='[';
   for(int h=0;h<5;++h) { if(h) data+=','; data+=(p==1&&(!active||!match.known(p,h,0)))?"-2":std::to_string(match.hands[p][h]+(match.foil(p,h)?int(cards.size()):0)); }
   data+=']';
 }
 data+="],\"handOrigins\":[";
 std::array<int,10> originalCards{};originalCards.fill(-2);
 for(int p=0;p<2;++p){if(p)data+=',';data+='[';for(int h=0;h<5;++h){if(h)data+=',';const int origin=match.origins[p][h];
  data+=std::to_string(origin);if(active&&match.known(p,h,0))originalCards[origin]=match.hands[p][h]+(match.foil(p,h)?int(cards.size()):0);
 }data+=']';}
 data+="],\"originalHands\":[";
 for(int p=0;p<2;++p){if(p)data+=',';data+='[';for(int h=0;h<5;++h){if(h)data+=',';data+=std::to_string(originalCards[p*5+h]);}data+=']';}
 data+="],\"board\":[";
 for(int i=0;i<9;++i) {
   if(i) data+=',';
   auto b=match.board[i];
   data+=b.card<0?"null":std::format("{{\"card\":{},\"owner\":{},\"flip\":{},\"origin\":{}}}",b.card+(b.origin>=0&&match.foilHands[b.origin/5][b.origin%5]?int(cards.size()):0),b.owner,lastResult.flipped[i]?"true":"false",b.origin);
 }
 data+=std::format("],\"rules\":{},\"settling\":{},\"stages\":[",match.rules,settling?"true":"false");
 for(std::size_t i=0;i<lastResult.stages.size();++i) {
   if(i) data+=',';
   const auto& stage=lastResult.stages[i];
   data+=std::format("{{\"label\":{},\"highlights\":{},\"captures\":{}}}",quote(captureLabel(stage.rule)),stage.highlights,stage.captures);
   data.pop_back();data+=",\"group\":"+quote(stage.group<0?"":creatureGroups[stage.group])+"}";
 }
 data+=std::format("],\"forcedHand\":{},\"redeals\":{},\"suddenDeath\":{},\"swapped\":[{},{}],\"opening\":{},\"musicEnabled\":{},\"musicAvailable\":{}}}",match.forcedHand,match.redeals,active&&match.suddenDeathPending()?"true":"false",match.swapped[0],match.swapped[1],active&&settling&&match.placed==0?"true":"false",musicEnabled?"true":"false",musicAvailable?"true":"false");
 return data;
}
}
