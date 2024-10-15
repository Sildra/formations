#include <iostream>
#include <chrono>
#include <string>

#include "event_runner.h"

// UTILS
static int global_id = 0;
const std::string get_id()
{
    thread_local const std::string id = std::string("Thread ").append(std::to_string(global_id++));
    return id;
}

void display(const std::string& val)
{
    std::string value = get_id();
    value.append(" ").append(val).append("\n");
    std::cout << value;
}

struct Event1 : public event_runner::IEvent { mutable bool eventRan = false; };
struct Event2 : public event_runner::IEvent { mutable bool eventRan = false; };

void runEvent1(const event_runner::IEvent& event) {
    if (auto* event1 = dynamic_cast<const Event1*>(&event)) {
        display("Event1 ran");
        event1->eventRan = true;
    } else {
        display("Event1 not eligible");
    }
}

void runEvent2(const event_runner::IEvent& event) {
    if (auto* event2 = dynamic_cast<const Event2*>(&event)) {
        display("Event2 ran");
        event2->eventRan = true;
    }
    else {
        display("Event2 not eligible");
    }
}

struct Application {
    event_runner::ThreadPool<runEvent1> eventRunner1 { 1 };
    event_runner::ThreadPool<runEvent2> eventRunner2 { 1 };
    void notifyAll(const event_runner::SharedEvent& event) {
        eventRunner1.schedule(event);
        eventRunner2.schedule(event);
    }
};

// TESTS
int main()
{
    std::cout << "EventRunner\n";
    {
        Application app;
        auto event1 = std::make_shared<Event1>();
        auto event2 = std::make_shared<Event2>();
        app.notifyAll(event1);
        app.notifyAll(event2);
        while (!event1->eventRan && event2->eventRan) ;
    }
    std::cout << "End ThreadPool\n";
    return 0;
}
