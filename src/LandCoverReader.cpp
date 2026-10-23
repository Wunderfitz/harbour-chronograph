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

#include "LandCoverReader.h"

#include "TileNetworkFactory.h"

#include <algorithm>

LandCoverReader::LandCoverReader(QObject *parent)
    : QThread(parent)
    , m_tiles(TileNetworkFactory::cacheDirectory())
{
}

/* A batch still running is given up: what it found is lost, and found again
 * the next time the activity is opened. */
LandCoverReader::~LandCoverReader()
{
    requestInterruption();
    wait();
}

void LandCoverReader::setPoints(const std::vector<Point> &points, bool forgetMissing)
{
    m_points = points;
    m_forgetMissing = forgetMissing;
}

const std::vector<LandCoverReader::Point> &LandCoverReader::points() const
{
    return m_points;
}

/* At the finest level the tiles at hand tell, if that is finer than what the
 * point has. */
void LandCoverReader::readPoints()
{
    if (m_forgetMissing)
        m_tiles.forgetMissing();
    for (Point &point : m_points) {
        if (isInterruptionRequested())
            return;
        int level = 0;
        point.kind = LandCover::classifyFinest(
                m_tiles, LandCover::kFinestLevel, std::max(LandCover::kCoarsestLevel, point.level + 1),
                point.latitude, point.longitude, &level, &point.untelling);
        point.level = level;
    }
}
