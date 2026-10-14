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

Page {
    id: overviewPage

    allowedOrientations: Orientation.All

    readonly property bool unavailable: stepCounter.status === StepCounter.Unavailable

    // A weekday's name for a yyyy-MM-dd key, in the phone's language. Built
    // from its parts, as new Date("yyyy-MM-dd") would be midnight UTC.
    function dayName(day) {
        var parts = day.split("-")
        var date = new Date(parts[0], parts[1] - 1, parts[2])
        return Qt.locale().dayName(date.getDay(), Locale.LongFormat)
    }

    SilicaFlickable {
        anchors.fill: parent
        contentHeight: Math.max(height, content.y + content.height + Theme.paddingLarge)

        PullDownMenu {
            MenuItem {
                text: qsTr("About Chronograph")
                onClicked: pageStack.push(Qt.resolvedUrl("AboutPage.qml"))
            }
        }

        PageHeader {
            id: header
            title: qsTr("Chronograph")
        }

        // Today's count with the previous days right below it, centred
        // together in what the header leaves of the page; in landscape the
        // page scrolls instead.
        Column {
            id: content

            visible: !overviewPage.unavailable
            y: Math.max(header.height, (overviewPage.height + header.height - height) / 2)
            width: parent.width

            Column {
                id: counter

                width: parent.width
                spacing: Theme.paddingSmall

                Label {
                    x: Theme.horizontalPageMargin
                    width: parent.width - 2 * Theme.horizontalPageMargin
                    horizontalAlignment: Text.AlignHCenter
                    // Shrinks rather than truncates should the number ever
                    // outgrow the width.
                    fontSizeMode: Text.HorizontalFit
                    minimumPixelSize: Theme.fontSizeExtraLarge
                    font.pixelSize: 2 * Theme.fontSizeHuge
                    font.family: Theme.fontFamilyHeading
                    color: Theme.highlightColor
                    text: Number(stepHistory.todaySteps).toLocaleString(Qt.locale(), "f", 0)
                }

                Label {
                    anchors.horizontalCenter: parent.horizontalCenter
                    font.pixelSize: Theme.fontSizeLarge
                    color: Theme.highlightColor
                    text: qsTr("Steps")
                }

                Label {
                    x: Theme.horizontalPageMargin
                    width: parent.width - 2 * Theme.horizontalPageMargin
                    horizontalAlignment: Text.AlignHCenter
                    wrapMode: Text.WordWrap
                    font.pixelSize: Theme.fontSizeSmall
                    color: Theme.secondaryHighlightColor
                    text: stepCounter.status === StepCounter.Starting
                          ? qsTr("Waiting for the accelerometer…")
                          : qsTr("Today")
                }
            }

            SectionHeader { text: qsTr("Previous days") }

            // Yesterday first; a day without any steps reads 0.
            Repeater {
                model: stepHistory.recentDays

                DetailItem {
                    label: index === 0 ? qsTr("Yesterday")
                                       : overviewPage.dayName(modelData.day)
                    value: Number(modelData.steps).toLocaleString(Qt.locale(), "f", 0)
                }
            }
        }

        ViewPlaceholder {
            enabled: overviewPage.unavailable
            text: qsTr("No accelerometer")
            hintText: qsTr("Chronograph counts steps with the accelerometer, which this device does not offer.")
        }

        VerticalScrollDecorator {}
    }
}
