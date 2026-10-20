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

#include "Track.h"

#include <algorithm>
#include <cmath>

/* None of the constants below has been tuned against recorded activities yet;
 * they are what a receiver's usual noise suggests. */

namespace {

/* The distance moves on in steps of at least this, in metres. A receiver
 * standing still wanders by a few metres; walking covers this in 3-4 s, so the
 * straight lines between steps cut no corner worth speaking of. */
const double kMinStep = 5.0;

/* The receiver's speeds are averaged over this many seconds. */
const double kSpeedWindow = 5.0;

/* Without them, the speed is the distance over this many seconds - longer, as
 * the distance moves in steps of kMinStep. */
const double kDistanceSpeedWindow = 10.0;

/* A point older than this says nothing about the current speed any more: no
 * fix for that long, and the speed reads 0. */
const double kSpeedStaleAfter = 5.0;

/* Time constant of the altitude's smoothing, in seconds. */
const double kAltitudeSmoothing = 5.0;

/* The climb counts once the smoothed altitude has risen this many metres above
 * where it last turned, and a descent of as much ends it. */
const double kClimbHysteresis = 3.0;

/* Mean radius of the earth, metres. */
const double kEarthRadius = 6371008.8;

const double kPi = 3.14159265358979323846;

double radians(double degrees)
{
    return degrees * kPi / 180.0;
}

} // namespace

const double Track::kMovingSpeed = 0.5;

void Track::begin(double time)
{
    if (isOpen())
        return;
    TrackSegment segment;
    segment.begin = time;
    segment.firstPoint = m_points.size();
    m_segments.push_back(segment);
    m_anchor = -1;
}

void Track::end(double time)
{
    if (!isOpen())
        return;
    TrackSegment &segment = m_segments.back();
    // Never before its own last point, nor before it began.
    double end = std::max(time, segment.begin);
    if (m_points.size() > segment.firstPoint)
        end = std::max(end, m_points.back().time);
    segment.end = end;
}

void Track::endAt(double time)
{
    if (!isOpen())
        return;
    TrackSegment &segment = m_segments.back();
    time = std::max(time, segment.begin);
    std::size_t kept = m_points.size();
    while (kept > segment.firstPoint && m_points[kept - 1].time > time)
        --kept;
    m_points.resize(kept);
    m_derived.resize(kept);
    m_climbBase = kept > 0 ? m_derived.back().climbBase : std::nan("");
    segment.end = time;
}

bool Track::isOpen() const
{
    return !m_segments.empty() && std::isnan(m_segments.back().end);
}

bool Track::isEmpty() const
{
    return m_points.empty();
}

bool Track::addPoint(const TrackPoint &point)
{
    if (!isOpen())
        return false;
    const TrackSegment &segment = m_segments.back();
    if (point.time < segment.begin || (!m_points.empty() && point.time < m_points.back().time))
        return false;

    Derived derived;
    derived.elapsed = elapsedAt(point.time);

    const Derived *previous = m_derived.empty() ? nullptr : &m_derived.back();
    derived.distance = previous ? previous->distance : 0.0;
    derived.elevationGain = previous ? previous->elevationGain : 0.0;
    derived.altitude = previous ? previous->altitude : std::nan("");

    const int index = int(m_points.size());
    if (m_anchor < 0) {
        m_anchor = index;
    } else {
        const TrackPoint &anchor = m_points[std::size_t(m_anchor)];
        const bool standing = !std::isnan(point.speed) && point.speed < kMovingSpeed;
        const double step = distanceBetween(anchor.latitude, anchor.longitude,
                                            point.latitude, point.longitude);
        if (!standing && step >= kMinStep) {
            derived.distance += step;
            m_anchor = index;
        }
    }

    if (!std::isnan(point.altitude)) {
        if (std::isnan(derived.altitude)) {
            derived.altitude = point.altitude;
        } else {
            const double dt = previous ? std::max(0.0, derived.elapsed - previous->elapsed) : 0.0;
            const double weight = 1.0 - std::exp(-dt / kAltitudeSmoothing);
            derived.altitude += weight * (point.altitude - derived.altitude);
        }
        if (std::isnan(m_climbBase)) {
            m_climbBase = derived.altitude;
        } else if (derived.altitude - m_climbBase >= kClimbHysteresis) {
            derived.elevationGain += derived.altitude - m_climbBase;
            m_climbBase = derived.altitude;
        } else if (m_climbBase - derived.altitude >= kClimbHysteresis) {
            m_climbBase = derived.altitude;
        }
    }

    derived.climbBase = m_climbBase;
    m_points.push_back(point);
    m_derived.push_back(derived);
    m_derived.back().speed = smoothSpeed(m_points.size() - 1);
    return true;
}

double Track::elapsedAt(double time) const
{
    double elapsed = 0;
    for (const TrackSegment &segment : m_segments) {
        if (time <= segment.begin)
            break;
        const double end = std::isnan(segment.end) ? time : std::min(time, segment.end);
        elapsed += end - segment.begin;
    }
    return elapsed;
}

double Track::duration() const
{
    if (m_segments.empty())
        return 0;
    if (!isOpen())
        return elapsedAt(m_segments.back().end);
    const TrackSegment &segment = m_segments.back();
    return elapsedAt(m_points.size() > segment.firstPoint ? m_points.back().time : segment.begin);
}

double Track::startTime() const
{
    return m_segments.empty() ? std::nan("") : m_segments.front().begin;
}

TrackState Track::stateAt(double elapsed) const
{
    TrackState state;
    state.elapsed = std::max(0.0, elapsed);
    state.point = pointBefore(state.elapsed);
    if (state.point < 0)
        return state;

    const std::size_t index = std::size_t(state.point);
    const Derived &derived = m_derived[index];
    state.distance = derived.distance;
    state.altitude = derived.altitude;
    state.elevationGain = derived.elevationGain;
    state.averageSpeed = state.elapsed > 0 ? state.distance / state.elapsed : 0;

    /* The speed holds while the point is fresh and its segment still going: a
     * pause takes no elapsed time, so "fresh" alone would carry the speed from
     * before a pause into the first seconds after it. */
    const TrackSegment &segment = m_segments[segmentOf(index)];
    const bool current = state.elapsed - derived.elapsed <= kSpeedStaleAfter
            && (std::isnan(segment.end) || state.elapsed <= elapsedAt(segment.end));
    state.speed = current ? derived.speed : 0;
    return state;
}

std::size_t Track::size() const
{
    return m_points.size();
}

const TrackPoint &Track::point(std::size_t index) const
{
    return m_points[index];
}

double Track::elapsedAtPoint(std::size_t index) const
{
    return m_derived[index].elapsed;
}

double Track::distanceAtPoint(std::size_t index) const
{
    return m_derived[index].distance;
}

double Track::speedAtPoint(std::size_t index) const
{
    return m_derived[index].speed;
}

double Track::altitudeAtPoint(std::size_t index) const
{
    return m_derived[index].altitude;
}

const std::vector<TrackSegment> &Track::segments() const
{
    return m_segments;
}

/* Haversine: exact on a sphere, and the sphere is within half a percent of the
 * ellipsoid - well below what a receiver gets right. */
double Track::distanceBetween(double latitude1, double longitude1,
                              double latitude2, double longitude2)
{
    const double dLatitude = radians(latitude2 - latitude1);
    const double dLongitude = radians(longitude2 - longitude1);
    const double a = std::sin(dLatitude / 2) * std::sin(dLatitude / 2)
            + std::cos(radians(latitude1)) * std::cos(radians(latitude2))
              * std::sin(dLongitude / 2) * std::sin(dLongitude / 2);
    return 2 * kEarthRadius * std::atan2(std::sqrt(a), std::sqrt(1 - a));
}

/* The current speed at the point just added, from its own segment only. */
double Track::smoothSpeed(std::size_t index) const
{
    const std::size_t first = m_segments.back().firstPoint;
    const double elapsed = m_derived[index].elapsed;

    if (!std::isnan(m_points[index].speed)) {
        double sum = 0;
        int count = 0;
        for (std::size_t i = index + 1; i-- > first;) {
            if (elapsed - m_derived[i].elapsed > kSpeedWindow)
                break;
            if (!std::isnan(m_points[i].speed)) {
                sum += m_points[i].speed;
                ++count;
            }
        }
        const double speed = sum / count;
        return speed < kMovingSpeed ? 0 : speed;
    }

    std::size_t from = index;
    while (from > first && elapsed - m_derived[from - 1].elapsed <= kDistanceSpeedWindow)
        --from;
    const double span = elapsed - m_derived[from].elapsed;
    if (span < 1.0)
        return 0;
    const double speed = (m_derived[index].distance - m_derived[from].distance) / span;
    return speed < kMovingSpeed ? 0 : speed;
}

/* The segment a point was recorded in: the last one begun before it. */
std::size_t Track::segmentOf(std::size_t index) const
{
    std::size_t segment = 0;
    while (segment + 1 < m_segments.size() && m_segments[segment + 1].firstPoint <= index)
        ++segment;
    return segment;
}

/* The last point at or before `elapsed`, -1 for none. */
int Track::pointBefore(double elapsed) const
{
    const auto after = std::upper_bound(
            m_derived.begin(), m_derived.end(), elapsed,
            [](double value, const Derived &derived) { return value < derived.elapsed; });
    return int(after - m_derived.begin()) - 1;
}
