/*
    Copyright (c) 2017, David Williams-King & Kent "ethereal" Williams-King
    All rights reserved.

    Redistribution and use in source and binary forms, with or without
    modification, are permitted provided that the following conditions are met:

    * Redistributions of source code must retain the above copyright notice,
      this list of conditions and the following disclaimer.

    * Redistributions in binary form must reproduce the above copyright notice,
      this list of conditions and the following disclaimer in the documentation
      and/or other materials provided with the distribution.

    * Neither the name of the copyright holder nor the names of its
      contributors may be used to endorse or promote products derived from
      this software without specific prior written permission.

    THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
    AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
    IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
    ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS BE
    LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
    CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
    SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
    INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
    CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
    ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
    POSSIBILITY OF SUCH DAMAGE.
*/

#ifndef CSPLIB_HPP
#define CSPLIB_HPP

#include <stdint.h>

#include <functional>
#include <vector>
#include <algorithm>
#include <map>

namespace csplib {

class Actor {
public:
    typedef uint64_t ActorID;

    class ActorState {
    public:
        virtual ~ActorState() {}

        virtual ActorState *clone() const = 0;
    };
private:
    ActorID m_id;
    ActorState *m_state;
public:
    Actor(ActorID id, ActorState *state) : m_id(id), m_state(state) {}
    ~Actor() { delete m_state; }

    ActorID id() const { return m_id; }
    ActorState *state() { return m_state; }
    const ActorState *state() const { return m_state; }

    Actor *clone() const {
        return new Actor(m_id, m_state->clone());
    }
};
typedef Actor::ActorID ActorID;  // bring into namespace

/** Represents a list of currently active Actors. A Stage may be snapshotted,
    so every copy of a Stage is a deep copy.
*/
class Stage {
private:
    std::map<ActorID, Actor *> m_actors;
public:
    Stage() {}
    explicit Stage(const Stage &other) {
        for(auto it : other.m_actors) {
            add(it.second->clone());
        }
    }
    Stage(Stage &&other) : m_actors(std::move(other.m_actors)) {}
    ~Stage() { for(auto it : m_actors) delete it.second; }

    Stage &operator = (const Stage &other) = delete;
    Stage &operator = (Stage &&other) = default;

    void add(Actor *actor) { m_actors[actor->id()] = actor; }
    void remove(ActorID id) { m_actors.erase(m_actors.find(id)); }
    Actor *get(ActorID id) {
        auto it = m_actors.find(id);
        return (it != m_actors.end() ? (*it).second : nullptr);
    }
    const Actor *get(ActorID id) const {
        auto it = m_actors.find(id);
        return (it != m_actors.end() ? (*it).second : nullptr);
    }
    size_t size() const { return m_actors.size(); }
};

class Timestamp {
private:
    uint64_t m_raw;
public:
    Timestamp(uint64_t raw) : m_raw(raw) {}

    bool operator < (const Timestamp &other) const {
        return m_raw < other.m_raw;
    }
    bool operator == (const Timestamp &other) const {
        return m_raw == other.m_raw;
    }
    uint64_t raw() const { return m_raw; }

    static Timestamp makeZero() {
        return Timestamp(0);
    }
};

class Event {
private:
    Timestamp m_when;
    ActorID m_target;
public:
    Event(Timestamp when, const ActorID &target)
        : m_when(when), m_target(target) {}
    virtual ~Event() {}

    const Timestamp &when() const { return m_when; }
    const ActorID &target() const { return m_target; }

    virtual bool apply(Stage &stage) = 0;

    virtual bool operator < (const Event &other) const {
        return m_when < other.m_when;
    }
    virtual bool operator == (const Event &other) const {
        return m_when == other.m_when;
    }
};

template <typename StateType, typename BaseEvent = Event>
class StateSpecificEvent : public BaseEvent {
public:
    using BaseEvent::BaseEvent;

    virtual bool apply(Stage &stage) {
        auto target = stage.get(this->target());
        if(!target) return false;  // target Actor does not exist
        auto specific = dynamic_cast<StateType *>(target->state());
        if(!specific) return false;  // Actor has wrong State type

        return apply(stage, specific);
    }
    virtual bool apply(Stage &stage, StateType *state) = 0;
};

class CallbackEvent : public Event {
public:
    // the wrapped event was invoked on this actor and returned this bool
    typedef std::function<void (ActorID, bool)> Callback;
private:
    Event *m_wrapped;
    bool m_lastValue;
    bool m_first;
    Callback m_callback;
public:
    CallbackEvent(Event *wrapped, Callback callback)
        : Event(wrapped->when(), wrapped->target()), m_wrapped(wrapped),
        m_first(true), m_callback(callback) {}

    virtual bool apply(Stage &stage) {
        bool value = m_wrapped->apply(stage);
        if(m_first || value != m_lastValue) {
            m_callback(m_wrapped->target(), value);
        }
        m_first = false;
        m_lastValue = value;

        return true; // this event always succeeds
    }
};

class StageSnapshot {
private:
    Stage m_stage;
    std::vector<Event *> m_events;
    Timestamp m_begin;
public:
    StageSnapshot(Timestamp begin) : m_begin(begin) {}
    StageSnapshot(Timestamp begin, const Stage &stage) :
        m_stage(stage), m_begin(begin) {}

    Stage &stage() { return m_stage; }
    const Stage &stage() const { return m_stage; }
    const Timestamp &begin() const { return m_begin; }

    /** Returns true if this event was added later than all others. */
    bool add(Event *event) {
        auto it = std::lower_bound(m_events.begin(), m_events.end(), event,
            [] (Event *a, Event *b) { return *a < *b; });
        m_events.insert(it, event);
        return it == m_events.end();
    }

    void setStage(Stage &&stage) { m_stage = std::move(stage); }

    const std::vector<Event *> &events() const { return m_events; }
};

class Timeline {
private:
    std::vector<StageSnapshot> m_snapshots;
    Stage m_stage;
public:
    Timeline() {
        // first snapshot is so old, it's before everything
        m_snapshots.push_back(StageSnapshot(Timestamp::makeZero()));
    }

    Stage &stage() { return m_stage; }
    const Stage &stage() const { return m_stage; }

    bool add(Event *event) {
        int closestIndex = indexOf(event);
        if(closestIndex == -1) return false;  // older than oldest snapshot

        // insert into snapshot
        bool lastAdded = m_snapshots[closestIndex].add(event);

        if(closestIndex + 1u == m_snapshots.size() && lastAdded) {
            // no rewriting of history
            event->apply(m_stage);
        }
        else {
            // reassemble all succeeding snapshots
            for(unsigned i = closestIndex; i < m_snapshots.size(); i ++) {
                auto stage = Stage(m_snapshots[i].stage());  // deep copy
                for(auto event : m_snapshots[i].events()) {
                    event->apply(stage);
                }
                if(i + 1 < m_snapshots.size()) {
                    // save results in next snapshot's base Stage
                    m_snapshots[i+1].setStage(std::move(stage));
                }
                else {
                    // save results in our latest Stage
                    m_stage = std::move(stage);
                }
            }
        }

        return true;
    }

    void snapshotAt(Timestamp now) {
        // push on copy of current snapshot
        m_snapshots.push_back(StageSnapshot(now, m_stage));
    }

    void limitSnapshots(int count) {
        if(count < 1) count = 1;
        if(m_snapshots.size() <= (unsigned)count) return;
        int delta = m_snapshots.size() - count;
        m_snapshots.erase(m_snapshots.begin(), m_snapshots.begin()+delta);
    }
private:
    int indexOf(Event *event) const {
        int size = static_cast<int>(m_snapshots.size());
        // linear search for now
        for(int i = 0; i < size; i ++) {
            if(event->when() < m_snapshots[i].begin()
                /*|| event->when() == m_snapshots[i].begin()*/) {

                return i - 1;
            }
        }
        return size - 1;
    }

    StageSnapshot &latest() { return m_snapshots.back(); }
};

} // namespace csplib

#endif
