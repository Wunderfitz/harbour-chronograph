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

#include "TrackSummaryReader.h"

#include "Gpx.h"
#include "LandCover.h"
#include "Track.h"

TrackSummaryReader::TrackSummaryReader(QObject *parent)
    : QThread(parent)
{
}

/* Files still to read are given up, and read at the next start. */
TrackSummaryReader::~TrackSummaryReader()
{
    requestInterruption();
    wait();
}

void TrackSummaryReader::setEntries(const std::vector<Entry> &entries)
{
    m_entries = entries;
}

const std::vector<TrackSummaryReader::Entry> &TrackSummaryReader::entries() const
{
    return m_entries;
}

void TrackSummaryReader::readSummaries()
{
    for (Entry &entry : m_entries) {
        if (isInterruptionRequested())
            return;
        Track track;
        QString type;
        entry.read = Gpx::read(entry.filePath, &track, &type);
        entry.distances = track.distanceByLandCover(int(track.size()) - 1, LandCover::KindCount);
        entry.climb = track.stateAt(track.duration()).elevationGain;
    }
}
