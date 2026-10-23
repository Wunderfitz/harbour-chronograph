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

#include "TrackReader.h"

#include "Gpx.h"

#include <utility>

TrackReader::TrackReader(QObject *parent)
    : QThread(parent)
{
}

/* A file is read in one go, so one being read is read to its end. */
TrackReader::~TrackReader()
{
    wait();
}

void TrackReader::setFilePath(const QString &filePath)
{
    m_filePath = filePath;
}

QString TrackReader::filePath() const
{
    return m_filePath;
}

bool TrackReader::succeeded() const
{
    return m_succeeded;
}

QString TrackReader::type() const
{
    return m_type;
}

Track TrackReader::takeTrack()
{
    Track track = std::move(m_track);
    m_track = Track();
    return track;
}

void TrackReader::readTrack()
{
    m_track = Track();
    m_type.clear();
    m_succeeded = Gpx::read(m_filePath, &m_track, &m_type);
}
