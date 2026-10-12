#include "GameTestAccess.h"
#include <imgui_internal.h>
#include <iostream>
#include <map>
#include <fstream>
#include <sstream>
struct Hit {ImGuiID id;ImRect bounds;std::string label,window;};
std::vector<Hit> hits;
void ImGuiTestEngineHook_ItemAdd(ImGuiContext* ctx,ImGuiID id,const ImRect& rect,const ImGuiLastItemData*){if(id)hits.push_back({id,rect,{},ctx->CurrentWindow?ctx->CurrentWindow->Name:""});}
void ImGuiTestEngineHook_ItemInfo(ImGuiContext*,ImGuiID id,const char* label,ImGuiItemStatusFlags){for(auto i=hits.rbegin();i!=hits.rend();++i)if(i->id==id){i->label=label;break;}}
void ImGuiTestEngineHook_Log(ImGuiContext*,const char*,...){}
const char* ImGuiTestEngine_FindItemDebugLabel(ImGuiContext*,ImGuiID){return "";}
namespace {
void require(bool value,const char* error){if(!value)throw std::runtime_error(error);}
struct Renderer:preview::Renderer{struct Pass {int slot,treatment;float strength,amount;};std::vector<Pass> passes;ImTextureID id=10;preview::Texture upload(const unsigned char*,int w,int h)override{return {id++,w,h,std::uint64_t(w)*h*4};}void release(preview::Texture)override{}preview::Texture material(preview::Texture t,int slot,float,float,float strength,int treatment,float amount)override{passes.push_back({slot,treatment,strength,amount});return t;}const char* name()const override{return "headless";}};
}
int main(int argc,char** argv){
 try{
    require(argc==3,"Pass source and fixtures directories");IMGUI_CHECKVERSION();ImGui::CreateContext();
    auto& io=ImGui::GetIO();io.DisplaySize={1600,1100};io.DeltaTime=1.f/60;io.IniFilename=nullptr;io.ConfigFlags|=ImGuiConfigFlags_NavEnableKeyboard|ImGuiConfigFlags_NavEnableGamepad;io.BackendFlags|=ImGuiBackendFlags_HasGamepad|ImGuiBackendFlags_RendererHasTextures;GImGui->TestEngineHookItems=true;
    Renderer renderer;preview::Options o;o.tesseraRoot=argv[1];o.previewRoot=o.tesseraRoot/"native";
    unsigned frames=0;std::vector<std::string> commands;
    {
      preview::GameApp app(renderer,o);app.connect([&](const std::string& c){commands.push_back(c);});
      const auto frame=[&](float dt=1.f/60){hits.clear();renderer.passes.clear();io.DeltaTime=dt;ImGui::NewFrame();app.draw();ImGui::Render();++frames;require(ImGui::GetDrawData()->Valid,"Invalid draw data");
        // Honour dynamic font atlas requests just like the production renderer.
        for(auto* texture:ImGui::GetPlatformIO().Textures){
          if(texture->Status==ImTextureStatus_WantCreate){texture->SetTexID(renderer.id++);texture->SetStatus(ImTextureStatus_OK);}
          else if(texture->Status==ImTextureStatus_WantUpdates)texture->SetStatus(ImTextureStatus_OK);
          else if(texture->Status==ImTextureStatus_WantDestroy){texture->SetTexID(ImTextureID_Invalid);texture->SetStatus(ImTextureStatus_Destroyed);}
        }
      };
      const auto read=[&](std::string name){std::ifstream input(std::filesystem::path(argv[2])/(name+".json"));preview::Json s;input>>s;return s;};
      const auto load=[&](std::string name,std::string page=""){app.reset();commands.clear();app.receive(read(name).dump());o.page=page;preview::GameTestAccess::page(app,o);frame();frame();};
      const auto matches=[&](const Hit& hit,const std::string& label){const auto* window=ImGui::FindWindowByName(hit.window.c_str());return hit.label==label||(hit.label.empty()&&window&&hit.id==ImHashStr(label.c_str(),0,window->ID));};
      const auto bounds=[&](std::string label,int ordinal=0){for(const auto& h:hits)if(matches(h,label)&&ordinal--==0)return h.bounds;throw std::runtime_error("Missing bounds: "+label);};
      const auto clickAt=[&](ImVec2 p,int mouse=0){io.AddMousePosEvent(p.x,p.y);frame();io.AddMouseButtonEvent(mouse,true);frame();io.AddMouseButtonEvent(mouse,false);frame();frame();};
      const auto click=[&](std::string label,int ordinal=0,int mouse=0){ImVec2 p;bool found=false;for(const auto& h:hits)if(matches(h,label)&&ordinal--==0){p=h.bounds.GetCenter();found=true;break;}if(!found){for(const auto& h:hits)std::cerr<<h.window<<"/"<<h.label<<'\n';throw std::runtime_error("Missing interactive item: "+label);}
        clickAt(p,mouse);};
      const auto command=[&](std::string prefix){return std::any_of(commands.begin(),commands.end(),[&](const auto& c){return c.starts_with(prefix);});};
      for(auto page:{"cards","decks","journal","rules","tournaments","appearance","match-appearance","controls","gameplay","developer"}){load(page==std::string("tournaments")?"tournaments":"album",page);for(int theme=0;theme<5;++theme){auto s=app.snapshot();s["settings"]["theme"]=theme;app.receive(s.dump());frame();}}
      // Saved appearance values flow through the real hand/board renderer.
      load("match");{
        auto s=app.snapshot();s["settings"]["opponentTreatment"]=2;s["settings"]["opponentStrength"]=65;
        const auto filtered=[&](int slot){return std::any_of(renderer.passes.begin(),renderer.passes.end(),[&](const auto& pass){return pass.slot==slot&&pass.treatment==2&&std::abs(pass.amount-.65f)<.001f;});};
        for(int scope=0;scope<3;++scope){
          s["settings"]["opponentScope"]=scope;app.receive(s.dump());frame();
          require(filtered(9)==(scope!=1),"Opponent hand filter ignores its scope");
          require(filtered(12)==(scope!=0)&&filtered(15)==(scope!=0)&&filtered(18)==(scope!=0),"Opponent board filter ignores its scope");
          require(!filtered(4)&&!filtered(11)&&!filtered(19),"Opponent filter reached a player-controlled card");
        }
        s["board"][1]["owner"]=0;s["board"][0]["owner"]=1;app.receive(s.dump());frame();
        require(!filtered(12)&&filtered(11),"Artwork treatment follows original ownership instead of current control");
        s["hands"][1][3]=-2;app.receive(s.dump());frame();require(!filtered(9),"Hidden hand art was revealed or filtered");
        s["settings"]["opponentStrength"]=0;app.receive(s.dump());frame();
        require(std::none_of(renderer.passes.begin(),renderer.passes.end(),[](const auto& p){return p.treatment!=0;}),"Zero strength still applies a filter");
      }
      load("multi-0");{
        auto before=app.snapshot();before["settings"]["opponentTreatment"]=3;app.receive(before.dump());frame();
        auto after=read("multi-1");after["settings"]=before["settings"];app.receive(after.dump());frame();
        const auto treated=[&](int slot){return std::any_of(renderer.passes.begin(),renderer.passes.end(),[&](const auto& pass){return pass.slot==slot&&pass.treatment==3;});};
        require(!treated(17),"Capture changed artwork treatment before the card flipped");
        for(int i=0;i<60;++i)frame(.1f);
        require(treated(17)&&treated(19),"Captured cards did not receive opponent treatment");
      }
      for(auto page:{"cards","decks"}){load("album",page);auto s=app.snapshot();s["settings"]["opponentTreatment"]=3;app.receive(s.dump());frame();
        require(std::none_of(renderer.passes.begin(),renderer.passes.end(),[](const auto& p){return p.treatment!=0;}),"Match filters leaked into the album");}
      load("album","appearance");click("appearance-match");click("##match-art-treatment");click("Grayscale");
      require(command("settings ")&&commands.back().ends_with("match-art-treatment 2"),"Artwork finish does not save through the settings backend");
      {auto s=app.snapshot();s["settings"]["opponentTreatment"]=2;app.receive(s.dump());frame();frame();
        click("preview-capture");frame(.7f);frame();
        require(std::none_of(renderer.passes.begin(),renderer.passes.end(),[](const auto& p){return p.slot==preview::MaterialSlot::appearance+3&&p.treatment!=0;}),"Preview capture did not restore player artwork");
        require(std::any_of(renderer.passes.begin(),renderer.passes.end(),[](const auto& p){return p.slot==preview::MaterialSlot::appearance+1&&p.treatment==2;}),"Preview capture changed the opponent's hand");
        click("preview-capture");frame(.7f);frame();
        require(std::any_of(renderer.passes.begin(),renderer.passes.end(),[](const auto& p){return p.slot==preview::MaterialSlot::appearance+3&&p.treatment==2;}),"Preview cannot return a card to the opponent");
      }
      load("album","match-appearance");{
        auto s=app.snapshot();s["settings"]["controlPalette"]=4;app.receive(s.dump());frame();frame();commands.clear();
        click("##Text");io.AddKeyEvent(ImGuiMod_Ctrl,true);io.AddKeyEvent(ImGuiKey_A,true);frame();io.AddKeyEvent(ImGuiKey_A,false);io.AddKeyEvent(ImGuiMod_Ctrl,false);io.AddInputCharactersUTF8("2846A0");frame();
        io.AddKeyEvent(ImGuiKey_Enter,true);frame();io.AddKeyEvent(ImGuiKey_Enter,false);frame();frame();
        require(command("settings ")&&commands.back().ends_with("match-player-colour 2639520"),"Custom colour does not save after hex editing");
      }
      load("album","match-appearance");{
        auto s=app.snapshot();s["settings"]["controlPalette"]=4;app.receive(s.dump());frame();frame();commands.clear();
        const auto hex=bounds("##Text");clickAt(ImVec2(hex.Max.x+ImGui::GetStyle().ItemInnerSpacing.x+hex.GetHeight()*.5f,hex.GetCenter().y));
        bool picked=false;
        for(const auto& h:hits){const auto* w=ImGui::FindWindowByName(h.window.c_str());
          if(w&&w->Active&&(w->Flags&ImGuiWindowFlags_Popup)&&h.id==ImHashStr("sv",0,ImHashStr("##picker",0,w->ID))){const auto point=h.bounds.GetCenter();clickAt(point);picked=true;break;}}
        require(picked,"Custom colour picker did not open");
        require(command("settings ")&&commands.back().find("match-player-colour ")!=std::string::npos,"Colour picker does not save on release");
        io.AddKeyEvent(ImGuiKey_Escape,true);frame();io.AddKeyEvent(ImGuiKey_Escape,false);frame();
      }
      load("album","match-appearance");{
        auto s=app.snapshot();s["settings"]["controlMarker"]=true;app.receive(s.dump());frame();frame();commands.clear();
        // Scroll the controls to the marker without moving the fixed preview.
        for(auto* w:GImGui->Windows)if(w->Active&&std::string_view(w->Name).find("/match-appearance-body_")!=std::string_view::npos)ImGui::SetScrollY(w,w->ScrollMax.y);
        frame();frame();click("match-control-marker");require(command("settings ")&&commands.back().ends_with("match-control-marker 0"),"Boolean appearance setting ignores the saved value");
      }
      load("album","journal");{
        auto* detail=ImGui::FindWindowByName("Tessera/player-details_" );
        // Locate the actual child by suffix (ImGui appends a stable child ID).
        for(auto* window:GImGui->Windows)if(std::string_view(window->Name).find("/player-details_")!=std::string_view::npos)detail=window;
        require(detail&&detail->Pos.x>io.DisplaySize.x*.65f,"Journal details are not in the right column");
        for(const auto& vertex:detail->DrawList->VtxBuffer)require(vertex.pos.x>=detail->Pos.x-1&&vertex.pos.x<=detail->Pos.x+detail->Size.x+1,"Journal detail text escaped its column");
      }
      load("lobby");click("setup-card");{
        const auto* window=ImGui::FindWindowByName("Artwork");require(window&&window->Size.x<io.DisplaySize.x*.75f&&window->Size.y<io.DisplaySize.y*.75f,"Inspection unexpectedly fills the screen");
        for(const auto& h:hits)if(h.window=="Artwork")require(window->Rect().Contains(h.bounds),"Inspection control leaves its panel");
        require(std::none_of(hits.begin(),hits.end(),[](const auto& h){return h.label=="spin"||h.label=="details"||h.label=="artfoil";}),"Inspection has redundant display controls");
        click("flip");click("reset-art");click("close-art");
      }
      load("album");click("Decks");click("new-deck");click("cancel");require(commands.empty(),"Cancelling deck creation submitted a write");
      load("album","decks");click("rename");frame();{
        require(std::none_of(hits.begin(),hits.end(),[](const auto& h){return h.window=="Deck";}),"Renaming a deck should be inline");
        io.AddInputCharactersUTF8("Affinity practice");frame();
        io.AddKeyEvent(ImGuiKey_Enter,true);frame();io.AddKeyEvent(ImGuiKey_Enter,false);frame();frame();
        require(command("deck-manage ")&&commands.back().ends_with(" Affinity practice"),"Inline rename did not submit the deck name");
      }
      load("album","decks");click("rename");frame();{
        io.AddInputCharactersUTF8("Cancelled name");frame();io.AddKeyEvent(ImGuiKey_Escape,true);frame();io.AddKeyEvent(ImGuiKey_Escape,false);frame();frame();
        require(commands.empty(),"Escape must cancel inline renaming without closing the album");
      }
      load("album","decks");{
        const auto medium=bounds("card").GetWidth();click("view-small");
        require(bounds("card").GetWidth()<medium*.85f,"Compact density did not reduce card size");
        click("view-large");require(bounds("card").GetWidth()>medium*1.2f,"Large view icon did not increase card size");
        click("view-medium");
      }
      {
        const auto child=[&](const char* name){for(auto* w:GImGui->Windows)if(w->Active&&std::string_view(w->Name).find(std::string("/")+name+"_")!=std::string_view::npos)return w;throw std::runtime_error(std::string("Missing child: ")+name);};
        load("album","cards");const auto cardPreview=bounds("selected-art");
        load("album","decks");const auto deckPreview=bounds("selected-art");
        require(std::abs(cardPreview.Min.x-deckPreview.Min.x)<1&&std::abs(cardPreview.GetWidth()-deckPreview.GetWidth())<1&&std::abs(cardPreview.GetHeight()-deckPreview.GetHeight())<1,"Cards and Decks disagree on preview size or position");
        const float navigation=child("saved-decks")->Size.x,contentX=bounds("rename").Min.x;
        for(const auto& [page,name]:std::array<std::pair<const char*,const char*>,3>{{{"journal","holds"},{"rules","rule-chapters"},{"tournaments","tournament-list"}}}){
            load(page==std::string_view("tournaments")?"tournaments":"album",page);
            require(std::abs(child(name)->Size.x-navigation)<1,"Navigation column changes width between tabs");
            if(page==std::string_view("rules"))require(std::abs(child("rule-content")->Pos.x-contentX)<1,"Rules content does not align with Decks");
        }
        load("album","gameplay");require(std::abs(child("settings-body")->Pos.x-contentX)<1,"Settings content does not align with Decks");
      }
      load("album","gameplay");{
        ImGuiWindow* child=nullptr;
        for(auto* w:GImGui->Windows)if(std::string_view(w->Name).find("/settings-body_")!=std::string_view::npos)child=w;
        require(child&&child->ScrollbarY,"Expected scrolling gameplay settings");
        for(const auto& label:{"games","vanilla-voices","##residents"})require(bounds(label).Max.x<child->InnerRect.Max.x-8,"Settings control touches its scrollbar");
      }
      load("shop");{
        for(const auto& id:{"##shop-tier","##shop-rarity"}){
          click(id);frame();
          ImGuiWindow* popup=nullptr;for(auto* w:GImGui->Windows)if(w->Active&&std::string_view(w->Name).starts_with("##Combo_"))popup=w;
          require(popup&&popup->WindowPadding.x>0&&popup->WindowPadding.y>0,"Dropdown lacks padding");
          int choices=0;for(const auto& h:hits)if(h.window==popup->Name&&!h.label.empty()&&!h.label.starts_with("##")){
            ++choices;require(h.bounds.Min.y>=popup->InnerRect.Min.y&&h.bounds.Max.y<=popup->InnerRect.Max.y,"Dropdown clips an option vertically");
            require(h.bounds.GetWidth()>=io.Fonts->Fonts[0]->CalcTextSizeA(h.bounds.GetHeight()*18/32.f,1e5f,0,h.label.c_str()).x,"Dropdown clips an option horizontally");
          }
          require(choices>=5,"Dropdown omitted choices");click(id==std::string_view("##shop-tier")?"All tiers":"All rarities");
        }
      }
      for(const char* tab:{"Buy","Sell","Buy back"}){
        load("shop");auto s=app.snapshot();
        s["shop"]["buy"]={{{"id",0},{"count",3},{"price",10},{"buyback",3},{"salePrice",10}}};
        s["shop"]["sell"]={{{"id",0},{"count",3},{"surplus",3},{"price",10},{"warnings",0},{"reserved",0},{"lastHandAt",0}}};
        app.receive(s.dump());frame();frame();click(tab);
        click("quantity-less"); // Lower bound remains one.
        for(int i=0;i<5;++i)click("quantity-more"); // Stock caps the upper bound.
        click("quantity-less"); // Buy/sell exactly two copies.
        click("trade");const std::string action=std::string_view(tab)=="Sell"?"sell":std::string_view(tab)=="Buy back"?"buyback":"buy";
        require(command("shop ")&&commands.back().ends_with(action+" 0 2 0"),"Quantity controls submitted the wrong count or shop action");
      }
      click("Buy");
      load("lobby");click("play");require(command("start "),"Lobby did not send a validated start command");
      load("lobby");click("edit-deck");require(command("browse "),"Edit deck did not use the shared album route");
      const auto ruleOffer=[&](bool visiting=true){
        load("lobby");auto s=app.snapshot();s["revision"]=90;
        s["collection"]["ruleRequest"]=visiting?0:ttcg::Plus;
        s["collection"]["culture"]={{"offerRule",visiting?ttcg::Decimation:0},{"source","Solstheim"},{"replaces",visiting?ttcg::Legion:0}};
        app.receive(s.dump());frame();frame();return s;
      };
      for(bool accept:{true,false}){
        auto s=ruleOffer();const auto* popup=ImGui::GetTopMostPopupModal();
        require(popup&&std::string_view(popup->Name)=="Rule request","Visiting rule offer is not a modal dialogue");
        require(popup->Size.x<io.DisplaySize.x*.5f&&popup->Size.y<io.DisplaySize.y*.5f,"Rule dialogue is not compact");
        require(ImLengthSqr(popup->Rect().GetCenter()-io.DisplaySize*.5f)<4,"Rule dialogue is not centred");
        require(std::none_of(hits.begin(),hits.end(),[](const auto& h){return h.label=="play"||h.label=="edit-deck"||h.label=="setup-card";}),"Match setup is interactive before answering the rule offer");
        require(GImGui->NavId==ImHashStr("accept-rule",0,popup->ID),"Rule dialogue does not initially focus Yes");
        click(accept?"accept-rule":"decline-rule");
        require(commands.size()==1&&commands.back().starts_with("culture ")&&commands.back().ends_with(accept?"trial 1":"trial 0"),"Rule dialogue sent the wrong visiting-rule answer");
        click(accept?"decline-rule":"accept-rule");app.receive(s.dump());frame();click("accept-rule");
        require(commands.size()==1,"Repeated input or an identical snapshot submitted a second rule answer");
        s["revision"]=91;s["collection"]["culture"]["offerRule"]=0;app.receive(s.dump());frame();frame();
        require(!ImGui::GetTopMostPopupModal(),"Answered rule offer left a modal blocking setup");
        click("play");require(command("start "),"Answering the rule offer did not return to match setup");
      }
      for(bool accept:{true,false}){
        ruleOffer(false);click(accept?"accept-rule":"decline-rule");
        require(commands.size()==1&&commands.back().starts_with("terms ")&&commands.back().ends_with("rule "+std::to_string(accept?ttcg::Plus:0)),"Practice rule dialogue sent the wrong terms");
      }
      for(auto key:{ImGuiKey_Escape,ImGuiKey_GamepadFaceRight}){
        ruleOffer();io.AddKeyEvent(key,true);frame();io.AddKeyEvent(key,false);frame();frame();
        require(commands.size()==1&&command("close ")&&!command("culture "),"Back must leave the rule dialogue without answering it");
      }
      ruleOffer();app.escape();frame();require(commands.size()==1&&command("close "),"Native Escape cannot leave the rule dialogue");
      ruleOffer();click("accept-rule");frame(5.1f);click("accept-rule");require(commands.size()==2,"A timed-out rule answer cannot be retried");
      for(auto key:{ImGuiKey_Enter,ImGuiKey_GamepadFaceDown}){
        ruleOffer();io.AddKeyEvent(key,true);frame();io.AddKeyEvent(key,false);frame();frame();
        if(commands.size()!=1||!command("culture ")||!commands.back().ends_with("trial 1"))throw std::runtime_error(std::string("Rule offer Confirm failed for ")+ImGui::GetKeyName(key)+"; commands "+std::to_string(commands.size())+", navigation visible "+std::to_string(GImGui->NavCursorVisible));
      }
      load("album","controls");click("collection");require(command("binding "),"Binding capture unavailable");
      {auto s=read("album");s["settings"]["capturing"]="collection";s["revision"]=99;app.receive(s.dump());commands.clear();app.captureKey(65,ttcg::HotkeyLShift);require(command("binding "),"Modifier binding not submitted");require(commands.back().ends_with("set collection 321"),"Modifier binding changed scan code");}
      load("lobby");click("setup-card");require(preview::GameTestAccess::inspecting(app),"Lobby card did not open inspection");click("close-art");
      load("lobby");{auto s=app.snapshot();s["collection"]["deck"][0]=-1;app.receive(s.dump());frame();click("play");require(command("browse "),"Incomplete deck did not route to the shared editor");require(!command("start "),"Incomplete deck tried to start a match");}
      load("album","decks");{
        const auto source=bounds("card").GetCenter(),target=bounds("slot",0).GetCenter();
        io.AddMousePosEvent(source.x,source.y);frame();io.AddMouseButtonEvent(0,true);frame();
        io.AddMousePosEvent(source.x+20,source.y-20);frame();frame();require(preview::GameTestAccess::dragVisible(app),"Dragging a card has no visible card preview");
        io.AddMousePosEvent(target.x,target.y);frame();frame();io.AddMouseButtonEvent(0,false);frame();frame();
        require(command("deck "),"Dropping into a deck slot did not submit the edited hand");
      }
      load("album");click("card",0,1);require(preview::GameTestAccess::inspecting(app),"Album inspection unavailable");{
        const auto selected=preview::GameTestAccess::inspected(app);click("next-art");require(preview::GameTestAccess::inspected(app)!=selected,"Inspector gallery did not advance");
        io.AddKeyEvent(ImGuiKey_GamepadFaceRight,true);frame();io.AddKeyEvent(ImGuiKey_GamepadFaceRight,false);frame();frame();require(!preview::GameTestAccess::inspecting(app),"Controller Back failed to close artwork");require(!command("close "),"Controller Back closed the album behind its modal");
      }
      load("interrupted");click("resume");require(command("interrupted "),"Interrupted match cannot be resumed");
      load("trade");{
        auto s=app.snapshot();s["collection"]["reward"]=true;s["collection"]["trade"]=2;s["collection"]["required"]=2;s["collection"]["outcome"]=0;app.receive(s.dump());frame();frame();commands.clear();
        frame(1.1f);frame();click("trade-card",5);click("trade-card",8);click("trade-card",9);click("take-cards");
        require(command("claim-many ")&&commands.back().ends_with(" 9"),"Diff must preserve slots 0 and 3, cap selection, and send their mask");
      }
      load("trade");click("trade-card",0,1);require(preview::GameTestAccess::inspecting(app),"Trade card inspection unavailable");click("close-art");for(int i=0;i<50;++i)frame(.1f);click("rematch");require(command("prepare "),"Rematch command unavailable after inspection");
      load("trade");{auto s=app.snapshot();s["collection"]["canPlay"]=false;app.receive(s.dump());for(int i=0;i<50;++i)frame(.1f);require(std::none_of(hits.begin(),hits.end(),[](const auto& h){return h.label=="rematch";}),"Depleted opponent still offers Play again");}
      load("trade");{
        auto state=app.snapshot();auto& c=state["collection"];c["reward"]=true;c["staked"]=true;c["trade"]=1;c["required"]=1;c["outcome"]=0;
        app.receive(state.dump());frame(1.1f);frame();
        require(bounds("trade-card",5).Min.y<bounds("trade-card",0).Min.y,"Opponent's trade hand must be above yours");
        click("trade-card",5);click("trade-card",7);click("take-cards");require(commands.back().ends_with(" 2"),"One must replace the previous choice");
        c["reward"]=false;c["staked"]=false;c["selectedMask"]=4;c["transferOwners"][7]=0;app.receive(state.dump());frame();
        click("trade-card",7,1);const int phase=preview::GameTestAccess::tradePhase(app);
        for(int i=0;i<30;++i)frame(.1f);
        require(preview::GameTestAccess::tradePhase(app)==phase,"Trade presentation advanced underneath inspection");
        click("close-art");for(int i=0;i<30;++i)frame(.1f);
        require(preview::GameTestAccess::tradePhase(app)==5,"Settled transfer did not reach Play again");
        const auto* result=ImGui::FindWindowByName("Game result");require(result->Size.x<io.DisplaySize.x*.5f,"Post-match controls must use a compact prompt");
        app.receive(state.dump());frame();require(preview::GameTestAccess::tradePhase(app)==5,"Repeated snapshot replayed the trade animation");
      }
      load("trade");{
        auto state=app.snapshot();state["collection"]["paying"]=true;state["collection"]["staked"]=true;app.receive(state.dump());for(int i=0;i<40;++i)frame(.1f);
        require(std::none_of(hits.begin(),hits.end(),[](const auto& h){return h.label=="rematch";}),"Unsettled payout offered a rematch");
        click("take-cards");require(command("payout "),"Failed settlement cannot be retried");
      }
      for(int trade:{0,1,2,3,4})for(int outcome:{-1,0,1}){
        // Exercise automatic settlements too: both directions for Direct, all
        // five cards for All, draws with no transfers, and casual games.
        load("trade");auto state=app.snapshot();auto& c=state["collection"];
        c["trade"]=trade;c["outcome"]=outcome;c["reward"]=false;c["paying"]=false;c["staked"]=false;c["wager"]=0;c["canPlay"]=true;
        for(int origin=0;origin<10;++origin){const int owner=origin/5;
            const bool moves=trade==3?origin==1||origin==7:outcome>=0&&owner!=outcome&&origin%5<(trade==4?5:trade==2?2:trade==1?1:0);
            c["transferOwners"][origin]=moves?1-owner:owner;
        }
        app.receive(state.dump());for(int i=0;i<60;++i)frame(.1f);
        require(preview::GameTestAccess::tradePhase(app)==5,"Automatic trade did not finish");
        require(!command("claim ")&&!command("claim-many ")&&!command("payout "),"Trade presentation repeated a settled transaction");
        click("rematch");require(command("prepare "),"Automatic trade stranded the result screen");
      }
      load("pack");{
        auto s=app.snapshot();s["collection"]["revealed"]={0,1,2,3,4,5,6,7};s["collection"]["revealID"]=2;app.receive(s.dump());frame(2.f);frame();
        const auto footer=bounds("pack-continue");int cards=0;for(const auto& h:hits)if(h.label=="revealed-card"){++cards;require(h.bounds.Min.x>=0&&h.bounds.Max.x<=io.DisplaySize.x&&h.bounds.Max.y<footer.Min.y,"Starter card leaves the pack modal");}require(cards==8,"Starter receipt lost cards");
        click("revealed-card",7);require(preview::GameTestAccess::inspecting(app),"Starter reward cannot be inspected");click("close-art");click("pack-continue");
      }
      load("shop");click("trade");require(command("shop "),"Buying a card did not reach the existing shop backend");
      load("tournaments","tournaments");commands.clear();click("tournament-action");click("confirm");require(command("tournament "),"Tournament registration unavailable");
      load("final-c0-r3");click("upgrade");require(command("tournament "),"Foil victory selection unavailable");require(commands.back().find(" foil ")!=std::string::npos,"Wrong reward command");
      load("final-c2-r3");click("leave-tournament");require(!command("tournament "),"Unused foil upgrades forfeited without confirmation");click("cancel");require(!command("tournament "),"Cancel forfeited upgrades");
      load("pack");auto identical=app.snapshot();app.receive(identical.dump());frame();click("pack-continue");frame();app.receive(identical.dump());frame();require(std::none_of(hits.begin(),hits.end(),[](const auto& h){return h.label=="pack-continue";}),"Dismissed pack reopened on identical snapshot");
      for(auto name:{"opening","multi-1","combo-1","final-c2-r3-animation"}){
        load(name);auto s=app.snapshot();commands.clear();for(int i=0;i<70;++i){if(i==3)app.receive(s.dump());frame(.1f);}int settled=0;for(const auto& c:commands)if(c.starts_with("settled "))++settled;require(settled==1,"Presentation did not acknowledge exactly once");
      }
      load("album","rules");std::ifstream rules(o.tesseraRoot/"TesseraUI/rules.json");preview::Json reference;rules>>reference;
      require(!reference.at("demos").contains("hidden"),"Hidden hands must not be a separate rule demonstration");
      require(preview::rulesLabel(0).empty(),"Hidden hands must not be displayed as a rule");
      for(const auto& page:reference.at("pages")){require(page.at("id")!="hidden","Hidden hands must not have a rules page");o.topic=page.at("id");o.page="rules";preview::GameTestAccess::page(app,o);frame();frame();}
      for(const auto& [topic,frames]:reference.at("demos").items()){
        if(topic=="open"||topic=="threeopen")continue;
        load("album","rules");o.topic=topic;preview::GameTestAccess::page(app,o);preview::GameTestAccess::demoStart(app);frame();frame();
        for(int step=0;step<int(frames.size())-1;++step){
          if(preview::GameTestAccess::demoStep(app)!=step)throw std::runtime_error("Tutorial "+topic+" expected step "+std::to_string(step)+" but reached "+std::to_string(preview::GameTestAccess::demoStep(app)));
          const int hand=preview::number(frames[step],"hand",-1),square=preview::number(frames[step],"square",-1);
          if(hand>=0){ // The selected demonstration card is drawn last, above the stack.
            int ordinal=0,selectedOrdinal=-1;
            for(const auto& hit:hits)if(hit.label=="hand"){
              const auto* window=ImGui::FindWindowByName(hit.window.c_str());const auto seed=ImHashData(&hand,sizeof(hand),window->ID);
              if(hit.id==ImHashStr("hand",0,seed))selectedOrdinal=ordinal;
              ++ordinal;
            }
            require(selectedOrdinal>=0,"Tutorial has no selectable card");click("hand",selectedOrdinal);
            if(!preview::GameTestAccess::demoSelected(app))throw std::runtime_error("Tutorial "+topic+" card selection failed at "+std::to_string(step)+", ordinal "+std::to_string(selectedOrdinal));
            click("square",square);
          }else click("demo-next");
          for(int i=0;i<45;++i)frame(.1f);
        }
        require(preview::GameTestAccess::demoStep(app)==int(frames.size())-1,"Tutorial did not finish");
      }
      for(const auto [width,height]:std::array<std::pair<int,int>,4>{{{1280,720},{1440,900},{3440,1440},{3840,2160}}}){
        io.DisplaySize=ImVec2(width,height);
        for(auto page:{"cards","decks","rules","appearance","match-appearance","journal","tournaments"}){load(page==std::string("tournaments")?"tournaments":"album",page);auto s=app.snapshot();for(int percentage:{60,95,100}){s["settings"]["windowSize"]=percentage;app.receive(s.dump());frame();frame();}}
        load("opening");for(int i=0;i<60;++i)frame(.1f);load("trade");frame();
        for(int theme=0;theme<5;++theme){auto s=ruleOffer();s["settings"]["theme"]=theme;app.receive(s.dump());frame();frame();
          const auto* popup=ImGui::GetTopMostPopupModal();require(popup&&ImRect(ImVec2(0,0),io.DisplaySize).Contains(popup->Rect()),"Rule dialogue leaves the viewport");
          for(const auto& h:hits)if(h.window=="Rule request")require(popup->Rect().Contains(h.bounds),"Rule dialogue clips a choice");}
      }
      io.DisplaySize={1600,1100};
      for(const auto& entry:std::filesystem::directory_iterator(argv[2]))if(entry.path().extension()==".json"){load(entry.path().stem().string());frame();}
      require(app.missingAssets()==0,"Native screens reference missing artwork");
    }
    ImGui::DestroyContext();std::cout<<"PASS: production native screens, five themes, input commands, settings modifiers, tournament victories, pack receipts and revision-bound animation acknowledgements ("<<frames<<" frames)\n";return 0;
 }catch(const std::exception& e){std::cerr<<"FAIL: "<<e.what()<<'\n';return 1;}
}
