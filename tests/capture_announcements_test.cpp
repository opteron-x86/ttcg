#include "Presentation.h"
#include "Lesson.h"
#include <nlohmann/json.hpp>
#include <cassert>
#include <iostream>

using namespace ttcg;
using Json=nlohmann::json;
Json snapshot(const Match& match,const Result& result){
 auto data=Json::parse(snapshotJson(match,1,1,"Hulda",true,false,result,true));
 assert(data["stages"].size()==result.stages.size());
 for(unsigned i=0;i<result.stages.size();++i){
  assert(data["stages"][i]["captures"]==result.stages[i].captures);
  assert(data["stages"][i]["highlights"]==result.stages[i].highlights);
 }
 for(int i=0;i<9;++i)assert(data["modifiers"][i]==match.modifier(match.board[i].card,i));
 return data;
}
int main(){
 for(unsigned rule:{Ascension,Descension,Dominion}){
  assert(captureLabel(rule).empty());
  assert(captureLabel(rule|Same|Plus)=="Same / Plus");
  auto demo=ruleExample(rule);bool triggered=false;
  while(!demo.complete()){
   const auto move=demo.expected();assert(demo.match.turn==0?demo.advance(move.hand,move.square):demo.advance());
   auto data=snapshot(demo.match,demo.last);
   for(unsigned i=0;i<demo.last.stages.size();++i)if(demo.last.stages[i].rule==rule){
    triggered=true;assert(data["stages"][i]["label"]=="");
   }
  }
  assert(triggered); // Silent announcements still retain real strength changes.
 }
 auto waves=Lesson(true,true).match;
 waves.board[5]={Lesson::card("murkwater-goblin"),1};++waves.placed;
 const auto cascade=waves.play({3,0});assert(cascade.stages.size()==3);
 const auto data=snapshot(waves,cascade);
 assert(data["stages"][0]["label"]=="Plus");
 assert(data["stages"][1]["label"]=="Combo"&&data["stages"][2]["label"]=="");
 assert(data["stages"][1]["captures"]==4&&data["stages"][2]["captures"]==32);
 assert(snapshot(waves,cascade)==data); // Snapshot publication cannot consume an announcement.
 for(int move=0;move<2;++move){
  CaptureAnnouncements announcements;
  assert(announcements.next(CaptureStage::Combo)=="Combo");
  assert(announcements.next(CaptureStage::Combo).empty());
  assert(announcements.next(Legion)=="Legion");
  assert(announcements.next(Decimation)=="Decimation");
 }
 std::cout<<"Capture announcements: silent strength rules, one Combo per chain, repeatable snapshots, ordered waves and unchanged modifiers passed\n";
}
