#include <iostream>

#include "csplib.hpp"

using namespace csplib;

class IntState : public ActorState {
public:
    IntState(int v) : value(v) {}
    int value;

    IntState *clone() const {
        return new IntState(value);
    }
};

class CreateEvent : public Event {
public:
    CreateEvent(Timestamp when, ActorID target) : Event(when, target) {}

    virtual bool apply(ActorRegistry &registry) {
        // can't create if already exists
        if(registry.find(target())) return false;

        registry.setMapping(target(), new IntState(0));

        return true;
    }
};

class IncrementEvent : public Event {
public:
    IncrementEvent(Timestamp when, ActorID target) : Event(when, target) {}

    virtual bool apply(ActorRegistry &registry) {
        auto target = registry.find(this->target());
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

    virtual bool apply(ActorRegistry &registry) {
        auto target = registry.find(this->target());
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
    csplib::Stage stage;

    stage.accept(new CreateEvent(Timestamp(1005), 100));
    stage.accept(new CreateEvent(Timestamp(1006), 101));
    stage.accept(new DoubleEvent(Timestamp(1008), 101));

    auto is = dynamic_cast<const IntState *>(stage.latest().registry().find(101));
    std::cout << "Before rollback & increment: " << is->value << std::endl;

    // insert increment in between creation and doubling events
    stage.accept(new IncrementEvent(Timestamp(1007), 101));

    is = dynamic_cast<const IntState *>(stage.latest().registry().find(101));
    std::cout << "After rollback & increment: " << is->value << std::endl;

    stage.makeNewSnapshot(Timestamp(1010));
    stage.makeNewSnapshot(Timestamp(1020));
    stage.makeNewSnapshot(Timestamp(1030));

    stage.accept(new IncrementEvent(Timestamp(1009), 100));

    is = dynamic_cast<const IntState *>(stage.latest().registry().find(100));
    std::cout << "After rollback & increment (100): " << is->value << std::endl;

    return 0;
}
