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

#ifndef TILECACHE_H
#define TILECACHE_H

#include "LandCover.h"

#include <QHash>
#include <QImage>
#include <QList>
#include <QNetworkDiskCache>
#include <QSet>

/**
 * The map's tiles as LandCover reads them: straight from the disk cache that
 * the QML engine's network access keeps them in (see TileNetworkFactory),
 * never from the network. So only tiles the map has shown are read -
 * OpenStreetMap's tile usage policy allows fetching no other - and reading
 * them costs no data.
 *
 * Decoded tiles are kept for a while, as the points of a track come one after
 * another over the same few tiles. Tiles found missing are remembered too,
 * until forgetMissing(): the map may have loaded them since.
 */
class TileCache : public LandCover::Tiles
{
public:
    explicit TileCache(const QString &directory);

    const std::uint32_t *tile(int level, int x, int y) override;

    void forgetMissing();

private:
    QNetworkDiskCache m_cache;
    QHash<quint64, QImage> m_tiles;
    /* The keys of m_tiles, the one read last at the end. */
    QList<quint64> m_recent;
    QSet<quint64> m_missing;
};

#endif // TILECACHE_H
