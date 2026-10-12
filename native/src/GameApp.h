#pragma once
#include "App.h"
#include "GameState.h"
#include <set>
namespace preview {
class GameApp final : public App {
    friend struct GameTestAccess;
    Json state,reference;
    std::function<void(const std::string&)> submit;
    Presentation presentation,demoPresentation;
    Json demoAnimation;std::string demoIdentity;
    int tab=0,settingsPage=0,appearancePage=0,journalHold=-1,journalPlayer=-1,journalFilter=0;
    int previewOwner=1;double previewCapturedAt=-1;
    int shopTab=0,shopSelected=-1,shopQuantity=1,shopTier=0,shopRarity=0,shopSurplus=1;
    unsigned eventID=0,foilSelection=0,claimMask=0;bool history=false,showRuleReference=false;
    std::set<unsigned> seenEvents;
    std::string topic="play";int demoStep=0;bool demoSelected=false;double demoChanged=0;
    bool pendingCommand=false,ruleAnswerPending=false,escapeRequested=false,showTableDetails=false,previewBack=false;
    double sentAt=0;std::uint64_t seenReveal=0, dismissedReveal=0;
    struct TradeView {
        enum Phase {None,Gathering,Selecting,Reviewing,Transferring,Done} phase=None;
        double started=0;
        int inspected=5;
        std::array<ImVec2,10> sources{},sizes{};
        unsigned sourceMask=0;
    } trade;
    int wager=0,groupHand=-1,groupSquare=-1;std::vector<int> groupChoices;
    std::vector<int> inspectionList;
    int inspectionModifier=0;
    char journalQuery[128]{},shopQuery[128]{};
    std::string confirmationTitle,confirmationCopy,confirmationLabel="Confirm",confirmationVerb,confirmationArgs;
    std::uint64_t confirmSession=0,confirmRevision=0;
    ImVec2 frameAt{},frameSize{};
    std::array<ImFont*,5> interfaceFonts{},nameFonts{},valueFonts{};
    bool sourceReady=false;
    void send(std::string_view verb,std::string args={},bool exclusive=true);
    void request(Json command) override;
    void closeView();
    void applySettings();
    void confirm(std::string title,std::string copy,std::string verb,std::string args={},std::string action="Confirm");
    void confirmation();
    void header(ImVec2 at,ImVec2 size);
    bool chapter(const std::string& id,const std::string& title,ImVec2 at,ImVec2 size,bool active,const std::string& detail={},float trailing=0,int marker=0);
    void textBlock(const std::string&,float width,float pixels=19,ImU32 colour=0);
    bool combo(const char* id,const char* label,int& value,const std::vector<std::string>& choices,float width);
    void displayCard(int display,ImVec2 at,ImVec2 size,bool chosen=false,int owner=-1,int slot=0,int modifier=0,bool large=false,CardContext context=CardContext::General);
    void inspectCard(int display,bool metadata=true,int modifier=0) override;
    float metadata(const CardView& card,ImVec2 at,float width,bool draw=true);
    float ruleChips(ImVec2 at,float width,unsigned flags,int trade=-1,bool casual=false,bool interactive=false,const Json* changes=nullptr,bool centred=false);
    const Json* tournament(unsigned id)const;
    void journal(ImVec2 at,ImVec2 size);
    void tournaments(ImVec2 at,ImVec2 size);
    void tournamentOutcome();
    void shop(ImVec2 at,ImVec2 size);
    void settings(ImVec2 at,ImVec2 size);
    void matchAppearancePreview(ImVec2 at,ImVec2 size);
    void lobby(ImVec2 at,ImVec2 size);
    bool ruleRequestModal();
    void matchPanel(ImVec2 at,ImVec2 size);
    std::pair<ImVec2,ImVec2> table(const Json& data,ImVec2 at,ImVec2 size,bool demo=false,bool liveLesson=false);
    void tickPresentation();
    void ruleSounds(const Json& data,Presentation& clock);
    void place(int hand,int square);
    void groupModal();
    void resultModal();
    void rules(ImVec2 at,ImVec2 size);
    void rulesModal();
    void lessonPanel(ImVec2 at,ImVec2 size);
    void nativeInspector();
    void inspect() override { nativeInspector(); }
    void takeTrade();
    void nativeAlbum(ImVec2 at,ImVec2 size);
    void nativeDecks(ImVec2 at,ImVec2 size);
    void nativeFilters(ImVec2 at,float width,bool deck=false);
    bool filtersOpen=false;
    Filter appliedFilter;bool filterDirty=true;
    Filter cardFilter,deckFilter;int filterPage=-1;
    void pickerFilters(ImVec2 at,float width) override {nativeFilters(at,width,true);}
    void drawDetail(ImVec2 at,ImVec2 size) override;
public:
    GameApp(Renderer&,const Options&);
    void connect(std::function<void(const std::string&)> handler){submit=std::move(handler);}
    void receive(std::string_view);
    void reset();
    void escape(){escapeRequested=true;}
    void draw() override;
    void captureKey(unsigned scan,unsigned modifiers);
    bool capturing()const{return !label(member(state,"settings"),"capturing").empty();}
    const Json& snapshot()const{return state;}
};
}
