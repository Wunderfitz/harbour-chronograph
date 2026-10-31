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

#ifndef TRACKCHART_H
#define TRACKCHART_H

#include <cstddef>
#include <limits>
#include <vector>

class Track;

/* One axis of a chart: the values at its two ends, and those in between worth
 * a grid line and a label. `from` is at the left or the bottom, `to` at the
 * right or the top, so an axis drawn upside down has from > to. */
struct ChartAxis {
    double from = 0;
    double to = 1;
    std::vector<double> ticks;
};

/**
 * One of a track's values over its time, for a chart on the activity page:
 * the pace, the speed or the elevation at each point, and axes that show them
 * well.
 *
 * Plain C++ without Qt, like Track, so tests/tst_trackchart.cpp drives it on
 * the host. The values are those the page shows at that point - the current
 * pace or speed, the smoothed altitude - against the time with the pauses left out, so
 * a chart never disagrees with the values beside it.
 *
 * - Pace is in seconds per kilometre, and there is none while standing or
 *   slower than half an hour per kilometre, as on the page. Its axis is upside
 *   down - faster is higher - and spans what the pace was most of the time:
 *   the slowest and fastest twentieth may run off its ends, or the seconds of
 *   setting off and stopping would squash all the rest flat. It always takes
 *   in the average.
 * - Speed is in kilometres per hour, 0 while standing. Its axis is the right
 *   way up and spans what the speed was most of the time, as the pace's does,
 *   but never less than 5 km/h, and never below 0. It takes in the average
 *   too.
 * - Elevation is in metres. Its axis spans all of it, but never less than
 *   20 m, so that a receiver's wander on the flat does not look like hills.
 * - Time runs from the start to the end - to the last point while recording -
 *   over at least a minute.
 */
class TrackChart
{
public:
    enum Quantity { Pace, Elevation, Speed };

    explicit TrackChart(Quantity quantity = Pace);

    Quantity quantity() const;

    /* Takes over the points the track has gained since the last call - all of
     * them again if it has fewer, a different track - and fits the axes. */
    void update(const Track &track);

    std::size_t size() const;
    /* Seconds into the activity, pauses left out. */
    double time(std::size_t index) const;
    /* NaN where there is none. */
    double value(std::size_t index) const;
    bool hasValues() const;
    /* The whole activity's average pace or speed; NaN for none, and for
     * elevation. */
    double average() const;

    const ChartAxis &timeAxis() const;
    const ChartAxis &valueAxis() const;

    /* From `low` to `high` in the smallest of `steps` (ascending) that takes
     * at most `intervals` of them, both ends on a whole step; failing all, in
     * the largest doubled until it does. */
    static ChartAxis fit(double low, double high, const std::vector<double> &steps,
                         int intervals);

private:
    void fitAxes(const Track &track);

    Quantity m_quantity;
    std::vector<double> m_times;
    std::vector<double> m_values;
    bool m_hasValues = false;
    double m_average = std::numeric_limits<double>::quiet_NaN();
    ChartAxis m_timeAxis;
    ChartAxis m_valueAxis;
};

#endif // TRACKCHART_H
