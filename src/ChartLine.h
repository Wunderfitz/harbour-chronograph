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

#ifndef CHARTLINE_H
#define CHARTLINE_H

#include <QColor>
#include <QPointer>
#include <QQuickPaintedItem>
#include <QVariantList>

#include "Activity.h"
#include "TrackChart.h"

class QPolygonF;

/**
 * One of an activity's values over its time - the pace, the speed or the
 * elevation - drawn as a line chart filling the item: a grid line at each of
 * the value axis's ticks, the line, broken where there is no value and at each
 * pause, and for the pace and the speed a dotted line at the average. In a
 * replay the part up to `progress` is drawn full and the rest faint, as on the
 * map.
 *
 * What the axes stand for comes from TrackChart; the labels are left to QML,
 * which places them by the properties below.
 */
class ChartLine : public QQuickPaintedItem
{
    Q_OBJECT

    Q_PROPERTY(Activity *activity READ activity WRITE setActivity NOTIFY activityChanged)
    Q_PROPERTY(Quantity quantity READ quantity WRITE setQuantity NOTIFY quantityChanged)
    Q_PROPERTY(QColor color READ color WRITE setColor NOTIFY styleChanged)
    Q_PROPERTY(QColor averageColor READ averageColor WRITE setAverageColor NOTIFY styleChanged)
    Q_PROPERTY(QColor gridColor READ gridColor WRITE setGridColor NOTIFY styleChanged)
    Q_PROPERTY(qreal lineWidth READ lineWidth WRITE setLineWidth NOTIFY styleChanged)
    /* The last point drawn full; -1 draws all of them full. */
    Q_PROPERTY(int progress READ progress WRITE setProgress NOTIFY progressChanged)

    /* Whether there is anything to draw at all. */
    Q_PROPERTY(bool hasValues READ hasValues NOTIFY scaleChanged)
    /* The values at the top and bottom edges - seconds per kilometre, upside
     * down, kilometres per hour or metres - and those to label. */
    Q_PROPERTY(qreal topValue READ topValue NOTIFY scaleChanged)
    Q_PROPERTY(qreal bottomValue READ bottomValue NOTIFY scaleChanged)
    Q_PROPERTY(QVariantList valueTicks READ valueTicks NOTIFY scaleChanged)
    /* Seconds from the left edge to the right, and those to label. */
    Q_PROPERTY(qreal length READ length NOTIFY scaleChanged)
    Q_PROPERTY(QVariantList timeTicks READ timeTicks NOTIFY scaleChanged)

public:
    enum Quantity {
        Pace = TrackChart::Pace,
        Elevation = TrackChart::Elevation,
        Speed = TrackChart::Speed
    };
    Q_ENUM(Quantity)

    explicit ChartLine(QQuickItem *parent = nullptr);

    Activity *activity() const;
    void setActivity(Activity *activity);
    Quantity quantity() const;
    void setQuantity(Quantity quantity);
    QColor color() const;
    void setColor(const QColor &color);
    QColor averageColor() const;
    void setAverageColor(const QColor &color);
    QColor gridColor() const;
    void setGridColor(const QColor &color);
    qreal lineWidth() const;
    void setLineWidth(qreal lineWidth);
    int progress() const;
    void setProgress(int progress);

    bool hasValues() const;
    qreal topValue() const;
    qreal bottomValue() const;
    QVariantList valueTicks() const;
    qreal length() const;
    QVariantList timeTicks() const;

    void paint(QPainter *painter) override;

signals:
    void activityChanged();
    void quantityChanged();
    void styleChanged();
    void progressChanged();
    void scaleChanged();

private:
    void handleTrackChanged();
    void drawPart(QPainter *painter, std::size_t from, std::size_t to);
    void flush(QPainter *painter, QPolygonF &line);
    double xOf(double time) const;
    double yOf(double value) const;

    QPointer<Activity> m_activity;
    QMetaObject::Connection m_trackConnection;
    QMetaObject::Connection m_durationConnection;
    TrackChart m_chart;
    QColor m_color = Qt::red;
    QColor m_averageColor = Qt::red;
    QColor m_gridColor = Qt::gray;
    qreal m_lineWidth = 2;
    int m_progress = -1;
};

#endif // CHARTLINE_H
