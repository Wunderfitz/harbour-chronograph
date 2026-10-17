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

#include "TrackChart.h"

#include "Track.h"

#include <algorithm>
#include <cmath>

namespace {

/* Below this, in metres per second, there is no pace: over half an hour per
 * kilometre. The same as Units.js's slowestPace, so the chart has a pace
 * wherever the page shows one. */
const double kSlowestSpeed = 1000.0 / 1800.0;

/* The share of the paces at either end the axis need not reach. */
const double kPaceOutliers = 0.05;

/* The least an axis spans: seconds per kilometre, metres, seconds. */
const double kMinimumPaceSpan = 60.0;
const double kMinimumElevationSpan = 20.0;
const double kMinimumLength = 60.0;

/* At most this many steps between grid lines, up and across: a chart on the
 * activity page is a few centimetres in each direction. */
const int kValueIntervals = 3;
const int kTimeIntervals = 4;

/* Steps an axis may take, in seconds - each a whole number of them that reads
 * well as minutes and seconds - or in metres. */
const std::vector<double> &paceSteps()
{
    static const std::vector<double> steps { 10, 15, 30, 60, 120, 300, 600, 900 };
    return steps;
}

const std::vector<double> &timeSteps()
{
    static const std::vector<double> steps {
        15, 30, 60, 120, 300, 600, 900, 1200, 1800,
        3600, 7200, 10800, 21600, 43200, 86400
    };
    return steps;
}

/* 1, 2, 5, 10, 20, 50... */
const std::vector<double> &elevationSteps()
{
    static std::vector<double> steps;
    if (steps.empty()) {
        for (double decade = 1; decade <= 10000; decade *= 10) {
            steps.push_back(decade);
            steps.push_back(2 * decade);
            steps.push_back(5 * decade);
        }
    }
    return steps;
}

/* Widens [low, high] to at least `span` around its middle. */
void widen(double &low, double &high, double span)
{
    if (high - low >= span)
        return;
    const double middle = (low + high) / 2;
    low = middle - span / 2;
    high = middle + span / 2;
}

} // namespace

TrackChart::TrackChart(Quantity quantity)
    : m_quantity(quantity)
{
}

TrackChart::Quantity TrackChart::quantity() const
{
    return m_quantity;
}

void TrackChart::update(const Track &track)
{
    if (track.size() < m_values.size()) {
        m_times.clear();
        m_values.clear();
    }
    for (std::size_t i = m_values.size(); i < track.size(); ++i) {
        m_times.push_back(track.elapsedAtPoint(i));
        if (m_quantity == Elevation) {
            m_values.push_back(track.altitudeAtPoint(i));
        } else {
            const double speed = track.speedAtPoint(i);
            m_values.push_back(speed >= kSlowestSpeed ? 1000.0 / speed : std::nan(""));
        }
    }
    fitAxes(track);
}

std::size_t TrackChart::size() const
{
    return m_values.size();
}

double TrackChart::time(std::size_t index) const
{
    return m_times[index];
}

double TrackChart::value(std::size_t index) const
{
    return m_values[index];
}

bool TrackChart::hasValues() const
{
    return m_hasValues;
}

double TrackChart::average() const
{
    return m_average;
}

const ChartAxis &TrackChart::timeAxis() const
{
    return m_timeAxis;
}

const ChartAxis &TrackChart::valueAxis() const
{
    return m_valueAxis;
}

ChartAxis TrackChart::fit(double low, double high, const std::vector<double> &steps,
                          int intervals)
{
    ChartAxis axis;
    if (!std::isfinite(low) || !std::isfinite(high) || high < low || steps.empty())
        return axis;
    intervals = std::max(1, intervals);
    const auto fits = [&](double step) {
        return std::ceil(high / step) - std::floor(low / step) <= intervals;
    };
    double step = steps.back();
    for (double candidate : steps) {
        if (fits(candidate)) {
            step = candidate;
            break;
        }
    }
    while (!fits(step))
        step *= 2;

    axis.from = std::floor(low / step) * step;
    axis.to = std::ceil(high / step) * step;
    // Both on the same step: one step's room above it.
    if (axis.to <= axis.from)
        axis.to = axis.from + step;
    const int count = int(std::lround((axis.to - axis.from) / step));
    for (int i = 0; i <= count; ++i)
        axis.ticks.push_back(axis.from + i * step);
    return axis;
}

void TrackChart::fitAxes(const Track &track)
{
    /* Time: the ends are the activity's, not whole steps, so that the line
     * goes all the way across. */
    m_timeAxis = ChartAxis();
    m_timeAxis.to = std::max(kMinimumLength, track.duration());
    double step = timeSteps().back();
    for (double candidate : timeSteps()) {
        if (std::floor(m_timeAxis.to / candidate) <= kTimeIntervals) {
            step = candidate;
            break;
        }
    }
    while (std::floor(m_timeAxis.to / step) > kTimeIntervals)
        step *= 2;
    const int count = int(std::floor(m_timeAxis.to / step));
    for (int i = 0; i <= count; ++i)
        m_timeAxis.ticks.push_back(i * step);

    m_average = std::nan("");
    if (m_quantity == Pace && !track.isEmpty() && track.duration() > 0) {
        const double speed = track.distanceAtPoint(track.size() - 1) / track.duration();
        if (speed >= kSlowestSpeed)
            m_average = 1000.0 / speed;
    }

    std::vector<double> values;
    values.reserve(m_values.size());
    for (double value : m_values) {
        if (!std::isnan(value))
            values.push_back(value);
    }
    m_hasValues = !values.empty();
    m_valueAxis = ChartAxis();
    if (!m_hasValues)
        return;

    double low;
    double high;
    if (m_quantity == Elevation) {
        const auto range = std::minmax_element(values.begin(), values.end());
        low = *range.first;
        high = *range.second;
        widen(low, high, kMinimumElevationSpan);
        m_valueAxis = fit(low, high, elevationSteps(), kValueIntervals);
        return;
    }

    const std::size_t last = values.size() - 1;
    const auto fastest = values.begin() + std::ptrdiff_t(std::floor(kPaceOutliers * last));
    const auto slowest = values.begin() + std::ptrdiff_t(std::ceil((1 - kPaceOutliers) * last));
    std::nth_element(values.begin(), fastest, values.end());
    low = *fastest;
    std::nth_element(values.begin(), slowest, values.end());
    high = *slowest;
    if (!std::isnan(m_average)) {
        low = std::min(low, m_average);
        high = std::max(high, m_average);
    }
    widen(low, high, kMinimumPaceSpan);
    // No pace is under nothing per kilometre, however fast the drive.
    if (low < 0) {
        high -= low;
        low = 0;
    }
    m_valueAxis = fit(low, high, paceSteps(), kValueIntervals);
    std::swap(m_valueAxis.from, m_valueAxis.to);
}
