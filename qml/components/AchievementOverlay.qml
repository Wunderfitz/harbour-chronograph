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
import "../js/Occasions.js" as Occasions
import "../js/Units.js" as Units

// One achievement over the page, as Fernschreiber lays its overlays: the
// medal, what it measures, the level reached with its value and date, and
// what the next level takes. A tap anywhere closes it - or, to announce a
// level just reached, only its OK button does. Fill the page with it.
//
// A calendar category lists the days it has collected below - only those
// it collected just now, to announce a level - but never one still to
// collect: those are for finding out.
Item {
    id: overlay

    // One entry of Achievements.read(); shown by show().
    property var achievement: ({ "key": "", "icon": "", "badge": "", "level": 0, "unit": "",
                                 "thresholds": [], "minimumDistance": 0, "best": 0, "current": 0,
                                 "reached": "", "total": 0, "collected": [], "newlyCollected": [] })
    readonly property bool shown: opacity > 0
    // A level just reached: a heading over it, and closed by OK alone, which
    // says confirmed() and leaves the rest to the page.
    property bool confirmable
    property alias heading: headingLabel.text
    // Under the heading, as which of how many it comes.
    property alias subheading: subheadingLabel.text

    signal confirmed()
    readonly property int level: achievement.level
    readonly property bool highest: level >= achievement.thresholds.length
    readonly property bool calendar: achievement.unit === "days"
    // The days listed: what the announcement is for, or all collected.
    readonly property var occasions: !calendar ? []
                                     : confirmable ? (achievement.newlyCollected || [])
                                     : (achievement.collected || [])

    function show(entry) {
        achievement = entry
        opacity = 1
        flickable.contentY = 0
    }

    function hide() {
        opacity = 0
    }

    // A value as the category counts it. Thresholds are round, so they go
    // without the decimals a measured value has.
    function format(value, threshold) {
        var number
        switch (achievement.unit) {
        case "distance":
            return (threshold ? Number(value / 1000).toLocaleString(Qt.locale(), "f", 0)
                              : Units.distance(value)) + " km"
        case "pace":
            return Units.pace(value) + " min/km"
        case "speed":
            return Units.speed(value) + " km/h"
        case "climb":
            return Units.metres(value) + " m"
        case "duration":
            if (!threshold)
                return Units.duration(value)
            return value < 3600
                    //: Minutes, as an achievement's threshold
                    ? qsTr("%1 min").arg(Math.round(value / 60))
                    //: Hours, as an achievement's threshold
                    : qsTr("%1 h").arg(Number(Math.round(value / 3600)).toLocaleString(Qt.locale(), "f", 0))
        case "count":
            number = Math.round(value)
            return number === 1 ? qsTr("1 activity")
                                : qsTr("%1 activities").arg(Number(number).toLocaleString(Qt.locale(), "f", 0))
        case "weeks":
            number = Math.round(value)
            return number === 1 ? qsTr("1 week")
                                : qsTr("%1 weeks").arg(Number(number).toLocaleString(Qt.locale(), "f", 0))
        case "steps":
            number = Math.round(value)
            return number === 1 ? qsTr("1 step")
                                : qsTr("%1 steps").arg(Number(number).toLocaleString(Qt.locale(), "f", 0))
        case "days":
            number = Math.round(value)
            //: Days a calendar achievement has collected or needs
            return number === 1 ? qsTr("1 day") : qsTr("%1 days").arg(number)
        default:
            return Units.none
        }
    }

    // yyyy-MM-dd, built from its parts as OverviewPage builds its days.
    function formatDay(day) {
        var parts = day.split("-")
        return Format.formatDate(new Date(parts[0], parts[1] - 1, parts[2]), Formatter.DateLong)
    }

    visible: shown
    opacity: 0
    // An animation rather than an Animator, which would set the opacity only
    // once done: the page fades out along with it.
    Behavior on opacity { FadeAnimation {} }

    Rectangle {
        anchors.fill: parent
        color: Theme.overlayBackgroundColor
        opacity: Theme.opacityOverlay
    }

    // Scrolls should it not fit, as in landscape.
    SilicaFlickable {
        id: flickable
        anchors.fill: parent
        contentHeight: Math.max(height, content.height + 2 * Theme.paddingLarge)

        // Under the content, all of it: the flickable would take a tap
        // from anything beneath it.
        MouseArea {
            width: parent.width
            height: flickable.contentHeight
            enabled: overlay.shown
            onClicked: {
                if (!overlay.confirmable)
                    overlay.hide()
            }
        }

        Column {
            id: content
            y: Math.max(Theme.paddingLarge, (flickable.height - height) / 2)
            x: Theme.horizontalPageMargin
            width: parent.width - 2 * Theme.horizontalPageMargin
            spacing: Theme.paddingMedium

            Label {
                id: headingLabel
                visible: text !== ""
                width: parent.width
                horizontalAlignment: Text.AlignHCenter
                wrapMode: Text.WordWrap
                font.pixelSize: Theme.fontSizeExtraLarge
                font.family: Theme.fontFamilyHeading
                color: Theme.highlightColor
            }

            Label {
                id: subheadingLabel
                visible: text !== ""
                width: parent.width
                horizontalAlignment: Text.AlignHCenter
                font.pixelSize: Theme.fontSizeSmall
                color: Theme.secondaryHighlightColor
            }

            AchievementMedal {
                anchors.horizontalCenter: parent.horizontalCenter
                width: Theme.itemSizeHuge
                achievement: overlay.achievement
            }

            Label {
                width: parent.width
                horizontalAlignment: Text.AlignHCenter
                wrapMode: Text.WordWrap
                font.pixelSize: Theme.fontSizeLarge
                font.family: Theme.fontFamilyHeading
                color: Theme.highlightColor
                text: app.achievementName(overlay.achievement.key)
            }

            Label {
                width: parent.width
                horizontalAlignment: Text.AlignHCenter
                font.pixelSize: Theme.fontSizeMedium
                color: overlay.level > 0 ? app.achievementLevelColor(overlay.level) : Theme.secondaryHighlightColor
                text: overlay.level > 0 ? app.achievementLevelName(overlay.level) : qsTr("Not achieved yet")
            }

            Label {
                width: parent.width
                horizontalAlignment: Text.AlignHCenter
                wrapMode: Text.WordWrap
                font.pixelSize: Theme.fontSizeSmall
                color: Theme.highlightColor
                text: app.achievementDescription(overlay.achievement.key, overlay.achievement.minimumDistance,
                                                 overlay.achievement.total)
            }

            Item { width: 1; height: Theme.paddingMedium }

            Column {
                width: parent.width

                DetailItem {
                    visible: overlay.level > 0
                    label: overlay.calendar ? qsTr("Collected") : qsTr("Best")
                    value: overlay.calendar
                           //: Days a calendar achievement has collected, of how many there are
                           ? qsTr("%1 of %2").arg(Math.round(overlay.achievement.best)).arg(overlay.achievement.total)
                           : overlay.format(overlay.achievement.best, false)
                }

                DetailItem {
                    visible: overlay.level > 0
                    label: qsTr("Reached on")
                    value: overlay.achievement.reached ? overlay.formatDay(overlay.achievement.reached) : ""
                }

                // Below the best only should activities have been deleted
                // since, or nothing at all yet.
                DetailItem {
                    visible: overlay.achievement.current > 0
                             && (overlay.level === 0 || overlay.achievement.current < overlay.achievement.best)
                    label: qsTr("So far")
                    value: overlay.format(overlay.achievement.current, false)
                }

                DetailItem {
                    visible: !overlay.highest
                    label: qsTr("Next level")
                    value: overlay.highest ? ""
                                           : app.achievementLevelName(overlay.level + 1)
                }

                DetailItem {
                    visible: !overlay.highest
                    label: qsTr("Needs")
                    value: overlay.highest ? "" : overlay.format(overlay.achievement.thresholds[overlay.level], true)
                }
            }

            Label {
                visible: overlay.highest
                width: parent.width
                horizontalAlignment: Text.AlignHCenter
                wrapMode: Text.WordWrap
                font.pixelSize: Theme.fontSizeSmall
                color: Theme.secondaryHighlightColor
                text: qsTr("The highest level there is")
            }

            // The days collected, each with what happened on it and the
            // date of the activity that collected it.
            Column {
                visible: overlay.occasions.length > 0
                width: parent.width
                spacing: Theme.paddingSmall

                Item { width: 1; height: Theme.paddingMedium }

                Label {
                    width: parent.width
                    horizontalAlignment: Text.AlignHCenter
                    wrapMode: Text.WordWrap
                    font.pixelSize: Theme.fontSizeSmall
                    color: Theme.secondaryHighlightColor
                    text: overlay.confirmable ? qsTr("Newly collected") : qsTr("Days collected")
                }

                Repeater {
                    model: overlay.occasions

                    Column {
                        width: parent.width

                        Label {
                            width: parent.width
                            horizontalAlignment: Text.AlignHCenter
                            wrapMode: Text.WordWrap
                            font.pixelSize: Theme.fontSizeSmall
                            color: Theme.highlightColor
                            text: Occasions.title(modelData.key, modelData.year)
                        }

                        Label {
                            width: parent.width
                            horizontalAlignment: Text.AlignHCenter
                            font.pixelSize: Theme.fontSizeExtraSmall
                            color: Theme.secondaryHighlightColor
                            text: overlay.formatDay(modelData.reached)
                        }
                    }
                }
            }

            Item {
                visible: overlay.confirmable
                width: 1
                height: Theme.paddingLarge
            }

            Button {
                visible: overlay.confirmable
                anchors.horizontalCenter: parent.horizontalCenter
                text: qsTr("OK")
                onClicked: overlay.confirmed()
            }
        }
    }
}
