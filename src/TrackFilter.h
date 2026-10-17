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

#ifndef TRACKFILTER_H
#define TRACKFILTER_H

#include "Track.h"

/**
 * Smooths the receiver's fixes before anything is shown or recorded: position,
 * speed, course and altitude come out of it, the receiver's own only go in.
 *
 * Plain C++ without Qt, like Track, so tests/tst_trackfilter.cpp drives it on
 * the host.
 *
 * - A Kalman filter with a constant-velocity model follows the position and
 *   the velocity. Each fix weighs by its own horizontal accuracy against what
 *   the earlier ones say, so a poor fix moves the estimate little and a good
 *   one much. The receiver's speed and course, where it gives both, go in as a
 *   measured velocity.
 * - A fix too far off from where the estimate expects it is an outlier and is
 *   left out; one somewhat off counts for less the further off it is. The same
 *   goes for the receiver's velocity on its own: a speed spike is left out
 *   while the position of its fix is still taken.
 * - The filter needs a few fixes in a row that agree before it gives an
 *   estimate, so a first fix far off does not start the track. Should it ever
 *   be the estimate that is off - the fixes keep disagreeing with it, but agree
 *   among themselves - it gives in after a few seconds and follows them.
 * - The altitude has a filter of its own, with the vertical accuracy, taken
 *   only from fixes whose position was taken.
 *
 * How quickly the movement may change is the mode's, along the way and across
 * it apart: a change of pace takes longer than a turn, so a sudden change of
 * speed is an outlier where a sudden turn is not. A car turns and brakes
 * harder than a walker, a rowing boat barely climbs. The mode can change at
 * any time.
 */
class TrackFilter
{
public:
    enum Mode {
        Walking,
        Running,
        Cycling,
        Rowing,
        Driving
    };

    void setMode(Mode mode);
    Mode mode() const;

    /* Takes a fix as the receiver gave it, timed as Track wants it. Returns
     * whether there is an estimate for it now: false for an outlier, and for
     * fixes before the filter has settled. */
    bool add(const TrackPoint &fix);

    /* Whether estimate() holds anything yet. */
    bool hasEstimate() const;

    /* The estimate as of the last fix add() returned true for, with its time.
     * Position, altitude, speed and course are filtered; the accuracies are
     * that fix's own, as they describe the signal. The altitude is NaN where
     * the fix had none, the course while standing. */
    const TrackPoint &estimate() const;

    /* Forgets everything, the mode excepted. */
    void reset();

private:
    /* One guess at where the receiver is and how it moves: the position and
     * the velocity on two axes, east and north, or one, up. */
    struct Motion {
        bool valid = false;
        int axes = 2;
        /* What the state is for. */
        double time = 0;
        /* The axes' positions, then their velocities: metres, metres per
         * second. */
        double state[4] = {0, 0, 0, 0};
        double covariance[4][4] = {};
        /* Fixes taken since it was started, and the time of the latest that
         * clearly agreed with it. */
        int taken = 0;
        double lastAgreed = 0;
    };

    /* A fix as the filter takes it: metres, and variances on each axis. */
    struct Measurement {
        double position[2];
        double positionVariance;
        bool hasVelocity;
        double velocity[2];
        double velocityVariance;
    };

    static void start(Motion &motion, int axes, double time, const Measurement &measurement,
                      double initialSpeed);
    static void predict(Motion &motion, double time, const double noise[2][2]);
    static bool take(Motion &motion, const Measurement &measurement);
    static bool correct(Motion &motion, int offset, const double *value, double variance,
                        bool *agreed);

    void predictHorizontal(Motion &motion, double time) const;
    bool addHorizontal(const TrackPoint &fix);
    bool followCandidate(double time, const Measurement &measurement);
    void addVertical(const TrackPoint &fix);
    void toMetres(double latitude, double longitude, double *position) const;
    void toDegrees(const double *position, double *latitude, double *longitude) const;
    void recentre();

    Mode m_mode = Running;

    /* East and north are metres from here; moved along with the estimate, so
     * that a flat projection stays exact enough. */
    bool m_hasOrigin = false;
    double m_originLatitude = 0;
    double m_originLongitude = 0;
    double m_metresPerDegreeEast = 0;

    /* The estimate, and a second guess started from the fixes it leaves out,
     * should those agree among themselves. */
    Motion m_main;
    Motion m_candidate;
    Motion m_altitude;

    bool m_hasEstimate = false;
    TrackPoint m_estimate;
};

#endif // TRACKFILTER_H
