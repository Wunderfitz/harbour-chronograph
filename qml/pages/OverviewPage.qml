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

// The recorded activities, newest first, over the steps: today's count and the
// three days before in the lower third - in landscape the right third. The
// pulley starts a new activity or shows the statistics; tapping one in the
// list replays it.
Page {
    id: overviewPage

    allowedOrientations: Orientation.All

    readonly property bool unavailable: stepCounter.status === StepCounter.Unavailable

    // The activity page, compiled in the background as the app starts rather
    // than on the first tap, which it held up by a tenth of a second on a
    // phone. Still pushed by its URL, which finds it compiled - or, tapped
    // before then, compiles it as before.
    readonly property Component activityPage:
        Qt.createComponent(Qt.resolvedUrl("ActivityPage.qml"), Component.Asynchronous)

    // A weekday's name for a yyyy-MM-dd key, in the phone's language. Built
    // from its parts, as new Date("yyyy-MM-dd") would be midnight UTC.
    function dayName(day) {
        var parts = day.split("-")
        var date = new Date(parts[0], parts[1] - 1, parts[2])
        return Qt.locale().dayName(date.getDay(), Locale.ShortFormat)
    }

    // The activity history reads a page of activities at a time; the list
    // asks it for the next as it scrolls towards the end of what it has.
    SilicaListView {
        id: activityList

        anchors {
            left: parent.left
            top: parent.top
            right: overviewPage.isPortrait ? parent.right : stepsPanel.left
            bottom: overviewPage.isPortrait ? stepsPanel.top : parent.bottom
        }
        clip: true
        model: activityHistory

        PullDownMenu {
            MenuItem {
                text: qsTr("About Chronograph")
                onClicked: pageStack.push(Qt.resolvedUrl("AboutPage.qml"))
            }
            MenuItem {
                text: qsTr("Statistics")
                onClicked: pageStack.push(Qt.resolvedUrl("StatisticsPage.qml"))
            }
            MenuItem {
                text: qsTr("Start activity")
                onClicked: pageStack.push(Qt.resolvedUrl("ActivityPage.qml"))
            }
        }

        header: PageHeader {
            title: qsTr("Chronograph")
        }

        delegate: ListItem {
            id: delegate

            // Deleted only once the remorse timer has run out; tapping the
            // countdown keeps the activity. The file name is taken now, as
            // the delegate may be gone by then.
            function deleteActivity() {
                var fileName = model.fileName
                remorseAction(qsTr("Activity deleted"), function() {
                    activityHistory.remove(fileName)
                }, app.deleteRemorseTimeout)
            }

            contentHeight: Theme.itemSizeMedium
            onClicked: pageStack.push(Qt.resolvedUrl("ActivityPage.qml"),
                                      { fileName: model.fileName, type: model.type })
            ListView.onRemove: animateRemoval(delegate)

            menu: ContextMenu {
                MenuItem {
                    text: qsTr("Delete activity")
                    onClicked: delegate.deleteActivity()
                }
            }

            // The type's pictogram, as tall as the two lines beside it. Tinted
            // by Icon, so it suits a dark and a light ambience alike.
            Icon {
                id: typeIcon
                anchors {
                    left: parent.left
                    leftMargin: Theme.horizontalPageMargin
                    verticalCenter: parent.verticalCenter
                }
                width: height
                height: details.height
                sourceSize.width: width
                sourceSize.height: height
                highlighted: delegate.highlighted
                source: app.activityTypeIcon(model.type)
            }

            Column {
                id: details
                anchors {
                    left: typeIcon.right
                    right: parent.right
                    leftMargin: Theme.paddingLarge
                    rightMargin: Theme.horizontalPageMargin
                    verticalCenter: parent.verticalCenter
                }

                Item {
                    width: parent.width
                    height: typeLabel.height

                    Label {
                        id: typeLabel
                        width: parent.width - distanceLabel.width - Theme.paddingMedium
                        truncationMode: TruncationMode.Fade
                        color: delegate.highlighted ? Theme.highlightColor : Theme.primaryColor
                        text: app.activityTypeName(model.type)
                    }

                    Label {
                        id: distanceLabel
                        anchors.right: parent.right
                        color: delegate.highlighted ? Theme.highlightColor : Theme.primaryColor
                        text: Units.distance(model.distance) + " km"
                    }
                }

                Item {
                    width: parent.width
                    height: startLabel.height

                    Label {
                        id: startLabel
                        width: parent.width - durationLabel.width - Theme.paddingMedium
                        truncationMode: TruncationMode.Fade
                        font.pixelSize: Theme.fontSizeExtraSmall
                        color: delegate.highlighted ? Theme.secondaryHighlightColor : Theme.secondaryColor
                        text: Format.formatDate(model.start, Formatter.DateMedium) + ", "
                              + Format.formatDate(model.start, Formatter.TimeValue)
                    }

                    Label {
                        id: durationLabel
                        anchors.right: parent.right
                        font.pixelSize: Theme.fontSizeExtraSmall
                        color: delegate.highlighted ? Theme.secondaryHighlightColor : Theme.secondaryColor
                        text: Units.duration(model.duration)
                    }
                }
            }
        }

        ViewPlaceholder {
            enabled: activityList.count === 0
            text: qsTr("No activities yet")
            hintText: qsTr("Pull down to start one")
        }

        VerticalScrollDecorator {}
    }

    // Today's steps with the three days before them, centred in their third.
    Item {
        id: stepsPanel

        anchors {
            left: overviewPage.isPortrait ? parent.left : undefined
            right: parent.right
            top: overviewPage.isPortrait ? undefined : parent.top
            bottom: parent.bottom
        }
        width: overviewPage.isPortrait ? overviewPage.width : overviewPage.width / 3
        height: overviewPage.isPortrait ? overviewPage.height / 3 : overviewPage.height

        Separator {
            anchors.top: parent.top
            width: parent.width
            visible: overviewPage.isPortrait
            color: Theme.primaryColor
            horizontalAlignment: Qt.AlignHCenter
        }

        Column {
            id: steps

            visible: !overviewPage.unavailable
            anchors.verticalCenter: parent.verticalCenter
            width: parent.width
            spacing: Theme.paddingSmall

            Label {
                x: Theme.horizontalPageMargin
                width: parent.width - 2 * Theme.horizontalPageMargin
                horizontalAlignment: Text.AlignHCenter
                // Shrinks rather than truncates should the number ever
                // outgrow the width.
                fontSizeMode: Text.HorizontalFit
                minimumPixelSize: Theme.fontSizeLarge
                font.pixelSize: Theme.fontSizeHuge
                font.family: Theme.fontFamilyHeading
                color: Theme.highlightColor
                text: Number(stepHistory.todaySteps).toLocaleString(Qt.locale(), "f", 0)
            }

            Label {
                anchors.horizontalCenter: parent.horizontalCenter
                font.pixelSize: Theme.fontSizeMedium
                color: Theme.highlightColor
                text: qsTr("Steps")
            }

            Label {
                x: Theme.horizontalPageMargin
                width: parent.width - 2 * Theme.horizontalPageMargin
                horizontalAlignment: Text.AlignHCenter
                wrapMode: Text.WordWrap
                font.pixelSize: Theme.fontSizeExtraSmall
                color: Theme.secondaryHighlightColor
                text: stepCounter.status === StepCounter.Starting
                      ? qsTr("Waiting for the accelerometer…")
                      : qsTr("Today")
            }

            Item { width: 1; height: Theme.paddingMedium }

            // Yesterday first; a day without any steps reads 0. Side by side
            // under the count in portrait, one below the other in landscape.
            Grid {
                id: previousDays

                anchors.horizontalCenter: parent.horizontalCenter
                columns: overviewPage.isPortrait ? 3 : 1
                columnSpacing: Theme.paddingLarge
                rowSpacing: Theme.paddingSmall

                Repeater {
                    model: stepHistory.recentDays

                    Column {
                        width: overviewPage.isPortrait
                               ? (steps.width - 2 * Theme.horizontalPageMargin - 2 * previousDays.columnSpacing) / 3
                               : steps.width - 2 * Theme.horizontalPageMargin

                        Label {
                            width: parent.width
                            horizontalAlignment: Text.AlignHCenter
                            font.pixelSize: Theme.fontSizeMedium
                            color: Theme.primaryColor
                            text: Number(modelData.steps).toLocaleString(Qt.locale(), "f", 0)
                        }

                        Label {
                            width: parent.width
                            horizontalAlignment: Text.AlignHCenter
                            truncationMode: TruncationMode.Fade
                            font.pixelSize: Theme.fontSizeExtraSmall
                            color: Theme.secondaryColor
                            text: index === 0 ? qsTr("Yesterday")
                                              : overviewPage.dayName(modelData.day)
                        }
                    }
                }
            }
        }

        Column {
            visible: overviewPage.unavailable
            anchors.verticalCenter: parent.verticalCenter
            x: Theme.horizontalPageMargin
            width: parent.width - 2 * Theme.horizontalPageMargin
            spacing: Theme.paddingSmall

            Label {
                width: parent.width
                horizontalAlignment: Text.AlignHCenter
                wrapMode: Text.WordWrap
                font.pixelSize: Theme.fontSizeLarge
                color: Theme.highlightColor
                text: qsTr("No accelerometer")
            }

            Label {
                width: parent.width
                horizontalAlignment: Text.AlignHCenter
                wrapMode: Text.WordWrap
                font.pixelSize: Theme.fontSizeExtraSmall
                color: Theme.secondaryHighlightColor
                text: qsTr("Chronograph counts steps with the accelerometer, which this device does not offer.")
            }
        }
    }
}
