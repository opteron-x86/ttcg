#pragma once
#include <RE/Skyrim.h>
#include <SKSE/SKSE.h>
#include "Session.h"
#include <atomic>
#include <memory>
#include <functional>
#include <string>
namespace preview::skyrim {
inline constexpr char menuName[]="TesseraMenu";
class Host final:public RE::BSTEventSink<RE::InputEvent*>,public RE::BSTEventSink<RE::MenuOpenCloseEvent>{
    struct State;std::unique_ptr<State> state;Host();~Host()override;
    bool hasFocus()const;void queueClose(const char*);void releaseTextInput();void syncTextInput(std::uint64_t);
public:
    static Host& get();void initialize();void shown();void hidden();void close(const char*);void reset(const char*);void draw();
    void connect(std::function<void(const std::string&)>,std::function<void()>);
    void receive(std::string);void clear();void escape();bool focus(bool pause);void unfocus();bool focused()const;bool anyFocus()const;bool pauses()const;
    RE::BSEventNotifyControl ProcessEvent(RE::InputEvent* const*,RE::BSTEventSource<RE::InputEvent*>*)override;
    RE::BSEventNotifyControl ProcessEvent(const RE::MenuOpenCloseEvent*,RE::BSTEventSource<RE::MenuOpenCloseEvent>*)override;
};
}
