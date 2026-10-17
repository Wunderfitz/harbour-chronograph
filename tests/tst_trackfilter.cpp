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
 * Host-side checks for TrackFilter: generated activities of every type, with
 * the receiver erring as a phone's does - an offset wandering slowly, a little
 * fresh noise, and outliers in the position, the speed and the altitude -
 * each recorded twice, raw and filtered, and both held against the truth.
 * Every track here is generated, with generic parameters and fixed seeds.
 * Build and run from the project root:
 *
 *   g++ -std=c++11 -Wall -Wextra -O2 -Isrc tests/tst_trackfilter.cpp \
 *       src/TrackFilter.cpp src/Track.cpp -o /tmp/tst_trackfilter && /tmp/tst_trackfilter
 */

#include "Track.h"
#include "TrackFilter.h"

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <random>
#include <vector>

namespace {

/* Metres per degree of latitude on the sphere Track measures on. */
const double kMetresPerDegree = 111195.08;
const double kStartTime = 1790000000.0;
const double kLatitude = 52.5;
const double kLongitude = 13.4;
const double kPi = 3.14159265358979323846;

/* Seconds the filter gets to settle before its errors count. */
const double kWarmUp = 10.0;

int g_failures = 0;

void expectBelow(const char *name, double actual, double limit)
{
    const bool ok = actual < limit;
    std::printf("%s %-62s %9.2f (expected below %.2f)\n", ok ? "PASS" : "FAIL",
                name, actual, limit);
    if (!ok)
        ++g_failures;
}

void expectNear(const char *name, double actual, double expected, double tolerance)
{
    const bool ok = std::fabs(actual - expected) <= tolerance;
    std::printf("%s %-62s %9.2f (expected %.2f +- %.2f)\n", ok ? "PASS" : "FAIL",
                name, actual, expected, tolerance);
    if (!ok)
        ++g_failures;
}

double square(double value)
{
    return value * value;
}

/* Where the activity really was at one second of it. */
struct Truth {
    double north = 0;
    double east = 0;
    double altitude = 100;
    double speed = 0;
    /* Radians from north. */
    double heading = 0;
    double distance = 0;
};

/* A route as legs, each going at one speed in one direction, the altitude
 * changing at a steady rate. Speed and direction change no faster than
 * `acceleration` allows, along the way and across it: a walker turns a corner
 * in a step, a car takes a few seconds. */
struct Leg {
    int seconds;
    double speed;
    double heading;
    double climb;
};

std::vector<Truth> route(const std::vector<Leg> &legs, double acceleration = 2.0)
{
    std::vector<Truth> truth;
    Truth now;
    now.speed = legs.front().speed;
    now.heading = legs.front().heading * kPi / 180.0;
    for (const Leg &leg : legs) {
        const double heading = leg.heading * kPi / 180.0;
        for (int i = 0; i < leg.seconds; ++i) {
            now.speed += std::max(-acceleration, std::min(acceleration, leg.speed - now.speed));
            double turn = std::remainder(heading - now.heading, 2 * kPi);
            const double maxTurn = acceleration / std::max(now.speed, 0.5);
            now.heading += std::max(-maxTurn, std::min(maxTurn, turn));
            now.north += now.speed * std::cos(now.heading);
            now.east += now.speed * std::sin(now.heading);
            now.altitude += leg.climb;
            now.distance += now.speed;
            truth.push_back(now);
        }
    }
    return truth;
}

/* A loop of `laps` rectangles of `width` by `height` metres at `speed`, the
 * altitude rising `climb` metres along the long sides and falling again. */
std::vector<Truth> loops(double speed, double width, double height, int laps, double climb)
{
    std::vector<Leg> legs;
    const int across = int(width / speed);
    const int up = int(height / speed);
    for (int lap = 0; lap < laps; ++lap) {
        legs.push_back({across, speed, 90, climb / across});
        legs.push_back({up, speed, 0, 0});
        legs.push_back({across, speed, 270, -climb / across});
        legs.push_back({up, speed, 180, 0});
    }
    return route(legs);
}

/* How the receiver errs. Its position is off by an offset that wanders,
 * `wander` metres on average and changing over `wanderTime` seconds, plus a
 * little fresh noise; it says it is accurate to `accuracy`. Now and then a
 * position jumps for a few fixes - with the accuracy claimed as ever - the
 * speed spikes, or the altitude does. */
struct Receiver {
    double accuracy = 5.0;
    double wander = 3.0;
    double wanderTime = 30.0;
    double noise = 1.0;
    double speedNoise = 0.15;
    double altitudeWander = 3.0;
    double altitudeNoise = 1.0;
    bool hasSpeed = true;
    /* Chances per fix, and sizes. */
    double jumpChance = 0.01;
    double jumpMin = 20.0;
    double jumpMax = 80.0;
    double speedSpikeChance = 0.02;
    double speedSpikeMax = 4.0;
    double altitudeSpikeChance = 0.02;
    double altitudeSpikeMax = 40.0;
    unsigned seed = 1;
};

/* Turns the truth into fixes, one a second; -1 in `gapFrom` for no gap,
 * otherwise no fixes from that second for `gapSeconds`. A fix from `offsetFrom`
 * on for `offsetSeconds` is `offset` metres east of where it should be. */
struct Recording {
    std::vector<TrackPoint> fixes;
    std::vector<std::size_t> truthIndex;
};

TrackPoint at(double north, double east)
{
    TrackPoint point;
    point.latitude = kLatitude + north / kMetresPerDegree;
    point.longitude = kLongitude + east / (kMetresPerDegree * std::cos(kLatitude * kPi / 180.0));
    return point;
}

void metresOf(const TrackPoint &point, double *north, double *east)
{
    *north = (point.latitude - kLatitude) * kMetresPerDegree;
    *east = (point.longitude - kLongitude) * kMetresPerDegree * std::cos(kLatitude * kPi / 180.0);
}

struct Disturbance {
    int gapFrom = -1;
    int gapSeconds = 0;
    int offsetFrom = -1;
    int offsetSeconds = 0;
    double offset = 0;
};

Recording record(const std::vector<Truth> &truth, const Receiver &receiver,
                 const Disturbance &disturbance = Disturbance())
{
    std::mt19937 random(receiver.seed);
    std::normal_distribution<double> normal;
    std::uniform_real_distribution<double> uniform;
    const double keep = std::exp(-1.0 / receiver.wanderTime);
    const double step = receiver.wander * std::sqrt(1.0 - keep * keep);
    const double altitudeKeep = std::exp(-1.0 / 60.0);
    const double altitudeStep = receiver.altitudeWander * std::sqrt(1.0 - altitudeKeep * altitudeKeep);

    double north = receiver.wander * normal(random);
    double east = receiver.wander * normal(random);
    double up = receiver.altitudeWander * normal(random);
    int jumpLeft = 0;
    double jumpNorth = 0;
    double jumpEast = 0;

    Recording recording;
    for (std::size_t i = 0; i < truth.size(); ++i) {
        north = keep * north + step * normal(random);
        east = keep * east + step * normal(random);
        up = altitudeKeep * up + altitudeStep * normal(random);
        const int second = int(i);
        if (second >= disturbance.gapFrom && second < disturbance.gapFrom + disturbance.gapSeconds)
            continue;

        if (jumpLeft == 0 && uniform(random) < receiver.jumpChance) {
            jumpLeft = 1 + int(uniform(random) * 3);
            const double size = receiver.jumpMin + uniform(random) * (receiver.jumpMax - receiver.jumpMin);
            const double angle = uniform(random) * 2 * kPi;
            jumpNorth = size * std::cos(angle);
            jumpEast = size * std::sin(angle);
        }
        double errorNorth = north + receiver.noise * normal(random);
        double errorEast = east + receiver.noise * normal(random);
        if (jumpLeft > 0) {
            errorNorth += jumpNorth;
            errorEast += jumpEast;
            --jumpLeft;
        }
        if (second >= disturbance.offsetFrom
                && second < disturbance.offsetFrom + disturbance.offsetSeconds)
            errorEast += disturbance.offset;

        const Truth &now = truth[i];
        TrackPoint fix = at(now.north + errorNorth, now.east + errorEast);
        fix.time = kStartTime + second + 1;
        fix.accuracy = receiver.accuracy * (0.8 + 0.4 * uniform(random));
        fix.altitude = now.altitude + up + receiver.altitudeNoise * normal(random);
        if (uniform(random) < receiver.altitudeSpikeChance)
            fix.altitude += (uniform(random) < 0.5 ? -1 : 1)
                    * (10.0 + uniform(random) * (receiver.altitudeSpikeMax - 10.0));
        if (receiver.hasSpeed) {
            double velocityNorth = now.speed * std::cos(now.heading) + receiver.speedNoise * normal(random);
            double velocityEast = now.speed * std::sin(now.heading) + receiver.speedNoise * normal(random);
            if (uniform(random) < receiver.speedSpikeChance) {
                const double spike = 1.0 + uniform(random) * (receiver.speedSpikeMax - 1.0);
                const double speed = std::sqrt(square(velocityNorth) + square(velocityEast));
                const double scale = speed > 0.1 ? (speed + spike) / speed : 1.0;
                velocityNorth *= scale;
                velocityEast *= scale;
                if (speed <= 0.1)
                    velocityNorth += spike;
            }
            fix.speed = std::sqrt(square(velocityNorth) + square(velocityEast));
            double course = std::atan2(velocityEast, velocityNorth) * 180.0 / kPi;
            fix.course = course < 0 ? course + 360.0 : course;
        }
        recording.fixes.push_back(fix);
        recording.truthIndex.push_back(i);
    }
    return recording;
}

/* What a recording comes to, raw or filtered. */
struct Result {
    double distance = 0;
    double elevationGain = 0;
    /* Of the points recorded, against where they really were. */
    double positionRms = 0;
    double positionMax = 0;
    /* Of the speeds recorded, and of the current speed Track shows. */
    double speedRms = 0;
    double speedMax = 0;
    double shownSpeedMax = 0;
    double altitudeMax = 0;
    std::size_t points = 0;
};

Result replay(const std::vector<Truth> &truth, const Recording &recording, bool filtered,
              TrackFilter::Mode mode)
{
    TrackFilter filter;
    filter.setMode(mode);
    Track track;
    track.begin(kStartTime);
    Result result;
    double positionSum = 0;
    double speedSum = 0;
    int counted = 0;
    for (std::size_t i = 0; i < recording.fixes.size(); ++i) {
        TrackPoint point = recording.fixes[i];
        if (filtered) {
            if (!filter.add(point))
                continue;
            point = filter.estimate();
        }
        if (!track.addPoint(point))
            continue;
        if (point.time - kStartTime < kWarmUp)
            continue;
        const Truth &now = truth[recording.truthIndex[i]];
        double north = 0;
        double east = 0;
        metresOf(point, &north, &east);
        const double error = std::sqrt(square(north - now.north) + square(east - now.east));
        positionSum += square(error);
        result.positionMax = std::max(result.positionMax, error);
        const double shown = track.speedAtPoint(track.size() - 1);
        const double speedError = std::fabs((std::isnan(point.speed) ? shown : point.speed) - now.speed);
        speedSum += square(speedError);
        result.speedMax = std::max(result.speedMax, speedError);
        result.shownSpeedMax = std::max(result.shownSpeedMax, std::fabs(shown - now.speed));
        if (!std::isnan(point.altitude))
            result.altitudeMax = std::max(result.altitudeMax, std::fabs(point.altitude - now.altitude));
        ++counted;
    }
    track.end(kStartTime + truth.size());
    const TrackState state = track.stateAt(track.duration());
    result.distance = state.distance;
    result.elevationGain = state.elevationGain;
    result.positionRms = counted ? std::sqrt(positionSum / counted) : 0;
    result.speedRms = counted ? std::sqrt(speedSum / counted) : 0;
    result.points = track.size();
    return result;
}

double climbOf(const std::vector<Truth> &truth)
{
    double gain = 0;
    for (std::size_t i = 1; i < truth.size(); ++i)
        gain += std::max(0.0, truth[i].altitude - truth[i - 1].altitude);
    return gain;
}

void print(const char *name, const Result &raw, const Result &filtered, double distance, double climb)
{
    std::printf("     %s: distance %.0f (raw %.0f, filtered %.0f), climb %.0f (raw %.0f, filtered %.0f)\n"
                "       position rms/max raw %.1f/%.1f filtered %.1f/%.1f; speed rms/max raw %.2f/%.2f"
                " filtered %.2f/%.2f; altitude max raw %.1f filtered %.1f\n",
                name, distance, raw.distance, filtered.distance, climb, raw.elevationGain,
                filtered.elevationGain, raw.positionRms, raw.positionMax, filtered.positionRms,
                filtered.positionMax, raw.speedRms, raw.speedMax, filtered.speedRms,
                filtered.speedMax, raw.altitudeMax, filtered.altitudeMax);
}

/* One activity of a type, recorded with outliers, raw and filtered: the
 * filtered one must be better in every respect and within the limits. The
 * climb only half as far off as the raw one: what remains is the receiver's
 * slow wander, which no filter can tell from a hill (see testAltitude). */
void testActivity(const char *name, TrackFilter::Mode mode, const std::vector<Truth> &truth,
                  const Receiver &receiver, double maxDistanceError, double maxPositionError,
                  double maxSpeedError)
{
    const Recording recording = record(truth, receiver);
    const Result raw = replay(truth, recording, false, mode);
    const Result filtered = replay(truth, recording, true, mode);
    const double distance = truth.back().distance;
    const double climb = climbOf(truth);
    print(name, raw, filtered, distance, climb);

    char label[160];
    std::snprintf(label, sizeof label, "%s: distance error, %%", name);
    expectBelow(label, 100.0 * std::fabs(filtered.distance - distance) / distance, maxDistanceError);
    std::snprintf(label, sizeof label, "%s: largest position error, m", name);
    expectBelow(label, filtered.positionMax, maxPositionError);
    std::snprintf(label, sizeof label, "%s: position error, rms, below the raw one", name);
    expectBelow(label, filtered.positionRms, raw.positionRms);
    std::snprintf(label, sizeof label, "%s: largest speed error, m/s", name);
    expectBelow(label, filtered.speedMax, maxSpeedError);
    std::snprintf(label, sizeof label, "%s: speed error, rms, below the raw one", name);
    expectBelow(label, filtered.speedRms, raw.speedRms);
    std::snprintf(label, sizeof label, "%s: climb error, m, below half the raw one", name);
    expectBelow(label, std::fabs(filtered.elevationGain - climb), std::fabs(raw.elevationGain - climb) / 2);
    std::snprintf(label, sizeof label, "%s: largest altitude error below the raw one", name);
    expectBelow(label, filtered.altitudeMax, raw.altitudeMax);
}

void testTypes()
{
    Receiver receiver;
    receiver.seed = 11;
    testActivity("walk", TrackFilter::Walking, loops(1.4, 200, 100, 3, 10), receiver, 3, 12, 1.0);
    receiver.seed = 12;
    testActivity("slow walk", TrackFilter::Walking, loops(0.8, 120, 60, 3, 5), receiver, 4, 10, 1.0);
    receiver.seed = 13;
    testActivity("run", TrackFilter::Running, loops(3.2, 400, 200, 3, 15), receiver, 3, 12, 1.5);
    receiver.seed = 14;
    testActivity("bike ride", TrackFilter::Cycling, loops(7.0, 1500, 600, 2, 30), receiver, 3, 12, 2.0);
    receiver.seed = 15;
    testActivity("rowing", TrackFilter::Rowing, route({{300, 4.0, 10, 0}, {60, 4.0, 100, 0},
                                                       {300, 4.0, 190, 0}, {60, 4.0, 280, 0}}),
                 receiver, 3, 12, 1.5);
    receiver.seed = 16;
    testActivity("drive", TrackFilter::Driving,
                 route({{120, 14.0, 30, 0.1}, {60, 22.0, 75, 0.2}, {200, 27.0, 120, -0.1},
                        {90, 12.0, 200, 0}, {120, 16.0, 290, 0.05}}),
                 receiver, 3, 15, 4.5);
}

/* Fair rather than good: twice the error, claimed as such. */
void testPoorSignal()
{
    Receiver receiver;
    receiver.accuracy = 20.0;
    receiver.wander = 8.0;
    receiver.noise = 3.0;
    receiver.speedNoise = 0.4;
    receiver.seed = 21;
    testActivity("walk, fair signal", TrackFilter::Walking, loops(1.4, 300, 150, 2, 0),
                 receiver, 5, 25, 1.5);

    // At the edge of fair, where Activity still lets fixes in: the filter
    // must settle at once and keep the route, if loosely.
    receiver.accuracy = 35.0;
    receiver.wander = 15.0;
    receiver.noise = 5.0;
    receiver.speedNoise = 0.8;
    receiver.seed = 22;
    testActivity("walk, signal at the edge of fair", TrackFilter::Walking, loops(1.4, 300, 150, 2, 0),
                 receiver, 10, 50, 2.0);
    const std::vector<Truth> truth = route({{60, 1.4, 0, 0}});
    const Recording recording = record(truth, receiver);
    TrackFilter filter;
    filter.setMode(TrackFilter::Walking);
    double settled = -1;
    for (const TrackPoint &fix : recording.fixes) {
        if (filter.add(fix) && settled < 0)
            settled = fix.time - kStartTime;
    }
    expectBelow("signal at the edge of fair: first estimate, s", settled, 10);
}

/* Without the receiver's speed and course, the positions alone. */
void testNoSpeed()
{
    Receiver receiver;
    receiver.hasSpeed = false;
    receiver.seed = 31;
    testActivity("walk, no receiver speed", TrackFilter::Walking, loops(1.4, 200, 100, 2, 0),
                 receiver, 5, 15, 1.2);
}

/* Standing for ten minutes with the receiver wandering and jumping: next to
 * no distance. */
void testStanding()
{
    const std::vector<Truth> truth = route({{600, 0.0, 0, 0}});
    Receiver receiver;
    receiver.seed = 41;
    const Recording recording = record(truth, receiver);
    const Result raw = replay(truth, recording, false, TrackFilter::Walking);
    const Result filtered = replay(truth, recording, true, TrackFilter::Walking);
    print("standing", raw, filtered, 0, 0);
    expectBelow("standing 10 min: distance, m", filtered.distance, 15);
    expectBelow("standing 10 min: largest position error, m", filtered.positionMax, 10);
    expectBelow("standing 10 min: largest speed shown, m/s", filtered.shownSpeedMax, Track::kMovingSpeed);
}

/* Standing, then off at the type's pace, and stopping again: the speed must
 * follow the truth within a few seconds either way, though the filter does
 * not let it change suddenly. */
void testSetOff(const char *name, TrackFilter::Mode mode, double speed, double within)
{
    const std::vector<Truth> truth = route({{60, 0.0, 0, 0}, {60, speed, 45, 0}, {60, 0.0, 45, 0}});
    Receiver receiver;
    receiver.jumpChance = 0;
    receiver.speedSpikeChance = 0;
    receiver.seed = 51;
    const Recording recording = record(truth, receiver);
    TrackFilter filter;
    filter.setMode(mode);
    // The seconds at which the truth, and then the estimate, got going and stopped.
    double going[2] = {-1, -1};
    double stopped[2] = {-1, -1};
    for (std::size_t i = 0; i < recording.fixes.size(); ++i) {
        const bool estimated = filter.add(recording.fixes[i]);
        const int second = int(recording.truthIndex[i]);
        const double speeds[2] = {truth[std::size_t(second)].speed, filter.estimate().speed};
        for (int which = 0; which < 2; ++which) {
            if (which == 1 && !estimated)
                continue;
            if (second >= 60 && second < 120 && going[which] < 0 && speeds[which] > 0.8 * speed)
                going[which] = second;
            if (second >= 120 && stopped[which] < 0 && speeds[which] < Track::kMovingSpeed)
                stopped[which] = second;
        }
    }
    char label[160];
    std::snprintf(label, sizeof label, "%s: setting off, s behind reaching 80 %% of the pace", name);
    expectBelow(label, going[1] - going[0], within);
    std::snprintf(label, sizeof label, "%s: stopping, s behind standing", name);
    expectBelow(label, stopped[1] - stopped[0], within);
}

void testSetOff()
{
    testSetOff("walk", TrackFilter::Walking, 1.4, 3);
    testSetOff("run", TrackFilter::Running, 3.5, 3);
    testSetOff("bike ride", TrackFilter::Cycling, 6.0, 3);
    testSetOff("rowing", TrackFilter::Rowing, 4.0, 3);
    testSetOff("drive", TrackFilter::Driving, 20.0, 3);
}

/* Altitude spikes with no wander underneath: they must vanish, on the flat
 * and up a hill. The receiver's slow wander, on the other hand, passes - it
 * looks just like a hill - and only Track's hysteresis keeps it from the
 * climb. */
void testAltitude()
{
    Receiver receiver;
    receiver.altitudeWander = 0;
    receiver.altitudeSpikeChance = 0.05;
    receiver.jumpChance = 0;
    receiver.seed = 55;
    const std::vector<Truth> flat = route({{1200, 1.4, 0, 0}});
    Recording recording = record(flat, receiver);
    Result raw = replay(flat, recording, false, TrackFilter::Walking);
    Result filtered = replay(flat, recording, true, TrackFilter::Walking);
    expectBelow("flat, altitude spikes: raw climb, m, for comparison", -raw.elevationGain, -50);
    expectBelow("flat, altitude spikes: climb, m", filtered.elevationGain, 1);
    expectBelow("flat, altitude spikes: largest altitude error, m", filtered.altitudeMax, 5);

    const std::vector<Truth> hill = loops(1.4, 200, 100, 3, 20);
    recording = record(hill, receiver);
    filtered = replay(hill, recording, true, TrackFilter::Walking);
    expectNear("a hill of 20 m three times, altitude spikes: climb, m", filtered.elevationGain, 60, 3);

    receiver.seed = 56;
    const std::vector<Truth> lake = route({{600, 4.0, 0, 0}});
    recording = record(lake, receiver);
    filtered = replay(lake, recording, true, TrackFilter::Rowing);
    expectBelow("rowing, altitude spikes: climb, m", filtered.elevationGain, 1);
}

/* The first fixes far off, as after a cold start: the estimate must not
 * start there, nor stay there. */
void testBadStart()
{
    const std::vector<Truth> truth = route({{120, 1.4, 0, 0}});
    Receiver receiver;
    receiver.jumpChance = 0;
    receiver.seed = 61;
    Recording recording = record(truth, receiver);
    for (int i = 0; i < 2; ++i) {
        double north = 0;
        double east = 0;
        metresOf(recording.fixes[std::size_t(i)], &north, &east);
        const TrackPoint off = at(north + 150, east - 90);
        recording.fixes[std::size_t(i)].latitude = off.latitude;
        recording.fixes[std::size_t(i)].longitude = off.longitude;
    }
    TrackFilter filter;
    filter.setMode(TrackFilter::Walking);
    double worst = 0;
    double firstTime = 0;
    for (std::size_t i = 0; i < recording.fixes.size(); ++i) {
        if (!filter.add(recording.fixes[i]))
            continue;
        if (firstTime == 0)
            firstTime = filter.estimate().time - kStartTime;
        double north = 0;
        double east = 0;
        metresOf(filter.estimate(), &north, &east);
        const Truth &now = truth[recording.truthIndex[i]];
        worst = std::max(worst, std::sqrt(square(north - now.north) + square(east - now.east)));
    }
    expectBelow("two first fixes 175 m off: largest error of any estimate, m", worst, 15);
    expectBelow("and the first estimate within, s", firstTime, 9);
}

/* Off by 40 m for half a minute, as with a reflection off a building: the
 * estimate may follow it or not, but must be back on the route soon after. */
void testLongOffset()
{
    const std::vector<Truth> truth = route({{240, 1.4, 0, 0}});
    Receiver receiver;
    receiver.jumpChance = 0;
    receiver.seed = 71;
    Disturbance disturbance;
    disturbance.offsetFrom = 60;
    disturbance.offsetSeconds = 30;
    disturbance.offset = 40;
    const Recording recording = record(truth, receiver, disturbance);
    TrackFilter filter;
    filter.setMode(TrackFilter::Walking);
    double worstAfter = 0;
    double lastBefore = 0;
    for (std::size_t i = 0; i < recording.fixes.size(); ++i) {
        if (!filter.add(recording.fixes[i]))
            continue;
        const int second = int(recording.truthIndex[i]);
        double north = 0;
        double east = 0;
        metresOf(filter.estimate(), &north, &east);
        const Truth &now = truth[recording.truthIndex[i]];
        const double error = std::sqrt(square(north - now.north) + square(east - now.east));
        if (second >= 60 + 30 + 15)
            worstAfter = std::max(worstAfter, error);
        lastBefore = second;
    }
    expectBelow("40 m off for 30 s: largest error from 15 s after, m", worstAfter, 12);
    expectNear("and fixes still taken at the end", lastBefore, 239, 0);
}

/* A minute without fixes in a tunnel, the car going on: the estimate must
 * pick the route up again at once. */
void testTunnel()
{
    const std::vector<Truth> truth = route({{120, 20.0, 0, 0}, {120, 20.0, 60, 0}});
    Receiver receiver;
    receiver.jumpChance = 0;
    receiver.seed = 81;
    Disturbance disturbance;
    disturbance.gapFrom = 100;
    disturbance.gapSeconds = 60;
    const Recording recording = record(truth, receiver, disturbance);
    TrackFilter filter;
    filter.setMode(TrackFilter::Driving);
    double worstAfter = 0;
    double before = -1;
    Track track;
    track.begin(kStartTime);
    for (std::size_t i = 0; i < recording.fixes.size(); ++i) {
        if (!filter.add(recording.fixes[i]))
            continue;
        track.addPoint(filter.estimate());
        if (before < 0)
            before = truth[recording.truthIndex[i]].distance;
        const int second = int(recording.truthIndex[i]);
        double north = 0;
        double east = 0;
        metresOf(filter.estimate(), &north, &east);
        const Truth &now = truth[recording.truthIndex[i]];
        if (second >= 165)
            worstAfter = std::max(worstAfter, std::sqrt(square(north - now.north) + square(east - now.east)));
    }
    const Truth &in = truth[99];
    const Truth &out = truth[160];
    const double straight = std::sqrt(square(out.north - in.north) + square(out.east - in.east));
    expectBelow("a minute in a tunnel: largest error from 5 s after, m", worstAfter, 30);
    expectNear("and the distance, the tunnel as a straight line", track.stateAt(track.duration()).distance,
               truth.back().distance - before - (out.distance - in.distance) + straight, 30);
}

/* A drive recorded as a walk, the type left as it was: worse, but the
 * route is still followed. */
void testWrongType()
{
    const std::vector<Truth> truth = route({{120, 14.0, 30, 0}, {60, 22.0, 75, 0},
                                            {200, 27.0, 120, 0}, {90, 12.0, 200, 0}});
    Receiver receiver;
    receiver.seed = 91;
    const Recording recording = record(truth, receiver);
    const Result filtered = replay(truth, recording, true, TrackFilter::Walking);
    const double distance = truth.back().distance;
    expectBelow("a drive filtered as a walk: distance error, %",
                100.0 * std::fabs(filtered.distance - distance) / distance, 10);
    expectBelow("and its largest position error, m", filtered.positionMax, 100);
    expectBelow("and points kept, share missing, %",
                100.0 * (1.0 - double(filtered.points) / recording.fixes.size()), 10);
}

/* Over many seeds, so that no tuning holds for one random sequence only. */
void testSeeds()
{
    double worstWalk = 0;
    double worstRun = 0;
    double rawWorstWalk = 0;
    double rawWorstRun = 0;
    for (unsigned seed = 100; seed < 120; ++seed) {
        Receiver receiver;
        receiver.seed = seed;
        const std::vector<Truth> walk = loops(1.4, 200, 100, 2, 0);
        const Recording walkRecording = record(walk, receiver);
        const double walked = walk.back().distance;
        const Result walkResult = replay(walk, walkRecording, true, TrackFilter::Walking);
        const Result walkRaw = replay(walk, walkRecording, false, TrackFilter::Walking);
        worstWalk = std::max(worstWalk, 100.0 * std::fabs(walkResult.distance - walked) / walked);
        rawWorstWalk = std::max(rawWorstWalk, 100.0 * std::fabs(walkRaw.distance - walked) / walked);

        const std::vector<Truth> run = loops(3.2, 400, 200, 2, 0);
        const Recording runRecording = record(run, receiver);
        const double ran = run.back().distance;
        const Result runResult = replay(run, runRecording, true, TrackFilter::Running);
        const Result runRaw = replay(run, runRecording, false, TrackFilter::Running);
        worstRun = std::max(worstRun, 100.0 * std::fabs(runResult.distance - ran) / ran);
        rawWorstRun = std::max(rawWorstRun, 100.0 * std::fabs(runRaw.distance - ran) / ran);
    }
    std::printf("     20 seeds: worst distance error walk raw %.1f %% filtered %.1f %%, run raw %.1f %% filtered %.1f %%\n",
                rawWorstWalk, worstWalk, rawWorstRun, worstRun);
    expectBelow("20 walks with outliers: worst distance error, %", worstWalk, 6);
    expectBelow("20 runs with outliers: worst distance error, %", worstRun, 5);
}

void testOutput()
{
    TrackFilter filter;
    expectNear("no estimate before any fix", filter.hasEstimate() ? 1 : 0, 0, 0);
    TrackPoint fix = at(0, 0);
    fix.accuracy = 5;
    fix.verticalAccuracy = 7;
    fix.speed = 0;
    fix.course = 0;
    for (int i = 0; i < 5; ++i) {
        fix.time = kStartTime + i;
        fix.altitude = i == 4 ? std::nan("") : 50.0;
        expectNear("no estimate before five agreeing fixes", filter.add(fix) ? 1 : 0, i == 4 ? 1 : 0, 0);
    }
    expectNear("an estimate after five", filter.hasEstimate() ? 1 : 0, 1, 0);
    expectNear("with their time", filter.estimate().time, kStartTime + 4, 0);
    expectNear("and the last fix's accuracies", filter.estimate().verticalAccuracy, 7, 0);
    expectNear("no altitude where the fix had none", std::isnan(filter.estimate().altitude) ? 1 : 0, 1, 0);
    expectNear("no course while standing", std::isnan(filter.estimate().course) ? 1 : 0, 1, 0);
    filter.reset();
    expectNear("none after a reset", filter.hasEstimate() ? 1 : 0, 0, 0);
}

} // namespace

int main()
{
    testOutput();
    testTypes();
    testPoorSignal();
    testNoSpeed();
    testStanding();
    testSetOff();
    testAltitude();
    testBadStart();
    testLongOffset();
    testTunnel();
    testWrongType();
    testSeeds();

    std::printf("%s\n", g_failures ? "FAILED" : "All passed");
    return g_failures ? 1 : 0;
}
