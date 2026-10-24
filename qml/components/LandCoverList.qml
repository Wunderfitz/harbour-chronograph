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
import "../js/Units.js" as Units

// How far an activity, or all of them, went over each land cover, the most first: the colour
// the map paints it in, its name, the distance and its share of the whole.
// The land not known - the map's tiles of it were never seen - comes last.
// As tall as its rows, unless given a height.
Item {
    id: landCover

    // Activity.landCover: the keys and the metres.
    property var entries: []
    property alias label: captionLabel.text
    // Shown in place of the list while it is empty.
    property alias emptyText: emptyLabel.text

    implicitHeight: rows.y + rows.height

    readonly property real total: {
        var sum = 0
        for (var i = 0; i < entries.length; ++i)
            sum += entries[i].distance
        return sum
    }

    Label {
        id: captionLabel
        font.pixelSize: Theme.fontSizeExtraSmall
        color: Theme.secondaryColor
    }

    // The widest share, so that the shares line up.
    Label {
        id: widestShare
        visible: false
        font.pixelSize: Theme.fontSizeSmall
        text: qsTr("%1%").arg(100)
    }

    Column {
        id: rows
        anchors {
            top: captionLabel.bottom
            topMargin: Theme.paddingMedium
            left: parent.left
            right: parent.right
        }
        spacing: Theme.paddingSmall

        Repeater {
            model: landCover.entries

            Item {
                width: parent.width
                height: nameLabel.height

                Rectangle {
                    id: swatch
                    anchors.verticalCenter: parent.verticalCenter
                    width: Theme.paddingLarge + Theme.paddingSmall
                    height: width
                    radius: Theme.paddingSmall / 2
                    color: app.landCoverColor(modelData.key)
                    border.width: Math.max(1, Math.round(Theme.pixelRatio))
                    border.color: Theme.rgba(Theme.primaryColor, Theme.opacityLow)
                }

                Label {
                    id: nameLabel
                    anchors {
                        left: swatch.right
                        leftMargin: Theme.paddingMedium
                        right: distanceLabel.left
                        rightMargin: Theme.paddingMedium
                    }
                    truncationMode: TruncationMode.Fade
                    font.pixelSize: Theme.fontSizeSmall
                    color: modelData.key ? Theme.highlightColor : Theme.secondaryHighlightColor
                    text: app.landCoverName(modelData.key)
                }

                Label {
                    id: distanceLabel
                    anchors {
                        right: shareLabel.left
                        rightMargin: Theme.paddingMedium
                    }
                    font.pixelSize: Theme.fontSizeSmall
                    color: nameLabel.color
                    text: Units.distance(modelData.distance) + " km"
                }

                Label {
                    id: shareLabel
                    anchors.right: parent.right
                    width: widestShare.width
                    horizontalAlignment: Text.AlignRight
                    font.pixelSize: Theme.fontSizeSmall
                    color: Theme.secondaryHighlightColor
                    //: A share of the activity's distance; %1 is the whole percent
                    text: qsTr("%1%").arg(Math.round(100 * modelData.distance / landCover.total))
                }
            }
        }
    }

    Label {
        id: emptyLabel
        visible: landCover.entries.length === 0
        anchors.centerIn: parent
        width: parent.width
        horizontalAlignment: Text.AlignHCenter
        wrapMode: Text.WordWrap
        font.pixelSize: Theme.fontSizeExtraSmall
        color: Theme.secondaryHighlightColor
    }
}
