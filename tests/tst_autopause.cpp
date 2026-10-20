/*
    Copyright (C) 2026 Sebastian J. Wolf and other contributors

    This file is part of Chronograph.

    Chronograph is free software: you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation, either version 3 of the License, or
    (at your option) any later version.

    Chronograph is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
    GNU General Public License for more details.

    You should have received a copy of the GNU General Public License
    along with Chronograph. If not, see <http://www.gnu.org/licenses/>.
*/

/*
 * Host-side checks for AutoPause: standing, too fast, steps, a signal gone, a
 * resume by hand and the stop after half an hour, for every type. Every
 * activity here is generated - a speed a second, as the filter gives it, and
 * steps at a steady cadence. Build and run from the project root:
 *
 *   g++ -std=c++11 -Wall -Wextra -O2 -Isrc tests/tst_autopause.cpp \
 *       src/AutoPause.cpp -o /tmp/tst_autopause && /tmp/tst_autopause
 */

#include "AutoPause.h"

#include <cmath>
#include <cstdio>
#include <random>
#include <vector>

namespace {

const double kStartTime = 1790000000.0;
const double kNoSignal = std::numeric_limits<double>::quiet_NaN();

int g_failures = 0;

void expectNear(const char *name, double actual, double expected, double tolerance)
{
    const bool ok = std::fabs(actual - expected) <= tolerance;
    std::printf("%s %-58s %10.2f (expected %.2f +- %.2f)\n", ok ? "PASS" : "FAIL",
                name, actual, expected, tolerance);
    if (!ok)
        ++g_failures;
}

struct Event {
    AutoPause::Action action;
    /* When it was decided, and for a pause, as of when. */
    double time;
    double pauseTime;
};

/* An activity as the activity page would feed it: a speed a second, steps in
 * between, check() after each and once a second, and the actions taken. */
struct Activity {
    Activity(TrackFilter::Mode mode, bool stepsCounted)
    {
        autoPause.setMode(mode);
        autoPause.setStepsCounted(stepsCounted);
        autoPause.restart(time, true);
    }

    /* `seconds` at `speed` - kNoSignal for none - with a step every
     * `stepInterval` seconds, none if 0. */
    void go(int seconds, double speed, double stepInterval = 0)
    {
        for (int i = 0; i < seconds; ++i) {
            if (stepInterval > 0) {
                if (nextStep <= time)
                    nextStep = time + stepInterval;
                for (; nextStep <= time + 1.0; nextStep += stepInterval) {
                    autoPause.addStep(nextStep);
                    take(nextStep);
                }
            } else {
                nextStep = 0;
            }
            time += 1.0;
            if (!std::isnan(speed))
                autoPause.addSpeed(time, speed);
            take(time);
        }
    }

    /* From `from` to `to` m/s in `seconds`, without steps. */
    void ramp(double from, double to, int seconds)
    {
        for (int i = 1; i <= seconds; ++i)
            go(1, from + (to - from) * i / seconds);
    }

    void take(double now)
    {
        const AutoPause::Action action = autoPause.check(now);
        if (action == AutoPause::None)
            return;
        Event event;
        event.action = action;
        event.time = now;
        event.pauseTime = autoPause.pauseTime();
        events.push_back(event);
    }

    int count(AutoPause::Action action) const
    {
        int n = 0;
        for (const Event &event : events)
            n += event.action == action;
        return n;
    }

    /* The first `action` at or after `after`; a time of NaN for none. */
    Event first(AutoPause::Action action, double after = 0) const
    {
        for (const Event &event : events) {
            if (event.action == action && event.time >= after)
                return event;
        }
        Event none;
        none.action = AutoPause::None;
        none.time = std::nan("");
        none.pauseTime = std::nan("");
        return none;
    }

    AutoPause autoPause;
    double time = kStartTime;
    double nextStep = 0;
    std::vector<Event> events;
};

const double kWalk = 1.4;
const double kWalkStep = 0.55;
const double kRun = 3.0;
const double kRunStep = 0.36;

/* Walking, standing half a minute, walking on. With steps, the pause is as of
 * the last step and a step resumes; without, the speed decides both. */
void testStanding()
{
    Activity walk(TrackFilter::Walking, true);
    walk.go(60, kWalk, kWalkStep);
    const double stoppedAt = walk.time;
    walk.go(30, 0.1);
    const double setOffAt = walk.time;
    walk.go(30, kWalk, kWalkStep);
    const Event pause = walk.first(AutoPause::Pause);
    expectNear("walk, standing: one pause", walk.count(AutoPause::Pause), 1, 0);
    expectNear("walk, standing: as of the standing", pause.pauseTime, stoppedAt, 1);
    expectNear("walk, standing: decided after 10 s", pause.time - stoppedAt, 11, 0.5);
    expectNear("walk, standing: a step resumes", walk.first(AutoPause::Resume).time - setOffAt, 0, 1);
    expectNear("walk, standing: one resume", walk.count(AutoPause::Resume), 1, 0);

    Activity noSteps(TrackFilter::Walking, false);
    noSteps.go(60, kWalk);
    noSteps.go(30, 0.1);
    noSteps.go(30, kWalk);
    expectNear("walk, no steps: as of the standing",
               noSteps.first(AutoPause::Pause).pauseTime, stoppedAt, 1);
    expectNear("walk, no steps: the speed resumes after 3 s",
               noSteps.first(AutoPause::Resume).time - setOffAt, 4, 1);

    // Steps but no speed: the receiver says standing, the walker is not.
    Activity lagging(TrackFilter::Walking, true);
    lagging.go(60, kWalk, kWalkStep);
    lagging.go(30, 0.1, kWalkStep);
    expectNear("walk, steps but no speed: no pause", lagging.count(AutoPause::Pause), 0, 0);
    const double lastStepAt = lagging.time;
    lagging.go(30, 0.1);
    expectNear("walk, then no steps: paused as of the last step",
               lagging.first(AutoPause::Pause).pauseTime, lastStepAt, 1);
    expectNear("walk, then no steps: 5 s after it",
               lagging.first(AutoPause::Pause).time - lastStepAt, 5, 1);
}

/* A runner's traffic light: a short one passes, a longer one pauses. */
void testRunningStops()
{
    Activity shortStop(TrackFilter::Running, true);
    shortStop.go(60, kRun, kRunStep);
    shortStop.go(4, 0.2);
    shortStop.go(60, kRun, kRunStep);
    expectNear("run, 4 s stop: no pause", shortStop.count(AutoPause::Pause), 0, 0);

    Activity longStop(TrackFilter::Running, true);
    longStop.go(60, kRun, kRunStep);
    const double stoppedAt = longStop.time;
    longStop.go(20, 0.2);
    const double setOffAt = longStop.time;
    longStop.go(60, kRun, kRunStep);
    expectNear("run, 20 s stop: paused as of it", longStop.first(AutoPause::Pause).pauseTime,
               stoppedAt, 1);
    expectNear("run, 20 s stop: resumed at the first step",
               longStop.first(AutoPause::Resume).time - setOffAt, 0, 0.5);

    Activity sprint(TrackFilter::Running, true);
    sprint.go(60, kRun, kRunStep);
    sprint.go(10, 9.0, 0.25);
    sprint.go(60, kRun, kRunStep);
    expectNear("run, 10 s sprint: no pause", sprint.count(AutoPause::Pause), 0, 0);
}

/* On a bus while walking: paused as of getting that fast, not resumed by a
 * stop on the way or standing after it, resumed by walking on. */
void testBus(bool stepsCounted, double resumeAfter, const char *name)
{
    char label[80];
    Activity walk(TrackFilter::Walking, stepsCounted);
    walk.go(60, kWalk, kWalkStep);
    walk.go(5, 0.1);
    const double boardedAt = walk.time;
    walk.ramp(0, 10, 4);
    walk.go(120, 10);
    walk.ramp(10, 0, 10);
    walk.go(30, 0);
    walk.ramp(0, 10, 10);
    walk.go(60, 10);
    walk.ramp(10, 0, 10);
    walk.go(20, 0);
    const double setOffAt = walk.time;
    walk.go(60, kWalk, kWalkStep);

    const Event pause = walk.first(AutoPause::Pause);
    std::snprintf(label, sizeof label, "%s: paused as of getting fast", name);
    expectNear(label, pause.pauseTime, boardedAt + 2, 0.5);
    std::snprintf(label, sizeof label, "%s: decided after 15 s", name);
    expectNear(label, pause.time - pause.pauseTime, 15, 0.5);
    std::snprintf(label, sizeof label, "%s: one pause", name);
    expectNear(label, walk.count(AutoPause::Pause), 1, 0);
    std::snprintf(label, sizeof label, "%s: one resume", name);
    expectNear(label, walk.count(AutoPause::Resume), 1, 0);
    std::snprintf(label, sizeof label, "%s: resumed walking on", name);
    expectNear(label, walk.first(AutoPause::Resume).time - setOffAt, resumeAfter, 0.6);
}

/* Walking onto a train as it leaves, and along it on the way: the steps on
 * board resume nothing, neither while it is fast nor at a station. */
void testTrain()
{
    Activity walk(TrackFilter::Walking, true);
    walk.go(60, kWalk, kWalkStep);
    const double boardedAt = walk.time;
    walk.go(5, 5.0, kWalkStep);
    walk.go(60, 30);
    walk.go(20, 30, kWalkStep);
    walk.go(60, 30);
    walk.ramp(30, 0, 10);
    walk.go(60, 0);
    const double setOffAt = walk.time;
    walk.go(60, kWalk, kWalkStep);
    const Event pause = walk.first(AutoPause::Pause);
    expectNear("train: paused as of getting fast", pause.pauseTime, boardedAt + 1, 0.5);
    expectNear("train: one pause", walk.count(AutoPause::Pause), 1, 0);
    expectNear("train: resumed only walking off", walk.first(AutoPause::Resume).time - setOffAt,
               0, 0.6);
    expectNear("train: one resume", walk.count(AutoPause::Resume), 1, 0);
}

void testCycling()
{
    Activity downhill(TrackFilter::Cycling, false);
    downhill.go(60, 6);
    downhill.go(60, 20);
    downhill.go(60, 6);
    expectNear("cycle, 72 km/h downhill: no pause", downhill.count(AutoPause::Pause), 0, 0);

    Activity car(TrackFilter::Cycling, false);
    car.go(60, 6);
    const double fastAt = car.time;
    car.go(60, 30);
    car.go(60, 6);
    const Event pause = car.first(AutoPause::Pause);
    expectNear("cycle, in a car: paused as of it", pause.pauseTime, fastAt + 1, 0.5);
    expectNear("cycle, in a car: decided after 30 s", pause.time - fastAt, 31, 0.5);
    expectNear("cycle, in a car: resumed 10 s in range", car.first(AutoPause::Resume).time - fastAt,
               60 + 11, 0.5);

    Activity light(TrackFilter::Cycling, true);
    light.go(60, 6);
    light.ramp(6, 0, 3);
    const double stoppedAt = light.time;
    light.go(30, 0);
    light.ramp(0, 6, 3);
    expectNear("cycle, traffic light: steps ignored, paused",
               light.first(AutoPause::Pause).pauseTime, stoppedAt, 1);
}

void testDriving()
{
    Activity drive(TrackFilter::Driving, false);
    drive.go(120, 15);
    drive.go(60, 0);
    drive.go(120, 40);
    expectNear("drive, traffic light, 144 km/h: no pause", drive.count(AutoPause::Pause), 0, 0);
    const double parkedAt = drive.time;
    drive.go(300, 0);
    const double offAt = drive.time;
    drive.go(60, 15);
    expectNear("drive, parked: paused as of it", drive.first(AutoPause::Pause).pauseTime,
               parkedAt + 1, 0.5);
    expectNear("drive, parked: decided after 3 minutes",
               drive.first(AutoPause::Pause).time - parkedAt, 181, 0.5);
    expectNear("drive, off again: resumed", drive.first(AutoPause::Resume).time - offAt, 4, 0.5);
}

void testRowing()
{
    Activity row(TrackFilter::Rowing, false);
    row.go(60, 3);
    const double driftAt = row.time;
    row.go(40, 0.3);
    row.go(60, 3);
    expectNear("row, drifting: paused as of it", row.first(AutoPause::Pause).pauseTime,
               driftAt + 1, 0.5);
    expectNear("row, drifting: decided after 20 s", row.first(AutoPause::Pause).time - driftAt,
               21, 0.5);
    expectNear("row, rowing on: resumed", row.count(AutoPause::Resume), 1, 0);
}

/* Started standing: paused as of the start, so nothing of it counts. */
void testStartStanding()
{
    Activity walk(TrackFilter::Walking, true);
    const double startedAt = walk.time;
    walk.go(20, 0);
    walk.go(20, kWalk, kWalkStep);
    expectNear("started standing: paused as of the start",
               walk.first(AutoPause::Pause).pauseTime, startedAt, 1);
    expectNear("started standing: resumed walking", walk.count(AutoPause::Resume), 1, 0);
}

/* Resumed by hand while standing: no pause until there was movement. */
void testResumedByHand()
{
    Activity walk(TrackFilter::Walking, true);
    walk.go(60, kWalk, kWalkStep);
    walk.go(15, 0);
    expectNear("by hand: paused", walk.count(AutoPause::Pause), 1, 0);
    const double resumedAt = walk.time;
    walk.autoPause.restart(resumedAt, false);
    walk.go(60, 0);
    expectNear("by hand: standing on, no pause", walk.count(AutoPause::Pause), 1, 0);
    walk.go(30, kWalk, kWalkStep);
    const double stoppedAt = walk.time;
    walk.go(15, 0);
    expectNear("by hand: walked, then stood: paused again", walk.count(AutoPause::Pause), 2, 0);
    expectNear("by hand: as of the standing", walk.first(AutoPause::Pause, resumedAt).pauseTime,
               stoppedAt, 1);

    Activity bus(TrackFilter::Running, false);
    bus.go(60, kRun);
    bus.go(30, 12);
    expectNear("by hand in a bus: paused", bus.count(AutoPause::Pause), 1, 0);
    bus.autoPause.restart(bus.time, false);
    bus.go(120, 12);
    expectNear("by hand in a bus: not again", bus.count(AutoPause::Pause), 1, 0);
}

/* Without a speed nothing is decided from it, and a run of slow seconds starts
 * over once the signal is back. */
void testNoSignal()
{
    Activity walk(TrackFilter::Walking, false);
    walk.go(60, kWalk);
    walk.go(5, 0);
    walk.go(30, kNoSignal);
    walk.go(6, 0);
    expectNear("no signal while standing: no pause yet", walk.count(AutoPause::Pause), 0, 0);
    const double backAt = walk.time - 6;
    walk.go(10, 0);
    expectNear("no signal while standing: then as of its return",
               walk.first(AutoPause::Pause).pauseTime, backAt + 1, 0.5);

    Activity tunnel(TrackFilter::Cycling, false);
    tunnel.go(60, 6);
    tunnel.go(120, kNoSignal);
    tunnel.go(60, 6);
    expectNear("no signal while moving: no pause", tunnel.count(AutoPause::Pause), 0, 0);
}

/* Paused on its own for half an hour: stopped, once. */
void testStop()
{
    Activity walk(TrackFilter::Walking, true);
    walk.go(60, kWalk, kWalkStep);
    walk.go(40 * 60, 0);
    const Event pause = walk.first(AutoPause::Pause);
    const Event stop = walk.first(AutoPause::Stop);
    expectNear("half an hour paused: stopped", stop.time - pause.pauseTime, 30 * 60, 0.5);
    expectNear("half an hour paused: once", walk.count(AutoPause::Stop), 1, 0);
    walk.go(60, kWalk, kWalkStep);
    expectNear("half an hour paused: nothing after", walk.count(AutoPause::Resume), 0, 0);

    Activity halted(TrackFilter::Walking, true);
    halted.autoPause.halt();
    halted.go(60, 0);
    expectNear("halted: nothing", double(halted.events.size()), 0, 0);
}

/* A walk's speed jitters, and so does standing's: neither must flip. */
void testNoise()
{
    std::mt19937 random(7);
    std::normal_distribution<double> walking(kWalk, 0.25);
    std::normal_distribution<double> standing(0.1, 0.08);
    Activity walk(TrackFilter::Walking, false);
    for (int i = 0; i < 20 * 60; ++i)
        walk.go(1, std::max(0.0, walking(random)));
    expectNear("noisy walk, 20 minutes: no pause", walk.count(AutoPause::Pause), 0, 0);
    const double stoppedAt = walk.time;
    for (int i = 0; i < 5 * 60; ++i)
        walk.go(1, std::max(0.0, standing(random)));
    expectNear("noisy standing: paused once", walk.count(AutoPause::Pause), 1, 0);
    expectNear("noisy standing: as of it", walk.first(AutoPause::Pause).pauseTime, stoppedAt, 2);
    expectNear("noisy standing: no resume", walk.count(AutoPause::Resume), 0, 0);
}

} // namespace

int main()
{
    testStanding();
    testRunningStops();
    testBus(true, 0, "bus, steps");
    testBus(false, 11, "bus, no steps");
    testTrain();
    testCycling();
    testDriving();
    testRowing();
    testStartStanding();
    testResumedByHand();
    testNoSignal();
    testStop();
    testNoise();

    std::printf("%s\n", g_failures ? "FAILED" : "All passed");
    return g_failures ? 1 : 0;
}
