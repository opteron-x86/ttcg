#pragma once
#include "AlbumView.h"
#include "InterfaceSettings.h"
#include <sstream>
namespace preview {
inline std::string label(const Json& j,const char* key,std::string fallback={}) {return j.contains(key)&&j[key].is_string()?j[key].get<std::string>():fallback;}
inline int number(const Json& j,const char* key,int fallback=0) {return j.contains(key)&&j[key].is_number_integer()?j[key].get<int>():fallback;}
inline bool flag(const Json& j,const char* key,bool fallback=false) {return j.contains(key)&&j[key].is_boolean()?j[key].get<bool>():j.contains(key)&&j[key].is_number_integer()?j[key].get<int>()!=0:fallback;}
inline int itemNumber(const Json& a,int index,int fallback=-1){return a.is_array()&&index>=0&&index<int(a.size())&&a[index].is_number_integer()?a[index].get<int>():fallback;}
inline std::string join(const Json& a,std::string_view delimiter=" · "){std::string result;if(a.is_array())for(const auto& s:a)if(s.is_string()){if(!result.empty())result+=delimiter;result+=s.get<std::string>();}return result;}
inline const Json& member(const Json& j,const char* key){static const Json empty=Json::object();auto it=j.find(key);return it==j.end()?empty:*it;}
inline const Json& arrayMember(const Json& j,const char* key){static const Json empty=Json::array();auto it=j.find(key);return it==j.end()||!it->is_array()?empty:*it;}
inline const auto ruleNames=[]{std::array<std::pair<unsigned,const char*>,ttcg::ruleFlags.size()> result{};for(unsigned i=0;i<result.size();++i)result[i]={ttcg::ruleFlags[i],ttcg::ruleName(ttcg::ruleFlags[i])};return result;}();
inline const char* tradeName(int value){static const char* names[]{"None","One","Diff","Direct","All"};return names[std::clamp(value,0,4)];}
inline std::string rulesLabel(unsigned rules,bool=true){std::string result;for(auto [bit,name]:ruleNames)if(rules&bit){if(!result.empty())result+=" · ";result+=name;}return result;}
inline AlbumView albumFromGame(const Json& snapshot) {
    AlbumView result;result.connected=result.ready=true;
    const auto& c=snapshot.at("collection");const auto& all=snapshot.at("cards");
    if(!all.is_array()||all.empty()||all.size()%2||all.size()>2*(ttcg::baseCards.size()+ttcg::maximumExpansionCards))throw std::runtime_error("Invalid native catalog");
    const int count=int(all.size()/2);result.cards.clear();result.cards.reserve(count);
    const auto& owned=c.at("owned");const auto& known=c.at("known");
    if(owned.size()!=all.size()||known.size()!=all.size())throw std::runtime_error("Invalid native collection");
    for(int i=0;i<count;++i){const auto& raw=all[i];CardView card;
        card.id=raw.at("cardID");card.key=raw.at("id");card.name=raw.at("name");card.art=raw.at("art");card.thumbnail=raw.value("thumbnail",card.art);
        card.setKey=raw.value("setId",std::string{});card.rarity=raw.at("rarity");card.set=raw.value("setName",std::string("Tessera"));card.sides=raw.at("sides").get<std::array<int,4>>();card.tier=raw.at("tier");card.unique=flag(raw,"unique");
        for(const auto& name:arrayMember(raw,"groups"))for(unsigned g=0;g<ttcg::creatureGroups.size();++g)if(name.get<std::string>()==ttcg::creatureGroups[g])card.groups|=1ull<<g;
        for(const auto& name:arrayMember(raw,"affinities"))for(unsigned a=0;a<ttcg::attributeNames.size();++a)if(name==ttcg::attributeNames[a])card.affinityMask|=1u<<a;
        for(int finish=0;finish<2;++finish){card.owned[finish]=owned[i+finish*count];card.known[finish]=flag(snapshot,"revealCards")||(known[i+finish*count].is_boolean()?known[i+finish*count].get<bool>():known[i+finish*count].get<int>()>0)||card.owned[finish]>0;}
        if(card.id==0||card.tier<1||card.tier>10||ttcg::isFoil(card.id))throw std::runtime_error("Invalid native card identity");
        result.cards.push_back(std::move(card));
    }
    const auto identity=[&](int id){return id<0?0u:result.cards.at(id%count).id|(id>=count?ttcg::foilFlag:0);};
    for(int h=0;h<5;++h)result.deck[h]=identity(itemNumber(c.at("deck"),h));
    result.activeDeck=c.value("activeDeck",1u);result.revision=snapshot.at("revision");
    for(const auto& deck:arrayMember(c,"savedDecks")){ttcg::SavedDeck d;d.id=deck.at("id");d.name=deck.at("name");for(int h=0;h<5;++h)d.cards[h]=identity(itemNumber(deck.at("cards"),h));result.decks.push_back(d);}
    result.writable=label(snapshot,"phase")=="ready"&&member(snapshot,"interruptedMatch").is_null()&&!flag(c,"staked")&&!flag(c,"paying");
    result.packs=c.at("packs").get<std::array<int,2>>();result.notice=label(c,"notice");
    result.revealID=c.value("revealID",0ull);result.revealKind=label(c,"revealKind")=="tournament"?1:0;
    for(const auto& n:arrayMember(c,"revealed"))result.revealed.push_back(identity(n.get<int>()));
    // A sequence number alone is not a receipt: reopening clears its contents.
    if(result.revealed.empty())result.revealID=0;
    return result;
}
// Presentation clocks use immutable revisions. Every capture wave turns together;
// acknowledged revisions cannot replay when audio/settings send the same state.
struct Presentation {
    std::uint64_t session=~0ull,revision=~0ull;double start=0,duration=0;
    bool acknowledged=false,openingSound=false,resultSound=false;
    int placedSquare=-1,placedHand=-1,placedPlayer=-1;
    std::vector<double> stageStarts;std::vector<bool> stageSounds;
    void begin(const Json& state,double now,const Json& previous=Json{}){
        auto s=state.value("session",0ull),r=state.value("revision",0ull);
        if(s==session&&r==revision)return;
        session=s;revision=r;start=now;duration=0;acknowledged=openingSound=resultSound=false;stageStarts.clear();stageSounds.clear();placedSquare=placedHand=placedPlayer=-1;
        if(!flag(state,"settling"))return;
        if(flag(state,"opening"))duration=(itemNumber(member(state,"swapped"),0)>=0&&number(state,"redeals")==0?2.2:0)+1.15+1.1;
        else {
            const auto& before=arrayMember(previous,"board");const auto& after=arrayMember(state,"board");
            if(before.size()==9&&after.size()==9)for(int q=0;q<9;++q)if(before[q].is_null()&&!after[q].is_null()){
                placedSquare=q;placedPlayer=number(after[q],"owner");const int id=number(after[q],"card",-1);
                const auto& hands=arrayMember(previous,"hands");const auto& current=arrayMember(state,"hands");
                if(hands.size()==2&&current.size()==2)for(int h=0;h<5;++h)
                    if(itemNumber(current[placedPlayer],h)==-1&&(itemNumber(hands[placedPlayer],h)==id||itemNumber(hands[placedPlayer],h)==-2))placedHand=h;
            }
            if(placedHand>=0)duration=.36;
            for(const auto& stage:arrayMember(state,"stages")){
                stageStarts.push_back(duration);
                const bool announced=!label(stage,"label").empty();
                if(announced||number(stage,"captures"))duration+=announced?1.16:.4;
            }
            stageSounds.resize(stageStarts.size());
            if(flag(state,"suddenDeath"))duration+=1.6;
            if(label(state,"phase")=="result")duration+=1.8;
        }
    }
    bool pending(const Json& state)const{return flag(state,"settling")&&!acknowledged;}
};
}
