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

// Records an activity, or replays one. Pushed without a fileName it starts the
// receiver and waits for Start; with one it shows that stored activity. The
// values take the upper half - or, swiped to, charts of the pace and the
// elevation - the map the lower, and the control at the bottom follows the
// activity: Start, then Pause or Resume - held to stop - and once it is over
// a slider through it. Beside the button, until the stop, a switch for the
// auto-pause, which pauses and resumes the recording on its own.
//
// Leaving the page while recording stops the activity and stores it: Activity
// does that as it goes with the page. Once stored, the pulley deletes it.
Page {
    id: page

    // A stored activity's GPX file, to replay it.
    property string fileName

    readonly property bool finished: activity.status === Activity.Finished
    readonly property bool running: activity.status === Activity.Recording
                                    || activity.status === Activity.Paused
    // Over and in the history: replayed, or recorded and stopped.
    readonly property bool stored: finished && activity.fileName !== ""

    // What a chart says while it has nothing to draw.
    readonly property string chartEmptyText: finished ? qsTr("Not recorded")
                                                      : qsTr("Shown as the activity goes on")

    // The page has no header to keep clear of a notch for it, so the values
    // keep clear themselves, the way Silica's own pulley menu does: below it
    // in upright portrait; in landscape, where it sits halfway down whichever
    // side the phone was turned to, away from the left edge as well.
    readonly property real topInset: Screen.hasCutouts && orientation === Orientation.Portrait
                                     ? Screen.topCutout.height : 0
    readonly property real sideInset: Screen.hasCutouts && !isPortrait
                                      ? Math.max(0, Screen.topCutout.height + Theme.paddingSmall
                                                    - Theme.horizontalPageMargin)
                                      : 0

    allowedOrientations: Orientation.All

    // A new activity's map starts where the receiver last had a position,
    // rather than in the middle of the world until the filter has settled.
    Component.onCompleted: {
        if (!fileName && activity.hasLastPosition)
            map.centerOn(activity.lastLatitude, activity.lastLongitude)
    }

    function signalColor(quality) {
        switch (quality) {
        case Activity.Good:
            return "#43a047"
        case Activity.Fair:
            return "#fdd835"
        default:
            return "#e53935"
        }
    }

    function signalText(quality) {
        switch (quality) {
        case Activity.NoPositioning:
            return qsTr("No positioning on this device")
        case Activity.LocationOff:
            return qsTr("Location is switched off")
        case Activity.NoSignal:
            return page.finished ? qsTr("No position") : qsTr("Searching for GPS…")
        case Activity.Poor:
            return qsTr("Poor GPS signal")
        case Activity.Fair:
            return qsTr("Fair GPS signal")
        default:
            return qsTr("Good GPS signal")
        }
    }

    // Deleted only once the remorse timer has run out, then the page goes
    // back to the list. Leaving the page earlier deletes it at once - that
    // is how Silica's remorse works - and then there is no page to leave.
    function deleteActivity() {
        var fileName = activity.fileName
        Remorse.popupAction(page, qsTr("Activity deleted"), function() {
            activityHistory.remove(fileName)
            if (page.status === PageStatus.Active)
                pageStack.pop()
        }, app.deleteRemorseTimeout)
    }

    // When the activity started, as the list says it; a space before then.
    function startText() {
        var start = activity.startTime
        if (activity.status === Activity.Ready || !start || isNaN(start.getTime()))
            return " "
        return Format.formatDate(start, Formatter.DateMedium) + ", "
                + Format.formatDate(start, Formatter.TimeValue)
    }

    function showWholeTrack() {
        var bounds = activity.bounds()
        if (bounds.width >= 0 && bounds.height >= 0)
            map.fit(bounds.y, bounds.x, bounds.y + bounds.height, bounds.x + bounds.width)
    }

    Activity {
        id: activity
        history: activityHistory
        pedometer: stepCounter
        fileName: page.fileName

        // A finished activity is shown whole, for the slider to go through.
        onStatusChanged: if (status === Activity.Finished) page.showWholeTrack()
    }

    // A flickable only for the pulley, which deletes a stored activity: the
    // page does not scroll. Not interactive until there is something to
    // delete, so that it takes no drag from the controls while recording.
    SilicaFlickable {
        anchors.fill: parent
        contentHeight: height
        interactive: page.stored

        PullDownMenu {
            visible: page.stored
            MenuItem {
                text: qsTr("Delete activity")
                onClicked: page.deleteActivity()
            }
        }

        Column {
            id: values

            x: page.sideInset
            width: (page.isPortrait ? page.width : page.width / 2) - page.sideInset

            // Until the start, the type to choose; from then on, and in a
            // replay, a header naming it and when it started. Both are always
            // there and the place is as tall as the taller of them, so nothing
            // below moves when one gives way to the other.
            Item {
                id: titleArea

                // A replay has its header from the beginning, not faded in.
                readonly property bool showHeader: page.fileName !== ""
                                                   || activity.status !== Activity.Ready

                width: parent.width
                height: Math.max(header.y + header.height, typeBox.y + typeBox.height)

                ComboBox {
                    id: typeBox
                    y: page.topInset
                    width: parent.width
                    // Faded out and then disabled, but never hidden: a hidden
                    // ComboBox loses its height. Disabled only once gone, as
                    // it dims at once.
                    opacity: titleArea.showHeader ? 0 : 1
                    enabled: opacity > 0
                    label: qsTr("Activity type")
                    currentIndex: Math.max(0, app.activityTypes.indexOf(activity.type))
                    menu: ContextMenu {
                        Repeater {
                            model: app.activityTypes
                            MenuItem { text: app.activityTypeName(modelData) }
                        }
                    }
                    onCurrentIndexChanged: activity.type = app.activityTypes[currentIndex]

                    Behavior on opacity { FadeAnimation {} }
                }

                PageHeader {
                    id: header
                    // Silica's own header keeps clear of a notch once it has a
                    // topMargin (5.x); before that it is moved down here.
                    y: header.topMargin === undefined ? page.topInset : 0
                    opacity: titleArea.showHeader ? 1 : 0
                    visible: opacity > 0
                    title: app.activityTypeName(activity.type)
                    // A blank line until there is a start, so that the
                    // header is as tall before as after.
                    description: page.startText()

                    Behavior on opacity { FadeAnimation {} }
                }
            }

            Item {
                width: parent.width
                height: Theme.itemSizeExtraSmall

                Row {
                    x: Theme.horizontalPageMargin
                    width: parent.width - 2 * Theme.horizontalPageMargin
                    anchors.verticalCenter: parent.verticalCenter
                    spacing: Theme.paddingMedium

                    Rectangle {
                        anchors.verticalCenter: parent.verticalCenter
                        width: Theme.paddingLarge
                        height: width
                        radius: width / 2
                        color: page.signalColor(activity.signalQuality)
                    }

                    Label {
                        anchors.verticalCenter: parent.verticalCenter
                        font.pixelSize: Theme.fontSizeSmall
                        color: Theme.highlightColor
                        text: page.signalText(activity.signalQuality)
                    }

                    Label {
                        anchors.verticalCenter: parent.verticalCenter
                        visible: activity.signalQuality > Activity.NoSignal && !isNaN(activity.accuracy)
                        font.pixelSize: Theme.fontSizeSmall
                        color: Theme.secondaryHighlightColor
                        text: qsTr("± %1 m").arg(Units.metres(activity.accuracy))
                    }
                }
            }

            // What the half page leaves: the values, or swiped to, the charts of
            // the pace and the elevation over the activity's time - with a dot
            // for each below, the one shown highlighted.
            Item {
                width: parent.width
                height: Math.max(grid.height + Theme.paddingLarge + dots.height
                                 + dots.anchors.bottomMargin,
                                 (page.isPortrait ? page.height / 2 : page.height)
                                 - titleArea.height - Theme.itemSizeExtraSmall)

                SlideshowView {
                    id: views

                    anchors {
                        top: parent.top
                        bottom: dots.top
                    }
                    width: parent.width
                    itemWidth: width
                    itemHeight: height
                    clip: true

                    model: VisualItemModel {
                        // The values sit in the middle.
                        Item {
                            width: views.itemWidth
                            height: views.itemHeight

                            Grid {
                                id: grid

                                readonly property real cellWidth: (width - columnSpacing) / 2

                                x: Theme.horizontalPageMargin
                                width: parent.width - 2 * Theme.horizontalPageMargin
                                anchors.verticalCenter: parent.verticalCenter
                                columns: 2
                                columnSpacing: Theme.paddingLarge
                                rowSpacing: Theme.paddingMedium

                                ActivityValue {
                                    width: grid.cellWidth
                                    label: qsTr("Duration")
                                    value: Units.duration(activity.elapsed)
                                }
                                ActivityValue {
                                    width: grid.cellWidth
                                    label: qsTr("Distance")
                                    value: Units.distance(activity.distance)
                                    unit: "km"
                                }
                                ActivityValue {
                                    width: grid.cellWidth
                                    label: qsTr("Pace")
                                    value: Units.pace(activity.speed)
                                    unit: "min/km"
                                }
                                ActivityValue {
                                    width: grid.cellWidth
                                    label: qsTr("Speed")
                                    value: Units.speed(activity.speed)
                                    unit: "km/h"
                                }
                                ActivityValue {
                                    width: grid.cellWidth
                                    label: qsTr("Average pace")
                                    value: Units.pace(activity.averageSpeed)
                                    unit: "min/km"
                                }
                                ActivityValue {
                                    width: grid.cellWidth
                                    label: qsTr("Average speed")
                                    value: Units.speed(activity.averageSpeed)
                                    unit: "km/h"
                                }
                                ActivityValue {
                                    width: grid.cellWidth
                                    label: qsTr("Elevation")
                                    value: Units.metres(activity.altitude)
                                    unit: "m"
                                }
                                ActivityValue {
                                    width: grid.cellWidth
                                    label: qsTr("Elevation gain")
                                    value: Units.metres(activity.elevationGain)
                                    unit: "m"
                                }
                            }
                        }

                        // One above the other, the time along the bottom for both;
                        // their lines the same height.
                        Item {
                            id: charts

                            readonly property real chartHeight:
                                (height - Theme.paddingSmall - Theme.paddingLarge
                                 - elevationChart.labelHeight) / 2

                            width: views.itemWidth
                            height: views.itemHeight

                            ActivityChart {
                                id: paceChart
                                x: Theme.horizontalPageMargin
                                y: Theme.paddingSmall
                                width: parent.width - 2 * Theme.horizontalPageMargin
                                height: charts.chartHeight
                                activity: activity
                                quantity: ChartLine.Pace
                                progress: page.finished ? activity.currentPoint : -1
                                showTime: false
                                label: qsTr("Pace")
                                unit: "min/km"
                                emptyText: page.chartEmptyText
                            }

                            ActivityChart {
                                id: elevationChart
                                x: paceChart.x
                                y: paceChart.y + paceChart.height + Theme.paddingLarge
                                width: paceChart.width
                                height: charts.chartHeight + labelHeight
                                activity: activity
                                quantity: ChartLine.Elevation
                                progress: paceChart.progress
                                label: qsTr("Elevation")
                                unit: "m"
                                emptyText: page.chartEmptyText
                            }
                        }
                    }
                }

                Row {
                    id: dots

                    anchors {
                        horizontalCenter: parent.horizontalCenter
                        bottom: parent.bottom
                        // Clear of the map below, which would otherwise crowd them.
                        bottomMargin: Theme.paddingMedium
                    }
                    height: Theme.paddingLarge
                    spacing: Theme.paddingMedium

                    Repeater {
                        model: views.count

                        Rectangle {
                            anchors.verticalCenter: parent.verticalCenter
                            width: Math.round(1.5 * Theme.paddingSmall)
                            height: width
                            radius: width / 2
                            color: index === views.currentIndex ? Theme.highlightColor
                                                                : Theme.secondaryColor
                        }
                    }
                }
            }
        }

        TileMap {
            id: map

            anchors {
                left: page.isPortrait ? parent.left : values.right
                right: parent.right
                top: page.isPortrait ? values.bottom : parent.top
                bottom: controls.top
            }
            activity: activity
            hasPosition: activity.hasPosition
            latitude: activity.latitude
            longitude: activity.longitude
            accuracy: activity.accuracy
            direction: activity.direction
            progress: page.finished ? activity.currentPoint : -1

            // A stored activity is finished before the map has its size, and a
            // turn of the phone changes it.
            onWidthChanged: if (page.finished) page.showWholeTrack()
            onHeightChanged: if (page.finished) page.showWholeTrack()
        }

        Item {
            id: controls

            anchors {
                left: map.left
                right: parent.right
                bottom: parent.bottom
            }
            height: page.finished ? replaySlider.height + Theme.paddingSmall
                                  : startButton.height + hint.height + 2 * Theme.paddingLarge

            // Set rather than bound, as a TextSwitch that checks itself loses
            // its binding: on until switched off, at any time.
            TextSwitch {
                id: autoPauseSwitch
                visible: !page.finished
                anchors {
                    left: parent.left
                    right: startButton.left
                    rightMargin: Theme.paddingMedium
                    verticalCenter: startButton.verticalCenter
                }
                automaticCheck: false
                checked: activity.autoPause
                text: qsTr("Auto-pause")
                onClicked: activity.autoPause = !activity.autoPause
            }

            Button {
                id: startButton
                visible: activity.status === Activity.Ready
                anchors {
                    right: parent.right
                    rightMargin: Theme.horizontalPageMargin
                    top: parent.top
                    topMargin: Theme.paddingLarge
                }
                preferredWidth: Theme.buttonWidthSmall
                text: qsTr("Start")
                onClicked: activity.start()
            }

            HoldButton {
                visible: page.running
                anchors {
                    right: parent.right
                    rightMargin: Theme.horizontalPageMargin
                    top: parent.top
                    topMargin: Theme.paddingLarge
                }
                preferredWidth: Theme.buttonWidthSmall
                idleText: activity.status === Activity.Paused ? qsTr("Resume") : qsTr("Pause")
                //: The countdown on the pause button while it is held; %1 is the seconds left
                holdText: qsTr("Stop in %1")
                onTapped: activity.status === Activity.Paused ? activity.resume() : activity.pause()
                onHeld: activity.stop()
            }

            // At the very bottom, what the button does - or that the recording
            // paused on its own, or has had no position for a while.
            Label {
                id: hint

                readonly property bool alert: activity.autoPaused || activity.signalLost

                visible: !page.finished
                anchors {
                    top: startButton.bottom
                    topMargin: Theme.paddingSmall
                }
                // Centred on the whole page, also in landscape, where the
                // controls take only its right half.
                x: (page.width - width) / 2 - parent.x
                width: Math.min(implicitWidth, page.width - 2 * Theme.horizontalPageMargin)
                truncationMode: TruncationMode.Fade
                font.pixelSize: Theme.fontSizeExtraSmall
                color: alert ? Theme.highlightColor : Theme.secondaryColor
                text: activity.autoPaused ? qsTr("Paused automatically")
                                          : activity.signalLost ? qsTr("GPS signal lost")
                                          : activity.status === Activity.Ready
                                            ? qsTr("Best started once the signal is green")
                                            : qsTr("Hold for 3 seconds to stop")
            }

            Slider {
                id: replaySlider
                visible: page.finished && activity.duration > 0
                anchors.bottom: parent.bottom
                width: parent.width
                minimumValue: 0
                // At least a second, which Silica's slider needs to draw itself.
                maximumValue: Math.max(1, activity.duration)
                stepSize: 1
                valueText: Units.duration(value)
                label: qsTr("Replay")
                onValueChanged: if (page.finished) activity.replayPosition = value

                // From the end. Set rather than bound: a value bound alongside the
                // maximum may be clamped to the old one first, and stay there.
                Component.onCompleted: value = maximumValue
                onMaximumValueChanged: value = maximumValue
            }

            Label {
                visible: page.finished && activity.duration <= 0
                anchors.centerIn: parent
                width: parent.width - 2 * Theme.horizontalPageMargin
                horizontalAlignment: Text.AlignHCenter
                wrapMode: Text.WordWrap
                font.pixelSize: Theme.fontSizeSmall
                color: Theme.secondaryHighlightColor
                text: qsTr("No position was good enough to record, so nothing was stored.")
            }
        }
    }
}
