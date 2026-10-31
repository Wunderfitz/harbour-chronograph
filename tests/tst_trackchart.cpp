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
 * Host-side checks for TrackChart, which has no Qt in it for that reason: the
 * pace, speed and elevation at each point, the average pace and speed, and
 * axes that take in what matters - with the pace upside down, setting off and
 * stopping left to run off the edge, and the flat not made to look like
 * hills. Every track here
 * is generated, a fix a second. Build and run from the project root:
 *
 *   g++ -std=c++11 -Wall -Wextra -O2 -Isrc tests/tst_trackchart.cpp \
 *       src/TrackChart.cpp src/Track.cpp -o /tmp/tst_trackchart && /tmp/tst_trackchart
 */

#include "Track.h"
#include "TrackChart.h"

#include <cmath>
#include <cstdio>

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

void expectBetween(const char *name, double actual, double low, double high)
{
    const bool ok = actual >= low && actual <= high;
    std::printf("%s %-58s %10.2f (expected %.2f to %.2f)\n", ok ? "PASS" : "FAIL",
                name, actual, low, high);
    if (!ok)
        ++g_failures;
}

/* Where the axis puts `value`: 0 at `from`, 1 at `to`. */
double place(const ChartAxis &axis, double value)
{
    return (value - axis.from) / (axis.to - axis.from);
}

/* Every tick on the axis, in steps of one size. */
bool ticksEven(const ChartAxis &axis)
{
    if (axis.ticks.size() < 2)
        return false;
    const double step = axis.ticks[1] - axis.ticks[0];
    for (std::size_t i = 0; i < axis.ticks.size(); ++i) {
        const double tick = axis.ticks[i];
        if (place(axis, tick) < -1e-9 || place(axis, tick) > 1 + 1e-9)
            return false;
        if (i > 0 && std::fabs(tick - axis.ticks[i - 1] - step) > 1e-9)
            return false;
    }
    return true;
}

/* A fix `north` metres up from the reference point. */
TrackPoint fix(double time, double north, double speed, double altitude)
{
    TrackPoint point;
    point.time = time;
    point.latitude = kLatitude + north / kMetresPerDegree;
    point.longitude = kLongitude;
    point.accuracy = 5.0;
    point.speed = speed;
    point.altitude = altitude;
    return point;
}

/* Goes north at `speed` for `seconds`, one fix a second, at `altitude`. */
void go(Track &track, double &time, double &north, double speed, int seconds,
        double altitude = 100.0)
{
    for (int i = 0; i < seconds; ++i) {
        time += 1.0;
        north += speed;
        track.addPoint(fix(time, north, speed, altitude));
    }
}

void testEmpty()
{
    Track track;
    TrackChart chart;
    chart.update(track);
    expectNear("empty: no values", chart.hasValues() ? 1 : 0, 0, 0);
    expectNear("empty: no average", std::isnan(chart.average()) ? 1 : 0, 1, 0);
    expectNear("empty: time spans a minute", chart.timeAxis().to, 60, 0);
    expectNear("empty: time ticks every 15 s", double(chart.timeAxis().ticks.size()), 5, 0);
}

void testSteadyRun()
{
    Track track;
    double time = kStartTime;
    double north = 0;
    track.begin(time);
    go(track, time, north, 3.0, 1800);
    track.end(time);

    TrackChart chart(TrackChart::Pace);
    chart.update(track);
    const ChartAxis &axis = chart.valueAxis();
    expectNear("steady run: pace at a point, s/km", chart.value(900), 1000.0 / 3.0, 0.01);
    expectNear("steady run: average pace, s/km", chart.average(), 1000.0 / 3.0, 1.0);
    expectNear("steady run: pace upside down (faster higher)", axis.to < axis.from ? 1 : 0, 1, 0);
    expectBetween("steady run: pace spans a minute or two", axis.from - axis.to, 60, 120);
    expectBetween("steady run: the pace well inside the axis", place(axis, 1000.0 / 3.0), 0.1, 0.9);
    expectNear("steady run: pace ticks even and on the axis", ticksEven(axis) ? 1 : 0, 1, 0);
    expectBetween("steady run: pace ticks", double(axis.ticks.size()), 3, 4);
    expectNear("steady run: time to the end", chart.timeAxis().to, 1800, 0.01);
    expectNear("steady run: time ticks every 10 min", chart.timeAxis().ticks[1], 600, 0);
    expectNear("steady run: time ticks", double(chart.timeAxis().ticks.size()), 4, 0);
}

/* Stopping at lights: the seconds of slowing down and setting off pass through
 * any pace there is, and must not stretch the axis over them. */
void testStops()
{
    Track track;
    double time = kStartTime;
    double north = 0;
    track.begin(time);
    for (int stop = 0; stop < 10; ++stop) {
        go(track, time, north, 3.0, 150);
        go(track, time, north, 2.0, 1);
        go(track, time, north, 1.0, 1);
        go(track, time, north, 0.0, 15);
        go(track, time, north, 1.0, 1);
        go(track, time, north, 2.0, 1);
    }
    track.end(time);

    TrackChart chart(TrackChart::Pace);
    chart.update(track);
    double slowest = 0;
    int none = 0;
    for (std::size_t i = 0; i < chart.size(); ++i) {
        if (std::isnan(chart.value(i)))
            ++none;
        else
            slowest = std::max(slowest, chart.value(i));
    }
    const ChartAxis &axis = chart.valueAxis();
    expectBetween("stops: no pace while standing (points)", none, 100, 170);
    expectBetween("stops: slowest pace on the way, s/km", slowest, 700, 1800);
    expectBetween("stops: the axis stays near the running pace, s/km", axis.from, 360, 480);
    expectBetween("stops: running pace inside the axis", place(axis, 1000.0 / 3.0), 0.1, 0.9);
    expectBetween("stops: average inside the axis", place(axis, chart.average()), 0, 1);
}

void testStanding()
{
    Track track;
    double time = kStartTime;
    double north = 0;
    track.begin(time);
    go(track, time, north, 0.0, 60);
    TrackChart chart(TrackChart::Pace);
    chart.update(track);
    expectNear("standing: no pace", chart.hasValues() ? 1 : 0, 0, 0);
    expectNear("standing: no average", std::isnan(chart.average()) ? 1 : 0, 1, 0);
    expectNear("standing: no pace ticks", double(chart.valueAxis().ticks.size()), 0, 0);
}

/* At 150 km/h the pace is 24 s/km: the axis must not go below nothing. */
void testFastDrive()
{
    Track track;
    double time = kStartTime;
    double north = 0;
    track.begin(time);
    go(track, time, north, 150 / 3.6, 600);
    TrackChart chart(TrackChart::Pace);
    chart.update(track);
    expectNear("fast drive: the top of the axis at 0:00", chart.valueAxis().to, 0, 0);
    expectBetween("fast drive: pace inside the axis", place(chart.valueAxis(), 24), 0.1, 0.9);
}

/* A ride at 20 km/h with a sprint and a slow climb now and then: the speed
 * the right way up, the sprints and climbs inside the axis or just off it. */
void testSteadyRide()
{
    Track track;
    double time = kStartTime;
    double north = 0;
    track.begin(time);
    for (int lap = 0; lap < 6; ++lap) {
        go(track, time, north, 20 / 3.6, 240);
        go(track, time, north, 30 / 3.6, 30);
        go(track, time, north, 12 / 3.6, 30);
    }
    track.end(time);

    TrackChart chart(TrackChart::Speed);
    chart.update(track);
    const ChartAxis &axis = chart.valueAxis();
    expectNear("steady ride: speed at a point, km/h", chart.value(100), 20, 0.01);
    expectBetween("steady ride: average speed, km/h", chart.average(), 19, 21);
    expectNear("steady ride: speed the right way up", axis.to > axis.from ? 1 : 0, 1, 0);
    expectBetween("steady ride: the axis from 10 km/h or below", axis.from, 0, 10);
    expectBetween("steady ride: the axis to 30 km/h or above", axis.to, 30, 40);
    expectBetween("steady ride: the speed well inside the axis", place(axis, 20), 0.1, 0.9);
    expectNear("steady ride: speed ticks even and on the axis", ticksEven(axis) ? 1 : 0, 1, 0);
    expectBetween("steady ride: speed ticks", double(axis.ticks.size()), 3, 4);
}

/* Stopping at lights: the speed falls to 0, which the axis takes in, as the
 * stops are more than a twentieth of the time. */
void testSpeedStops()
{
    Track track;
    double time = kStartTime;
    double north = 0;
    track.begin(time);
    for (int stop = 0; stop < 10; ++stop) {
        go(track, time, north, 6.0, 150);
        go(track, time, north, 3.0, 2);
        go(track, time, north, 0.0, 15);
        go(track, time, north, 3.0, 2);
    }
    track.end(time);

    TrackChart chart(TrackChart::Speed);
    chart.update(track);
    int standing = 0;
    for (std::size_t i = 0; i < chart.size(); ++i) {
        if (chart.value(i) == 0)
            ++standing;
    }
    const ChartAxis &axis = chart.valueAxis();
    expectBetween("speed stops: 0 km/h while standing (points)", standing, 80, 170);
    expectNear("speed stops: the axis from 0", axis.from, 0, 0);
    expectBetween("speed stops: riding speed inside the axis", place(axis, 6.0 * 3.6), 0.5, 1);
    expectBetween("speed stops: average inside the axis", place(axis, chart.average()), 0, 1);
}

/* Standing has a speed, 0, unlike a pace: the axis from 0 over 5 km/h. */
void testSpeedStanding()
{
    Track track;
    double time = kStartTime;
    double north = 0;
    track.begin(time);
    go(track, time, north, 0.0, 60);
    TrackChart chart(TrackChart::Speed);
    chart.update(track);
    expectNear("speed standing: a speed", chart.hasValues() ? 1 : 0, 1, 0);
    expectNear("speed standing: average 0 km/h", chart.average(), 0, 0);
    expectNear("speed standing: the axis from 0", chart.valueAxis().from, 0, 0);
    expectBetween("speed standing: the axis to 5 km/h or so", chart.valueAxis().to, 5, 6);
}

void testSpeedFastDrive()
{
    Track track;
    double time = kStartTime;
    double north = 0;
    track.begin(time);
    go(track, time, north, 150 / 3.6, 600);
    TrackChart chart(TrackChart::Speed);
    chart.update(track);
    expectBetween("speed fast drive: 150 km/h inside the axis",
                  place(chart.valueAxis(), 150), 0.1, 0.9);
    expectNear("speed fast drive: ticks even and on the axis",
               ticksEven(chart.valueAxis()) ? 1 : 0, 1, 0);
}

void testFlat()
{
    Track track;
    double time = kStartTime;
    double north = 0;
    track.begin(time);
    for (int i = 0; i < 600; ++i)
        go(track, time, north, 1.4, 1, 100.0 + 2.0 * std::sin(i / 60.0));
    TrackChart chart(TrackChart::Elevation);
    chart.update(track);
    const ChartAxis &axis = chart.valueAxis();
    expectNear("flat: elevation the right way up", axis.to > axis.from ? 1 : 0, 1, 0);
    expectBetween("flat: elevation spans at least 20 m", axis.to - axis.from, 20, 30);
    expectBetween("flat: the walk inside the axis", place(axis, 100), 0.25, 0.75);
    expectNear("flat: elevation ticks even and on the axis", ticksEven(axis) ? 1 : 0, 1, 0);
    expectNear("flat: no average for elevation", std::isnan(chart.average()) ? 1 : 0, 1, 0);
}

void testClimb()
{
    Track track;
    double time = kStartTime;
    double north = 0;
    track.begin(time);
    for (int i = 0; i < 1200; ++i)
        go(track, time, north, 1.4, 1, 120.0 + 230.0 * i / 1199.0);
    TrackChart chart(TrackChart::Elevation);
    chart.update(track);
    const ChartAxis &axis = chart.valueAxis();
    expectNear("climb: elevation at the start", chart.value(0), 120, 0.01);
    expectNear("climb: the axis from 100 m", axis.from, 100, 0);
    expectNear("climb: the axis to 400 m", axis.to, 400, 0);
    expectNear("climb: elevation ticks every 100 m", double(axis.ticks.size()), 4, 0);
}

void testNoAltitude()
{
    Track track;
    double time = kStartTime;
    double north = 0;
    track.begin(time);
    go(track, time, north, 3.0, 60, std::nan(""));
    TrackChart chart(TrackChart::Elevation);
    chart.update(track);
    expectNear("no altitude: no elevation", chart.hasValues() ? 1 : 0, 0, 0);
}

/* The chart's time is the activity's, pauses left out. */
void testPause()
{
    Track track;
    double time = kStartTime;
    double north = 0;
    track.begin(time);
    go(track, time, north, 3.0, 300);
    track.end(time);
    time += 600;
    track.begin(time);
    go(track, time, north, 3.0, 300);
    track.end(time);
    TrackChart chart(TrackChart::Pace);
    chart.update(track);
    expectNear("pause: last point's time leaves it out", chart.time(chart.size() - 1), 600, 0.01);
    expectNear("pause: time to the end", chart.timeAxis().to, 600, 0.01);
}

/* While recording the chart takes the points as they come; it must end where
 * a chart of the whole track does. Then a shorter track: another activity. */
void testIncremental()
{
    Track track;
    double time = kStartTime;
    double north = 0;
    TrackChart live(TrackChart::Pace);
    track.begin(time);
    for (int i = 0; i < 400; ++i) {
        go(track, time, north, i < 200 ? 3.0 : 2.0, 1);
        live.update(track);
    }
    TrackChart whole(TrackChart::Pace);
    whole.update(track);
    double difference = 0;
    for (std::size_t i = 0; i < whole.size(); ++i)
        difference = std::max(difference, std::fabs(live.value(i) - whole.value(i)));
    expectNear("incremental: as many points", double(live.size()), double(whole.size()), 0);
    expectNear("incremental: the same paces", difference, 0, 1e-9);
    expectNear("incremental: the same axis", live.valueAxis().from - whole.valueAxis().from, 0, 0);

    Track other;
    time = kStartTime;
    north = 0;
    other.begin(time);
    go(other, time, north, 4.0, 30);
    live.update(other);
    expectNear("another track: starts over", double(live.size()), 30, 0);
    expectNear("another track: its pace", live.value(20), 250, 0.01);
}

void testFit()
{
    const std::vector<double> steps { 1, 2, 5, 10, 20, 50 };
    ChartAxis axis = TrackChart::fit(95, 115, steps, 3);
    expectNear("fit: 95-115 from 90", axis.from, 90, 0);
    expectNear("fit: 95-115 to 120", axis.to, 120, 0);
    expectNear("fit: 95-115 in steps of 10", axis.ticks[1] - axis.ticks[0], 10, 0);
    axis = TrackChart::fit(0, 1000, steps, 3);
    expectNear("fit: beyond the steps, the largest doubled", axis.ticks[1] - axis.ticks[0], 400, 0);
    axis = TrackChart::fit(50, 50, steps, 3);
    expectNear("fit: a single value gets room", axis.to - axis.from, 1, 0);
    axis = TrackChart::fit(std::nan(""), 10, steps, 3);
    expectNear("fit: nothing to fit, no ticks", double(axis.ticks.size()), 0, 0);
}

} // namespace

int main()
{
    testEmpty();
    testSteadyRun();
    testStops();
    testStanding();
    testFastDrive();
    testSteadyRide();
    testSpeedStops();
    testSpeedStanding();
    testSpeedFastDrive();
    testFlat();
    testClimb();
    testNoAltitude();
    testPause();
    testIncremental();
    testFit();

    std::printf("%s\n", g_failures ? "FAILED" : "All passed");
    return g_failures ? 1 : 0;
}
