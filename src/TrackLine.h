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

#ifndef TRACKLINE_H
#define TRACKLINE_H

#include <QColor>
#include <QPointer>
#include <QPointF>
#include <QQuickPaintedItem>

#include <vector>

#include "Activity.h"

/**
 * An activity's route, drawn over the map: one line per segment, so nothing
 * joins the two ends of a pause. In a replay the part up to `progress` is
 * drawn full and the rest faint.
 *
 * Painting thousands of points is too slow to do for every frame of a pan or
 * a pinch, so the map does not ask for it: it sets the view this item paints
 * for (centerX, centerY, zoom) a few times a second at most, and moves and
 * scales the item in between (see TileMap.qml).
 */
class TrackLine : public QQuickPaintedItem
{
    Q_OBJECT

    Q_PROPERTY(Activity *activity READ activity WRITE setActivity NOTIFY activityChanged)
    /* The view: its centre in Web Mercator coordinates, 0 to 1 from the top
     * left of the world, and the zoom level with the size of one tile. */
    Q_PROPERTY(qreal centerX READ centerX WRITE setCenterX NOTIFY viewChanged)
    Q_PROPERTY(qreal centerY READ centerY WRITE setCenterY NOTIFY viewChanged)
    Q_PROPERTY(qreal zoom READ zoom WRITE setZoom NOTIFY viewChanged)
    Q_PROPERTY(qreal tileSize READ tileSize WRITE setTileSize NOTIFY viewChanged)
    Q_PROPERTY(QColor color READ color WRITE setColor NOTIFY styleChanged)
    Q_PROPERTY(qreal lineWidth READ lineWidth WRITE setLineWidth NOTIFY styleChanged)
    /* The last point drawn full; -1 draws all of them full. */
    Q_PROPERTY(int progress READ progress WRITE setProgress NOTIFY progressChanged)

public:
    explicit TrackLine(QQuickItem *parent = nullptr);

    Activity *activity() const;
    void setActivity(Activity *activity);
    qreal centerX() const;
    void setCenterX(qreal centerX);
    qreal centerY() const;
    void setCenterY(qreal centerY);
    qreal zoom() const;
    void setZoom(qreal zoom);
    qreal tileSize() const;
    void setTileSize(qreal tileSize);
    QColor color() const;
    void setColor(const QColor &color);
    qreal lineWidth() const;
    void setLineWidth(qreal lineWidth);
    int progress() const;
    void setProgress(int progress);

    void paint(QPainter *painter) override;

signals:
    void activityChanged();
    void viewChanged();
    void styleChanged();
    void progressChanged();

private:
    void handleTrackChanged();
    void drawPart(QPainter *painter, std::size_t from, std::size_t to, double world);

    QPointer<Activity> m_activity;
    QMetaObject::Connection m_trackConnection;
    /* The track's points in Web Mercator coordinates, worked out once each. */
    std::vector<QPointF> m_projected;
    qreal m_centerX = 0.5;
    qreal m_centerY = 0.5;
    qreal m_zoom = 0;
    qreal m_tileSize = 256;
    QColor m_color = Qt::red;
    qreal m_lineWidth = 4;
    int m_progress = -1;
};

#endif // TRACKLINE_H
