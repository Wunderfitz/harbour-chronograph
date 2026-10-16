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
 * Host-side checks for Track, which has no Qt in it for exactly this reason:
 * the time with pauses left out, the distance with and without the receiver's
 * wander, the current and average speed, the climb, and what a replay sees at
 * a given moment. Every track here is generated; fixes come once a second, as
 * from the phone's receiver. Build and run from the project root:
 *
 *   g++ -std=c++11 -Wall -Wextra -O2 -Isrc tests/tst_track.cpp \
 *       src/Track.cpp -o /tmp/tst_track && /tmp/tst_track
 */

#include "Track.h"

#include <cmath>
#include <cstdio>
#include <random>

namespace {

/* Metres per degree of latitude on the sphere Track measures on. */
const double kMetresPerDegree = 111195.08;
const double kStartTime = 1790000000.0;
const double kLatitude = 52.5;
const double kLongitude = 13.4;

int g_failures = 0;

void expectNear(const char *name, double actual, double expected, double tolerance)
{
    const bool ok = std::fabs(actual - expected) <= tolerance;
    std::printf("%s %-58s %10.2f (expected %.2f +- %.2f)\n", ok ? "PASS" : "FAIL",
                name, actual, expected, tolerance);
    if (!ok)
        ++g_failures;
}

void expectBelow(const char *name, double actual, double limit)
{
    const bool ok = actual < limit;
    std::printf("%s %-58s %10.2f (expected below %.2f)\n", ok ? "PASS" : "FAIL",
                name, actual, limit);
    if (!ok)
        ++g_failures;
}

/* A fix `north` and `east` metres from the reference point. */
TrackPoint fix(double time, double north, double east)
{
    TrackPoint point;
    point.time = time;
    point.latitude = kLatitude + north / kMetresPerDegree;
    point.longitude = kLongitude + east / (kMetresPerDegree * std::cos(kLatitude * M_PI / 180.0));
    point.accuracy = 5.0;
    return point;
}

/* A receiver's error: not fresh noise with every fix - the chipset filters its
 * output - but an offset that wanders, `sigma` metres on average and changing
 * over about `tau` seconds. */
struct Wander {
    Wander(double sigma, unsigned seed, double tau = 30.0)
        : sigma(sigma), keep(std::exp(-1.0 / tau)), random(seed) {}
    void next()
    {
        const double step = sigma * std::sqrt(1.0 - keep * keep);
        north = keep * north + step * normal(random);
        east = keep * east + step * normal(random);
    }
    double sigma;
    double keep;
    double north = 0;
    double east = 0;
    std::mt19937 random;
    std::normal_distribution<double> normal;
};

/* Walks north at `speed` for `seconds`, one fix a second, starting `north`
 * metres up. The receiver's speed comes with it unless `withSpeed` is false;
 * `wander` is the receiver's error, none if null. Returns where it ended. */
double walkNorth(Track &track, double &time, double north, double speed, int seconds,
                 bool withSpeed = true, Wander *wander = nullptr)
{
    for (int i = 0; i < seconds; ++i) {
        time += 1.0;
        north += speed;
        if (wander)
            wander->next();
        TrackPoint point = fix(time, north + (wander ? wander->north : 0.0),
                               wander ? wander->east : 0.0);
        if (withSpeed)
            point.speed = speed;
        track.addPoint(point);
    }
    return north;
}

void testEmpty()
{
    Track track;
    const TrackState state = track.stateAt(10);
    expectNear("an empty track: no point", state.point, -1, 0);
    expectNear("an empty track: no distance", state.distance, 0, 0);
    expectNear("an empty track: no duration", track.duration(), 0, 0);
}

void testSteadyRun()
{
    Track track;
    double time = kStartTime;
    track.begin(time);
    walkNorth(track, time, 0, 3.0, 600);
    track.end(time);

    const TrackState end = track.stateAt(track.duration());
    expectNear("steady run: duration", track.duration(), 600, 0.001);
    // From the first fix, 3 m in; the last step of less than 5 m is pending.
    expectNear("steady run: distance", end.distance, 1797, 5);
    expectNear("steady run: average speed", end.averageSpeed, 3.0, 0.02);
    expectNear("steady run: current speed", end.speed, 3.0, 0.01);

    const TrackState half = track.stateAt(300);
    expectNear("steady run, replayed at half: distance", half.distance, 897, 5);
    expectNear("steady run, replayed at half: point", half.point, 299, 0);
}

void testSpeedFromPositions()
{
    Track track;
    double time = kStartTime;
    track.begin(time);
    walkNorth(track, time, 0, 1.4, 300, false);
    const TrackState state = track.stateAt(track.duration());
    expectNear("walk without receiver speeds: distance", state.distance, 418, 5);
    expectNear("walk without receiver speeds: current speed", state.speed, 1.4, 0.3);
}

void testStandingStill()
{
    Track withSpeed;
    double time = kStartTime;
    withSpeed.begin(time);
    for (int i = 0; i < 300; ++i) {
        // Wandering by metres, saying it stands.
        TrackPoint point = fix(time += 1.0, 6.0 * std::sin(i / 7.0), 6.0 * std::cos(i / 11.0));
        point.speed = 0.2;
        withSpeed.addPoint(point);
    }
    const TrackState state = withSpeed.stateAt(withSpeed.duration());
    expectNear("standing, receiver says so: distance", state.distance, 0, 0);
    expectNear("standing, receiver says so: current speed", state.speed, 0, 0);

    Track without;
    time = kStartTime;
    without.begin(time);
    Wander wander(2.0, 5);
    walkNorth(without, time, 0, 0.0, 600, false, &wander);
    expectBelow("standing 10 min, no receiver speed, 2 m of wander: distance",
                without.stateAt(without.duration()).distance, 30);
}

void testNoisyWalk()
{
    Track track;
    double time = kStartTime;
    track.begin(time);
    Wander wander(2.0, 6);
    walkNorth(track, time, 0, 1.4, 600, true, &wander);
    expectNear("walk with 2 m of wander: distance within 5 %",
               track.stateAt(track.duration()).distance, 840, 42);

    Track fast;
    time = kStartTime;
    fast.begin(time);
    Wander quick(2.0, 7, 5.0);
    walkNorth(fast, time, 0, 1.4, 600, true, &quick);
    expectNear("walk with 2 m of wander over 5 s: distance within 10 %",
               fast.stateAt(fast.duration()).distance, 840, 84);
}

void testPause()
{
    Track track;
    double time = kStartTime;
    track.begin(time);
    double north = walkNorth(track, time, 0, 2.0, 60);
    track.end(time);

    // Moved on while paused: none of it counts, nor does the gap.
    time += 120;
    north += 300;
    track.begin(time);
    TrackState afterResume = track.stateAt(track.elapsedAt(time + 2));
    expectNear("pause: no speed right after resuming", afterResume.speed, 0, 0);
    walkNorth(track, time, north, 2.0, 60);
    track.end(time);

    expectNear("pause: duration leaves it out", track.duration(), 120, 0.001);
    const TrackState state = track.stateAt(track.duration());
    // 2 x 118 m from each segment's first fix, less the pending steps.
    expectNear("pause: distance leaves the gap out", state.distance, 231, 6);
    expectNear("pause: two segments", double(track.segments().size()), 2, 0);
    expectNear("pause: elapsed at the resume", track.elapsedAt(kStartTime + 60 + 120), 60, 0.001);
}

void testStaleSpeed()
{
    Track track;
    double time = kStartTime;
    track.begin(time);
    walkNorth(track, time, 0, 3.0, 60);
    // Ten seconds without a fix, still recording.
    expectNear("no fix for a moment: speed holds", track.stateAt(track.elapsedAt(time + 3)).speed, 3, 0.01);
    expectNear("no fix for 10 s: speed reads 0", track.stateAt(track.elapsedAt(time + 10)).speed, 0, 0);
}

void testClimb(double noise, double climb, double tolerance, const char *name, unsigned seed)
{
    std::mt19937 random(seed);
    std::normal_distribution<double> jitter(0.0, noise > 0 ? noise : 1.0);
    Track track;
    double time = kStartTime;
    track.begin(time);
    for (int i = 0; i < 600; ++i) {
        TrackPoint point = fix(time += 1.0, 1.5 * i, 0);
        point.speed = 1.5;
        point.altitude = 100.0 + climb * i / 599.0 + (noise > 0 ? jitter(random) : 0.0);
        track.addPoint(point);
    }
    const TrackState state = track.stateAt(track.duration());
    expectNear(name, state.elevationGain, climb, tolerance);
}

void testAltitude()
{
    testClimb(0.0, 50.0, 3.0, "climb of 50 m, clean: gain", 1);
    testClimb(1.0, 50.0, 6.0, "climb of 50 m, 1 m of noise: gain", 2);
    testClimb(1.0, 0.0, 5.0, "flat, 1 m of noise: gain", 3);
    testClimb(2.0, 0.0, 10.0, "flat, 2 m of noise: gain", 4);

    Track track;
    double time = kStartTime;
    track.begin(time);
    walkNorth(track, time, 0, 1.0, 5);
    expectNear("no altitude from the receiver: none shown",
               std::isnan(track.stateAt(track.duration()).altitude) ? 1 : 0, 1, 0);
}

void testRejects()
{
    Track track;
    expectNear("a fix before the start is ignored", track.addPoint(fix(kStartTime, 0, 0)) ? 1 : 0, 0, 0);
    track.begin(kStartTime);
    track.addPoint(fix(kStartTime + 10, 0, 0));
    expectNear("a fix older than the last is ignored",
               track.addPoint(fix(kStartTime + 5, 0, 0)) ? 1 : 0, 0, 0);
    track.end(kStartTime + 20);
    expectNear("a fix while paused is ignored",
               track.addPoint(fix(kStartTime + 25, 0, 0)) ? 1 : 0, 0, 0);
    expectNear("one point kept", double(track.size()), 1, 0);
}

} // namespace

int main()
{
    testEmpty();
    testSteadyRun();
    testSpeedFromPositions();
    testStandingStill();
    testNoisyWalk();
    testPause();
    testStaleSpeed();
    testAltitude();
    testRejects();

    std::printf("%s\n", g_failures ? "FAILED" : "All passed");
    return g_failures ? 1 : 0;
}
