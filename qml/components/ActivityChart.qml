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

import QtQuick 2.2
import Sailfish.Silica 1.0
import de.ygriega.chronograph 1.0
import "../js/Units.js" as Units

// One of the activity page's charts: a value over the activity's time, with
// what it is above, the value's labels on the left and the time's below -
// unless showTime leaves those to the chart underneath, which has the same
// time axis. ChartLine draws the rest and says where the labels go.
Item {
    id: chart

    property alias activity: line.activity
    property alias quantity: line.quantity
    // See ChartLine.progress.
    property alias progress: line.progress
    property alias label: captionLabel.text
    property alias unit: unitLabel.text
    // Shown in place of the chart while there is nothing to draw.
    property alias emptyText: emptyLabel.text
    property bool showTime: true
    // The caption in the highlight colour, as while the chart is pressed.
    property bool highlighted

    // The height of one line of the axes' labels.
    readonly property real labelHeight: widest.height

    function valueText(value) {
        switch (line.quantity) {
        case ChartLine.Pace:
            return Units.paceOf(value)
        case ChartLine.Speed:
            return Number(value).toLocaleString(Qt.locale(), "f", 0)
        default:
            return Units.metres(value)
        }
    }

    Row {
        id: caption
        spacing: Theme.paddingSmall

        Label {
            id: captionLabel
            font.pixelSize: Theme.fontSizeExtraSmall
            color: chart.highlighted ? Theme.highlightColor : Theme.secondaryColor
        }

        Label {
            id: unitLabel
            anchors.baseline: captionLabel.baseline
            font.pixelSize: Theme.fontSizeTiny
            color: chart.highlighted ? Theme.highlightColor : Theme.secondaryColor
        }
    }

    // The widest of the value's labels: a pace of 10:00, 1.000 m or 100 km/h.
    Label {
        id: widest
        visible: false
        font.pixelSize: Theme.fontSizeTiny
        text: "00:00"
    }

    Item {
        id: plot

        anchors {
            top: caption.bottom
            topMargin: Theme.paddingSmall
            bottom: timeAxis.top
            left: parent.left
            right: parent.right
        }

        Repeater {
            model: line.hasValues ? line.valueTicks : []

            Label {
                x: widest.width - width
                y: line.y + (line.topValue - modelData) / (line.topValue - line.bottomValue)
                   * line.height - height / 2
                font.pixelSize: Theme.fontSizeTiny
                color: Theme.secondaryColor
                text: chart.valueText(modelData)
            }
        }

        // Half a label clear of the top and the bottom, so the outer ones fit.
        ChartLine {
            id: line
            x: widest.width + Theme.paddingMedium
            y: widest.height / 2
            width: parent.width - x
            height: Math.max(0, parent.height - widest.height)
            visible: hasValues
            color: Theme.highlightColor
            averageColor: Theme.secondaryHighlightColor
            gridColor: Theme.rgba(Theme.primaryColor, Theme.opacityFaint)
            lineWidth: Math.max(2, Math.round(2 * Theme.pixelRatio))
        }

        Label {
            id: emptyLabel
            visible: !line.hasValues
            anchors.centerIn: parent
            width: parent.width
            horizontalAlignment: Text.AlignHCenter
            wrapMode: Text.WordWrap
            font.pixelSize: Theme.fontSizeExtraSmall
            color: Theme.secondaryHighlightColor
        }
    }

    Item {
        id: timeAxis

        anchors {
            bottom: parent.bottom
            left: parent.left
            right: parent.right
        }
        height: chart.showTime ? widest.height : 0
        visible: chart.showTime && line.hasValues

        Repeater {
            model: line.timeTicks

            // Under its tick, but never past either end.
            Label {
                x: Math.max(0, Math.min(timeAxis.width - width,
                                        line.x + modelData / line.length * line.width - width / 2))
                font.pixelSize: Theme.fontSizeTiny
                color: Theme.secondaryColor
                text: Units.duration(modelData)
            }
        }
    }
}
