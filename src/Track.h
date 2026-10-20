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

#ifndef TRACK_H
#define TRACK_H

#include <cstddef>
#include <limits>
#include <vector>

/* One position fix: as the receiver gave it, or as TrackFilter smoothed it.
 * Whatever it did not say is NaN. */
struct TrackPoint {
    /* Seconds since the epoch, UTC. */
    double time = 0;
    /* Degrees, WGS 84. */
    double latitude = 0;
    double longitude = 0;
    /* Metres. */
    double altitude = std::numeric_limits<double>::quiet_NaN();
    /* Horizontal and vertical, metres. */
    double accuracy = std::numeric_limits<double>::quiet_NaN();
    double verticalAccuracy = std::numeric_limits<double>::quiet_NaN();
    /* Over ground, metres per second - the receiver's own, which is far steadier
     * than one worked out from the positions. */
    double speed = std::numeric_limits<double>::quiet_NaN();
    /* Degrees clockwise from true north. */
    double course = std::numeric_limits<double>::quiet_NaN();
};

/* A stretch of recording between start or resume and pause or stop. */
struct TrackSegment {
    double begin = 0;
    /* NaN while the segment is still being recorded. */
    double end = std::numeric_limits<double>::quiet_NaN();
    /* Index of its first point; equal to the next segment's when it has none. */
    std::size_t firstPoint = 0;
};

/* What a track says at one moment of it. */
struct TrackState {
    /* Seconds since the start, pauses left out. */
    double elapsed = 0;
    /* Metres. */
    double distance = 0;
    /* Metres per second, now and since the start. 0 while standing. */
    double speed = 0;
    double averageSpeed = 0;
    /* Smoothed, metres; NaN until a fix brings one. */
    double altitude = std::numeric_limits<double>::quiet_NaN();
    /* Metres climbed since the start. */
    double elevationGain = 0;
    /* The latest point at that moment, -1 for none yet. */
    int point = -1;
};

/**
 * An activity as recorded: its segments and the fixes within them, and what
 * follows from those - the time, the distance, the speed and the climb at each
 * point.
 *
 * Plain C++ without Qt, like StepDetector, so tests/tst_track.cpp drives it on
 * the host. Live recording and replay go through the same calls - begin(),
 * addPoint() and end() in the order things happened - so a replay of a stored
 * activity shows exactly what was shown while it was recorded.
 *
 * - The time counts only the segments; pauses are left out.
 * - The distance adds up straight lines between points at least a few metres
 *   apart, so that the wander of a receiver standing still adds nothing, and a
 *   fix that says it is standing does not move it on. Nothing is counted across
 *   a pause.
 * - The current speed is the receiver's, averaged over a few seconds; for fixes
 *   without one, the distance over the last few seconds.
 * - The altitude is smoothed and the climb counted with hysteresis, as a
 *   receiver's altitude wanders by metres even standing still.
 */
class Track
{
public:
    /* Starts a segment: the start, or a resume after a pause. */
    void begin(double time);
    /* Ends the current one: a pause, or the stop. */
    void end(double time);
    /* Ends it at a moment that may lie behind its latest points, which are
     * dropped: for a pause noticed only some time after it began. Never
     * before the segment began. */
    void endAt(double time);
    bool isOpen() const;
    bool isEmpty() const;

    /* Adds a fix to the open segment. A fix outside one, or older than the
     * last, is ignored; returns whether it was kept. */
    bool addPoint(const TrackPoint &point);

    /* Seconds spent recording up to `time`. */
    double elapsedAt(double time) const;
    /* Seconds spent recording in all; up to the last point while still open. */
    double duration() const;
    /* The first begin, NaN before it. */
    double startTime() const;

    /* Everything at `elapsed` seconds into the activity: what the last point
     * before it said, with the current speed dropping to 0 once that point is
     * more than a few seconds old. */
    TrackState stateAt(double elapsed) const;

    std::size_t size() const;
    const TrackPoint &point(std::size_t index) const;
    double elapsedAtPoint(std::size_t index) const;
    double distanceAtPoint(std::size_t index) const;
    double speedAtPoint(std::size_t index) const;
    /* Smoothed, as stateAt() gives it; NaN until a fix brought one. */
    double altitudeAtPoint(std::size_t index) const;
    const std::vector<TrackSegment> &segments() const;

    /* Metres between two positions on the WGS 84 mean sphere. */
    static double distanceBetween(double latitude1, double longitude1,
                                  double latitude2, double longitude2);

    /* Speeds below this are standing still, above it moving. */
    static const double kMovingSpeed;

private:
    struct Derived {
        double elapsed = 0;
        double distance = 0;
        double speed = 0;
        double altitude = std::numeric_limits<double>::quiet_NaN();
        double elevationGain = 0;
        /* m_climbBase after the point, to go back to should it be dropped. */
        double climbBase = std::numeric_limits<double>::quiet_NaN();
    };

    double smoothSpeed(std::size_t index) const;
    std::size_t segmentOf(std::size_t index) const;
    int pointBefore(double elapsed) const;

    std::vector<TrackPoint> m_points;
    std::vector<Derived> m_derived;
    std::vector<TrackSegment> m_segments;

    /* The point the distance was last measured from, -1 at a segment's start. */
    int m_anchor = -1;
    /* Hysteresis for the climb: where the altitude last turned. */
    double m_climbBase = std::numeric_limits<double>::quiet_NaN();
};

#endif // TRACK_H
