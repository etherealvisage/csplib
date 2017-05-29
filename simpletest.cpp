#include <iostream>

#include "csplib.hpp"

using namespace csplib;

class IntState : public ActorState {
public:
    int value;
    IntState(int v) : value(v) {}

    IntState *clone() const { return new IntState(value); }
};

class CreateEvent : public Event {
public:
    CreateEvent(Timestamp when, ActorID target) : Event(when, target) {}

    virtual bool apply(Stage &stage) {
        // can't create if already exists
        if(stage.find(target())) return false;

        stage.setMapping(target(), new IntState(0));

        return true;
    }
};

class IncrementEvent : public Event {
public:
    IncrementEvent(Timestamp when, ActorID target) : Event(when, target) {}

    virtual bool apply(Stage &stage) {
        auto target = stage.find(this->target());
        // needs to exist
        if(!target) return false;
        // needs to be IntState
        auto is = dynamic_cast<IntState *>(target);
        if(!is) return false;

        is->value ++;
        return true;
    }
};

class DoubleEvent : public Event {
public:
    DoubleEvent(Timestamp when, ActorID target) : Event(when, target) {}

    virtual bool apply(Stage &stage) {
        auto target = stage.find(this->target());
        // needs to exist
        if(!target) return false;
        // needs to be IntState
        auto is = dynamic_cast<IntState *>(target);
        if(!is) return false;

        is->value *= 2;

        return true;
    }
};

int main() {
    csplib::Timeline timeline;

    timeline.add(new CreateEvent(Timestamp(1005), 100));
    timeline.add(new CreateEvent(Timestamp(1006), 101));
    timeline.add(new DoubleEvent(Timestamp(1008), 101));

    auto is = dynamic_cast<const IntState *>(timeline.latest().stage().find(101));
    std::cout << "Before rollback & increment: " << is->value << std::endl;

    // insert increment in between creation and doubling events
    timeline.add(new IncrementEvent(Timestamp(1007), 101));

    is = dynamic_cast<const IntState *>(timeline.latest().stage().find(101));
    std::cout << "After rollback & increment: " << is->value << std::endl;

    timeline.makeNewSnapshot(Timestamp(1010));
    timeline.makeNewSnapshot(Timestamp(1020));
    timeline.makeNewSnapshot(Timestamp(1030));

    timeline.add(new IncrementEvent(Timestamp(1009), 100));

    is = dynamic_cast<const IntState *>(timeline.latest().stage().find(100));
    std::cout << "After rollback & increment (100): " << is->value << std::endl;

    return 0;
}
