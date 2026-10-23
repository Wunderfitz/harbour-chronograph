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

#ifndef TRACKREADER_H
#define TRACKREADER_H

#include "Track.h"

#include <QString>
#include <QThread>

/**
 * Reads a stored activity's GPX file away from the UI thread: a long
 * activity's file takes a tenth of a second and more on a phone. Set the
 * file, start(), and take the track once finished() has come.
 */
class TrackReader : public QThread
{
    Q_OBJECT
    void run() Q_DECL_OVERRIDE {
        readTrack();
    }
public:
    explicit TrackReader(QObject *parent = nullptr);
    ~TrackReader() override;

    /* Only while not running. */
    void setFilePath(const QString &filePath);
    QString filePath() const;

    /* Only once finished. Whether the file could be read; the type it names,
     * if any; the track, which is handed over. */
    bool succeeded() const;
    QString type() const;
    Track takeTrack();

private:
    void readTrack();

    QString m_filePath;
    bool m_succeeded = false;
    QString m_type;
    Track m_track;
};

#endif // TRACKREADER_H
