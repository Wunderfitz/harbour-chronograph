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

#include "TrackLine.h"

#include <QPainter>
#include <QPolygonF>

#include <algorithm>
#include <cmath>

namespace {

const double kPi = 3.14159265358979323846;

/* Web Mercator's latitude limit: beyond it the world would not be square. */
const double kMaxLatitude = 85.05112878;

/* The share of the line's colour the part not yet reached in a replay gets. */
const qreal kFaintOpacity = 0.4;

QPointF project(const TrackPoint &point)
{
    const double latitude = std::max(-kMaxLatitude, std::min(kMaxLatitude, point.latitude));
    const double sine = std::sin(latitude * kPi / 180.0);
    return QPointF((point.longitude + 180.0) / 360.0,
                   0.5 - std::log((1 + sine) / (1 - sine)) / (4 * kPi));
}

} // namespace

TrackLine::TrackLine(QQuickItem *parent)
    : QQuickPaintedItem(parent)
{
    setAntialiasing(true);
    connect(this, &TrackLine::viewChanged, this, [this]() { update(); });
    connect(this, &TrackLine::styleChanged, this, [this]() { update(); });
    connect(this, &TrackLine::progressChanged, this, [this]() { update(); });
}

Activity *TrackLine::activity() const
{
    return m_activity;
}

void TrackLine::setActivity(Activity *activity)
{
    if (m_activity == activity)
        return;
    disconnect(m_trackConnection);
    m_activity = activity;
    m_projected.clear();
    if (m_activity)
        m_trackConnection = connect(m_activity, &Activity::trackChanged,
                                    this, &TrackLine::handleTrackChanged);
    emit activityChanged();
    handleTrackChanged();
}

qreal TrackLine::centerX() const
{
    return m_centerX;
}

void TrackLine::setCenterX(qreal centerX)
{
    if (m_centerX == centerX)
        return;
    m_centerX = centerX;
    emit viewChanged();
}

qreal TrackLine::centerY() const
{
    return m_centerY;
}

void TrackLine::setCenterY(qreal centerY)
{
    if (m_centerY == centerY)
        return;
    m_centerY = centerY;
    emit viewChanged();
}

qreal TrackLine::zoom() const
{
    return m_zoom;
}

void TrackLine::setZoom(qreal zoom)
{
    if (m_zoom == zoom)
        return;
    m_zoom = zoom;
    emit viewChanged();
}

qreal TrackLine::tileSize() const
{
    return m_tileSize;
}

void TrackLine::setTileSize(qreal tileSize)
{
    if (m_tileSize == tileSize)
        return;
    m_tileSize = tileSize;
    emit viewChanged();
}

QColor TrackLine::color() const
{
    return m_color;
}

void TrackLine::setColor(const QColor &color)
{
    if (m_color == color)
        return;
    m_color = color;
    emit styleChanged();
}

qreal TrackLine::lineWidth() const
{
    return m_lineWidth;
}

void TrackLine::setLineWidth(qreal lineWidth)
{
    if (m_lineWidth == lineWidth)
        return;
    m_lineWidth = lineWidth;
    emit styleChanged();
}

int TrackLine::progress() const
{
    return m_progress;
}

void TrackLine::setProgress(int progress)
{
    if (m_progress == progress)
        return;
    m_progress = progress;
    emit progressChanged();
}

void TrackLine::paint(QPainter *painter)
{
    if (!m_activity || m_projected.size() < 2)
        return;
    painter->setRenderHint(QPainter::Antialiasing);
    const double world = m_tileSize * std::pow(2.0, m_zoom);
    const std::vector<TrackSegment> &segments = m_activity->track().segments();
    const std::size_t count = m_projected.size();

    for (int pass = 0; pass < 2; ++pass) {
        const bool full = pass == 1;
        QColor color = m_color;
        if (!full)
            color.setAlphaF(color.alphaF() * kFaintOpacity);
        painter->setPen(QPen(color, m_lineWidth, Qt::SolidLine, Qt::RoundCap, Qt::RoundJoin));

        for (std::size_t s = 0; s < segments.size(); ++s) {
            const std::size_t first = std::min(segments[s].firstPoint, count);
            const std::size_t end = s + 1 < segments.size()
                    ? std::min(segments[s + 1].firstPoint, count) : count;
            if (m_progress < 0) {
                if (full)
                    drawPart(painter, first, end, world);
                continue;
            }
            // The two parts share the point at `progress`, so they meet.
            const std::size_t split = std::size_t(m_progress) + 1;
            if (full)
                drawPart(painter, first, std::min(end, split), world);
            else
                drawPart(painter, std::max(first, split - 1), end, world);
        }
    }
}

/* Takes over only the points that are new, unless the track went back to fewer
 * than before - a different activity. */
void TrackLine::handleTrackChanged()
{
    if (!m_activity) {
        m_projected.clear();
        update();
        return;
    }
    const Track &track = m_activity->track();
    if (track.size() < m_projected.size())
        m_projected.clear();
    for (std::size_t i = m_projected.size(); i < track.size(); ++i)
        m_projected.push_back(project(track.point(i)));
    update();
}

/* Points closer than a pixel to the last one drawn are left out: with a long
 * activity on a small scale that is most of them. */
void TrackLine::drawPart(QPainter *painter, std::size_t from, std::size_t to, double world)
{
    if (to <= from + 1)
        return;
    const double halfWidth = width() / 2;
    const double halfHeight = height() / 2;
    QPolygonF line;
    line.reserve(int(to - from));
    for (std::size_t i = from; i < to; ++i) {
        const QPointF point((m_projected[i].x() - m_centerX) * world + halfWidth,
                            (m_projected[i].y() - m_centerY) * world + halfHeight);
        if (!line.isEmpty() && i + 1 < to
                && std::fabs(point.x() - line.last().x()) < 1
                && std::fabs(point.y() - line.last().y()) < 1)
            continue;
        line.append(point);
    }
    painter->drawPolyline(line);
}
