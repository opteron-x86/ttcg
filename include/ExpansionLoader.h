#pragma once
#include "CardRegistry.h"
#include <nlohmann/json.hpp>
#include <filesystem>
#include <fstream>
#include <set>
#include <stdexcept>

namespace ttcg {
struct ExpansionDefinition {std::string id,name;std::vector<ExpansionCard> cards;};
inline void expansionFields(const nlohmann::json& object,std::initializer_list<std::string_view> allowed){
 if(!object.is_object())throw std::runtime_error("expected a JSON object");
 for(const auto& [key,value]:object.items())if(std::find(allowed.begin(),allowed.end(),key)==allowed.end())throw std::runtime_error("unknown field: "+key);
}
inline std::string expansionText(const nlohmann::json& value,std::size_t maximum){
 if(!value.is_string())throw std::runtime_error("expected a string");
 auto text=value.get<std::string>();
 if(text.empty()||text.size()>maximum||text.front()==' '||text.back()==' ')throw std::runtime_error("empty, padded or oversized text");
 // JSON's UTF-8 parser checks encoding. Reject controls in display text.
 for(unsigned char c:text)if(c<32||c==127)throw std::runtime_error("control character in text");
 return text;
}
inline std::string expansionArtPath(const nlohmann::json& value,const std::string& set){
 auto path=expansionText(value,240);
 if(!path.starts_with("expansions/"+set+"/"))throw std::runtime_error("art must be beneath expansions/"+set+"/");
 for(char c:path)if(!((c>='a'&&c<='z')||(c>='A'&&c<='Z')||(c>='0'&&c<='9')||c=='_'||c=='-'||c=='.'||c=='/'))throw std::runtime_error("art paths must use ASCII letters, digits, / . _ or -");
 std::size_t at=0;while(at<path.size()){auto end=path.find('/',at);auto part=path.substr(at,end==path.npos?path.size()-at:end-at);if(part.empty()||part=="."||part==".."||part.back()=='.')throw std::runtime_error("unsafe art path");if(end==path.npos)break;at=end+1;}
 const auto ext=std::filesystem::path(path).extension().string();
 if(ext!=".png"&&ext!=".webp"&&ext!=".jpg"&&ext!=".jpeg")throw std::runtime_error("art must be PNG, WebP or JPEG (lowercase extension)");
 return path;
}
inline nlohmann::json parseIntegrationJson(std::string_view bytes){
 // Reject duplicate JSON properties as well as duplicate card IDs.
 std::vector<std::set<std::string>> objects;
 auto callback=[&](int depth,nlohmann::json::parse_event_t event,nlohmann::json& item){
  if(depth>16)throw std::runtime_error("JSON nesting exceeds 16 levels");
  using E=nlohmann::json::parse_event_t;
  if(event==E::object_start)objects.emplace_back();
  else if(event==E::key&&!objects.back().insert(item.get<std::string>()).second)throw std::runtime_error("duplicate JSON property: "+item.get<std::string>());
  else if(event==E::object_end)objects.pop_back();
  return true;
 };
 return nlohmann::json::parse(bytes,callback);
}
inline ExpansionDefinition parseExpansion(std::string_view bytes){
 if(bytes.size()>2*1024*1024)throw std::runtime_error("manifest exceeds 2 MiB");
 const auto json=parseIntegrationJson(bytes);
 expansionFields(json,{"schemaVersion","setId","name","cards"});
 if(!json.at("schemaVersion").is_number_integer()||json.at("schemaVersion")!=1)throw std::runtime_error("unsupported schemaVersion (expected 1)");
 ExpansionDefinition result;result.id=expansionText(json.at("setId"),64);result.name=expansionText(json.at("name"),96);
 if(!validExpansionIdentifier(result.id)||result.id=="tessera")throw std::runtime_error("invalid or reserved setId");
 const auto& entries=json.at("cards");
 if(!entries.is_array()||entries.empty()||entries.size()>512)throw std::runtime_error("cards must contain 1–512 definitions");
 std::set<std::string> ids;
 for(const auto& entry:entries){
  std::string context="card "+std::to_string(result.cards.size()+1);
  try{
   expansionFields(entry,{"id","name","tier","rarity","sides","groups","affinities","unique","art","thumbnail","acquisition"});
   ExpansionCard c;c.setId=result.id;c.setName=result.name;
   auto id=expansionText(entry.at("id"),64);context=id;
   if(!validExpansionIdentifier(id)||!ids.insert(id).second)throw std::runtime_error("invalid or duplicate card id");
   c.key=result.id+":"+id;c.name=expansionText(entry.at("name"),128);
   auto rank=[](const auto& v){if(!v.is_number_integer()||v<1||v>10)throw std::runtime_error("tiers and side values must be integers from 1 to 10");return v.template get<int>();};
   c.tier=rank(entry.at("tier"));const auto& sides=entry.at("sides");
   if(!sides.is_array()||sides.size()!=4)throw std::runtime_error("sides must be [top, right, bottom, left]");
   for(unsigned i=0;i<4;++i)c.sides[i]=rank(sides[i]);
   c.rarity=expansionText(entry.at("rarity"),16);
   if(c.rarity!="Common"&&c.rarity!="Rare"&&c.rarity!="Epic"&&c.rarity!="Legendary")throw std::runtime_error("unknown rarity");
   if(!entry.at("unique").is_boolean())throw std::runtime_error("unique must be true or false");
   c.unique=entry.at("unique").get<bool>();
   const auto& groups=entry.at("groups");const auto& affinities=entry.at("affinities");
   if(!groups.is_array()||groups.empty()||groups.size()>creatureGroups.size())throw std::runtime_error("groups must contain one or more existing groups");
   for(const auto& value:groups){auto name=expansionText(value,32);auto it=std::find(creatureGroups.begin(),creatureGroups.end(),name);if(it==creatureGroups.end())throw std::runtime_error("unknown group: "+name);auto bit=1ull<<(it-creatureGroups.begin());if(c.groupMask&bit)throw std::runtime_error("duplicate group");c.groupMask|=bit;}
   if(!affinities.is_array()||affinities.size()>attributeNames.size())throw std::runtime_error("affinities must be an array (empty for Neutral)");
   for(const auto& value:affinities){auto name=expansionText(value,32);auto it=std::find(attributeNames.begin(),attributeNames.end(),name);if(it==attributeNames.end())throw std::runtime_error("unknown affinity: "+name);auto bit=1u<<(it-attributeNames.begin());if(c.affinityMask&bit)throw std::runtime_error("duplicate affinity");c.affinityMask|=bit;if(!c.focus.empty())c.focus+=" / ";c.focus+=name;}
   if(c.focus.empty())c.focus="Neutral";
   c.groups=groups.dump();c.affinities=affinities.dump();
   c.art=entry.contains("art")?expansionArtPath(entry.at("art"),result.id):"backs/mosaic.png";
   c.thumbnail=entry.contains("thumbnail")?expansionArtPath(entry.at("thumbnail"),result.id):c.art;
   if(entry.contains("acquisition")){
    const auto& a=entry.at("acquisition");expansionFields(a,{"standardPacks","tournamentPacks","merchants","tournamentPrizes"});
    auto flag=[&](const char* key){if(!a.contains(key))return false;if(!a[key].is_boolean())throw std::runtime_error(std::string(key)+" must be true or false");return a[key].get<bool>();};
    c.standardPacks=flag("standardPacks");c.tournamentPacks=flag("tournamentPacks");
    if(a.contains("merchants")){const auto kind=expansionText(a["merchants"],16);if(kind=="regular")c.merchants=1;else if(kind=="specialist")c.merchants=2;else if(kind=="all")c.merchants=3;else if(kind!="none")throw std::runtime_error("merchants must be none, regular, specialist or all");}
    if(a.contains("tournamentPrizes")){
     const auto& prizes=a["tournamentPrizes"];
     if(!prizes.is_array()||prizes.size()>2)throw std::runtime_error("tournamentPrizes must be an array of invitational and/or masters");
     for(const auto& value:prizes){
      const auto circuit=expansionText(value,16);const unsigned bit=circuit=="invitational"?1:circuit=="masters"?2:0;
      if(!bit)throw std::runtime_error("tournamentPrizes entries must be invitational or masters");
      if(c.tournamentPrizes&bit)throw std::runtime_error("duplicate tournamentPrizes entry");
      c.tournamentPrizes|=bit;
     }
     if(c.tournamentPrizes&&(!c.unique||c.rarity!="Legendary"))throw std::runtime_error("tournamentPrizes requires a unique Legendary card");
    }
    if(c.unique&&(c.standardPacks||c.tournamentPacks||c.merchants))throw std::runtime_error("unique cards must use tournament prizes or explicit rewards, not generated stock or packs");
   }
   result.cards.push_back(std::move(c));
  }catch(const std::exception& e){throw std::runtime_error(context+": "+e.what());}
 }
 return result;
}
struct ExpansionLoadReport {unsigned sets=0,cards=0;std::vector<std::string> errors;};
inline ExpansionLoadReport loadCardExpansions(const std::filesystem::path& root="Data/SKSE/Plugins/Tessera/Expansions"){
 ExpansionLoadReport report;
 if(!cardSets.empty()){report.errors.push_back("expansions are already loaded; restart Skyrim to change them");return report;}
 std::error_code ec;std::vector<std::filesystem::path> paths;
 for(std::filesystem::directory_iterator it(root,ec),end;!ec&&it!=end;it.increment(ec))if(it->is_regular_file(ec)&&it->path().extension()==".json")paths.push_back(it->path());
 if(ec&&ec!=std::errc::no_such_file_or_directory)report.errors.push_back("cannot read expansion directory: "+ec.message());
 std::sort(paths.begin(),paths.end());
 if(paths.size()>128){report.errors.push_back("more than 128 expansion manifests; no expansions loaded");return report;}
 std::vector<std::pair<std::filesystem::path,ExpansionDefinition>> parsed;std::map<std::string,unsigned> sets;
 for(const auto& path:paths){try{
  // Read through the game's virtual filesystem without canonicalizing paths.
  std::ifstream stream(path,std::ios::binary);if(!stream)throw std::runtime_error("cannot open manifest");
  std::string text(2*1024*1024+1,'\0');stream.read(text.data(),text.size());text.resize(static_cast<std::size_t>(stream.gcount()));
  if(stream.bad())throw std::runtime_error("cannot read manifest");
  auto set=parseExpansion(text);++sets[set.id];parsed.emplace_back(path,std::move(set));
 }catch(const std::exception& e){report.errors.push_back(path.filename().string()+": "+e.what());}}
 for(auto& [path,set]:parsed){
  if(sets[set.id]!=1){report.errors.push_back(path.filename().string()+": duplicate setId "+set.id+"; all conflicting sets skipped");continue;}
  if(expansionCards.size()+set.cards.size()>maximumExpansionCards){report.errors.push_back(path.filename().string()+": exceeds total limit of 4096 expansion cards");continue;}
  report.cards+=static_cast<unsigned>(set.cards.size());++report.sets;registerExpansion(std::move(set.id),std::move(set.name),std::move(set.cards));
 }
 return report;
}
}
