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
import "../js/Units.js" as Units

// Today's steps - or, while an activity is recorded or paused, that activity,
// with an action to pause or resume it.
CoverBackground {
    id: cover

    readonly property bool unavailable: stepCounter.status === StepCounter.Unavailable

    // The activity page's Activity, from start to stop; null otherwise.
    readonly property QtObject activity: activityHistory.recording
    readonly property bool recording: activity !== null
    readonly property bool paused: recording && activity.status === Activity.Paused

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
        visible: !cover.recording
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
            text: Number(stepHistory.todaySteps).toLocaleString(Qt.locale(), "f", 0)
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

    // The activity: its type, then the time large, the distance and the pace.
    // Every value checks for the activity, which goes before this does.
    Column {
        visible: cover.recording
        anchors {
            left: parent.left
            right: parent.right
            top: parent.top
            margins: Theme.paddingLarge
        }
        spacing: Theme.paddingSmall

        Label {
            width: parent.width
            horizontalAlignment: Text.AlignLeft
            truncationMode: TruncationMode.Fade
            font.pixelSize: Theme.fontSizeLarge
            color: Theme.highlightColor
            text: cover.recording ? app.activityTypeName(cover.activity.type) : ""
        }

        Label {
            width: parent.width
            visible: cover.paused
            horizontalAlignment: Text.AlignLeft
            truncationMode: TruncationMode.Fade
            font.pixelSize: Theme.fontSizeSmall
            color: Theme.primaryColor
            text: qsTr("Paused")
        }

        Label {
            width: parent.width
            horizontalAlignment: Text.AlignLeft
            fontSizeMode: Text.HorizontalFit
            minimumPixelSize: Theme.fontSizeLarge
            font.pixelSize: Theme.fontSizeHuge
            font.family: Theme.fontFamilyHeading
            color: Theme.primaryColor
            text: cover.recording ? Units.duration(cover.activity.elapsed) : ""
        }

        Row {
            width: parent.width
            spacing: Theme.paddingSmall

            Label {
                id: distanceValue
                width: Math.min(implicitWidth, parent.width - distanceUnit.width - parent.spacing)
                fontSizeMode: Text.HorizontalFit
                minimumPixelSize: Theme.fontSizeMedium
                font.pixelSize: Theme.fontSizeLarge
                font.family: Theme.fontFamilyHeading
                color: Theme.primaryColor
                text: cover.recording ? Units.distance(cover.activity.distance) : ""
            }

            Label {
                id: distanceUnit
                anchors.baseline: distanceValue.baseline
                font.pixelSize: Theme.fontSizeExtraSmall
                color: Theme.secondaryColor
                text: "km"
            }
        }

        Row {
            width: parent.width
            spacing: Theme.paddingSmall

            Label {
                id: paceValue
                width: Math.min(implicitWidth, parent.width - paceUnit.width - parent.spacing)
                fontSizeMode: Text.HorizontalFit
                minimumPixelSize: Theme.fontSizeMedium
                font.pixelSize: Theme.fontSizeLarge
                font.family: Theme.fontFamilyHeading
                color: Theme.primaryColor
                text: cover.recording ? Units.pace(cover.activity.speed) : ""
            }

            Label {
                id: paceUnit
                anchors.baseline: paceValue.baseline
                font.pixelSize: Theme.fontSizeExtraSmall
                color: Theme.secondaryColor
                text: "min/km"
            }
        }
    }

    // One list per state, as lipstick takes an enabled list wholesale. While
    // no activity is recorded neither is enabled, and the cover has no action.
    CoverActionList {
        enabled: cover.recording && !cover.paused

        CoverAction {
            iconSource: "image://theme/icon-cover-pause"
            onTriggered: cover.activity.pause()
        }
    }

    CoverActionList {
        enabled: cover.paused

        CoverAction {
            iconSource: "image://theme/icon-cover-play"
            onTriggered: cover.activity.resume()
        }
    }
}
