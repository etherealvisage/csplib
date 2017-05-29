#ifndef CSPLIB_HPP
#define CSPLIB_HPP

#include <stdint.h>

#include <functional>
#include <vector>
#include <algorithm>
#include <map>

namespace csplib {

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

    static Timestamp makeZero() {
        return Timestamp(0);
    }
};

typedef uint64_t ActorID;

class ActorState {
private:
    ActorID m_actorID;
public:
    virtual ~ActorState() {}

    const ActorID &actorID() const { return m_actorID; }

    virtual ActorState *clone() const = 0;
};

class Stage {
private:
    std::map<ActorID, ActorState *> m_actors;
public:
    void setMapping(ActorID id, ActorState *actor) { m_actors[id] = actor; }
    void clearMapping(ActorID id) { m_actors.erase(m_actors.find(id)); }
    ActorState *find(ActorID id) {
        auto it = m_actors.find(id);
        return (it != m_actors.end() ? (*it).second : nullptr);
    }
    const ActorState *find(ActorID id) const {
        auto it = m_actors.find(id);
        return (it != m_actors.end() ? (*it).second : nullptr);
    }

    Stage clone() const {
        Stage result;

        for(auto actor : m_actors) {
            result.m_actors[actor.first] = actor.second->clone();
        }

        return std::move(result);
    }

    void destroy() {
        for(auto actor : m_actors) delete actor.second;
        m_actors.clear();
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

class CallbackEvent : public Event {
public:
    typedef std::function<void (ActorID, bool)> Callback;
private:
    Event *m_event;
    bool m_lastValue;
    bool m_first;
    Callback m_callback;
public:
    CallbackEvent(Event *event, Callback callback)
        : Event(event->when(), event->target()), m_event(event),
        m_first(true), m_callback(callback) {}

    virtual bool apply(Stage &stage) {
        bool value = m_event->apply(stage);
        if(m_first || value != m_lastValue) {
            m_callback(m_event->target(), value);
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
        m_stage(stage.clone()), m_begin(begin) {}

    Stage &stage() { return m_stage; }
    const Stage &stage() const { return m_stage; }
    const Timestamp &begin() const { return m_begin; }

    void addEvent(Event *event) {
        // TODO: replace with more efficient version than this?
        m_events.push_back(event);
        std::stable_sort(m_events.begin(), m_events.end(),
            [](Event *a, Event *b){ return *a < *b; });
    }

    void updateRegistryTo(const Stage &stage)
        { m_stage.destroy(); m_stage = stage.clone(); }
    void replaceRegistryWith(Stage &stage)
        { m_stage.destroy(); m_stage = stage; }

    const std::vector<Event *> &events() const { return m_events; }
};

class Timeline {
private:
    std::vector<StageSnapshot> m_snapshots;
    StageSnapshot m_latest;
public:
    Timeline() : m_latest(Timestamp::makeZero()) {
        // First snapshot is so old, it's before everything
        m_snapshots.push_back(StageSnapshot(Timestamp::makeZero()));
    }

    const StageSnapshot &latest() const { return m_latest; }

    void add(Event *event) {
        int closestIndex = indexOf(event);
        if(closestIndex == -1) {
            // event is too old for us.
            return;
        }

        // insert into snapshot
        m_snapshots[closestIndex].addEvent(event);

        // now we need to reassemble all future snapshots
        auto stage = m_snapshots[closestIndex].stage().clone();
        for(unsigned i = closestIndex+1; i < m_snapshots.size(); i ++) {
            for(auto event : m_snapshots[i-1].events()) {
                event->apply(stage);
            }
            m_snapshots[i].updateRegistryTo(stage);
        }
        // handle last snapshot specially, because it updates the current
        for(auto event : m_snapshots.back().events()) {
            event->apply(stage);
        }
        m_latest.replaceRegistryWith(stage);
    }

    void makeNewSnapshot(Timestamp now) {
        // push on copy of current snapshot
        m_snapshots.push_back(StageSnapshot(now, m_latest.stage()));
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
        for(int i = 0; i < size; i ++) {
            if(event->when() < m_snapshots[i].begin()
                || event->when() == m_snapshots[i].begin()) {

                return i - 1;
            }
        }
        return size - 1;
    }
};

} // namespace csplib

#endif
