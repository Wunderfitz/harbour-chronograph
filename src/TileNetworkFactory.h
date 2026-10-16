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

#ifndef TILENETWORKFACTORY_H
#define TILENETWORKFACTORY_H

#include <QQmlNetworkAccessManagerFactory>

/**
 * The network access of the QML engine, which loads the map's tiles as plain
 * Images from the OpenStreetMap tile servers.
 *
 * Their usage policy asks for two things a QML Image does not do by itself:
 * a User-Agent that names the app, and tiles kept on the phone as long as the
 * server says they are good for, rather than fetched again. So every request
 * goes out with Chronograph's User-Agent, and through a disk cache below the
 * app's cache directory.
 */
class TileNetworkFactory : public QQmlNetworkAccessManagerFactory
{
public:
    /* Called by the engine, possibly from its loader threads. */
    QNetworkAccessManager *create(QObject *parent) override;
};

#endif // TILENETWORKFACTORY_H
