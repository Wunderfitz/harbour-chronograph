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

#include "Gpx.h"

#include "Track.h"

#include <QDateTime>
#include <QDebug>
#include <QFile>
#include <QSaveFile>
#include <QStringList>
#include <QXmlStreamReader>
#include <QXmlStreamWriter>

#include <cmath>
#include <vector>

namespace {

const QString kGpxNamespace = QStringLiteral("http://www.topografix.com/GPX/1/1");
const QString kGarminNamespace = QStringLiteral("http://www.garmin.com/xmlschemas/TrackPointExtension/v2");
const QString kOwnNamespace = QStringLiteral("https://github.com/Wunderfitz/harbour-chronograph/gpx/1");

/* xsd:dateTime in UTC, to the millisecond - Qt 5.6 has no ISODateWithMs yet. */
QString timeText(double seconds)
{
    return QDateTime::fromMSecsSinceEpoch(qRound64(seconds * 1000.0), Qt::UTC)
            .toString(QStringLiteral("yyyy-MM-dd'T'HH:mm:ss.zzz'Z'"));
}

/* NaN if it is not a time. One without a zone is taken as UTC, as GPX says. */
double parseTime(const QString &text)
{
    QDateTime time = QDateTime::fromString(text.trimmed(), Qt::ISODate);
    if (!time.isValid())
        return std::nan("");
    if (time.timeSpec() == Qt::LocalTime)
        time.setTimeSpec(Qt::UTC);
    return time.toMSecsSinceEpoch() / 1000.0;
}

double parseNumber(const QString &text)
{
    bool ok = false;
    const double value = text.trimmed().toDouble(&ok);
    return ok ? value : std::nan("");
}

QString number(double value, int decimals)
{
    return QString::number(value, 'f', decimals);
}

void writePoint(QXmlStreamWriter &xml, const TrackPoint &point)
{
    xml.writeStartElement(kGpxNamespace, QStringLiteral("trkpt"));
    xml.writeAttribute(QStringLiteral("lat"), number(point.latitude, 7));
    xml.writeAttribute(QStringLiteral("lon"), number(point.longitude, 7));
    if (!std::isnan(point.altitude))
        xml.writeTextElement(kGpxNamespace, QStringLiteral("ele"), number(point.altitude, 1));
    xml.writeTextElement(kGpxNamespace, QStringLiteral("time"), timeText(point.time));

    const bool speed = !std::isnan(point.speed);
    const bool course = !std::isnan(point.course);
    const bool accuracy = !std::isnan(point.accuracy);
    if (speed || course || accuracy) {
        xml.writeStartElement(kGpxNamespace, QStringLiteral("extensions"));
        if (speed || course) {
            xml.writeStartElement(kGarminNamespace, QStringLiteral("TrackPointExtension"));
            if (speed)
                xml.writeTextElement(kGarminNamespace, QStringLiteral("speed"), number(point.speed, 2));
            if (course)
                xml.writeTextElement(kGarminNamespace, QStringLiteral("course"), number(point.course, 1));
            xml.writeEndElement();
        }
        if (accuracy)
            xml.writeTextElement(kOwnNamespace, QStringLiteral("accuracy"), number(point.accuracy, 1));
        xml.writeEndElement();
    }
    xml.writeEndElement();
}

/* A segment as read, before it goes into the track. */
struct Segment {
    double begin = std::nan("");
    double end = std::nan("");
    std::vector<TrackPoint> points;
};

void addSegment(Track *track, const Segment &segment)
{
    if (segment.points.empty() && std::isnan(segment.begin))
        return;
    const double begin = std::isnan(segment.begin) ? segment.points.front().time : segment.begin;
    double end = segment.end;
    if (std::isnan(end))
        end = segment.points.empty() ? begin : segment.points.back().time;
    track->begin(begin);
    for (const TrackPoint &point : segment.points)
        track->addPoint(point);
    track->end(end);
}

} // namespace

namespace Gpx {

bool write(const QString &filePath, const Track &track, const QString &type)
{
    QSaveFile file(filePath);
    if (!file.open(QIODevice::WriteOnly)) {
        qWarning() << "Cannot write" << filePath << file.errorString();
        return false;
    }

    QXmlStreamWriter xml(&file);
    xml.setAutoFormatting(true);
    xml.setAutoFormattingIndent(1);
    xml.writeStartDocument();
    xml.writeDefaultNamespace(kGpxNamespace);
    xml.writeNamespace(kGarminNamespace, QStringLiteral("gpxtpx"));
    xml.writeNamespace(kOwnNamespace, QStringLiteral("chronograph"));
    xml.writeStartElement(kGpxNamespace, QStringLiteral("gpx"));
    xml.writeAttribute(QStringLiteral("version"), QStringLiteral("1.1"));
    xml.writeAttribute(QStringLiteral("creator"), QStringLiteral("Chronograph for Sailfish OS"));

    if (!std::isnan(track.startTime())) {
        xml.writeStartElement(kGpxNamespace, QStringLiteral("metadata"));
        xml.writeTextElement(kGpxNamespace, QStringLiteral("time"), timeText(track.startTime()));
        xml.writeEndElement();
    }

    xml.writeStartElement(kGpxNamespace, QStringLiteral("trk"));
    xml.writeTextElement(kGpxNamespace, QStringLiteral("type"), type);
    const std::vector<TrackSegment> &segments = track.segments();
    for (std::size_t s = 0; s < segments.size(); ++s) {
        const TrackSegment &segment = segments[s];
        const std::size_t end = s + 1 < segments.size() ? segments[s + 1].firstPoint : track.size();
        xml.writeStartElement(kGpxNamespace, QStringLiteral("trkseg"));
        for (std::size_t i = segment.firstPoint; i < end; ++i)
            writePoint(xml, track.point(i));
        // GPX wants a segment's extensions after its points.
        double segmentEnd = segment.end;
        if (std::isnan(segmentEnd))
            segmentEnd = end > segment.firstPoint ? track.point(end - 1).time : segment.begin;
        xml.writeStartElement(kGpxNamespace, QStringLiteral("extensions"));
        xml.writeTextElement(kOwnNamespace, QStringLiteral("begin"), timeText(segment.begin));
        xml.writeTextElement(kOwnNamespace, QStringLiteral("end"), timeText(segmentEnd));
        xml.writeEndElement();
        xml.writeEndElement();
    }
    xml.writeEndElement();

    xml.writeEndElement();
    xml.writeEndDocument();

    if (xml.hasError() || !file.commit()) {
        qWarning() << "Cannot write" << filePath << file.errorString();
        return false;
    }
    return true;
}

/* Goes by the elements' local names, so that a point's speed and course are
 * found in GPX 1.0's own elements as well as in Garmin's extension. */
bool read(const QString &filePath, Track *track, QString *type)
{
    QFile file(filePath);
    if (!file.open(QIODevice::ReadOnly)) {
        qWarning() << "Cannot read" << filePath << file.errorString();
        return false;
    }

    QXmlStreamReader xml(&file);
    Segment segment;
    TrackPoint point;
    bool inSegment = false;
    bool inPoint = false;
    bool hasTime = false;
    /* The open elements, less those read whole with readElementText(). */
    QStringList open;

    while (!xml.atEnd()) {
        xml.readNext();
        if (xml.isStartElement()) {
            const QString name = xml.name().toString();
            const QString parent = open.isEmpty() ? QString() : open.last();
            if (name == QLatin1String("trkseg")) {
                segment = Segment();
                inSegment = true;
            } else if (name == QLatin1String("trkpt") && inSegment) {
                point = TrackPoint();
                point.latitude = parseNumber(xml.attributes().value(QStringLiteral("lat")).toString());
                point.longitude = parseNumber(xml.attributes().value(QStringLiteral("lon")).toString());
                inPoint = true;
                hasTime = false;
            } else if (inPoint) {
                if (name == QLatin1String("ele")) {
                    point.altitude = parseNumber(xml.readElementText());
                } else if (name == QLatin1String("time")) {
                    point.time = parseTime(xml.readElementText());
                    hasTime = !std::isnan(point.time);
                } else if (name == QLatin1String("speed")) {
                    point.speed = parseNumber(xml.readElementText());
                } else if (name == QLatin1String("course")) {
                    point.course = parseNumber(xml.readElementText());
                } else if (name == QLatin1String("accuracy")) {
                    point.accuracy = parseNumber(xml.readElementText());
                }
            } else if (inSegment) {
                if (name == QLatin1String("begin"))
                    segment.begin = parseTime(xml.readElementText());
                else if (name == QLatin1String("end"))
                    segment.end = parseTime(xml.readElementText());
            } else if (name == QLatin1String("type") && parent == QLatin1String("trk")) {
                *type = xml.readElementText().trimmed();
            }
            if (xml.isStartElement())
                open.append(name);
        } else if (xml.isEndElement()) {
            const QStringRef name = xml.name();
            if (name == QLatin1String("trkpt") && inPoint) {
                inPoint = false;
                // Without a time a fix has no place in the track.
                if (hasTime && !std::isnan(point.latitude) && !std::isnan(point.longitude))
                    segment.points.push_back(point);
            } else if (name == QLatin1String("trkseg") && inSegment) {
                inSegment = false;
                addSegment(track, segment);
            }
            if (!open.isEmpty())
                open.removeLast();
        }
    }

    if (xml.hasError()) {
        qWarning() << "Cannot read" << filePath << xml.errorString();
        return false;
    }
    return !track->segments().empty();
}

} // namespace Gpx
