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

#ifndef LANDCOVERREADER_H
#define LANDCOVERREADER_H

#include "TileCache.h"

#include <QThread>

#include <cstddef>
#include <cstdint>
#include <vector>

/**
 * Reads the land cover of track points off the map's cached tiles, away from
 * the UI thread: decoding the tiles of a long activity takes seconds. One
 * batch at a time - set the points, start(), and read them back once
 * finished() has come.
 *
 * The points are copies, as the track goes on changing meanwhile: each says
 * where in the track it was and when it was taken, so that the caller can
 * tell whether the track still holds it. The tile cache is only ever used in
 * here, by one batch after another.
 */
class LandCoverReader : public QThread
{
    Q_OBJECT
    void run() Q_DECL_OVERRIDE {
        readPoints();
    }
public:
    struct Point {
        std::size_t index = 0;
        double time = 0;
        double latitude = 0;
        double longitude = 0;
        /* The level its land cover is known from, 0 for none; once read, the
         * level found, 0 if none finer was. */
        int level = 0;
        /* What was found there, if anything; Unknown until read. */
        int kind = 0;
        /* See LandCover::classifyFinest(). */
        std::uint32_t untelling = 0;
    };

    explicit LandCoverReader(QObject *parent = nullptr);
    ~LandCoverReader() override;

    /* Only while not running. forgetMissing tries again the tiles found
     * missing before, which the map may have loaded since. */
    void setPoints(const std::vector<Point> &points, bool forgetMissing);
    /* Only once finished. */
    const std::vector<Point> &points() const;

private:
    void readPoints();

    TileCache m_tiles;
    std::vector<Point> m_points;
    bool m_forgetMissing = false;
};

#endif // LANDCOVERREADER_H
