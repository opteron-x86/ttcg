#include "QuestAliasText.h"
#include <cassert>
#include <iostream>
#include <memory>
#include <vector>

struct Instance {
    std::uint32_t id=0;
    std::vector<ttcg::AliasTextName> stringData;
    std::vector<float> valueData;
    unsigned journalStage=0;
};
struct Quest {
    std::vector<Instance*> instanceData;
    unsigned currentInstanceID=0;
    unsigned stage=20, offer=17, reservedReward=8;
};
int main(){
    std::vector<std::unique_ptr<Instance>> allocated;
    auto create=[&](unsigned id){
        allocated.push_back(std::make_unique<Instance>());
        auto* p=allocated.back().get();p->id=id;return p;
    };
    const std::vector<ttcg::AliasTextName> names{{2,100},{3,200},{4,300}};
    Quest old;
    // Create text records for an accepted quest instance.
    assert(ttcg::refreshQuestAliasText(old,names,create));
    assert(old.currentInstanceID==0 && old.instanceData.size()==1);
    assert(old.instanceData[0]->id==0 && old.instanceData[0]->stringData.size()==3);
    assert(old.stage==20 && old.offer==17 && old.reservedReward==8);
    for(int i=0;i<20;++i)assert(ttcg::refreshQuestAliasText(old,names,create));
    assert(allocated.size()==1 && old.instanceData[0]->stringData.size()==3);
    // Preserve completed instances, unrelated text and quest globals.
    auto* active=create(5);active->stringData={{2,99},{9,900}};
    active->valueData={42};active->journalStage=20;
    old.instanceData.push_back(nullptr);old.instanceData.push_back(active);old.currentInstanceID=5;
    assert(ttcg::refreshQuestAliasText(old,names,create));
    assert(allocated.size()==2 && old.instanceData.size()==3);
    assert(active->stringData.size()==4 && active->stringData[0].fullNameFormID==100);
    assert(active->stringData[1].aliasID==9 && active->stringData[1].fullNameFormID==900);
    assert(active->valueData==std::vector<float>{42} && active->journalStage==20);
    assert(old.instanceData[0]->id==0 && old.instanceData[0]->stringData.size()==3);
    // Create the active instance when other instances already exist.
    old.currentInstanceID=6;
    assert(ttcg::refreshQuestAliasText(old,names,create));
    assert(old.instanceData.size()==4 && old.instanceData.back()->id==6);
    Quest empty;
    assert(!ttcg::refreshQuestAliasText(empty,{},create));
    assert(!ttcg::refreshQuestAliasText(empty,names,[](unsigned)->Instance*{return nullptr;}));
    assert(empty.instanceData.empty());
    std::cout<<"Quest text: missing records created; repeat refresh, globals and quest state preserved\n";
}
