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

#include "TileNetworkFactory.h"

#include <QDir>
#include <QNetworkAccessManager>
#include <QNetworkDiskCache>
#include <QNetworkRequest>
#include <QStandardPaths>

namespace {

const char kUserAgent[] = "Chronograph for Sailfish OS (+https://github.com/Wunderfitz/harbour-chronograph)";

/* A tile is 10 to 30 kB, so this holds a few thousand: the usual rounds and
 * their surroundings at every zoom level the map is used at. */
const qint64 kCacheSize = 50 * 1024 * 1024;

class TileNetworkAccessManager : public QNetworkAccessManager
{
public:
    explicit TileNetworkAccessManager(QObject *parent)
        : QNetworkAccessManager(parent)
    {
    }

protected:
    QNetworkReply *createRequest(Operation operation, const QNetworkRequest &request,
                                 QIODevice *data) override
    {
        QNetworkRequest named(request);
        named.setRawHeader("User-Agent", kUserAgent);
        return QNetworkAccessManager::createRequest(operation, named, data);
    }
};

} // namespace

QNetworkAccessManager *TileNetworkFactory::create(QObject *parent)
{
    QNetworkAccessManager *manager = new TileNetworkAccessManager(parent);
    QNetworkDiskCache *cache = new QNetworkDiskCache(manager);
    cache->setCacheDirectory(
            QDir(QStandardPaths::writableLocation(QStandardPaths::CacheLocation))
            .filePath(QStringLiteral("tiles")));
    cache->setMaximumCacheSize(kCacheSize);
    manager->setCache(cache);
    return manager;
}
