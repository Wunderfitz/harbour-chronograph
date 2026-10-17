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

#include "ChartLine.h"

#include <QPainter>
#include <QPolygonF>

#include <algorithm>
#include <cmath>

namespace {

/* The share of the line's colour the part not yet reached in a replay gets,
 * as on the map. */
const qreal kFaintOpacity = 0.4;

/* The current point in a replay, as a multiple of the line's width. */
const qreal kPointSize = 3;

QVariantList toList(const std::vector<double> &values)
{
    QVariantList list;
    for (double value : values)
        list.append(value);
    return list;
}

bool sameAxis(const ChartAxis &a, const ChartAxis &b)
{
    return a.from == b.from && a.to == b.to && a.ticks == b.ticks;
}

/* A point of the line, and which of the chart's it is. */
struct Vertex {
    QPointF point;
    std::size_t index;
};

} // namespace

ChartLine::ChartLine(QQuickItem *parent)
    : QQuickPaintedItem(parent)
{
    setAntialiasing(true);
    connect(this, &ChartLine::styleChanged, this, [this]() { update(); });
    connect(this, &ChartLine::progressChanged, this, [this]() { update(); });
    connect(this, &ChartLine::widthChanged, this, [this]() { update(); });
    connect(this, &ChartLine::heightChanged, this, [this]() { update(); });
}

Activity *ChartLine::activity() const
{
    return m_activity;
}

void ChartLine::setActivity(Activity *activity)
{
    if (m_activity == activity)
        return;
    disconnect(m_trackConnection);
    disconnect(m_durationConnection);
    m_activity = activity;
    if (m_activity) {
        m_trackConnection = connect(m_activity, &Activity::trackChanged,
                                    this, &ChartLine::handleTrackChanged);
        // The stop ends the last segment, which may lengthen the activity.
        m_durationConnection = connect(m_activity, &Activity::durationChanged,
                                       this, &ChartLine::handleTrackChanged);
    }
    emit activityChanged();
    handleTrackChanged();
}

ChartLine::Quantity ChartLine::quantity() const
{
    return Quantity(m_chart.quantity());
}

void ChartLine::setQuantity(Quantity quantity)
{
    if (this->quantity() == quantity)
        return;
    m_chart = TrackChart(TrackChart::Quantity(quantity));
    emit quantityChanged();
    handleTrackChanged();
}

QColor ChartLine::color() const
{
    return m_color;
}

void ChartLine::setColor(const QColor &color)
{
    if (m_color == color)
        return;
    m_color = color;
    emit styleChanged();
}

QColor ChartLine::averageColor() const
{
    return m_averageColor;
}

void ChartLine::setAverageColor(const QColor &color)
{
    if (m_averageColor == color)
        return;
    m_averageColor = color;
    emit styleChanged();
}

QColor ChartLine::gridColor() const
{
    return m_gridColor;
}

void ChartLine::setGridColor(const QColor &color)
{
    if (m_gridColor == color)
        return;
    m_gridColor = color;
    emit styleChanged();
}

qreal ChartLine::lineWidth() const
{
    return m_lineWidth;
}

void ChartLine::setLineWidth(qreal lineWidth)
{
    if (m_lineWidth == lineWidth)
        return;
    m_lineWidth = lineWidth;
    emit styleChanged();
}

int ChartLine::progress() const
{
    return m_progress;
}

void ChartLine::setProgress(int progress)
{
    if (m_progress == progress)
        return;
    m_progress = progress;
    emit progressChanged();
}

bool ChartLine::hasValues() const
{
    return m_chart.hasValues();
}

qreal ChartLine::topValue() const
{
    return m_chart.valueAxis().to;
}

qreal ChartLine::bottomValue() const
{
    return m_chart.valueAxis().from;
}

QVariantList ChartLine::valueTicks() const
{
    return toList(m_chart.valueAxis().ticks);
}

qreal ChartLine::length() const
{
    return m_chart.timeAxis().to;
}

QVariantList ChartLine::timeTicks() const
{
    return toList(m_chart.timeAxis().ticks);
}

void ChartLine::paint(QPainter *painter)
{
    if (!m_activity || !m_chart.hasValues())
        return;
    painter->setRenderHint(QPainter::Antialiasing);

    painter->setPen(QPen(m_gridColor, 1));
    for (double tick : m_chart.valueAxis().ticks) {
        const qreal y = qBound(0.5, std::floor(yOf(tick)) + 0.5, height() - 0.5);
        painter->drawLine(QPointF(0, y), QPointF(width(), y));
    }

    // Under the line, which matters more.
    if (!std::isnan(m_chart.average())) {
        painter->setPen(QPen(m_averageColor, m_lineWidth, Qt::DotLine));
        const qreal y = yOf(m_chart.average());
        painter->drawLine(QPointF(0, y), QPointF(width(), y));
    }

    const std::vector<TrackSegment> &segments = m_activity->track().segments();
    const std::size_t count = m_chart.size();
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
                    drawPart(painter, first, end);
                continue;
            }
            // The two parts share the point at `progress`, so they meet.
            const std::size_t split = std::size_t(m_progress) + 1;
            if (full)
                drawPart(painter, first, std::min(end, split));
            else
                drawPart(painter, std::max(first, split - 1), end);
        }
    }

    if (m_progress >= 0 && std::size_t(m_progress) < count
            && !std::isnan(m_chart.value(std::size_t(m_progress)))) {
        const std::size_t index = std::size_t(m_progress);
        const qreal radius = kPointSize * m_lineWidth / 2;
        painter->setPen(Qt::NoPen);
        painter->setBrush(m_color);
        painter->drawEllipse(QPointF(xOf(m_chart.time(index)), yOf(m_chart.value(index))),
                             radius, radius);
    }
}

/* Takes over the new points, and tells QML about the axes only when they
 * changed: the labels' repeaters would rebuild themselves every second. */
void ChartLine::handleTrackChanged()
{
    const bool hadValues = m_chart.hasValues();
    const ChartAxis valueAxis = m_chart.valueAxis();
    const ChartAxis timeAxis = m_chart.timeAxis();
    if (m_activity)
        m_chart.update(m_activity->track());
    else
        m_chart.update(Track());
    if (m_chart.hasValues() != hadValues || !sameAxis(m_chart.valueAxis(), valueAxis)
            || !sameAxis(m_chart.timeAxis(), timeAxis))
        emit scaleChanged();
    update();
}

/* A long activity has many points to a column of pixels. Of those in each,
 * only the first, the highest, the lowest and the last are drawn, in their
 * order: that looks exactly like all of them. */
void ChartLine::drawPart(QPainter *painter, std::size_t from, std::size_t to)
{
    QPolygonF line;
    bool open = false;
    int column = 0;
    Vertex first;
    Vertex top;
    Vertex bottom;
    Vertex last;

    const auto closeColumn = [&]() {
        const Vertex &earlier = top.index <= bottom.index ? top : bottom;
        const Vertex &later = top.index <= bottom.index ? bottom : top;
        line.append(first.point);
        if (earlier.index != first.index)
            line.append(earlier.point);
        if (later.index != earlier.index)
            line.append(later.point);
        if (last.index != later.index)
            line.append(last.point);
        open = false;
    };

    for (std::size_t i = from; i < to; ++i) {
        const double value = m_chart.value(i);
        if (std::isnan(value)) {
            if (open)
                closeColumn();
            flush(painter, line);
            continue;
        }
        const Vertex vertex { QPointF(xOf(m_chart.time(i)), yOf(value)), i };
        const int c = int(std::floor(vertex.point.x()));
        if (open && c != column)
            closeColumn();
        if (!open) {
            column = c;
            first = top = bottom = last = vertex;
            open = true;
            continue;
        }
        if (vertex.point.y() < top.point.y())
            top = vertex;
        if (vertex.point.y() > bottom.point.y())
            bottom = vertex;
        last = vertex;
    }
    if (open)
        closeColumn();
    flush(painter, line);
}

/* A value alone between gaps is a dot. */
void ChartLine::flush(QPainter *painter, QPolygonF &line)
{
    if (line.size() == 1)
        painter->drawPoint(line.first());
    else if (line.size() > 1)
        painter->drawPolyline(line);
    line.clear();
}

double ChartLine::xOf(double time) const
{
    return time / m_chart.timeAxis().to * width();
}

/* Half a line's width in from the edges, so that a value at the end of the
 * axis is drawn whole. */
double ChartLine::yOf(double value) const
{
    const ChartAxis &axis = m_chart.valueAxis();
    const double inset = m_lineWidth / 2;
    return inset + (axis.to - value) / (axis.to - axis.from) * (height() - 2 * inset);
}
