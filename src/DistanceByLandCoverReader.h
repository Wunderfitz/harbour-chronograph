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

#ifndef DISTANCEBYLANDCOVERREADER_H
#define DISTANCEBYLANDCOVERREADER_H

#include <QString>
#include <QThread>

#include <vector>

/**
 * Reads stored activities' GPX files for how far each went over each land
 * cover, away from the UI thread: a long activity's file takes a tenth of a
 * second and more on a phone, and this reads all of those whose distances
 * the database does not hold yet - the activities stored before it did.
 * Set the activities, start(), and read them back once finished() has come.
 */
class DistanceByLandCoverReader : public QThread
{
    Q_OBJECT
    void run() Q_DECL_OVERRIDE {
        readDistances();
    }
public:
    struct Entry {
        qint64 id = -1;
        QString filePath;
        /* Whether the file could be read; once read. */
        bool read = false;
        /* Metres per LandCover::Kind; once read. */
        std::vector<double> distances;
    };

    explicit DistanceByLandCoverReader(QObject *parent = nullptr);
    ~DistanceByLandCoverReader() override;

    /* Only while not running. */
    void setEntries(const std::vector<Entry> &entries);
    /* Only once finished. */
    const std::vector<Entry> &entries() const;

private:
    void readDistances();

    std::vector<Entry> m_entries;
};

#endif // DISTANCEBYLANDCOVERREADER_H
