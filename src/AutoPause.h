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

#ifndef AUTOPAUSE_H
#define AUTOPAUSE_H

#include "TrackFilter.h"

#include <limits>

/**
 * Decides when a recording pauses and resumes on its own, from the filter's
 * speed and, walking or running, the steps counted.
 *
 * Plain C++ without Qt, like Track, so tests/tst_autopause.cpp drives it on
 * the host. It only decides: the caller feeds it the speeds and steps, asks
 * check() what to do, and does it.
 *
 * - Standing: the speed below the type's limit for some seconds - and walking
 *   or running, no step for some seconds either - pauses, as of the moment
 *   the standing began, so the waiting is not counted. Moving again resumes:
 *   the speed above a second, higher limit for a few seconds, or, walking or
 *   running, a step.
 * - Too fast for the type - a bus, a train, a car - for some seconds pauses
 *   too, as of the moment it got that fast. Back within the type's range
 *   resumes: walking or running, at a step; otherwise, or without steps, once
 *   the speed has been within it for ten seconds. A stop on the way does not.
 * - Paused this way for half an hour, the activity is over: stop it.
 *
 * Without a fresh speed - no signal - it decides nothing from the speed, and
 * a run of slow or fast seconds starts over once the signal is back.
 *
 * After a resume by hand it waits for movement before it pauses again, so
 * that it does not pause again at once what the user has just resumed.
 */
class AutoPause
{
public:
    enum Action {
        None,
        /* Pause the recording as of pauseTime(). */
        Pause,
        /* Resume it now. */
        Resume,
        /* Paused for so long that the activity is over: stop it. */
        Stop
    };

    void setMode(TrackFilter::Mode mode);
    TrackFilter::Mode mode() const;

    /* Whether steps are counted at all - the accelerometer may be missing.
     * Walking or running without them, it goes by the speed alone. */
    void setStepsCounted(bool counted);

    /* Starts watching a recording that goes on from `time`: at the start,
     * after a resume by hand, and when auto-pause is switched on. Unless
     * `armed`, it waits for movement before it pauses. */
    void restart(double time, bool armed);
    /* Watches nothing until the next restart(). */
    void halt();

    /* The filter's speed at `time`, metres per second. Also while paused. */
    void addSpeed(double time, double speed);
    /* A step counted at `time`. */
    void addStep(double time);

    /* What to do at `time`. Call it after each of the above and once a second;
     * it takes the action returned as done. */
    Action check(double time);

    bool isPaused() const;
    /* When the movement the last Pause was for ended. */
    double pauseTime() const;

private:
    enum State {
        Halted,
        Watching,
        PausedStanding,
        PausedTooFast
    };

    struct Rules;
    const Rules &rules() const;
    bool usesSteps() const;
    bool speedFresh(double time) const;
    bool newStep() const;
    void forgetRuns();
    void pause(double time, double since, State state);

    TrackFilter::Mode m_mode = TrackFilter::Running;
    bool m_stepsCounted = false;

    State m_state = Halted;
    bool m_armed = false;
    /* When watching or the pause began, as far as steps are concerned: a step
     * counts as new after it. */
    double m_since = 0;
    double m_pauseTime = std::numeric_limits<double>::quiet_NaN();

    double m_speed = std::numeric_limits<double>::quiet_NaN();
    double m_speedTime = std::numeric_limits<double>::quiet_NaN();
    double m_lastStep = std::numeric_limits<double>::quiet_NaN();
    /* Since when the speed has been below the standing limit, above the
     * type's range, and within it - each NaN while it is not. */
    double m_slowSince = std::numeric_limits<double>::quiet_NaN();
    double m_fastSince = std::numeric_limits<double>::quiet_NaN();
    double m_movingSince = std::numeric_limits<double>::quiet_NaN();
    /* The last speed above the type's range: steps before it, walking along
     * a train, say, do not tell that the user moves on their own. */
    double m_lastFast = std::numeric_limits<double>::quiet_NaN();
};

#endif // AUTOPAUSE_H
