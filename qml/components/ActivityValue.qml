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

// One of the activity page's values: the number large with its unit beside
// it, and what it is below.
Item {
    id: activityValue

    property alias value: valueLabel.text
    property alias unit: unitLabel.text
    property alias label: captionLabel.text
    // Text.AlignHCenter or Text.AlignRight moves the number, its unit and the
    // caption to the middle or the right edge; the statistics page centres
    // them.
    property int horizontalAlignment: Text.AlignLeft

    implicitHeight: column.height

    Column {
        id: column
        width: parent.width

        // As wide as what it holds, so that it can be moved along.
        Row {
            id: valueRow
            x: activityValue.horizontalAlignment === Text.AlignRight
               ? column.width - width
               : activityValue.horizontalAlignment === Text.AlignHCenter ? (column.width - width) / 2 : 0
            spacing: Theme.paddingSmall

            Label {
                id: valueLabel
                // Shrinks rather than truncates, should an activity run long.
                width: Math.min(implicitWidth, column.width
                                - (unitLabel.visible ? unitLabel.width + valueRow.spacing : 0))
                fontSizeMode: Text.HorizontalFit
                minimumPixelSize: Theme.fontSizeMedium
                font.pixelSize: Theme.fontSizeExtraLarge
                font.family: Theme.fontFamilyHeading
                color: Theme.highlightColor
            }

            // Hidden without a unit, so that the row ends with the number.
            Label {
                id: unitLabel
                visible: text.length > 0
                anchors.baseline: valueLabel.baseline
                font.pixelSize: Theme.fontSizeSmall
                color: Theme.secondaryHighlightColor
            }
        }

        Label {
            id: captionLabel
            width: parent.width
            horizontalAlignment: activityValue.horizontalAlignment
            truncationMode: TruncationMode.Fade
            font.pixelSize: Theme.fontSizeExtraSmall
            color: Theme.secondaryColor
        }
    }
}
