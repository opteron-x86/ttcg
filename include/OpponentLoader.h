#pragma once
#include "ExpansionLoader.h"
#include "OpponentRegistry.h"
#include <charconv>
namespace ttcg {
struct OpponentIntegration {std::string id;std::vector<OpponentDefinition> opponents;std::vector<NpcGrantDefinition> grants;std::vector<WorldForm> excludes;std::set<std::string> excludedPlugins;};
inline unsigned integrationNumber(const nlohmann::json& value,unsigned maximum){if(!value.is_number_integer()||value<0||value>maximum)throw std::runtime_error("integer outside supported range");return value.get<unsigned>();}
inline bool integrationBool(const nlohmann::json& obj,const char* key,bool fallback=false){if(!obj.contains(key))return fallback;if(!obj[key].is_boolean())throw std::runtime_error(std::string(key)+" must be true or false");return obj[key].get<bool>();}
inline WorldForm parseWorldForm(const nlohmann::json& obj){
 expansionFields(obj,{"plugin","formId"});WorldForm f;f.plugin=normalizedPlugin(expansionText(obj.at("plugin"),240));auto text=expansionText(obj.at("formId"),8);if(text.starts_with("0x"))text.erase(0,2);
 auto [end,error]=std::from_chars(text.data(),text.data()+text.size(),f.local,16);if(error!=std::errc{}||end!=text.data()+text.size()||!validWorldForm(f))throw std::runtime_error("invalid plugin/local formId (hex, without load-order prefix)");return f;
}
inline std::vector<OpponentCardGrant> parseNpcCards(const nlohmann::json& array){
 if(!array.is_array()||array.size()>128)throw std::runtime_error("cards must be an array of at most 128 entries");std::vector<OpponentCardGrant> result;std::set<std::pair<std::string,bool>> used;
 unsigned total=0;for(const auto& obj:array){expansionFields(obj,{"card","quantity","foil"});OpponentCardGrant g;g.card=expansionText(obj.at("card"),129);if(!validExpansionKey(g.card))throw std::runtime_error("cards require permanent set:card keys");g.quantity=obj.contains("quantity")?integrationNumber(obj["quantity"],99):1;g.foil=integrationBool(obj,"foil");if(!g.quantity||!used.emplace(g.card,g.foil).second||(total+=g.quantity)>128)throw std::runtime_error("duplicate card or more than 128 copies");result.push_back(std::move(g));}return result;
}
inline std::vector<OpponentGate> parseOpponentGates(const nlohmann::json& array){
 if(!array.is_array()||array.size()>16)throw std::runtime_error("availability must contain at most 16 conditions");std::vector<OpponentGate> result;
 for(const auto& obj:array){expansionFields(obj,{"quest","stageDone","minimumStage","faction","subject","global","minimum"});OpponentGate g;
  const unsigned kinds=unsigned(obj.contains("quest"))+unsigned(obj.contains("faction"))+unsigned(obj.contains("global"));if(kinds!=1)throw std::runtime_error("condition needs exactly one quest, faction or global");
  if(obj.contains("quest")){g.form=parseWorldForm(obj["quest"]);if(obj.contains("stageDone")==obj.contains("minimumStage"))throw std::runtime_error("quest condition needs stageDone or minimumStage");g.kind=obj.contains("stageDone")?0:1;g.value=integrationNumber(obj.at(g.kind?"minimumStage":"stageDone"),65535);if(obj.contains("subject")||obj.contains("minimum"))throw std::runtime_error("invalid quest condition fields");}
  else if(obj.contains("faction")){g.kind=2;g.form=parseWorldForm(obj["faction"]);g.value=1;const auto subject=obj.value("subject",std::string("player"));if(subject!="player"&&subject!="opponent")throw std::runtime_error("faction subject must be player or opponent");g.player=subject=="player";if(obj.contains("stageDone")||obj.contains("minimumStage")||obj.contains("minimum"))throw std::runtime_error("invalid faction condition fields");}
  else {g.kind=3;g.form=parseWorldForm(obj["global"]);g.value=integrationNumber(obj.at("minimum"),1000000);if(obj.contains("stageDone")||obj.contains("minimumStage")||obj.contains("subject"))throw std::runtime_error("invalid global condition fields");}
  result.push_back(std::move(g));
 }return result;
}
inline OpponentIntegration parseOpponentIntegration(std::string_view bytes){
 if(bytes.size()>2*1024*1024)throw std::runtime_error("manifest exceeds 2 MiB");const auto json=parseIntegrationJson(bytes);expansionFields(json,{"schemaVersion","id","opponents","grants","exclude","excludePlugins"});
 if(!json.at("schemaVersion").is_number_integer()||json.at("schemaVersion")!=1)throw std::runtime_error("unsupported schemaVersion (expected 1)");
 OpponentIntegration out;out.id=expansionText(json.at("id"),64);if(!validExpansionIdentifier(out.id)||out.id=="tessera")throw std::runtime_error("invalid or reserved integration id");std::set<std::string> ids;
 auto entries=[&](const char* key)->nlohmann::json{auto a=json.value(key,nlohmann::json::array());if(!a.is_array()||a.size()>256)throw std::runtime_error(std::string(key)+" must be an array of at most 256 entries");return a;};
 for(const auto& obj:entries("opponents")){
  expansionFields(obj,{"id","actorBase","reference","skill","homeRegion","traveller","tournaments","requiredForLegendary","reputation","wager","collection","rules","trade","availability","dialogue","enabled"});OpponentDefinition d;
  auto id=expansionText(obj.at("id"),64);if(!validExpansionIdentifier(id)||!ids.insert(id).second)throw std::runtime_error("invalid or duplicate opponent id");d.key=out.id+":"+id;d.actor=parseWorldForm(obj.at("actorBase"));if(obj.contains("reference"))d.reference=parseWorldForm(obj["reference"]);
  const auto skill=obj.value("skill",std::string("Regular"));auto it=std::find(opponentSkillNames.begin(),opponentSkillNames.end(),skill);if(it==opponentSkillNames.end())throw std::runtime_error("unknown skill: "+skill);d.level=unsigned(it-opponentSkillNames.begin());d.wager=std::array{10u,25u,50u,75u,100u,200u}[d.level];
  if(obj.contains("homeRegion")){const auto region=expansionText(obj["homeRegion"],32);bool found=false;for(const auto& h:holds)if(region==h.name){d.hold=h.id;found=true;}if(!found&&region!="Elsewhere")throw std::runtime_error("unknown homeRegion: "+region);}
  d.traveller=integrationBool(obj,"traveller");d.tournaments=integrationBool(obj,"tournaments");d.requiredMaster=integrationBool(obj,"requiredForLegendary");d.enabled=integrationBool(obj,"enabled",true);
  if(d.requiredMaster&&(d.level!=5||!d.hold||d.traveller))throw std::runtime_error("requiredForLegendary requires a local Master");
  if(obj.contains("reputation")){const auto rank=expansionText(obj["reputation"],32);d.reputationGate=-2;for(unsigned r=0;r<=Legendary;++r)if(rank==reputationTitle(r))d.reputationGate=int(r);if(d.reputationGate==-2)throw std::runtime_error("unknown reputation rank");}else if(!d.hold||d.traveller)d.reputationGate=0;
  if(obj.contains("wager"))d.wager=integrationNumber(obj["wager"],maximumWager);
  if(obj.contains("collection")){const auto& c=obj["collection"];expansionFields(c,{"mode","theme","cards","restock"});const auto mode=c.value("mode",std::string("generated"));if(mode!="generated"&&mode!="authored")throw std::runtime_error("collection mode must be generated or authored");d.generated=mode=="generated";d.restock=integrationBool(c,"restock",true);if(c.contains("theme")){d.theme=expansionText(c["theme"],32);if(std::find(creatureGroups.begin(),creatureGroups.end(),d.theme)==creatureGroups.end())throw std::runtime_error("unknown collection theme");}if(c.contains("cards"))d.cards=parseNpcCards(c["cards"]);if(!d.generated&&d.cards.empty())throw std::runtime_error("authored collections need cards");}
  if(obj.contains("rules")){d.personalRules=true;d.rules=0;if(!obj["rules"].is_array())throw std::runtime_error("rules must be an array");for(const auto& rule:obj["rules"]){const auto name=expansionText(rule,32);unsigned bit=0;for(auto r:ruleFlags)if(name==ruleName(r))bit=r;if(!bit||(d.rules&bit))throw std::runtime_error("unknown or duplicate rule: "+name);d.rules|=bit;}if(!validRules(d.rules))throw std::runtime_error("incompatible rules");}
  if(obj.contains("trade")){if(!d.personalRules)throw std::runtime_error("personal trade requires rules (an empty array means no game rules)");const auto trade=expansionText(obj["trade"],16);bool found=false;for(unsigned t=0;t<=TradeAll;++t)if(trade==tradeName(t)){d.trade=t;found=true;}if(!found)throw std::runtime_error("unknown trade rule");}
  if(obj.contains("availability"))d.gates=parseOpponentGates(obj["availability"]);
  if(obj.contains("dialogue")){const auto& v=obj["dialogue"];expansionFields(v,{"mode","accept","decline","noCards"});const auto mode=v.value("mode",std::string("generic"));if(mode!="generic"&&mode!="custom")throw std::runtime_error("dialogue mode must be generic or custom");d.customDialogue=mode=="custom";if(v.contains("accept"))d.accept=parseWorldForm(v["accept"]);if(v.contains("decline"))d.decline=parseWorldForm(v["decline"]);if(v.contains("noCards"))d.noCards=parseWorldForm(v["noCards"]);if(!d.customDialogue&&(d.accept.local||d.decline.local||d.noCards.local))throw std::runtime_error("dialogue responses require custom mode");}
  out.opponents.push_back(std::move(d));
 }
 for(const auto& obj:entries("grants")){expansionFields(obj,{"id","actorBase","cards","availability"});NpcGrantDefinition d;auto id=expansionText(obj.at("id"),64);if(!validExpansionIdentifier(id)||!ids.insert(id).second)throw std::runtime_error("invalid or duplicate grant id");d.key=out.id+":"+id;d.actor=parseWorldForm(obj.at("actorBase"));d.cards=parseNpcCards(obj.at("cards"));if(d.cards.empty())throw std::runtime_error("empty grant");if(obj.contains("availability"))d.gates=parseOpponentGates(obj["availability"]);out.grants.push_back(std::move(d));}
 for(const auto& obj:entries("exclude"))out.excludes.push_back(parseWorldForm(obj));
 for(const auto& obj:entries("excludePlugins")){auto plugin=normalizedPlugin(expansionText(obj,240));if(!validWorldForm({plugin,1}))throw std::runtime_error("invalid excluded plugin");out.excludedPlugins.insert(std::move(plugin));}
 if(out.opponents.empty()&&out.grants.empty()&&out.excludes.empty()&&out.excludedPlugins.empty())throw std::runtime_error("empty integration");return out;
}
inline std::vector<std::string> loadOpponentIntegrations(const std::filesystem::path& root="Data/SKSE/Plugins/Tessera/Opponents"){
 opponentDefinitions.clear();npcGrantDefinitions.clear();excludedNpcs.clear();excludedNpcPlugins.clear();
 std::vector<std::string> errors;std::vector<std::filesystem::path> paths;std::error_code ec;
 for(std::filesystem::directory_iterator it(root,ec),end;!ec&&it!=end;it.increment(ec))if(it->is_regular_file(ec)&&it->path().extension()==".json")paths.push_back(it->path());
 if(ec&&ec!=std::errc::no_such_file_or_directory)errors.push_back("cannot read opponent directory: "+ec.message());std::sort(paths.begin(),paths.end());if(paths.size()>128)return {"more than 128 opponent manifests"};
 std::vector<std::pair<std::string,OpponentIntegration>> parsed;std::map<std::string,unsigned> ids,actors;
 for(const auto& path:paths)try{std::ifstream stream(path,std::ios::binary);if(!stream)throw std::runtime_error("cannot open manifest");std::string bytes(2*1024*1024+1,'\0');stream.read(bytes.data(),bytes.size());bytes.resize(stream.gcount());if(stream.bad())throw std::runtime_error("cannot read manifest");auto d=parseOpponentIntegration(bytes);++ids[d.id];for(const auto& p:d.opponents)++actors[p.actor.plugin+":"+std::to_string(p.actor.local)];parsed.emplace_back(path.filename().string(),std::move(d));}catch(const std::exception& e){errors.push_back(path.filename().string()+": "+e.what());}
 for(auto& [path,d]:parsed){if(ids[d.id]!=1){errors.push_back(path+": duplicate integration id; conflicting files skipped");continue;}for(auto& p:d.opponents){if(actors[p.actor.plugin+":"+std::to_string(p.actor.local)]!=1){errors.push_back(path+": conflicting opponent profiles for "+p.actor.plugin+":"+std::to_string(p.actor.local));continue;}const auto base=worldFormID(p.actor);if(std::any_of(baseOpponents.begin(),baseOpponents.end(),[&](const auto& built){return built.base==base||built.alternateBase==base;})){errors.push_back(path+": compiled opponent profile cannot be replaced; use grants to assign cards");continue;}opponentDefinitions.push_back(std::move(p));}for(auto& g:d.grants)npcGrantDefinitions.push_back(std::move(g));excludedNpcs.insert(excludedNpcs.end(),d.excludes.begin(),d.excludes.end());excludedNpcPlugins.insert(d.excludedPlugins.begin(),d.excludedPlugins.end());}
 return errors;
}
}
