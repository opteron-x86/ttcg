#pragma once
#include "GameApp.h"
#include <fstream>
namespace preview {
struct GameTestAccess {
    static const auto& themeSurfaces(const GameApp& app){return app.surfaces;}
    static const auto& boardSurfaces(const GameApp& app){return app.boardSurfaces;}
    static int theme(const GameApp& app){return app.theme;}
    static ImFont* interfaceFont(const GameApp& app){return app.font();}
    static void page(GameApp& app,const Options& o){
        if(o.page=="cards")app.tab=0;
        else if(o.page=="decks")app.tab=1;
        else if(o.page=="journal")app.tab=2;
        else if(o.page=="rules")app.tab=3;
        else if(o.page=="tournaments")app.tab=4;
        else if(o.page=="appearance")app.tab=5,app.settingsPage=0,app.appearancePage=0;
        else if(o.page=="match-appearance")app.tab=5,app.settingsPage=0,app.appearancePage=1;
        else if(o.page=="controls")app.tab=5,app.settingsPage=1;
        else if(o.page=="gameplay")app.tab=5,app.settingsPage=2;
        else if(o.page=="developer")app.tab=5,app.settingsPage=3;
        else if(o.page=="buy")app.shopTab=0;
        else if(o.page=="sell")app.shopTab=1;
        else if(o.page=="buyback")app.shopTab=2;
        if(!o.topic.empty())app.topic=o.topic,app.demoStep=o.demoStep;
        app.history=o.history;
        if(o.inspect)app.inspectCard(0);
    }
    static void load(GameApp& app,const Options& o){std::ifstream in(o.snapshot);Json state;in>>state;state["settings"]["theme"]=o.theme;app.receive(state.dump());page(app,o);}
    static bool dragVisible(const GameApp& app){return app.dragPreviewDrawn;}
    static void captureFrame(GameApp& app,const Options& o){if(o.presentationTime>=0)app.presentation.start=ImGui::GetTime()-o.presentationTime;}
    static int tradePhase(const GameApp& app){return app.trade.phase;}
    static int demoStep(const GameApp& app){return app.demoStep;}
    static bool inspecting(const GameApp& app){return app.inspecting;}
    static int inspected(const GameApp& app){return app.selected+(app.selectedFoil?app.collection.cards.size():0);}
    static int modifier(const GameApp& app){return app.inspectionModifier;}
    static bool demoSelected(const GameApp& app){return app.demoSelected;}
    static const Presentation& clock(const GameApp& app){return app.presentation;}
    static void demoStart(GameApp& app){app.demoStep=0;app.demoSelected=false;app.selectedHand=-1;app.demoAnimation={};app.demoIdentity.clear();app.demoPresentation={};}
};
}
