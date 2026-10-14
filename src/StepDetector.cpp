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

#include "StepDetector.h"

#include <algorithm>
#include <cmath>

/* The constants below were tuned on walks recorded on a Jolla Phone (2026) -
 * five times 100 steps: in the hand, in two different trouser pockets, faster,
 * and slower and softer; then a bag, stairs and a run - and on the synthetic
 * signals in tests/tst_stepdetector.cpp, which also model what the walks
 * showed. CLAUDE.md has the numbers behind each choice. */

namespace {

/* How quickly the gravity estimate follows the magnitude. A second is long
 * against a stride (0.3-1 s), so the bounce of walking stays in the signal, and
 * short enough that the estimate settles within a few seconds of the app starting
 * or the phone changing hands. */
const double kGravityTimeConstantS = 1.0;

/* The jitter filter. 40 ms puts its corner at about 4 Hz: above the cadence of a
 * run, below the rattle of a phone in a pocket. */
const double kSmoothingTimeConstantS = 0.04;

/* The threshold a peak has to clear is this share of how much the phone is
 * moving - the RMS of the signal over about the last second. A threshold tied to
 * the walk's own peaks instead could not serve a soft walk and a pocket at once:
 * low enough for the one, it counted the other's small bumps. Up to 0.85 the
 * recorded walks come out within 5 % each; from 0.9 on, peaks start to fall
 * under it and a pocket walk collapses, so this is as high as it goes. */
const double kThresholdShareOfRms = 0.85;

/* How far back that RMS looks. One second lets a run's vigour fade quickly when
 * it turns into a walk; at two seconds the threshold stayed too high for twice as
 * many walking steps. */
const double kRmsTimeConstantS = 1.0;

/* The threshold never drops below this, in m/s^2, so a phone lying still - a few
 * hundredths of noise - cannot clear it. A slow, soft walk peaks at 1 or more. */
const double kMinimumThreshold = 0.5;

/* Five steps a second is beyond a sprint; anything faster is the same stride
 * ringing on. */
const std::uint64_t kMinimumStepIntervalUs = 200000;

/* Slower than one step in two seconds is not the start of a walk. The run is over
 * and the next step has to prove itself again. */
const std::uint64_t kMaximumStepIntervalUs = 2000000;

/* A walk already under way may pause longer between counted steps: in a slow
 * walk whose softer leg goes unseen, one counted step can be well over 2 s from
 * the next, and at 2 s such a walk broke apart into short runs that never got
 * confirmed. Not before confirmation, though - there it let random jolts chain
 * up into walks. */
const std::uint64_t kMaximumWalkingIntervalUs = 2600000;

/* While a walk is not confirmed yet, each step may differ in length from the
 * mean of the steps before it by this factor at most. Shaking a phone or picking
 * it up gives irregular peaks; walking gives a beat. 1.3 rejected a little more
 * shaking still, but leaves no room for the uneven first strides of a real
 * walk. */
const double kRegularityFactor = 1.5;

/* How many regular steps it takes to accept that someone is walking. They are
 * held back until then and counted all at once, so nothing of a real walk is
 * lost - only walks shorter than this go unrecorded. Together with the factor
 * above, this was chosen on synthetic random jolts: 5 and 2.0 let about seven in
 * a hundred through as steps, 6 and 1.5 about one, while a walk whose strides
 * vary by 8 % is still counted in full. */
const int kStepsToConfirm = 6;

/* An echo: a peak this early in the walk's usual step, and this much smaller than
 * the step before it. Held in the hand, a step can be a large peak followed
 * shortly by a smaller bump, which counted as a step of its own. Both conditions,
 * because a walk that speeds into a run also brings peaks early - but those are
 * as strong as each other. */
const double kEchoIntervalShare = 0.5;
const double kEchoAmplitudeShare = 0.6;

/* A gap of about two steps in a walk is a step too soft to be seen. It is only
 * filled once the next interval is back near the usual one: a walk slowing down
 * gives long intervals too, but keeps giving them. */
const double kGapMinimumShare = 1.6;
const double kGapMaximumShare = 2.4;
const double kBeatMinimumShare = 0.7;
const double kBeatMaximumShare = 1.4;

/* The rhythm is only trusted once it rests on this many intervals. */
const int kMinimumIntervalsForRhythm = 3;

/* The grid the signal is kept on for autocorrelation, and the beats looked for
 * on it: from a sprint to a step slower than a slow walk's. */
const std::uint64_t kCorrelationGridUs = 20000;
const std::uint64_t kShortestBeatUs = 240000;
const std::uint64_t kLongestBeatUs = 2000000;

/* How strongly the signal has to repeat before its beat is believed. Walking
 * reaches 0.3 - 1.0, and stairs with the phone in a front pocket, the weakest
 * that is still told apart, barely clear it - so this cannot rise without
 * losing them. */
const double kMinimumCorrelation = 0.3;

/* A walk repeats after two strides as well as after one, and sometimes a little
 * better, by a few hundredths. The beat is the shortest lag that repeats at
 * least this well relative to the best one. Taking the best itself made the
 * run's count swing by five steps with the phase of the grid. */
const double kNearlyBestShare = 0.9;

/* The strongest repetition is often the stride, two steps: in a pocket, and in
 * a run with a weak leg, one leg's steps look unlike the other's. The step shows
 * as a smaller peak at half that lag, and is taken for the beat when it reaches
 * both of these - in a pocket it is often a fraction of the stride's. In the
 * hand, and in a slow walk, there is mostly nothing at half. */
const double kHalfBeatMinimumCorrelation = 0.1;
const double kHalfBeatShareOfBest = 0.25;

/* While walking, an interval between peaks of this many beats or more counts as
 * two steps, and one beat more as three. 1.7 rather than 1.5, so that a walk
 * slowing down - the beat lags the peaks by a second or two - is not counted
 * double. */
const double kTwoStepsBeats = 1.7;

/* While a run is being confirmed, a gap of about two beats is a skipped step
 * rather than the end of the run - but only when the run's mean step is within
 * this share of the beat. Without that condition rhythmic handling passed as a
 * walk: shaking the phone repeats too, and its peaks fit some multiple of it. */
const double kSkippedStepMinimumBeats = 1.5;
const double kSkippedStepMaximumBeats = 2.5;
const double kRunOnBeatTolerance = 0.3;

/* A pause in the samples this long means sensorfw stopped delivering for a while
 * (the sensor was suspended, or the app was). The filters start over rather than
 * treating the jump as one huge sample. */
const std::uint64_t kMaximumSampleGapUs = 1000000;

double filterFactor(double dtS, double timeConstantS)
{
    return dtS / (timeConstantS + dtS);
}

} // namespace

StepDetector::StepDetector()
{
}

int StepDetector::addSample(std::uint64_t timestampUs, double x, double y, double z)
{
    const double magnitude = std::sqrt(x * x + y * y + z * z);

    // A duplicate of the last reading carries nothing new.
    if (m_initialised && timestampUs == m_lastTimestampUs)
        return 0;

    if (!m_initialised || timestampUs < m_lastTimestampUs
            || timestampUs - m_lastTimestampUs > kMaximumSampleGapUs) {
        restartFilters(magnitude, timestampUs);
        return 0;
    }

    const double dtS = double(timestampUs - m_lastTimestampUs) / 1e6;
    m_lastTimestampUs = timestampUs;

    m_gravity += filterFactor(dtS, kGravityTimeConstantS) * (magnitude - m_gravity);
    const double dynamic = magnitude - m_gravity;
    m_smoothed += filterFactor(dtS, kSmoothingTimeConstantS) * (dynamic - m_smoothed);
    m_meanSquare += filterFactor(dtS, kRmsTimeConstantS) * (dynamic * dynamic - m_meanSquare);
    recordForCorrelation(timestampUs);

    // A walk that has gone quiet is over now, not when the next peak turns up.
    if (m_haveLastStep && timestampUs - m_lastStepUs > maximumIntervalUs())
        breakRun();

    if (!m_aboveThreshold) {
        const double threshold = std::max(kMinimumThreshold,
                                          kThresholdShareOfRms * std::sqrt(m_meanSquare));
        if (m_smoothed > threshold) {
            m_aboveThreshold = true;
            m_peakValue = m_smoothed;
            m_peakTimestampUs = timestampUs;
        }
        return 0;
    }

    if (m_smoothed > m_peakValue) {
        m_peakValue = m_smoothed;
        m_peakTimestampUs = timestampUs;
    }

    // The stride is over once the signal is back below gravity. Timing the step
    // by its peak rather than by this crossing keeps the intervals honest.
    if (m_smoothed < 0.0) {
        m_aboveThreshold = false;
        return registerPeak(m_peakTimestampUs, m_peakValue);
    }
    return 0;
}

void StepDetector::reset()
{
    *this = StepDetector();
}

void StepDetector::restartFilters(double magnitude, std::uint64_t timestampUs)
{
    m_initialised = true;
    m_lastTimestampUs = timestampUs;
    m_gravity = magnitude;
    m_smoothed = 0.0;
    m_aboveThreshold = false;
    m_correlationCount = 0;
    m_nextCorrelationSample = 0;
    m_nextCorrelationUs = timestampUs + kCorrelationGridUs;
}

void StepDetector::breakRun()
{
    m_haveLastStep = false;
    m_pendingSteps = 0;
    m_walking = false;
    m_intervalCount = 0;
    m_nextInterval = 0;
    m_gapPending = false;
}

int StepDetector::registerPeak(std::uint64_t timestampUs, double amplitude)
{
    const bool rhythmKnown = m_walking && m_intervalCount >= kMinimumIntervalsForRhythm;
    const double medianUs = rhythmKnown ? medianIntervalUs() : 0.0;

    std::uint64_t intervalUs = 0;
    if (m_haveLastStep) {
        intervalUs = timestampUs - m_lastStepUs;
        if (intervalUs < kMinimumStepIntervalUs)
            return 0;
        if (rhythmKnown && double(intervalUs) < kEchoIntervalShare * medianUs
                && amplitude < kEchoAmplitudeShare * m_lastAmplitude)
            return 0;
        if (intervalUs > maximumIntervalUs()) {
            breakRun();
            intervalUs = 0;
        }
    }

    const double periodUs = intervalUs > 0 && (m_walking || m_pendingSteps >= 2)
            ? stepPeriodUs() : 0.0;

    // Not walking yet and out of step with the run so far: whatever came before
    // was not a walk. This peak may be the first step of one, though. Measuring
    // against the run's mean interval rather than the last one keeps a chain of
    // jolts that slows down a little each time from passing as a beat. The mean
    // is per step, a skipped one included, so it stays comparable to the beat.
    int pendingWeight = 1;
    if (!m_walking && intervalUs > 0 && m_pendingSteps >= 2) {
        const double meanUs = double(m_lastStepUs - m_runStartUs) / (m_pendingSteps - 1);
        const double beats = periodUs > 0.0 ? double(intervalUs) / periodUs : 0.0;
        if (beats > kSkippedStepMinimumBeats && beats < kSkippedStepMaximumBeats
                && std::fabs(meanUs / periodUs - 1.0) < kRunOnBeatTolerance)
            pendingWeight = 2;
        else if (double(intervalUs) > kRegularityFactor * meanUs
                || kRegularityFactor * double(intervalUs) < meanUs)
            m_pendingSteps = 0;
    }

    // A gap that the walk's usual beat now follows again held a step.
    int missed = 0;
    if (m_walking && intervalUs > 0 && rhythmKnown) {
        const double interval = double(intervalUs);
        if (m_gapPending && interval >= kBeatMinimumShare * medianUs
                && interval <= kBeatMaximumShare * medianUs)
            missed = 1;
        m_gapPending = interval >= kGapMinimumShare * medianUs
                && interval <= kGapMaximumShare * medianUs;
    }

    m_haveLastStep = true;
    m_lastStepUs = timestampUs;
    m_lastAmplitude = amplitude;
    // A gap stays out of the rhythm it is measured against.
    if (intervalUs > 0 && !(m_walking && m_gapPending))
        rememberInterval(intervalUs);

    // Where the signal's beat is known, it says how many steps the interval held;
    // otherwise the rhythm of the peaks does.
    if (m_walking) {
        if (periodUs > 0.0) {
            const double beats = double(intervalUs) / periodUs;
            return beats >= kTwoStepsBeats + 1.0 ? 3 : beats >= kTwoStepsBeats ? 2 : 1;
        }
        return 1 + missed;
    }

    if (m_pendingSteps == 0) {
        m_runStartUs = timestampUs;
        m_intervalCount = 0;
        m_nextInterval = 0;
    }
    m_pendingSteps += pendingWeight;
    if (m_pendingSteps < kStepsToConfirm)
        return 0;

    m_walking = true;
    const int confirmed = m_pendingSteps;
    m_pendingSteps = 0;
    return confirmed;
}

void StepDetector::rememberInterval(std::uint64_t intervalUs)
{
    m_intervalsUs[m_nextInterval] = intervalUs;
    m_nextInterval = (m_nextInterval + 1) % kIntervalHistory;
    if (m_intervalCount < kIntervalHistory)
        ++m_intervalCount;
}

double StepDetector::medianIntervalUs() const
{
    // Eight values at most: an insertion sort is all it takes.
    std::uint64_t sorted[kIntervalHistory];
    for (int i = 0; i < m_intervalCount; ++i) {
        int j = i;
        for (; j > 0 && sorted[j - 1] > m_intervalsUs[i]; --j)
            sorted[j] = sorted[j - 1];
        sorted[j] = m_intervalsUs[i];
    }
    const int middle = m_intervalCount / 2;
    return m_intervalCount % 2
            ? double(sorted[middle])
            : (double(sorted[middle - 1]) + double(sorted[middle])) / 2.0;
}

std::uint64_t StepDetector::maximumIntervalUs() const
{
    return m_walking ? kMaximumWalkingIntervalUs : kMaximumStepIntervalUs;
}

void StepDetector::recordForCorrelation(std::uint64_t timestampUs)
{
    // The latest smoothed value stands for every grid point it reached; at the
    // usual 50 Hz that is one sample each.
    while (timestampUs >= m_nextCorrelationUs) {
        m_correlationSamples[m_nextCorrelationSample] = m_smoothed;
        m_nextCorrelationSample = (m_nextCorrelationSample + 1) % kCorrelationSamples;
        if (m_correlationCount < kCorrelationSamples)
            ++m_correlationCount;
        m_nextCorrelationUs += kCorrelationGridUs;
    }
}

double StepDetector::stepPeriodUs() const
{
    // Not before the window is full: the first seconds after a restart say
    // nothing about a beat yet.
    if (m_correlationCount < kCorrelationSamples)
        return 0.0;

    const int count = kCorrelationSamples;
    double x[kCorrelationSamples];
    double mean = 0.0;
    for (int i = 0; i < count; ++i) {
        x[i] = m_correlationSamples[(m_nextCorrelationSample + i) % count];
        mean += x[i];
    }
    mean /= count;
    double variance = 0.0;
    for (int i = 0; i < count; ++i) {
        x[i] -= mean;
        variance += x[i] * x[i];
    }
    variance /= count;
    if (variance <= 0.0)
        return 0.0;

    // Normalised autocorrelation for every lag in range, and one either side of
    // it, so that a peak at the edge can be recognised as one.
    const int shortestLag = int(kShortestBeatUs / kCorrelationGridUs);
    const int longestLag = int(kLongestBeatUs / kCorrelationGridUs);
    double correlation[kCorrelationSamples];
    for (int lag = shortestLag - 1; lag <= longestLag + 1; ++lag) {
        double sum = 0.0;
        for (int i = 0; i + lag < count; ++i)
            sum += x[i] * x[i + lag];
        correlation[lag] = sum / ((count - lag) * variance);
    }
    const auto isPeak = [&correlation](int lag) {
        return correlation[lag] > correlation[lag - 1] && correlation[lag] >= correlation[lag + 1];
    };

    int best = 0;
    for (int lag = shortestLag; lag <= longestLag; ++lag) {
        if (isPeak(lag) && (best == 0 || correlation[lag] > correlation[best]))
            best = lag;
    }
    if (best == 0 || correlation[best] < kMinimumCorrelation)
        return 0.0;

    // Two strides repeat as well as one, sometimes a little better: the beat is
    // the shortest lag that repeats nearly as well as the best.
    for (int lag = shortestLag; lag < best; ++lag) {
        if (isPeak(lag) && correlation[lag] >= kNearlyBestShare * correlation[best]) {
            best = lag;
            break;
        }
    }

    // A step at half the lag makes the strongest repetition a stride.
    int half = 0;
    for (int lag = std::max(shortestLag, int(best * 0.4)); lag <= int(best * 0.6) + 1; ++lag) {
        if (isPeak(lag) && (half == 0 || correlation[lag] > correlation[half]))
            half = lag;
    }
    if (half != 0 && correlation[half] >= kHalfBeatMinimumCorrelation
            && correlation[half] >= kHalfBeatShareOfBest * correlation[best])
        return double(half * kCorrelationGridUs);
    return double(best * kCorrelationGridUs);
}
