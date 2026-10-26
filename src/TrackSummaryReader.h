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

#ifndef TRACKSUMMARYREADER_H
#define TRACKSUMMARYREADER_H

#include <QString>
#include <QThread>

#include <vector>

/**
 * Reads stored activities' GPX files for what the database does not hold of
 * them yet - how far each went over each land cover, and how far it climbed
 * - away from the UI thread: a long activity's file takes a tenth of a
 * second and more on a phone, and this reads all of those stored before the
 * database held either. Set the activities, start(), and read them back once
 * finished() has come.
 */
class TrackSummaryReader : public QThread
{
    Q_OBJECT
    void run() Q_DECL_OVERRIDE {
        readSummaries();
    }
public:
    struct Entry {
        qint64 id = -1;
        QString filePath;
        /* Whether the file could be read; once read. */
        bool read = false;
        /* Metres per LandCover::Kind; once read. */
        std::vector<double> distances;
        /* Metres climbed; once read. */
        double climb = 0;
    };

    explicit TrackSummaryReader(QObject *parent = nullptr);
    ~TrackSummaryReader() override;

    /* Only while not running. */
    void setEntries(const std::vector<Entry> &entries);
    /* Only once finished. */
    const std::vector<Entry> &entries() const;

private:
    void readSummaries();

    std::vector<Entry> m_entries;
};

#endif // TRACKSUMMARYREADER_H
