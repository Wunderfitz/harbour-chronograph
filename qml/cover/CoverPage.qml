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
import "../components"

CoverBackground {
    id: cover

    readonly property bool unavailable: stepCounter.status === StepCounter.Unavailable

    // Anchored the way harbour-fernschreiber and harbour-lauscher anchor theirs:
    // a square as wide as the cover is tall, hung off the bottom right, so it
    // overflows the cover and reads large.
    BackgroundImage {
        id: backgroundImage
        width: parent.height - Theme.paddingLarge
        height: width
        sourceDimension: width
        anchors {
            verticalCenter: parent.verticalCenter
            centerIn: undefined
            bottom: parent.bottom
            bottomMargin: Theme.paddingMedium
            right: parent.right
            rightMargin: Theme.paddingMedium
        }
    }

    Column {
        anchors {
            left: parent.left
            right: parent.right
            top: parent.top
            margins: Theme.paddingLarge
        }
        spacing: Theme.paddingSmall

        Label {
            width: parent.width
            visible: !cover.unavailable
            horizontalAlignment: Text.AlignLeft
            fontSizeMode: Text.HorizontalFit
            minimumPixelSize: Theme.fontSizeLarge
            font.pixelSize: Theme.fontSizeHuge
            font.family: Theme.fontFamilyHeading
            color: Theme.primaryColor
            text: Number(stepCounter.steps).toLocaleString(Qt.locale(), "f", 0)
        }

        Label {
            width: parent.width
            horizontalAlignment: Text.AlignLeft
            truncationMode: TruncationMode.Fade
            font.pixelSize: Theme.fontSizeLarge
            color: Theme.highlightColor
            text: cover.unavailable ? qsTr("Chronograph") : qsTr("Steps")
        }

        Label {
            width: parent.width
            visible: stepCounter.status !== StepCounter.Counting
            horizontalAlignment: Text.AlignLeft
            wrapMode: Text.WordWrap
            maximumLineCount: 3
            truncationMode: TruncationMode.Fade
            font.pixelSize: Theme.fontSizeSmall
            color: Theme.primaryColor
            text: cover.unavailable ? qsTr("No accelerometer")
                                    : qsTr("Waiting for the accelerometer…")
        }
    }
}
