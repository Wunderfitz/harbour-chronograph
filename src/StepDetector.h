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

#ifndef STEPDETECTOR_H
#define STEPDETECTOR_H

#include <cstdint>

/**
 * Turns raw accelerometer samples into steps.
 *
 * Deliberately free of Qt: it is fed timestamps and three axes and answers with
 * a step count, so tests/tst_stepdetector.cpp can drive it on the host with
 * synthetic walks. StepCounter is the part that knows about sensors.
 *
 * The pipeline, per sample:
 *  1. The magnitude of the acceleration vector, so the phone's orientation in a
 *     pocket or hand does not matter.
 *  2. Gravity removed by subtracting a slow moving average of that magnitude.
 *  3. A short low-pass over the rest, which takes out sensor jitter but keeps the
 *     1-4 Hz of walking and running.
 *  4. Peak detection with hysteresis: a peak counts once the signal has risen
 *     above the threshold and fallen back below zero, so one stride produces one
 *     peak however ragged its top is. The threshold follows how much the phone
 *     is moving overall, so a soft walk and a run are both seen.
 *  5. Plausibility: peaks closer than a sprint allows are dropped, and the first
 *     few steps of a walk are held back until enough regular ones have followed
 *     - picking up the phone or shaking it should not count.
 *  6. Once a walk is confirmed, its rhythm corrects single peaks: a small bump
 *     right after a step is the same step echoing, and a gap of two steps
 *     followed by the usual beat again is a step too soft to be seen.
 *  7. The beat of the signal itself, found by autocorrelating its last four
 *     seconds, catches what the peaks miss: where one leg is too soft to give a
 *     peak of its own, the peaks come once per stride, but the signal still
 *     repeats once per step. A gap between peaks of two such beats is two steps,
 *     while walking and - once the run already keeps that beat - while it is
 *     being confirmed.
 *
 * All filters are time-constant based rather than per-sample, so the result does
 * not depend on the data rate sensorfw actually delivers.
 */
class StepDetector
{
public:
    StepDetector();

    /**
     * Feeds one accelerometer reading in m/s^2, timestamped in microseconds on
     * any monotonic clock. Returns how many steps this sample confirmed: usually
     * 0 or 1, and the whole held-back run at once when a walk is recognised.
     */
    int addSample(std::uint64_t timestampUs, double x, double y, double z);

    /** Forgets everything, as if no sample had been seen yet. */
    void reset();

private:
    void restartFilters(double magnitude, std::uint64_t timestampUs);
    void breakRun();
    int registerPeak(std::uint64_t timestampUs, double amplitude);
    void rememberInterval(std::uint64_t intervalUs);
    double medianIntervalUs() const;
    std::uint64_t maximumIntervalUs() const;
    void recordForCorrelation(std::uint64_t timestampUs);
    double stepPeriodUs() const;

    /* How many of the walk's latest step intervals its rhythm is judged by. */
    static const int kIntervalHistory = 8;

    /* The signal kept for autocorrelation: four seconds on a 20 ms grid, so the
     * window is the same whatever rate sensorfw delivers at. */
    static const int kCorrelationSamples = 200;

    bool m_initialised = false;
    std::uint64_t m_lastTimestampUs = 0;
    double m_gravity = 0.0;
    double m_smoothed = 0.0;
    double m_meanSquare = 0.0;

    bool m_aboveThreshold = false;
    double m_peakValue = 0.0;
    std::uint64_t m_peakTimestampUs = 0;

    bool m_haveLastStep = false;
    std::uint64_t m_lastStepUs = 0;
    std::uint64_t m_runStartUs = 0;
    double m_lastAmplitude = 0.0;
    int m_pendingSteps = 0;
    bool m_walking = false;

    std::uint64_t m_intervalsUs[kIntervalHistory] = {};
    int m_intervalCount = 0;
    int m_nextInterval = 0;
    bool m_gapPending = false;

    double m_correlationSamples[kCorrelationSamples] = {};
    int m_correlationCount = 0;
    int m_nextCorrelationSample = 0;
    std::uint64_t m_nextCorrelationUs = 0;
};

#endif // STEPDETECTOR_H
