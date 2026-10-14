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
 * Host-side checks for StepDetector, which has no Qt in it for exactly this
 * reason. Two kinds: synthetic signals pin down the behaviour the detector
 * promises (count a walk, ignore a table, a shake or a couple of steps, follow a
 * change of pace, not care about the data rate or orientation), and model
 * what real walks showed - echoes in the hand, a weak leg in a pocket, stairs,
 * rhythmic handling, the phone changing hands - without any recorded data:
 * every signal here is generated. The recordings of real walks are personal
 * data and stay in tests/raw-data/, which git ignores; where they are, they
 * are replayed as well (see testRecordings), elsewhere skipped.
 * Build and run from the project root:
 *
 *   g++ -std=c++11 -Wall -Wextra -O2 -Isrc tests/tst_stepdetector.cpp \
 *       src/StepDetector.cpp -o /tmp/tst_stepdetector && /tmp/tst_stepdetector
 */

#include "StepDetector.h"

#include <dirent.h>

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <fstream>
#include <random>
#include <sstream>
#include <string>
#include <vector>

namespace {

const double kPi = 3.14159265358979323846;
const double kGravity = 9.81;

int g_failures = 0;

struct Signal {
    double cadenceHz = 0.0;  // steps per second; 0 means standing still
    double amplitude = 0.0;  // m/s^2 of the bounce along gravity
    double noise = 0.15;     // m/s^2, standard deviation per axis
    double seconds = 0.0;
    double weakLeg = 1.0;    // every second step's bounce, as a share of the first's
};

/* Feeds `signal` at `rateHz` into `detector`, starting at `startUs`, with gravity
 * along the given unit axis. Each step is one period of the vertical bounce, with
 * a second harmonic so its top is not a clean sine. Returns the steps counted and
 * moves `startUs` on to where the signal ended. */
int feed(StepDetector &detector, const Signal &signal, double rateHz, std::uint64_t &startUs,
         std::mt19937 &random, double ax = 0.0, double ay = 0.0, double az = 1.0)
{
    std::normal_distribution<double> noise(0.0, signal.noise);
    const int samples = int(signal.seconds * rateHz);
    int steps = 0;
    for (int i = 0; i < samples; ++i) {
        const double t = i / rateHz;
        double bounce = 0.0;
        if (signal.cadenceHz > 0.0) {
            const double phase = 2.0 * kPi * signal.cadenceHz * t;
            const bool weak = int(signal.cadenceHz * t) % 2 == 1;
            bounce = (weak ? signal.weakLeg : 1.0) * signal.amplitude
                    * (std::sin(phase) + 0.3 * std::sin(2.0 * phase + 0.7));
        }
        const double along = kGravity + bounce;
        const std::uint64_t timestampUs = startUs + std::uint64_t(t * 1e6);
        steps += detector.addSample(timestampUs,
                                    along * ax + noise(random),
                                    along * ay + noise(random),
                                    along * az + noise(random));
    }
    startUs += std::uint64_t(signal.seconds * 1e6);
    return steps;
}

void expectNear(const char *name, int actual, int expected, int tolerance)
{
    const bool ok = std::abs(actual - expected) <= tolerance;
    std::printf("%s %-46s %4d steps (expected %d +/- %d)\n",
                ok ? "PASS" : "FAIL", name, actual, expected, tolerance);
    if (!ok)
        ++g_failures;
}

Signal still(double seconds)
{
    Signal s;
    s.seconds = seconds;
    return s;
}

Signal walk(double cadenceHz, double amplitude, double seconds)
{
    Signal s;
    s.cadenceHz = cadenceHz;
    s.amplitude = amplitude;
    s.seconds = seconds;
    return s;
}

void testStill()
{
    StepDetector detector;
    std::mt19937 random(1);
    std::uint64_t t = 1000000;
    expectNear("lying on a table, 5 min", feed(detector, still(300), 50, t, random), 0, 0);
}

void testWalk()
{
    StepDetector detector;
    std::mt19937 random(2);
    std::uint64_t t = 1000000;
    int steps = feed(detector, still(2), 50, t, random);
    steps += feed(detector, walk(1.8, 3.0, 60), 50, t, random);
    steps += feed(detector, still(5), 50, t, random);
    expectNear("walking 1.8 Hz, pocket, 60 s", steps, 108, 3);
}

void testGentleWalk()
{
    StepDetector detector;
    std::mt19937 random(3);
    std::uint64_t t = 1000000;
    int steps = feed(detector, still(2), 50, t, random);
    steps += feed(detector, walk(1.5, 1.6, 60), 50, t, random);
    expectNear("slow walk 1.5 Hz, in the hand, 60 s", steps, 90, 3);
}

void testRun()
{
    StepDetector detector;
    std::mt19937 random(4);
    std::uint64_t t = 1000000;
    int steps = feed(detector, still(2), 50, t, random);
    steps += feed(detector, walk(2.8, 9.0, 60), 50, t, random);
    expectNear("running 2.8 Hz, 60 s", steps, 168, 4);
}

void testRunThenWalk()
{
    StepDetector detector;
    std::mt19937 random(5);
    std::uint64_t t = 1000000;
    int steps = feed(detector, still(2), 50, t, random);
    steps += feed(detector, walk(2.8, 9.0, 30), 50, t, random);
    steps += feed(detector, walk(1.6, 2.0, 30), 50, t, random);
    /* The threshold follows the RMS of the last second, which still remembers
     * the run for the first two or three walking steps. What this guards against
     * is the walk after a run going unseen altogether. */
    expectNear("running 30 s, then a gentle walk 30 s", steps, 84 + 48, 6);
}

void testWalkThenRun()
{
    StepDetector detector;
    std::mt19937 random(11);
    std::uint64_t t = 1000000;
    int steps = feed(detector, still(3), 50, t, random);
    steps += feed(detector, walk(1.4, 3.0, 30), 50, t, random);
    steps += feed(detector, walk(2.8, 8.0, 30), 50, t, random);
    steps += feed(detector, still(3), 50, t, random);
    // The walk's rhythm must not mistake the run's quicker steps for echoes.
    expectNear("walking 30 s, then running 30 s", steps, 42 + 84, 4);
}

void testWeakLeg()
{
    /* Running with the phone in a back pocket, and on stairs with it in a front
     * pocket, one leg's steps hardly show: the peaks come once per stride. */
    StepDetector detector;
    std::mt19937 random(13);
    std::uint64_t t = 1000000;
    int steps = feed(detector, still(2), 50, t, random);
    Signal run = walk(2.8, 9.0, 40);
    run.weakLeg = 0.2;
    steps += feed(detector, run, 50, t, random);
    steps += feed(detector, still(3), 50, t, random);
    /* Without the signal's own beat this counts half. The strides that confirm
     * the run still count one step each - its beat is not known yet - so up to
     * six short is expected. */
    expectNear("running 2.8 Hz, one leg weak, 40 s", steps, 112, 7);
}

void testVerySlowWalk()
{
    StepDetector detector;
    std::mt19937 random(12);
    std::uint64_t t = 1000000;
    int steps = feed(detector, still(2), 50, t, random);
    steps += feed(detector, walk(0.9, 1.2, 60), 50, t, random);
    steps += feed(detector, still(3), 50, t, random);
    expectNear("very slow, soft walk 0.9 Hz, 60 s", steps, 54, 3);
}

void testShortWalk()
{
    StepDetector detector;
    std::mt19937 random(6);
    std::uint64_t t = 1000000;
    int steps = feed(detector, still(2), 50, t, random);
    steps += feed(detector, walk(1.8, 3.0, 2.0), 50, t, random);
    steps += feed(detector, still(5), 50, t, random);
    expectNear("three or four steps, then standing", steps, 0, 0);
}

void testShaking()
{
    StepDetector detector;
    std::mt19937 random(7);
    std::uniform_real_distribution<double> pause(0.05, 3.0);
    std::uniform_real_distribution<double> strength(3.0, 15.0);
    std::uint64_t t = 1000000;
    int steps = feed(detector, still(2), 50, t, random);
    // Irregular jolts: picked up, put down, waved about.
    for (int i = 0; i < 60; ++i) {
        Signal jolt = walk(4.0, strength(random), 0.25);
        steps += feed(detector, jolt, 50, t, random);
        steps += feed(detector, still(pause(random)), 50, t, random);
    }
    expectNear("60 irregular jolts", steps, 0, 2);
}

void testDataRates()
{
    const double rates[] = { 20, 30, 100 };
    for (double rate : rates) {
        StepDetector detector;
        std::mt19937 random(8);
        std::uint64_t t = 1000000;
        int steps = feed(detector, still(2), rate, t, random);
        steps += feed(detector, walk(1.8, 3.0, 60), rate, t, random);
        char name[64];
        std::snprintf(name, sizeof(name), "walking 60 s sampled at %d Hz", int(rate));
        expectNear(name, steps, 108, 3);
    }
}

void testOrientation()
{
    StepDetector detector;
    std::mt19937 random(9);
    std::uint64_t t = 1000000;
    const double d = 1.0 / std::sqrt(3.0);
    int steps = feed(detector, still(2), 50, t, random, d, d, d);
    steps += feed(detector, walk(1.8, 3.0, 60), 50, t, random, d, d, d);
    expectNear("walking, phone tilted on all axes", steps, 108, 3);
}

void testSensorPause()
{
    StepDetector detector;
    std::mt19937 random(10);
    std::uint64_t t = 1000000;
    int steps = feed(detector, still(2), 50, t, random);
    steps += feed(detector, walk(1.8, 3.0, 20), 50, t, random);
    t += 10000000; // sensorfw delivered nothing for ten seconds
    steps += feed(detector, walk(1.8, 3.0, 20), 50, t, random);
    expectNear("walk 20 s, sensor pause, walk 20 s", steps, 72, 4);
}

/* The signals below are shaped step by step rather than as a sine: each step is
 * one or more bumps (dips, if negative) after the foot lands, and steps vary a
 * little in timing and strength as real ones do. Their shapes model what a phone
 * feels in the hand, in a pocket or on stairs - none is taken from a recording. */
struct Bump {
    double atS;         // after the foot lands
    double amplitude;   // m/s^2; negative for a dip
    double widthS;      // standard deviation of the Gaussian
};

struct Gait {
    double stepS = 0.55;            // from one foot landing to the other
    std::vector<std::vector<Bump>> steps;   // repeating: one leg, the other, ...
    double timingJitter = 0.03;     // standard deviation, share of a step
    double strengthJitter = 0.1;    // standard deviation, share of a step's bumps
};

/* A stretch of acceleration along gravity, gravity itself left out, sampled at
 * rateHz from the stretch's start. */
struct Bounce {
    double rateHz;
    std::vector<double> values;

    Bounce(double rate, double seconds) : rateHz(rate), values(std::size_t(seconds * rate), 0.0) {}
    double seconds() const { return values.size() / rateHz; }

    void addBump(double centerS, double amplitude, double widthS)
    {
        const int first = std::max(0, int((centerS - 4.0 * widthS) * rateHz));
        const int last = std::min(int(values.size()) - 1, int((centerS + 4.0 * widthS) * rateHz));
        for (int i = first; i <= last; ++i) {
            const double d = (i / rateHz - centerS) / widthS;
            values[i] += amplitude * std::exp(-0.5 * d * d);
        }
    }

    /* Walks `gait` from fromS to toS and returns the steps taken. */
    int addGait(const Gait &gait, double fromS, double toS, std::mt19937 &random)
    {
        std::normal_distribution<double> timing(0.0, gait.timingJitter);
        std::normal_distribution<double> strength(0.0, gait.strengthJitter);
        int steps = 0;
        for (double t = fromS; t < toS; t += gait.stepS * (1.0 + timing(random))) {
            const std::vector<Bump> &bumps = gait.steps[steps % gait.steps.size()];
            const double factor = std::max(0.0, 1.0 + strength(random));
            for (const Bump &bump : bumps)
                addBump(t + bump.atS, factor * bump.amplitude, bump.widthS);
            ++steps;
        }
        return steps;
    }
};

/* Feeds `bounce` into `detector` like feed() does, the phone lying flat. The
 * bounce's own mean is taken out: the phone ends where it started. */
int feedBounce(StepDetector &detector, const Bounce &bounce, std::uint64_t &startUs,
               std::mt19937 &random, double noiseSd = 0.15)
{
    double mean = 0.0;
    for (double value : bounce.values)
        mean += value;
    mean /= bounce.values.size();
    std::normal_distribution<double> noise(0.0, noiseSd);
    int steps = 0;
    for (std::size_t i = 0; i < bounce.values.size(); ++i) {
        const std::uint64_t timestampUs = startUs + std::uint64_t(i / bounce.rateHz * 1e6);
        steps += detector.addSample(timestampUs, noise(random), noise(random),
                                    kGravity + bounce.values[i] - mean + noise(random));
    }
    startUs += std::uint64_t(bounce.seconds() * 1e6);
    return steps;
}

/* Replays `bounce` between a few seconds of lying still, with noise from `seed`. */
int replay(const Bounce &bounce, unsigned seed)
{
    StepDetector detector;
    std::mt19937 random(seed);
    std::uint64_t t = 1000000;
    int steps = feed(detector, still(3), 50, t, random);
    steps += feedBounce(detector, bounce, t, random);
    steps += feed(detector, still(4), 50, t, random);
    return steps;
}

/* Walks `gait` for `seconds` and checks the count against the steps taken. */
void expectGait(const char *name, const Gait &gait, double seconds, unsigned seed, int tolerance)
{
    std::mt19937 random(seed + 100);
    Bounce bounce(50, seconds + 2.0);
    const int taken = bounce.addGait(gait, 1.0, seconds + 1.0, random);
    expectNear(name, replay(bounce, seed), taken, tolerance);
}

void testSoftLeg()
{
    /* A slow walk at about 50 steps a minute in which one leg's steps are soft,
     * so that they only now and then clear the threshold: between counted steps
     * there is often a whole stride, 2.3 s. Guards the longer limit for a walk
     * under way (at 2 s it falls apart) and the threshold's floor. */
    Gait gait;
    gait.stepS = 1.15;
    gait.strengthJitter = 0.25;
    gait.steps = { { { 0.0, 2.7, 0.08 } }, { { 0.0, 1.3, 0.08 } } };
    expectGait("slow walk, one leg soft, 90 s", gait, 90, 1, 5);
}

void testSoftSteps()
{
    /* Steps that peak at about 1 m/s^2 - a soft walk on carpet, the phone in a
     * bag. The threshold's floor has to stay below them. */
    Gait gait;
    gait.stepS = 0.9;
    gait.steps = { { { 0.0, 1.1, 0.07 } } };
    expectGait("soft steps peaking at 1 m/s^2, 60 s", gait, 60, 1, 3);
}

void testFidgeting()
{
    /* Standing and shifting about: small bumps and dips at random, a few tenths
     * of a m/s^2 - above the noise of a phone lying still, below any step. */
    std::mt19937 random(203);
    std::uniform_real_distribution<double> pause(0.25, 1.5);
    std::uniform_real_distribution<double> strength(0.3, 0.7);
    std::uniform_real_distribution<double> sign(-1.0, 1.0);
    Bounce bounce(50, 120);
    for (double t = 1.0; t < 119.0; t += pause(random))
        bounce.addBump(t, (sign(random) < 0.0 ? -1.0 : 1.0) * strength(random), 0.08);
    expectNear("standing, fidgeting, 2 min", replay(bounce, 3), 0, 2);
}

void testStairs()
{
    /* Four flights of eight stairs with two level steps on each landing. Every
     * step on the stairs is a broad push and a sharp dip; one leg's push is weaker
     * and varies, so the peaks switch between steps and strides - while a flight
     * is still being confirmed as a walk, too. The beat has to bridge that. */
    std::mt19937 random(305);
    std::uniform_real_distribution<double> otherLeg(0.5, 1.2);
    std::normal_distribution<double> timing(0.0, 0.03);
    Bounce bounce(50, 4 * 8 * 0.5 + 3 * 2 * 0.55 + 4.0);
    int taken = 0;
    double t = 1.0;
    for (int flight = 0; flight < 4; ++flight) {
        for (int stair = 0; stair < 8; ++stair) {
            const double push = stair % 2 ? 2.5 * otherLeg(random) : 2.5;
            bounce.addBump(t, push, 0.07);
            bounce.addBump(t + 0.16, -1.2 * push, 0.03);
            ++taken;
            t += 0.5 * (1.0 + timing(random));
        }
        for (int step = 0; flight < 3 && step < 2; ++step) {
            bounce.addBump(t, 1.8, 0.06);
            ++taken;
            t += 0.55 * (1.0 + timing(random));
        }
    }
    expectNear("stairs, one leg's push weak, 4 flights", replay(bounce, 5), taken, 3);
}

void testSlowingDown()
{
    /* Six walks that slow down at once from 0.55 to 0.88 s a step. For a few
     * seconds the beat still says 0.55, and each interval is 1.6 of those - one
     * step, not two. Each change costs a step instead, which is the lesser
     * error. */
    std::mt19937 random(401);
    Gait brisk;
    brisk.stepS = 0.55;
    brisk.timingJitter = 0.015;
    brisk.steps = { { { 0.0, 3.0, 0.05 } } };
    Gait slow = brisk;
    slow.stepS = 0.88;
    Bounce bounce(50, 6 * 23 + 2);
    int taken = 0;
    for (int walk = 0; walk < 6; ++walk) {
        taken += bounce.addGait(brisk, 1 + 23 * walk, 11 + 23 * walk, random);
        taken += bounce.addGait(slow, 11 + 23 * walk, 21 + 23 * walk, random);
    }
    expectNear("six walks slowing down at once", replay(bounce, 1), taken - 6, 3);
}

void testRhythmicHandling()
{
    /* Handling the phone in rhythm: four quick jolts, then four at half that pace
     * that fit the first ones' beat as every second step - and a pause. Never six
     * regular steps in a row, so never a walk; the beat must not make one. */
    std::mt19937 random(504);
    std::uniform_real_distribution<double> pause(2.5, 3.5);
    std::uniform_real_distribution<double> strength(5.0, 8.0);
    std::normal_distribution<double> timing(0.0, 0.03);
    Bounce bounce(50, 180);
    for (double t = 1.0; t < 172.0; t += pause(random)) {
        for (int jolt = 0; jolt < 4; ++jolt) {
            bounce.addBump(t, strength(random), 0.05);
            t += 0.32 * (1.0 + timing(random));
        }
        t += 1.0;
        for (int jolt = 0; jolt < 4; ++jolt) {
            bounce.addBump(t, strength(random), 0.05);
            t += 0.64 * (1.0 + timing(random));
        }
    }
    expectNear("rhythmic handling in bursts, 3 min", replay(bounce, 4), 0, 2);
}

void testEchoes()
{
    /* In the hand every step is followed ~0.25 s later by a smaller bump, of a
     * strength that varies from step to step; the strongest clear the threshold.
     * They must not count. */
    std::mt19937 random(809);
    std::uniform_real_distribution<double> step(2.7, 3.3);
    std::uniform_real_distribution<double> echo(1.0, 2.4);
    std::normal_distribution<double> timing(0.0, 0.03);
    Bounce bounce(50, 64);
    int taken = 0;
    for (double t = 1.0; t < 62.0; t += 0.65 * (1.0 + timing(random))) {
        bounce.addBump(t, step(random), 0.05);
        bounce.addBump(t + 0.25, echo(random), 0.05);
        ++taken;
    }
    expectNear("in the hand, every step echoed, 60 s", replay(bounce, 9), taken, 5);
}

void testHandover()
{
    /* Running with the phone in a pocket - hard landings, one leg weaker - then
     * taking it out into the hand, running on, and putting it back. In the hand
     * the steps are a third as strong and the weak leg barely shows. Known
     * shortfall, accepted: the run is lost for a few seconds after the handover
     * and has to be confirmed again. */
    std::mt19937 random(701);
    Gait pocket;
    pocket.stepS = 0.39;
    pocket.steps = { { { 0.0, 14.0, 0.04 } }, { { 0.0, 5.0, 0.04 } } };
    Gait hand = pocket;
    hand.steps = { { { 0.0, 5.0, 0.04 } }, { { 0.0, 2.5, 0.04 } } };
    Bounce bounce(50, 50);
    int taken = bounce.addGait(pocket, 1, 16, random);
    bounce.addBump(16.3, 12.0, 0.15);   // pulled out of the pocket
    bounce.addBump(16.8, -8.0, 0.1);
    taken += bounce.addGait(hand, 16, 34, random);
    taken += bounce.addGait(pocket, 34, 48, random);
    expectNear("running, phone from pocket to hand and back", replay(bounce, 1), taken, 20);
}

/* The recordings of real walks are personal data: they and their parts files
 * live in tests/raw-data/, which git ignores, and exist only where they were
 * made. Where they are, each <name>.parts file there names its recording and
 * lists the parts to check, and they are replayed against the steps actually
 * taken; elsewhere this is skipped and the synthetic signals above are all
 * there is. */
const char kRawDataDirectory[] = "tests/raw-data";

struct Part {
    double fromS;   // seconds from the recording's first sample
    double toS;
    int steps;      // what was actually walked
    int tolerance;
    std::string name;
};

/* A parts file: "recording <file>" once, then one part per line - from, to,
 * steps, tolerance, name. "#" and "//" lines are comments. */
bool readParts(const std::string &path, std::string &recording, std::vector<Part> &parts)
{
    std::ifstream file(path.c_str());
    std::string line;
    while (std::getline(file, line)) {
        if (line.empty() || line[0] == '#' || line.compare(0, 2, "//") == 0)
            continue;
        std::istringstream fields(line);
        if (line.compare(0, 10, "recording ") == 0) {
            std::string keyword;
            fields >> keyword >> recording;
            continue;
        }
        Part part;
        if (!(fields >> part.fromS >> part.toS >> part.steps >> part.tolerance))
            return false;
        std::getline(fields >> std::ws, part.name);
        parts.push_back(part);
    }
    return !recording.empty() && !parts.empty();
}

void testRecording(const std::string &partsPath)
{
    std::string recording;
    std::vector<Part> parts;
    if (!readParts(partsPath, recording, parts)) {
        std::printf("FAIL cannot read %s\n", partsPath.c_str());
        ++g_failures;
        return;
    }
    const std::string path = std::string(kRawDataDirectory) + "/" + recording;
    std::ifstream file(path.c_str());
    if (!file) {
        std::printf("FAIL cannot open %s\n", path.c_str());
        ++g_failures;
        return;
    }

    StepDetector detector;
    std::vector<int> counted(parts.size(), 0);
    std::string line;
    std::getline(file, line); // header
    std::uint64_t firstUs = 0;
    bool first = true;
    while (std::getline(file, line)) {
        std::istringstream fields(line);
        std::uint64_t timestampUs;
        double x, y, z;
        char comma;
        if (!(fields >> timestampUs >> comma >> x >> comma >> y >> comma >> z))
            continue;
        if (first) {
            firstUs = timestampUs;
            first = false;
        }
        const int added = detector.addSample(timestampUs, x, y, z);
        const double atS = (timestampUs - firstUs) / 1e6;
        for (std::size_t i = 0; i < parts.size(); ++i) {
            if (atS >= parts[i].fromS && atS < parts[i].toS)
                counted[i] += added;
        }
    }

    for (std::size_t i = 0; i < parts.size(); ++i)
        expectNear(parts[i].name.c_str(), counted[i], parts[i].steps, parts[i].tolerance);
}

void testRecordings()
{
    std::vector<std::string> partsFiles;
    if (DIR *directory = opendir(kRawDataDirectory)) {
        while (dirent *entry = readdir(directory)) {
            const std::string name = entry->d_name;
            if (name.size() > 6 && name.compare(name.size() - 6, 6, ".parts") == 0)
                partsFiles.push_back(std::string(kRawDataDirectory) + "/" + name);
        }
        closedir(directory);
    }
    if (partsFiles.empty()) {
        std::printf("SKIP recordings of real walks: none in %s (personal data, local only)\n",
                    kRawDataDirectory);
        return;
    }
    std::sort(partsFiles.begin(), partsFiles.end());
    for (const std::string &partsFile : partsFiles)
        testRecording(partsFile);
}

} // namespace

int main()
{
    testStill();
    testWalk();
    testGentleWalk();
    testRun();
    testRunThenWalk();
    testWalkThenRun();
    testWeakLeg();
    testVerySlowWalk();
    testShortWalk();
    testShaking();
    testDataRates();
    testOrientation();
    testSensorPause();
    testSoftLeg();
    testSoftSteps();
    testFidgeting();
    testStairs();
    testSlowingDown();
    testRhythmicHandling();
    testEchoes();
    testHandover();
    testRecordings();

    std::printf("%s\n", g_failures ? "FAILED" : "All passed");
    return g_failures ? 1 : 0;
}
