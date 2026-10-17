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

#include "TrackFilter.h"

#include <algorithm>
#include <cmath>

/* None of the constants below has been tuned against recorded activities yet;
 * they were chosen on the generated tracks of tests/tst_trackfilter.cpp, whose
 * receiver errs the way a phone's is known to. */

namespace {

/* How a mode's movement may change: the spectral density of the
 * acceleration, in m²/s³ - about the square of a typical change of velocity
 * within a second, per second. The larger, the quicker the estimate follows,
 * and the less it smooths.
 *
 * Along the way and across it apart: speed changes slowly, but a corner turns
 * the velocity round within a second or two. So a sudden change of speed is
 * an outlier long before a sudden change of direction is. Standing, there is
 * no across, and only the slower rule holds. */
struct Profile {
    double along;
    double across;
    double vertical;
    /* How fast it may be going when the first fix does not say, m/s. */
    double initialSpeed;
};

/* In the order of TrackFilter::Mode. A walker changes pace gently and turns
 * on the spot; a runner changes pace more quickly and corners at speed; a
 * bike brakes harder still; a rowing boat keeps its pace, turns slowly and
 * stays on the water; a car accelerates, brakes, turns and climbs hardest.
 * Each is about the least that still follows setting off and stopping within
 * a second or two. */
const Profile kProfiles[] = {
    {0.1, 1.0, 0.01, 2.0},     // Walking
    {0.3, 4.0, 0.02, 5.0},     // Running
    {0.5, 4.0, 0.05, 10.0},    // Cycling
    {0.3, 1.0, 0.001, 5.0},    // Rowing
    {4.0, 10.0, 0.3, 30.0},    // Driving
};

/* Below about this speed, m/s, there is no telling along from across. */
const double kDirectionSpeed = 0.5;

/* The horizontal accuracy is taken as the error on each axis, and never
 * believed below this many metres: receivers that claim 1 or 2 m are still
 * off by more. Without one, a fix counts as this far off. */
const double kMinAccuracy = 3.0;
const double kUnknownAccuracy = 15.0;

/* Without a vertical accuracy, the altitude is taken to be this much worse
 * than the position, as it is with satellites all above the horizon. */
const double kVerticalShare = 1.5;

/* The receiver's speed is good to this, m/s, or to this share of the
 * horizontal accuracy per second where that is more. */
const double kMinSpeedError = 0.3;
const double kSpeedErrorShare = 0.05;

/* How fast the altitude may be changing at the start, m/s. */
const double kInitialClimb = 1.0;

/* A measurement off by more than this, in squared standard deviations of
 * what the estimate expects, is left out: 5 sigma. One off by more than 2
 * sigma counts for less, as if its noise were larger by the cube of how much
 * further off it is: at 3 sigma it moves the estimate a tenth as much as it
 * would otherwise, at 4 sigma barely at all. */
const double kRejectBeyond = 25.0;
const double kSoftBeyond = 4.0;

/* Fixes in a row that agree before an estimate counts: before that a fix
 * that disagrees starts it over. Outliers come in runs of a few fixes, and
 * one at the start must not pass for the start. */
const int kSettleAfter = 5;

/* Seconds without a fix that clearly agrees - within kSoftBeyond - after which
 * the estimate gives in to fixes that agree among themselves. */
const double kGiveInAfter = 10.0;

/* The projection's origin follows the estimate once it is this many metres
 * away. Within a kilometre a flat projection is exact to a decimetre. */
const double kRecentreBeyond = 1000.0;

/* Mean radius of the earth, metres, as Track has it. */
const double kEarthRadius = 6371008.8;

const double kPi = 3.14159265358979323846;
const double kMetresPerDegree = kEarthRadius * kPi / 180.0;

double square(double value)
{
    return value * value;
}

} // namespace

void TrackFilter::setMode(Mode mode)
{
    m_mode = mode;
}

TrackFilter::Mode TrackFilter::mode() const
{
    return m_mode;
}

bool TrackFilter::add(const TrackPoint &fix)
{
    if (std::isnan(fix.latitude) || std::isnan(fix.longitude))
        return false;
    if (!m_hasOrigin) {
        m_originLatitude = fix.latitude;
        m_originLongitude = fix.longitude;
        m_metresPerDegreeEast = kMetresPerDegree * std::cos(fix.latitude * kPi / 180.0);
        m_hasOrigin = true;
    }
    if (!addHorizontal(fix))
        return false;
    addVertical(fix);
    if (m_main.taken < kSettleAfter)
        return false;

    m_estimate = TrackPoint();
    m_estimate.time = fix.time;
    toDegrees(m_main.state, &m_estimate.latitude, &m_estimate.longitude);
    if (!std::isnan(fix.altitude) && m_altitude.valid)
        m_estimate.altitude = m_altitude.state[0];
    m_estimate.accuracy = fix.accuracy;
    m_estimate.verticalAccuracy = fix.verticalAccuracy;
    const double east = m_main.state[2];
    const double north = m_main.state[3];
    m_estimate.speed = std::sqrt(square(east) + square(north));
    if (m_estimate.speed >= Track::kMovingSpeed) {
        const double course = std::atan2(east, north) * 180.0 / kPi;
        m_estimate.course = course < 0 ? course + 360.0 : course;
    }
    m_hasEstimate = true;
    recentre();
    return true;
}

bool TrackFilter::hasEstimate() const
{
    return m_hasEstimate;
}

const TrackPoint &TrackFilter::estimate() const
{
    return m_estimate;
}

void TrackFilter::reset()
{
    const Mode mode = m_mode;
    *this = TrackFilter();
    m_mode = mode;
}

/* Returns whether the estimate took the fix, or started over from it. */
bool TrackFilter::addHorizontal(const TrackPoint &fix)
{
    const double accuracy = std::max(kMinAccuracy, std::isnan(fix.accuracy) ? kUnknownAccuracy
                                                                            : fix.accuracy);
    Measurement measurement;
    toMetres(fix.latitude, fix.longitude, measurement.position);
    measurement.positionVariance = square(accuracy);
    measurement.hasVelocity = !std::isnan(fix.speed) && !std::isnan(fix.course);
    if (measurement.hasVelocity) {
        const double course = fix.course * kPi / 180.0;
        measurement.velocity[0] = fix.speed * std::sin(course);
        measurement.velocity[1] = fix.speed * std::cos(course);
        measurement.velocityVariance = square(std::max(kMinSpeedError, kSpeedErrorShare * accuracy));
    }

    const double initialSpeed = kProfiles[m_mode].initialSpeed;
    if (!m_main.valid) {
        start(m_main, 2, fix.time, measurement, initialSpeed);
        return true;
    }
    predictHorizontal(m_main, fix.time);
    if (take(m_main, measurement)) {
        if (m_main.lastAgreed == fix.time)
            m_candidate.valid = false;
        else
            followCandidate(fix.time, measurement);
        return true;
    }
    // Not settled yet: it may be the estimate's first fixes that were off.
    if (m_main.taken < kSettleAfter) {
        start(m_main, 2, fix.time, measurement, initialSpeed);
        return true;
    }
    return followCandidate(fix.time, measurement);
}

/* An outlier - or the estimate is what is off. The fixes it does not clearly
 * agree with make a second guess; if they keep agreeing with that, and none
 * clearly with the estimate, the second guess takes over. Returns whether it
 * did. */
bool TrackFilter::followCandidate(double time, const Measurement &measurement)
{
    const double initialSpeed = kProfiles[m_mode].initialSpeed;
    if (m_candidate.valid) {
        predictHorizontal(m_candidate, time);
        if (!take(m_candidate, measurement))
            start(m_candidate, 2, time, measurement, initialSpeed);
    } else {
        start(m_candidate, 2, time, measurement, initialSpeed);
    }
    if (m_candidate.taken < kSettleAfter || time - m_main.lastAgreed < kGiveInAfter)
        return false;
    m_main = m_candidate;
    m_candidate.valid = false;
    return true;
}

/* An altitude far off is left out, but should they all be far off for a
 * while, the estimate starts over from them. */
void TrackFilter::addVertical(const TrackPoint &fix)
{
    if (std::isnan(fix.altitude))
        return;
    double accuracy = fix.verticalAccuracy;
    if (std::isnan(accuracy))
        accuracy = kVerticalShare * (std::isnan(fix.accuracy) ? kUnknownAccuracy : fix.accuracy);
    Measurement measurement;
    measurement.position[0] = fix.altitude;
    measurement.positionVariance = square(std::max(kMinAccuracy, accuracy));
    measurement.hasVelocity = false;

    if (m_altitude.valid) {
        const double noise[2][2] = {{kProfiles[m_mode].vertical, 0}, {0, 0}};
        predict(m_altitude, fix.time, noise);
        if (take(m_altitude, measurement) || fix.time - m_altitude.lastAgreed < kGiveInAfter)
            return;
    }
    start(m_altitude, 1, fix.time, measurement, kInitialClimb);
}

void TrackFilter::start(Motion &motion, int axes, double time, const Measurement &measurement,
                        double initialSpeed)
{
    motion = Motion();
    motion.valid = true;
    motion.axes = axes;
    motion.time = time;
    for (int axis = 0; axis < axes; ++axis) {
        motion.state[axis] = measurement.position[axis];
        motion.state[axes + axis] = measurement.hasVelocity ? measurement.velocity[axis] : 0.0;
        motion.covariance[axis][axis] = measurement.positionVariance;
        motion.covariance[axes + axis][axes + axis] = measurement.hasVelocity
                ? measurement.velocityVariance : square(initialSpeed);
    }
    motion.taken = 1;
    motion.lastAgreed = time;
}

/* The acceleration's spectral density on east and north: the mode's along
 * the current velocity, and across it as far as there is a direction to
 * speak of. */
void TrackFilter::predictHorizontal(Motion &motion, double time) const
{
    const Profile &profile = kProfiles[m_mode];
    const double east = motion.state[2];
    const double north = motion.state[3];
    // The share of across in what is not along, by the direction's unit normal.
    const double across = (profile.across - profile.along)
            / (square(east) + square(north) + square(kDirectionSpeed));
    const double noise[2][2] = {
        {profile.along + across * north * north, -across * east * north},
        {-across * east * north, profile.along + across * east * east}
    };
    predict(motion, time, noise);
}

/* Moves the state on to `time` at constant velocity, and lets its covariance
 * grow by what an acceleration of spectral density `noise` could have done
 * meanwhile. */
void TrackFilter::predict(Motion &motion, double time, const double noise[2][2])
{
    const double dt = std::max(0.0, time - motion.time);
    motion.time = time;
    if (dt <= 0)
        return;
    const int axes = motion.axes;
    for (int axis = 0; axis < axes; ++axis)
        motion.state[axis] += motion.state[axes + axis] * dt;

    // P = F P F' + Q, F moving each position on by its velocity times dt.
    double (&p)[4][4] = motion.covariance;
    for (int i = 0; i < axes; ++i) {
        for (int j = 0; j < axes; ++j) {
            const int vi = axes + i;
            const int vj = axes + j;
            p[i][j] += dt * (p[i][vj] + p[vi][j]) + dt * dt * p[vi][vj]
                    + noise[i][j] * dt * dt * dt / 3;
        }
    }
    for (int i = 0; i < axes; ++i) {
        for (int j = 0; j < axes; ++j) {
            const int vi = axes + i;
            const int vj = axes + j;
            p[i][vj] += dt * p[vi][vj] + noise[i][j] * dt * dt / 2;
        }
    }
    for (int i = 0; i < axes; ++i)
        for (int j = 0; j < axes; ++j)
            p[axes + i][axes + j] += noise[i][j] * dt;
    // Only the upper p[i][vj] were moved on above; the lower mirror them.
    for (int i = 0; i < axes; ++i)
        for (int j = 0; j < axes; ++j)
            p[axes + j][i] = p[i][axes + j];
}

/* The position, and the velocity where the fix has one, each checked on its
 * own: false if the position was left out, and with it the whole fix. A
 * velocity left out leaves the position taken. */
bool TrackFilter::take(Motion &motion, const Measurement &measurement)
{
    bool agreed = false;
    if (!correct(motion, 0, measurement.position, measurement.positionVariance, &agreed))
        return false;
    motion.taken += 1;
    if (agreed)
        motion.lastAgreed = motion.time;
    if (measurement.hasVelocity)
        correct(motion, motion.axes, measurement.velocity, measurement.velocityVariance, &agreed);
    return true;
}

/* The Kalman update with a measurement of the positions (`offset` 0) or of
 * the velocities (`offset` axes), each with `variance`. Returns false, and
 * changes nothing, if it is too far off what the state expects; `agreed` says
 * whether it was close enough to count in full. */
bool TrackFilter::correct(Motion &motion, int offset, const double *value, double variance,
                          bool *agreed)
{
    const int axes = motion.axes;
    const int size = 2 * axes;
    double (&p)[4][4] = motion.covariance;

    double innovation[2];
    for (int i = 0; i < axes; ++i)
        innovation[i] = value[i] - motion.state[offset + i];

    // What the state expects the measurement to be off by, and its inverse.
    double s[2][2];
    double inverse[2][2];
    double distance = 0;
    for (int pass = 0; pass < 2; ++pass) {
        for (int i = 0; i < axes; ++i)
            for (int j = 0; j < axes; ++j)
                s[i][j] = p[offset + i][offset + j] + (i == j ? variance : 0.0);
        if (axes == 1) {
            inverse[0][0] = 1.0 / s[0][0];
        } else {
            const double determinant = s[0][0] * s[1][1] - s[0][1] * s[1][0];
            inverse[0][0] = s[1][1] / determinant;
            inverse[1][1] = s[0][0] / determinant;
            inverse[0][1] = -s[0][1] / determinant;
            inverse[1][0] = -s[1][0] / determinant;
        }
        if (pass == 1)
            break;
        for (int i = 0; i < axes; ++i)
            for (int j = 0; j < axes; ++j)
                distance += innovation[i] * inverse[i][j] * innovation[j];
        *agreed = distance <= kSoftBeyond;
        if (distance > kRejectBeyond)
            return false;
        if (*agreed)
            break;
        const double beyond = distance / kSoftBeyond;
        variance *= beyond * beyond * beyond;
    }

    // K = P H' S^-1, then x += K innovation and P -= K H P.
    double gain[4][2];
    for (int i = 0; i < size; ++i) {
        for (int j = 0; j < axes; ++j) {
            gain[i][j] = 0;
            for (int k = 0; k < axes; ++k)
                gain[i][j] += p[i][offset + k] * inverse[k][j];
        }
    }
    for (int i = 0; i < size; ++i)
        for (int j = 0; j < axes; ++j)
            motion.state[i] += gain[i][j] * innovation[j];
    double updated[4][4];
    for (int i = 0; i < size; ++i) {
        for (int j = 0; j < size; ++j) {
            updated[i][j] = p[i][j];
            for (int k = 0; k < axes; ++k)
                updated[i][j] -= gain[i][k] * p[offset + k][j];
        }
    }
    // Kept symmetric against rounding.
    for (int i = 0; i < size; ++i)
        for (int j = 0; j < size; ++j)
            p[i][j] = (updated[i][j] + updated[j][i]) / 2;
    return true;
}

void TrackFilter::toMetres(double latitude, double longitude, double *position) const
{
    position[0] = (longitude - m_originLongitude) * m_metresPerDegreeEast;
    position[1] = (latitude - m_originLatitude) * kMetresPerDegree;
}

void TrackFilter::toDegrees(const double *position, double *latitude, double *longitude) const
{
    *latitude = m_originLatitude + position[1] / kMetresPerDegree;
    *longitude = m_originLongitude + position[0] / m_metresPerDegreeEast;
}

/* Moves the origin to the estimate, and both guesses with it. */
void TrackFilter::recentre()
{
    if (std::sqrt(square(m_main.state[0]) + square(m_main.state[1])) < kRecentreBeyond)
        return;
    double candidateLatitude = 0;
    double candidateLongitude = 0;
    if (m_candidate.valid)
        toDegrees(m_candidate.state, &candidateLatitude, &candidateLongitude);
    toDegrees(m_main.state, &m_originLatitude, &m_originLongitude);
    m_metresPerDegreeEast = kMetresPerDegree * std::cos(m_originLatitude * kPi / 180.0);
    m_main.state[0] = 0;
    m_main.state[1] = 0;
    if (m_candidate.valid)
        toMetres(candidateLatitude, candidateLongitude, m_candidate.state);
}
