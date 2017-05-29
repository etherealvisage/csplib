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

class ActorRegistry {
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

    ActorRegistry clone() const {
        ActorRegistry result;

        for(auto actor : m_actors) {
            result.m_actors[actor.first] = actor.second->clone();
        }

        return result;
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

    virtual bool apply(ActorRegistry &registry) = 0;

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

    virtual bool apply(ActorRegistry &registry) {
        bool value = m_event->apply(registry);
        if(m_first) {
            m_first = false;
            m_lastValue = value;
            m_callback(m_event->target(), value);
        }
        else if(value != m_lastValue) {
            m_callback(m_event->target(), value);
            m_lastValue = value;
        }

        return true; // this event always succeeds
    }
};

class Stage;

class StageSnapshot {
private:
    ActorRegistry m_registry;
    std::vector<Event *> m_events;
    Timestamp m_begin;
public:
    StageSnapshot(Timestamp begin) : m_begin(begin) {}
    StageSnapshot(Timestamp begin, const ActorRegistry &registry) :
        m_registry(registry.clone()), m_begin(begin) {}

    ActorRegistry &registry() { return m_registry; }
    const ActorRegistry &registry() const { return m_registry; }
    const Timestamp &begin() const { return m_begin; }

    void addEvent(Event *event) {
        // TODO: replace with more efficient version than this?
        m_events.push_back(event);
        std::stable_sort(m_events.begin(), m_events.end(),
            [](Event *a, Event *b){ return *a < *b; });
    }

    void updateRegistryTo(const ActorRegistry &registry)
        { m_registry.destroy(); m_registry = registry.clone(); }
    void replaceRegistryWith(ActorRegistry &registry)
        { m_registry.destroy(); m_registry = registry; }

    const std::vector<Event *> &events() const { return m_events; }
};

class Stage {
private:
    std::vector<StageSnapshot> m_snapshots;
    StageSnapshot m_latest;
public:
    Stage() : m_latest(Timestamp::makeZero()) {
        // First snapshot is so old, it's before everything
        m_snapshots.push_back(StageSnapshot(Timestamp::makeZero()));
    }

    const StageSnapshot &latest() const { return m_latest; }

    void accept(Event *event) {
        int closestIndex = -1;
        for(unsigned i = 0; i < m_snapshots.size(); i ++) {
            if(m_snapshots[i].begin() < event->when()) {
                closestIndex = i;
            }
            else break;
        }
        if(closestIndex == -1) {
            // event is too old for us.
            return;
        }

        // insert into snapshot
        m_snapshots[closestIndex].addEvent(event);

        // now we need to reassemble all future snapshots
        ActorRegistry areg =
            m_snapshots[closestIndex].registry().clone();
        for(unsigned i = closestIndex+1; i < m_snapshots.size(); i ++) {
            for(auto event : m_snapshots[i-1].events()) {
                event->apply(areg);
            }
            m_snapshots[i].updateRegistryTo(areg);
        }
        // handle last snapshot specially, because it updates the current
        for(auto event : m_snapshots.back().events()) {
            event->apply(areg);
        }
        m_latest.replaceRegistryWith(areg);
    }

    void makeNewSnapshot(Timestamp now) {
        // push on copy of current snapshot
        m_snapshots.push_back(StageSnapshot(now, m_latest.registry()));
    }

    void limitSnapshots(int count) {
        if(count < 1) count = 1;
        if(m_snapshots.size() <= (unsigned)count) return;
        int delta = m_snapshots.size() - count;
        m_snapshots.erase(m_snapshots.begin(), m_snapshots.begin()+delta);
    }
};

} // namespace csplib

#endif
