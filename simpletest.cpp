#include <iostream>

#include "csplib.hpp"

using namespace csplib;

class IntState : public Actor::ActorState {
public:
    int value;
    IntState(int v) : value(v) {}

    IntState *clone() const { return new IntState(value); }
};

class CreateEvent : public Event {
public:
    CreateEvent(Timestamp when, ActorID target) : Event(when, target) {}

    virtual bool apply(Stage &stage) {
        std::cout << "CREATE intState(0)\n";
        if(stage.get(target())) return false;  // abort if already exists
        stage.add(new Actor(target(), new IntState(0)));
        return true;
    }
};

class IncrementEvent : public StateSpecificEvent<IntState> {
public:
    IncrementEvent(Timestamp when, ActorID target)
        : StateSpecificEvent<IntState>(when, target) {}

    virtual bool apply(Stage &, IntState *state) {
        state->value ++;
        return true;
    }
};

class DoubleEvent : public StateSpecificEvent<IntState> {
public:
    DoubleEvent(Timestamp when, ActorID target)
        : StateSpecificEvent<IntState>(when, target) {}

    virtual bool apply(Stage &, IntState *state) {
        state->value *= 2;
        return true;
    }
};

int main() {
    csplib::Timeline timeline;

    timeline.add(new CreateEvent(Timestamp(1005), 100));
    timeline.add(new CreateEvent(Timestamp(1006), 101));
    timeline.add(new DoubleEvent(Timestamp(1008), 101));

    auto is = dynamic_cast<const IntState *>(timeline.stage().get(101)->state());
    std::cout << "Before rollback & increment: " << is->value << std::endl;

    // insert increment in between creation and doubling events
    timeline.add(new IncrementEvent(Timestamp(1007), 101));

    is = dynamic_cast<const IntState *>(timeline.stage().get(101)->state());
    std::cout << "After rollback & increment: " << is->value << std::endl;

    timeline.snapshotAt(Timestamp(1010));
    timeline.snapshotAt(Timestamp(1020));
    timeline.snapshotAt(Timestamp(1030));

    timeline.add(new IncrementEvent(Timestamp(1009), 100));

    is = dynamic_cast<const IntState *>(timeline.stage().get(100)->state());
    std::cout << "After rollback & increment (100): " << is->value << std::endl;

    return 0;
}
