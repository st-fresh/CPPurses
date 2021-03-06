#include <cppurses/system/system.hpp>

#include <algorithm>
#include <iterator>
#include <memory>
#include <mutex>
#include <stdexcept>
#include <thread>
#include <utility>
#include <vector>

#include <signals/slot.hpp>

#include <cppurses/painter/palette.hpp>
#include <cppurses/system/animation_engine.hpp>
#include <cppurses/system/detail/event_engine.hpp>
#include <cppurses/system/detail/event_queue.hpp>
#include <cppurses/system/detail/user_input_event_loop.hpp>
#include <cppurses/system/event.hpp>
#include <cppurses/system/event_loop.hpp>
#include <cppurses/system/events/focus_event.hpp>
#include <cppurses/system/events/resize_event.hpp>
#include <cppurses/system/focus.hpp>
#include <cppurses/system/system.hpp>
#include <cppurses/terminal/terminal.hpp>
#include <cppurses/widget/area.hpp>
#include <cppurses/widget/widget.hpp>

namespace cppurses {

sig::Slot<void()> System::quit = []() { System::exit(); };
sig::Signal<void(int)> System::exit_signal{};
Widget* System::head_{nullptr};
Widget* System::initial_focus_{nullptr};
bool System::exit_requested_{false};
detail::User_input_event_loop System::user_input_loop_;
Animation_engine System::animation_engine_;
Terminal System::terminal;

void System::post_event(std::unique_ptr<Event> event)
{
    detail::Event_engine::get().queue().append(std::move(event));
}

void System::exit(int exit_code)
{
    System::exit_requested_ = true;
    System::exit_signal(exit_code);
}

System::~System() { System::exit(0); }

void System::set_head(Widget* new_head)
{
    if (head_ != nullptr)
        head_->disable();
    head_ = new_head;
    if (head_ != nullptr) {
        head_->enable();
        post_event<Resize_event>(*head_,
                                 Area{terminal.width(), terminal.height()});
    }
}

int System::run(Widget& head)
{
    System::set_head(&head);
    return this->run();
}

int System::run()
{
    if (System::head() == nullptr)
        return -1;
    if (initial_focus_ != nullptr) {
        initial_focus_->enable(true, false);
        Focus::set_focus_to(*initial_focus_);
        System::send_event(Focus_in_event{*initial_focus_});
    }
    terminal.initialize();
    System::post_event<Resize_event>(*System::head(),
                                     Area{terminal.width(), terminal.height()});
    const auto exit_code = user_input_loop_.run();
    terminal.uninitialize();
    return exit_code;
}

}  // namespace cppurses
