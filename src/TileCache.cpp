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

#include "TileCache.h"

#include <QIODevice>
#include <QScopedPointer>
#include <QUrl>

namespace {

/* Decoded tiles kept: a quarter of a megabyte each. */
const int kKeptTiles = 16;

/* Level, column and row in one; a level has at most 2^30 of each. */
quint64 tileKey(int level, int x, int y)
{
    return (quint64(level) << 60) | (quint64(x) << 30) | quint64(y);
}

/* The URL each tile was cached under: as TileLayer.qml asks for it. */
QUrl tileUrl(int level, int x, int y)
{
    return QUrl(QStringLiteral("https://tile.openstreetmap.org/%1/%2/%3.png").arg(level).arg(x).arg(y));
}

} // namespace

TileCache::TileCache(const QString &directory)
{
    m_cache.setCacheDirectory(directory);
}

/* Read only: the map's own cache adds and removes the tiles. One held past
 * its expiry is read all the same, as nothing is fetched to refresh it. */
const std::uint32_t *TileCache::tile(int level, int x, int y)
{
    const quint64 key = tileKey(level, x, y);
    const auto found = m_tiles.constFind(key);
    if (found != m_tiles.constEnd()) {
        m_recent.removeOne(key);
        m_recent.append(key);
        return reinterpret_cast<const std::uint32_t *>(found->constBits());
    }
    if (m_missing.contains(key))
        return nullptr;

    QImage image;
    QScopedPointer<QIODevice> data(m_cache.data(tileUrl(level, x, y)));
    if (data)
        image = QImage::fromData(data->readAll(), "PNG");
    if (image.width() != LandCover::kTileSize || image.height() != LandCover::kTileSize) {
        m_missing.insert(key);
        return nullptr;
    }
    // A row of 256 such pixels takes exactly its 1024 bytes, so the rows follow
    // one another as LandCover reads them.
    image = image.convertToFormat(QImage::Format_RGB32);

    if (m_recent.size() >= kKeptTiles)
        m_tiles.remove(m_recent.takeFirst());
    m_recent.append(key);
    return reinterpret_cast<const std::uint32_t *>(m_tiles.insert(key, image)->constBits());
}

void TileCache::forgetMissing()
{
    m_missing.clear();
}
