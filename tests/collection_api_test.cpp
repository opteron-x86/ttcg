#include "CollectionAPIService.h"
#include "ExpansionRewards.h"
#include <cassert>
#include <cstring>
#include <iostream>
#include <thread>

int main() {
    using namespace ttcg;
    using namespace ttcg::collectionAPI;
    Store local;CollectionSave save;
    assert(!local.acquire()&&local.getRevision()==0);
    assert(local.refresh(save));
    const auto empty=local.acquire();
    assert(empty->summary.cardCount==cards.size()&&empty->summary.ownedCards==0);
    assert(!local.refresh(save)&&local.acquire()==empty);
    const auto id=cards[0].form;const auto key=namedCardKey(id);
    assert(key.starts_with("tessera:")&&namedCardID(key)==id);
    save.player[id]=3;save.player[id|foilFlag]=2;
    assert(local.refresh(save));auto first=local.acquire();
    const auto* card=first->find(key);
    assert(card&&card->standardCount==3&&card->foilCount==2);
    assert((card->flags&TESSERA_CARD_DISCOVERED)&&(card->flags&TESSERA_CARD_FOIL_DISCOVERED));
    assert(first->summary.ownedCards==1&&first->summary.standardCopies==3&&first->summary.foilCopies==2);
    assert(std::string(card->art).starts_with("TesseraUI/art/cards/"));
    assert(empty->find(key)->standardCount==0); // previously acquired snapshots are immutable
    save.player[id]-=2;save.contract.held=true;save.contract.escrow[0][id]=2;
    save.contract.escrow[1][cards[1].form]=1;
    assert(!local.refresh(save)&&local.acquire()==first); // reserving stakes is not a loss
    save.contract.held=false;save.contract.escrow={};save.contract.paying=true;
    save.contract.credits[0][id]=1; // one was lost; the other is still owed
    assert(local.refresh(save)&&local.acquire()->find(key)->standardCount==2);
    save.player[id]+=1;save.contract.credits[0][id]=0;
    assert(!local.refresh(save)); // partial payment does not double-count
    save.contract.paying=false;save.contract.credits={};save.discovered[id]=1;save.discovered[id|foilFlag]=1;
    save.player.erase(id);save.player.erase(id|foilFlag);
    assert(local.refresh(save));auto sold=local.acquire();
    assert(sold->summary.ownedCards==0&&sold->summary.discoveredCards==1&&sold->summary.foilDiscoveredCards==1);
    assert(card->standardCount==3&&std::strcmp(card->key,key.c_str())==0); // still valid after updates
    assert(local.reset()&&!local.acquire());const auto resetRevision=local.getRevision();
    assert(resetRevision>sold->summary.revision&&!local.reset());
    assert(local.refresh({})&&local.acquire()->summary.discoveredCards==0);
    assert(local.acquire()->summary.revision>resetRevision); // loading another character clears old data

    // Distribution flags must not hide legendary prizes or existing ownership.
    const auto god=std::find_if(cards.begin(),cards.end(),[](const auto& c){return c.tier==10&&c.unique&&!c.available&&!c.pending;});
    const auto pending=std::find_if(cards.begin(),cards.end(),[](const auto& c){return c.pending;});
    assert(god!=cards.end()&&pending!=cards.end());
    CollectionSave special;special.player[god->form]=1;special.player[pending->form|foilFlag]=1;
    assert(local.refresh(special));auto prizes=local.acquire();
    assert(prizes->summary.ownedCards==2&&prizes->summary.standardCopies==1&&prizes->summary.foilCopies==1);
    assert(prizes->find(namedCardKey(god->form))->flags&TESSERA_CARD_UNIQUE);
    assert(!(prizes->find(namedCardKey(god->form))->flags&TESSERA_CARD_PENDING));
    assert(prizes->find(namedCardKey(pending->form))->flags&TESSERA_CARD_PENDING);

    ExpansionCard expansion;
    expansion.key="api-test:wanderer";expansion.name="Wanderer";expansion.setId="api-test";expansion.setName="API test";
    expansion.art="expansions/api-test/face.webp";expansion.thumbnail="expansions/api-test/thumb.webp";
    expansion.rarity="Rare";expansion.groups="[\"Khajiit\"]";expansion.affinities="[]";expansion.sides={1,2,3,4};expansion.tier=2;
    registerExpansion("api-test","API test",{expansion});
    CollectionSave expanded;assert(grantExpansionCard(expanded,"api-test:wanderer",2,true));
    assert(local.refresh(expanded));auto added=local.acquire();auto* custom=added->find("api-test:wanderer");
    assert(custom&&custom->foilCount==2&&custom->standardCount==0&&custom->tier==2&&custom->sides[3]==4);
    assert(std::strcmp(custom->setID,"api-test")==0&&std::strcmp(custom->art,"TesseraUI/art/expansions/api-test/face.webp")==0);
    const auto numeric=expansionCardID("api-test:wanderer");
    expanded.missingCards[missingCardBase]="removed:card";expanded.player[missingCardBase]=3;
    assert(!local.refresh(expanded)); // unavailable/orphan handles are not another card
    assert(!added->find("removed:card")&&!added->find("bad-key"));
    cards.assign(baseCards.begin(),baseCards.end());cardGroups.assign(baseCardGroups.begin(),baseCardGroups.end());expansionCards.clear();expansionKeys.clear();cardSets.clear();
    assert(local.refresh(expanded)&&!local.acquire()->find("api-test:wanderer"));
    assert(local.acquire()->summary.ownedCards==0&&custom->foilCount==2); // immutable metadata outlives registry
    ExpansionCard filler=expansion;filler.key="other:filler";filler.setId="other";
    registerExpansion("other","Other",{filler});registerExpansion("api-test","API test",{expansion});
    assert(expansionCardID("api-test:wanderer")!=numeric);
    expanded.player.erase(numeric|foilFlag);expanded.discovered.clear();
    assert(grantExpansionCard(expanded,"api-test:wanderer",1,false));
    assert(local.refresh(expanded)&&local.acquire()->find("api-test:wanderer")->standardCount==1);

    // Exercise the same function table exported by TTCG.dll (null/bounds/lifetime).
    store.reset();assert(!api.AcquireSnapshot()&&!api.GetCard(nullptr,0)&&!api.GetSummary(nullptr));
    assert(store.refresh(expanded));const auto handle=api.AcquireSnapshot();
    assert(handle&&api.version==1&&api.GetSummary(handle)->cardCount==cards.size());
    assert(!api.GetCard(handle,UINT32_MAX)&&!api.FindCard(handle,nullptr)&&!api.FindCard(handle,"missing:key"));
    auto* held=api.FindCard(handle,"api-test:wanderer");assert(held&&held->standardCount==1);
    std::atomic_bool done=false;
    std::thread reader([&]{while(!done){auto snapshot=api.AcquireSnapshot();if(snapshot){auto summary=api.GetSummary(snapshot);assert(summary&&summary->size==sizeof(*summary));api.ReleaseSnapshot(snapshot);}}});
    for(unsigned i=0;i<200;++i){expanded.player[id]=i%4;store.refresh(expanded);if(i%10==0)store.reset();}
    done=true;reader.join();store.reset();assert(held->standardCount==1);
    api.ReleaseSnapshot(handle);api.ReleaseSnapshot(nullptr);
    std::cout<<"Collection API: stable keys, metadata, foils, discovery, stakes, credits, save reset, missing sets, immutable snapshots and concurrent reads passed\n";
}
