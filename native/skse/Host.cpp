#include "Host.h"
#include "Keys.h"
#include "GameApp.h"
#include "RendererDX11.h"
#include "DXIsolation.h"
#include <imgui_impl_dx11.h>
#include <imgui_impl_win32.h>
#include <imgui_internal.h>
#include <windows.h>
#include <algorithm>
#include <chrono>
#include <filesystem>
#include <mutex>
#include <vector>

namespace preview::skyrim {
void registerMenu();
namespace {
namespace log=SKSE::log;
std::filesystem::path dataDirectory() {
    std::wstring file(32768,L'\0');
    const auto count=GetModuleFileNameW(nullptr,file.data(),DWORD(file.size()));
    if(!count || count==file.size())throw std::runtime_error("Cannot locate Skyrim's Data directory");
    file.resize(count);return std::filesystem::path(file).parent_path()/"Data";
}
struct ContextGuard {
    ImGuiContext* previous=ImGui::GetCurrentContext();
    ~ContextGuard() { ImGui::SetCurrentContext(previous); }
};
bool foreignMenu(RE::UI* ui) {
    if(!ui)return true;
    // Yield input to another active menu.
    const auto own=ui->GetMenu(menuName), cursor=ui->GetMenu(RE::CursorMenu::MENU_NAME), hud=ui->GetMenu(RE::HUDMenu::MENU_NAME);
    for(const auto& menu:ui->menuStack) {
        if(menu && menu.get()!=own.get() && menu.get()!=cursor.get() && menu.get()!=hud.get() && menu->UsesMenuContext())return true;
    }
    // Include external focus menus that leave Skyrim unpaused.
    return ui->IsMenuOpen("MeridianUI_FocusMenu") || ui->IsMenuOpen(RE::MainMenu::MENU_NAME) ||
        ui->IsMenuOpen(RE::LoadingMenu::MENU_NAME) || ui->IsMenuOpen(RE::DialogueMenu::MENU_NAME);
}
}
struct Host::State {
    Session session;
    Options options;
    std::atomic<bool> ownsFocus{false},pauseWorld{true},resetApp{false},escapeApp{false};
    std::mutex snapshotMutex;
    std::optional<std::string> snapshot;
    std::string lastSnapshot;
    std::function<void(const std::string&)> command;
    std::function<void()> interrupted;
    bool initialized=false,textAllowed=false,reopen=false;
    std::atomic<bool> closeQueued{false},clearInput{true},resetRenderer{false},wantsText{false},gamepad{false};
    std::atomic<HWND> window{nullptr};
    struct Input {
        enum Kind { key,character,mouse,wheel,analog,capture } kind;
        int code;float value;
        std::uint64_t token;
    };
    std::mutex inputMutex;
    std::vector<Input> input;
    struct Graphics {
        ImGuiContext* imgui=nullptr;
        ID3D11Device* device=nullptr;
        ID3D11DeviceContext* context=nullptr;
        HWND window=nullptr;
        bool win32=false,dx11=false;
        std::unique_ptr<DXIsolation> isolation;
        std::unique_ptr<Renderer> renderer;
        std::unique_ptr<GameApp> app;
        unsigned missing=0;
        double totalMs=0,maxMs=0;
        std::uint64_t frames=0;
        ~Graphics() {
            ContextGuard guard;
            if(guard.previous==imgui)guard.previous=nullptr;
            if(imgui)ImGui::SetCurrentContext(imgui);
            app.reset();renderer.reset();isolation.reset();
            if(dx11)ImGui_ImplDX11_Shutdown();
            if(win32)ImGui_ImplWin32_Shutdown();
            if(imgui)ImGui::DestroyContext(imgui);
        }
        void initialize(ID3D11Device* d,ID3D11DeviceContext* c,IDXGISwapChain* swap,const Options& options) {
            device=d;context=c;
            DXGI_SWAP_CHAIN_DESC desc{};
            if(FAILED(swap->GetDesc(&desc))||!desc.OutputWindow)throw std::runtime_error("Cannot locate Skyrim's window");
            window=desc.OutputWindow;
            isolation=std::make_unique<DXIsolation>(device,context);
            IMGUI_CHECKVERSION();imgui=ImGui::CreateContext();ImGui::SetCurrentContext(imgui);
            auto& io=ImGui::GetIO();
            io.ConfigFlags|=ImGuiConfigFlags_NoMouseCursorChange;
            io.MouseDrawCursor=false;io.IniFilename=nullptr;io.LogFilename=nullptr;
            io.ConfigErrorRecoveryEnableAssert=false;io.ConfigErrorRecoveryEnableTooltip=false;
            imgui->ErrorCallback=[](ImGuiContext*,void*,const char* text) { log::error("Native UI: {}",text); };
            win32=ImGui_ImplWin32_Init(window);
            if(!win32)throw std::runtime_error("Cannot initialize native window input");
            dx11=ImGui_ImplDX11_Init(device,context);
            if(!dx11)throw std::runtime_error("Cannot initialize native DirectX renderer");
            renderer=makeDXRenderer(device,context);
            app=std::make_unique<GameApp>(*renderer,options);
        }
    };
    // Never destroy graphics in DllMain/static teardown, after Skyrim's device
    // has gone away. This host lives until process exit. Resets run in PostDisplay.
    std::unique_ptr<Graphics> graphics;
};
Host::Host():state(std::make_unique<State>()) {}
Host::~Host()=default;
Host& Host::get() { static auto* host=new Host;return *host; }
void Host::initialize() {
    auto& s=*state;
    if(s.initialized)return;
    auto* input=RE::BSInputDeviceManager::GetSingleton();auto* ui=RE::UI::GetSingleton();
    if(!input||!ui) { log::error("Native UI: Skyrim UI/input unavailable at DataLoaded");return; }
    s.options.tesseraRoot=dataDirectory();
    s.options.previewRoot=s.options.tesseraRoot/"TesseraUI";
    s.options.hosted=true;
    registerMenu();input->AddEventSink<RE::InputEvent*>(this);ui->AddEventSink<RE::MenuOpenCloseEvent>(this);
    s.initialized=true;
    log::info("Native Tessera menu registered");
}
void Host::connect(std::function<void(const std::string&)> command,std::function<void()> interrupted){state->command=std::move(command);state->interrupted=std::move(interrupted);}
void Host::receive(std::string data){std::scoped_lock lock(state->snapshotMutex);state->lastSnapshot=data;state->snapshot=std::move(data);}
void Host::clear(){std::scoped_lock lock(state->snapshotMutex);state->snapshot.reset();state->lastSnapshot.clear();state->resetApp=true;}
void Host::escape(){state->escapeApp=true;}
bool Host::pauses()const{return state->pauseWorld;}
bool Host::focused()const{return state->ownsFocus;}
bool Host::anyFocus()const{return focused()||foreignMenu(RE::UI::GetSingleton());}
bool Host::focus(bool pause){
    auto& s=*state;if(!s.initialized)return false;
    s.pauseWorld=pause;s.ownsFocus=true;s.closeQueued=false;s.clearInput=true;
    if(s.session.phase()==Session::Phase::closing){s.reopen=true;return true;}
    if(s.session.phase()!=Session::Phase::hidden)return true;
    if(!s.session.open())return false;
    RE::UIMessageQueue::GetSingleton()->AddMessage(menuName,RE::UI_MESSAGE_TYPE::kShow,nullptr);return true;
}
void Host::unfocus(){state->ownsFocus=false;state->reopen=false;close("focus released");}
bool Host::hasFocus() const {
    auto& s=*state;const auto window=s.window.load();auto* ui=RE::UI::GetSingleton();
    return s.session.active() && (!window || GetForegroundWindow()==window) && ui &&
        ui->IsMenuOpen(menuName) && !foreignMenu(ui);
}
void Host::shown(){auto& s=*state;if(!s.session.shown()||!s.ownsFocus){RE::UIMessageQueue::GetSingleton()->AddMessage(menuName,RE::UI_MESSAGE_TYPE::kHide,nullptr);return;}s.clearInput=true;}
void Host::releaseTextInput() {
    auto& s=*state;
    if(s.textAllowed) { if(auto* map=RE::ControlMap::GetSingleton())map->AllowTextInput(false);s.textAllowed=false; }
    s.wantsText=false;
}
void Host::syncTextInput(std::uint64_t token) {
    auto& s=*state;
    if(!s.session.accepts(token))return;
    const bool wants=s.wantsText;
    if(wants!=s.textAllowed)if(auto* map=RE::ControlMap::GetSingleton()) {
        map->AllowTextInput(wants);s.textAllowed=wants;
    }
}
void Host::close(const char* reason) {
    auto& s=*state;
    const bool queue=s.session.close();releaseTextInput();s.clearInput=true;
    if(queue) {
        RE::UIMessageQueue::GetSingleton()->AddMessage(menuName,RE::UI_MESSAGE_TYPE::kHide,nullptr);
        log::info("Closing native UI: {}",reason);
    }
}
void Host::hidden(){
    auto& s=*state;const bool unexpected=s.ownsFocus&&!s.reopen;const bool reopen=s.reopen;s.reopen=false;
    s.session.hidden();releaseTextInput();s.clearInput=true;s.closeQueued=false;
    {std::scoped_lock lock(s.inputMutex);s.input.clear();}
    if(reopen){focus(s.pauseWorld);return;}
    if(unexpected){s.ownsFocus=false;if(s.interrupted)SKSE::GetTaskInterface()->AddTask(s.interrupted);}
}
void Host::reset(const char* reason) { close(reason);state->resetRenderer=true; }
void Host::queueClose(const char* reason) {
    auto& s=*state;
    if(s.closeQueued.exchange(true))return;
    const auto token=s.session.token();
    SKSE::GetTaskInterface()->AddTask([this,token,reason] { if(state->session.accepts(token)){if(state->interrupted)state->interrupted();else unfocus();} });
}
void Host::draw() {
    auto& s=*state;
    if(!s.session.active() || s.closeQueued)return;
    if(!hasFocus()) { queueClose("focus or menu changed");return; }
    ContextGuard contextGuard;
    bool frameStarted=false;
    const auto start=std::chrono::steady_clock::now();
    try {
        auto* engine=RE::BSGraphics::Renderer::GetSingleton();
        if(!engine)throw std::runtime_error("Skyrim renderer unavailable");
        auto& data=engine->GetRuntimeData();
        auto* device=reinterpret_cast<ID3D11Device*>(data.forwarder);
        auto* context=reinterpret_cast<ID3D11DeviceContext*>(data.context);
        auto* swap=reinterpret_cast<IDXGISwapChain*>(data.renderWindows[0].swapChain);
        if(!device||!context||!swap)throw std::runtime_error("Skyrim DirectX device unavailable");
        if(s.resetRenderer.exchange(false)||(s.graphics&&s.graphics->device!=device))s.graphics.reset();
        if(!s.graphics) {
            s.graphics=std::make_unique<State::Graphics>();
            s.graphics->initialize(device,context,swap,s.options);s.window=s.graphics->window;
            s.resetApp=true;
            {std::scoped_lock lock(s.snapshotMutex);if(!s.lastSnapshot.empty())s.snapshot=s.lastSnapshot;}
            s.graphics->app->connect([&s](const std::string& command){if(s.command)s.command(command);});
            log::info("Native renderer ready; DirectX 11, isolated context, no browser");
        }
        auto& g=*s.graphics;ImGui::SetCurrentContext(g.imgui);
        const auto token=s.session.token();auto& io=ImGui::GetIO();
        DXIsolation::Frame isolated(*g.isolation);
        if(s.clearInput.exchange(false)) {
            io.ClearInputKeys();io.ClearInputMouse();io.ClearEventsQueue();
            g.app->resume();
        }
        if(s.resetApp.exchange(false))g.app->reset();
        std::optional<std::string> snapshot;
        {std::scoped_lock lock(s.snapshotMutex);snapshot.swap(s.snapshot);}
        if(snapshot)g.app->receive(*snapshot);
        if(s.escapeApp.exchange(false))g.app->escape();
        ImGui_ImplDX11_NewFrame();ImGui_ImplWin32_NewFrame();
        const auto size=isolated.targetSize();
        if(io.DisplaySize.x<=0||io.DisplaySize.y<=0)return;
        if(size.x>0&&size.y>0)io.DisplayFramebufferScale={size.x/io.DisplaySize.x,size.y/io.DisplaySize.y};
        // CursorMenu provides the coordinates Skyrim displays. Win32's OS cursor
        // can be locked at screen center while Skyrim uses a software cursor.
        if(auto* cursor=RE::MenuCursor::GetSingleton()) {
            const auto& c=cursor->GetRuntimeData();
            const float sx=c.screenWidthX>0?io.DisplaySize.x/c.screenWidthX:1.f;
            const float sy=c.screenWidthY>0?io.DisplaySize.y/c.screenWidthY:1.f;
            io.AddMousePosEvent(c.cursorPosX*sx,c.cursorPosY*sy);
        }
        io.AddFocusEvent(true);
        io.AddKeyEvent(ImGuiMod_Ctrl,(GetAsyncKeyState(VK_CONTROL)&0x8000)!=0);
        io.AddKeyEvent(ImGuiMod_Shift,(GetAsyncKeyState(VK_SHIFT)&0x8000)!=0);
        io.AddKeyEvent(ImGuiMod_Alt,(GetAsyncKeyState(VK_MENU)&0x8000)!=0);
        if(s.gamepad)io.BackendFlags|=ImGuiBackendFlags_HasGamepad;else io.BackendFlags&=~ImGuiBackendFlags_HasGamepad;
        std::vector<State::Input> input;
        { std::scoped_lock lock(s.inputMutex);input.swap(s.input); }
        for(const auto& event:input) {
            if(event.token!=token)continue;
            switch(event.kind) {
            case State::Input::capture:g.app->captureKey(unsigned(event.code),unsigned(event.value));break;
            case State::Input::key:io.AddKeyEvent(ImGuiKey(event.code),event.value>0);break;
            case State::Input::analog:io.AddKeyAnalogEvent(ImGuiKey(event.code),event.value>.1f,event.value);break;
            case State::Input::mouse:io.AddMouseButtonEvent(event.code,event.value>0);break;
            case State::Input::wheel:io.AddMouseWheelEvent(0,event.value);break;
            case State::Input::character:
                if(event.code<=0xffff)io.AddInputCharacterUTF16(ImWchar16(event.code));else io.AddInputCharacter(unsigned(event.code));break;
            }
        }
        ImGui::NewFrame();frameStarted=true;
        g.app->draw();
        if(s.wantsText.exchange(io.WantTextInput)!=io.WantTextInput)
            SKSE::GetTaskInterface()->AddTask([this,token] { syncTextInput(token); });
        if(g.app->closeRequested())queueClose("back/close button");
        ImGui::Render();frameStarted=false;
        isolated.bindTarget();ImGui_ImplDX11_RenderDrawData(ImGui::GetDrawData());
        const auto ms=std::chrono::duration<double,std::milli>(std::chrono::steady_clock::now()-start).count();
        ++g.frames;g.totalMs+=ms;g.maxMs=std::max(g.maxMs,ms);
        if(g.frames==1)log::info("First native frame: {:.1f} ms (includes initialization/art decode)",ms);
        if(g.frames%600==0)log::debug("Native UI: {} frames, {:.2f} ms mean CPU, {:.1f} ms peak; {} missing assets",g.frames,g.totalMs/g.frames,g.maxMs,g.app->missingAssets());
        if(g.app->missingAssets()!=g.missing) {
            g.missing=g.app->missingAssets();log::warn("Native UI missing {} art files; check Tessera's Card Art installation",g.missing);
        }
    } catch(const std::exception& error) {
        if(frameStarted && ImGui::GetCurrentContext() && ImGui::GetCurrentContext()->WithinFrameScope)ImGui::EndFrame();
        log::error("Native UI stopped drawing: {}",error.what());
        s.resetRenderer=true;queueClose("renderer error (see TTCG.log)");
    }
}
RE::BSEventNotifyControl Host::ProcessEvent(RE::InputEvent* const* events,RE::BSTEventSource<RE::InputEvent*>*) {
    auto& s=*state;
    if(!events)return RE::BSEventNotifyControl::kContinue;
    auto* manager=RE::BSInputDeviceManager::GetSingleton();auto* keyboard=manager?manager->GetKeyboard():nullptr;
    const auto pressed=[&](unsigned code) { return keyboard && (keyboard->GetRuntimeData().curState[code]&0x80)!=0; };
    const unsigned modifiers=ttcg::heldHotkeyModifiers(pressed);
    if(s.session.active()&&!hasFocus())queueClose("input focus changed");
    const bool focused=hasFocus();const auto token=s.session.token();
    if(manager)s.gamepad=manager->IsGamepadEnabled();
    std::vector<State::Input> batch;
    const auto push=[&](State::Input::Kind kind,int code,float value) { batch.push_back({kind,code,value,token}); };
    for(auto* event=*events;event;event=event->next) {
        auto* button=event->AsButtonEvent();
        if(!focused)continue;
        if(auto* character=event->AsCharEvent()) {
            if(character->keyCode>=32 && character->keyCode!=127 && character->keyCode<=0x10ffff)
                push(State::Input::character,int(character->keyCode),0);
        } else if(button) {
            const auto code=button->GetIDCode();
            if(button->GetDevice()==RE::INPUT_DEVICE::kGamepad) {
                const auto key=controllerKey(code);if(key!=ImGuiKey_None)push(State::Input::analog,key,std::clamp(button->Value(),0.f,1.f));
            } else if(button->IsDown()||button->IsUp()) {
                if(button->GetDevice()==RE::INPUT_DEVICE::kKeyboard) {
                    if(button->IsDown())push(State::Input::capture,int(code),float(modifiers));
                    const auto key=keyboardKey(code);if(key!=ImGuiKey_None)push(State::Input::key,key,button->IsDown()?1.f:0.f);
                } else if(button->GetDevice()==RE::INPUT_DEVICE::kMouse) {
                    if(code<5)push(State::Input::mouse,int(code),button->IsDown()?1.f:0.f);
                    if(button->IsDown() && (code==8||code==9))push(State::Input::wheel,0,(code==8?1.f:-1.f)*std::max(1.f,button->Value()));
                }
            }
        } else if(auto* stick=event->AsThumbstickEvent()) {
            const auto analog=[&](ImGuiKey key,float v) { push(State::Input::analog,key,std::clamp((v-.2f)/.8f,0.f,1.f)); };
            const bool left=stick->IsLeft();
            analog(left?ImGuiKey_GamepadLStickLeft:ImGuiKey_GamepadRStickLeft,-stick->xValue);
            analog(left?ImGuiKey_GamepadLStickRight:ImGuiKey_GamepadRStickRight,stick->xValue);
            analog(left?ImGuiKey_GamepadLStickUp:ImGuiKey_GamepadRStickUp,stick->yValue);
            analog(left?ImGuiKey_GamepadLStickDown:ImGuiKey_GamepadRStickDown,-stick->yValue);
        }
    }
    if(!batch.empty()) {
        std::scoped_lock lock(s.inputMutex);
        if(s.input.size()+batch.size()<=512)s.input.insert(s.input.end(),batch.begin(),batch.end());
        else { s.input.clear();s.clearInput=true; }
    }
    return RE::BSEventNotifyControl::kContinue;
}
RE::BSEventNotifyControl Host::ProcessEvent(const RE::MenuOpenCloseEvent* event,RE::BSTEventSource<RE::MenuOpenCloseEvent>*) {
    if(event && event->opening && event->menuName!=menuName && event->menuName!=RE::CursorMenu::MENU_NAME &&
       state->session.active() && foreignMenu(RE::UI::GetSingleton()))queueClose("another menu opened");
    return RE::BSEventNotifyControl::kContinue;
}
}
