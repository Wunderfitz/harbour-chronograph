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

#ifndef GPX_H
#define GPX_H

#include <QString>

class Track;

/**
 * A Track as a GPX 1.1 file: one track, one track segment per stretch between
 * pauses, one point per fix - so other apps read it as it is.
 *
 * What GPX itself has no place for goes into extensions: the receiver's speed
 * and course as Garmin's TrackPointExtension, which most sport sites read, and
 * in Chronograph's own namespace each point's accuracy and each segment's begin
 * and end. The last pair is what lets a replay leave the pauses out exactly as
 * the recording did. A file without them still reads, with the segments taken
 * to run from their first point to their last.
 */
namespace Gpx {

/* Writes the file in one go: it is either all there or not replaced at all. */
bool write(const QString &filePath, const Track &track, const QString &type);

/* Adds what the file holds to `track`, which should be empty, and its activity
 * type, if it names one, to `type`. False if the file cannot be read or holds
 * no track at all. */
bool read(const QString &filePath, Track *track, QString *type);

} // namespace Gpx

#endif // GPX_H
