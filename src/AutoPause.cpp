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

#include "AutoPause.h"

#include <algorithm>
#include <cmath>

/* None of the limits below has been tuned against recorded activities yet;
 * they are what a body or a vehicle can do. */

struct AutoPause::Rules {
    /* Below this speed, m/s, for this many seconds is standing. */
    double standingSpeed;
    double standingFor;
    /* Walking or running, also no step for this many seconds. */
    double noStepFor;
    /* Above this for this many seconds is moving again. */
    double movingSpeed;
    double movingFor;
    /* Above this for this many seconds is too fast for the type. */
    double fastSpeed;
    double fastFor;
};

namespace {

const double kNoLimit = std::numeric_limits<double>::infinity();

/* A speed older than this, in seconds, says nothing: the signal is gone. As
 * Activity counts the signal as lost. */
const double kSpeedStaleAfter = 5.0;

/* After too fast, back within the type's range for this many seconds resumes,
 * where steps cannot tell. Longer than moving again after standing, so that a
 * bus slowing down for a stop does not resume. */
const double kBackInRangeFor = 10.0;

/* Paused on its own for this many seconds, the activity is over. */
const double kStopAfter = 30 * 60;

} // namespace

void AutoPause::setMode(TrackFilter::Mode mode)
{
    m_mode = mode;
}

TrackFilter::Mode AutoPause::mode() const
{
    return m_mode;
}

void AutoPause::setStepsCounted(bool counted)
{
    m_stepsCounted = counted;
}

void AutoPause::restart(double time, bool armed)
{
    m_state = Watching;
    m_armed = armed;
    m_since = time;
    m_pauseTime = std::nan("");
    forgetRuns();
}

void AutoPause::halt()
{
    m_state = Halted;
}

void AutoPause::addSpeed(double time, double speed)
{
    if (std::isnan(speed))
        return;
    // Nothing is known about the time without a speed.
    if (!std::isnan(m_speedTime) && time - m_speedTime > kSpeedStaleAfter)
        forgetRuns();
    m_speed = speed;
    m_speedTime = time;

    const Rules &limits = rules();
    if (speed >= limits.standingSpeed)
        m_slowSince = std::nan("");
    else if (std::isnan(m_slowSince))
        m_slowSince = time;
    if (speed <= limits.fastSpeed)
        m_fastSince = std::nan("");
    else if (std::isnan(m_fastSince))
        m_fastSince = time;
    if (speed > limits.fastSpeed)
        m_lastFast = time;
    if (speed < limits.movingSpeed || speed > limits.fastSpeed)
        m_movingSince = std::nan("");
    else if (std::isnan(m_movingSince))
        m_movingSince = time;
}

void AutoPause::addStep(double time)
{
    m_lastStep = time;
}

AutoPause::Action AutoPause::check(double time)
{
    if (m_state == Halted)
        return None;

    const Rules &limits = rules();
    const bool fresh = speedFresh(time);
    const bool tooFast = fresh && m_speed > limits.fastSpeed;
    const bool moving = fresh && !std::isnan(m_movingSince);
    const bool stepped = usesSteps() && newStep();

    if (m_state == Watching) {
        if (!m_armed) {
            if ((moving && time - m_movingSince >= limits.movingFor) || (stepped && !tooFast))
                m_armed = true;
            else
                return None;
        }
        if (!fresh)
            return None;
        if (!std::isnan(m_fastSince) && time - m_fastSince >= limits.fastFor) {
            pause(time, m_fastSince, PausedTooFast);
            return Pause;
        }
        if (!std::isnan(m_slowSince) && time - m_slowSince >= limits.standingFor) {
            double since = m_slowSince;
            if (usesSteps() && !std::isnan(m_lastStep)) {
                if (time - m_lastStep < limits.noStepFor)
                    return None;
                since = std::max(since, m_lastStep);
            }
            pause(time, since, PausedStanding);
            return Pause;
        }
        return None;
    }

    if (time - m_pauseTime >= kStopAfter) {
        m_state = Halted;
        return Stop;
    }
    bool resume;
    if (m_state == PausedStanding) {
        // A step resumes even without a signal: walking indoors, say.
        resume = (moving && time - m_movingSince >= limits.movingFor) || (stepped && !tooFast);
    } else if (usesSteps()) {
        // But not after too fast: that may be a train in a tunnel.
        resume = stepped && fresh && !tooFast;
    } else {
        resume = moving && time - m_movingSince >= kBackInRangeFor;
    }
    if (!resume)
        return None;
    m_state = Watching;
    m_armed = true;
    m_since = time;
    m_pauseTime = std::nan("");
    return Resume;
}

bool AutoPause::isPaused() const
{
    return m_state == PausedStanding || m_state == PausedTooFast;
}

double AutoPause::pauseTime() const
{
    return m_pauseTime;
}

/* By TrackFilter::Mode. A traffic light pauses a run or a ride, but not a
 * drive: that pauses only when parked. The fastest limits leave room for what
 * a body does at its best - a marathon is run at under 6 m/s, an eight rows
 * at under 7, a bike goes downhill at 80 km/h. */
const AutoPause::Rules &AutoPause::rules() const
{
    static const Rules kRules[] = {
        // Walking
        { 0.4, 10, 5, 0.7, 3, 3.5, 15 },
        // Running
        { 1.0, 5, 3, 1.5, 2, 8.0, 15 },
        // Cycling
        { 1.0, 5, 0, 1.5, 2, 25.0, 30 },
        // Rowing: drifting, the boat still moves a little.
        { 0.5, 20, 0, 1.0, 5, 7.0, 15 },
        // Driving
        { 1.0, 180, 0, 3.0, 3, kNoLimit, 0 },
    };
    return kRules[m_mode];
}

/* Steps tell walking or running from standing. Riding, rowing or driving they
 * tell nothing, and without an accelerometer there are none. */
bool AutoPause::usesSteps() const
{
    return m_stepsCounted && (m_mode == TrackFilter::Walking || m_mode == TrackFilter::Running);
}

bool AutoPause::speedFresh(double time) const
{
    return !std::isnan(m_speedTime) && time - m_speedTime <= kSpeedStaleAfter;
}

/* A step since watching or the pause began, and since the speed was last too
 * fast. */
bool AutoPause::newStep() const
{
    if (std::isnan(m_lastStep) || m_lastStep <= m_since)
        return false;
    return std::isnan(m_lastFast) || m_lastStep > m_lastFast;
}

void AutoPause::forgetRuns()
{
    m_slowSince = std::nan("");
    m_fastSince = std::nan("");
    m_movingSince = std::nan("");
}

/* As of `since`, but never before watching began: a restart is a fresh look.
 * Steps count as new from the decision on, not from `since`: a step on the
 * bus before it was found too fast must not resume it. */
void AutoPause::pause(double time, double since, State state)
{
    m_pauseTime = std::max(since, m_since);
    m_state = state;
    m_since = time;
}
