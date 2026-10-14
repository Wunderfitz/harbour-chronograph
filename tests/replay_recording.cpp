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

/*
 * Replays a recording made on a phone (CHRONOGRAPH_RECORD, see StepCounter)
 * through StepDetector on the host, and prints what it counts next to what the
 * app counted at the time - which is how a change to the detector is checked
 * against real walks. Recordings are personal data: keep them in
 * tests/raw-data/, which git ignores. Build and run from the project root:
 *
 *   g++ -std=c++11 -Wall -Wextra -O2 -Isrc tests/replay_recording.cpp \
 *       src/StepDetector.cpp -o /tmp/replay_recording
 *   /tmp/replay_recording tests/raw-data/recording-<date>.csv [-v]
 *
 * -v also prints when each step was counted, in seconds from the start.
 */

#include "StepDetector.h"

#include <cstdio>
#include <cstring>
#include <fstream>
#include <sstream>
#include <string>

int main(int argc, char *argv[])
{
    if (argc < 2) {
        std::fprintf(stderr, "usage: %s <recording.csv> [-v]\n", argv[0]);
        return 2;
    }
    const bool verbose = argc > 2 && std::strcmp(argv[2], "-v") == 0;

    std::ifstream file(argv[1]);
    if (!file) {
        std::fprintf(stderr, "cannot open %s\n", argv[1]);
        return 2;
    }

    StepDetector detector;
    std::string line;
    std::getline(file, line); // header

    long samples = 0;
    int steps = 0;
    int recordedSteps = 0;
    std::uint64_t firstUs = 0;
    std::uint64_t lastUs = 0;
    std::uint64_t longestGapUs = 0;

    while (std::getline(file, line)) {
        std::istringstream fields(line);
        std::uint64_t timestampUs;
        double x, y, z;
        char comma;
        if (!(fields >> timestampUs >> comma >> x >> comma >> y >> comma >> z >> comma >> recordedSteps))
            continue;

        if (samples == 0)
            firstUs = timestampUs;
        else if (timestampUs > lastUs && timestampUs - lastUs > longestGapUs)
            longestGapUs = timestampUs - lastUs;
        lastUs = timestampUs;
        ++samples;

        const int added = detector.addSample(timestampUs, x, y, z);
        if (added > 0 && verbose)
            std::printf("%8.2f s  +%d\n", (timestampUs - firstUs) / 1e6, added);
        steps += added;
    }

    const double seconds = (lastUs - firstUs) / 1e6;
    std::printf("samples:          %ld over %.1f s (%.1f Hz)\n", samples, seconds,
                seconds > 0 ? samples / seconds : 0.0);
    std::printf("longest gap:      %.3f s\n", longestGapUs / 1e6);
    std::printf("counted on phone: %d\n", recordedSteps);
    std::printf("counted now:      %d\n", steps);
    return 0;
}
