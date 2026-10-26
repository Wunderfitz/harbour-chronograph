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
import "../components"

// Every achievement there is, in sections by group: its medal in the hue of
// the level reached, greyed out while none is, its name and the level below
// it. A tap shows what it measures and what the next level takes. Only read
// here: Achievements works them out as activities are stored and steps
// saved. Read as the page opens, and again should they change while it is.
Page {
    id: achievementsPage

    allowedOrientations: Orientation.All

    property var entries: achievements.read()

    // The entries by group, in the order they come: [{ group, entries }].
    readonly property var groups: {
        var groups = []
        for (var i = 0; i < entries.length; ++i) {
            var entry = entries[i]
            if (groups.length === 0 || groups[groups.length - 1].group !== entry.group)
                groups.push({ "group": entry.group, "entries": [] })
            groups[groups.length - 1].entries.push(entry)
        }
        return groups
    }

    readonly property int achievedCount: {
        var count = 0
        for (var i = 0; i < entries.length; ++i) {
            if (entries[i].level > 0)
                ++count
        }
        return count
    }

    // Three in a row in portrait, as many of that width as fit in landscape.
    readonly property int columns: Math.max(3, Math.floor(width / (Screen.width / 3)))
    readonly property real cellWidth: Math.floor((width - 2 * Theme.paddingMedium) / columns)

    Connections {
        target: achievements
        onUpdated: achievementsPage.entries = achievements.read()
    }

    SilicaFlickable {
        anchors.fill: parent
        contentHeight: content.height + Theme.paddingLarge
        // Faded out and still while the overlay covers it, so that its
        // text does not read through.
        interactive: !overlay.shown
        opacity: 1 - 0.9 * overlay.opacity

        Column {
            id: content
            width: parent.width

            PageHeader {
                title: qsTr("Achievements")
                //: How many achievement categories have a level reached
                description: qsTr("%1 of %2 achieved").arg(achievementsPage.achievedCount)
                                                    .arg(achievementsPage.entries.length)
            }

            Repeater {
                model: achievementsPage.groups

                Column {
                    width: parent.width

                    SectionHeader {
                        text: app.achievementGroupName(modelData.group)
                    }

                    Grid {
                        x: Theme.paddingMedium
                        columns: achievementsPage.columns

                        Repeater {
                            model: modelData.entries

                            BackgroundItem {
                                id: tile

                                readonly property bool achieved: modelData.level > 0

                                width: achievementsPage.cellWidth
                                height: tileContent.height + 2 * Theme.paddingMedium
                                onClicked: overlay.show(modelData)

                                Column {
                                    id: tileContent
                                    y: Theme.paddingMedium
                                    x: Theme.paddingSmall
                                    width: parent.width - 2 * Theme.paddingSmall
                                    spacing: Theme.paddingSmall

                                    AchievementMedal {
                                        anchors.horizontalCenter: parent.horizontalCenter
                                        width: Math.min(Theme.itemSizeExtraLarge, Math.round(parent.width * 0.6))
                                        achievement: modelData
                                        highlighted: tile.highlighted
                                    }

                                    // Two lines for every name, so that the
                                    // levels below them line up.
                                    Label {
                                        width: parent.width
                                        height: twoLines.height
                                        horizontalAlignment: Text.AlignHCenter
                                        verticalAlignment: Text.AlignTop
                                        wrapMode: Text.WordWrap
                                        maximumLineCount: 2
                                        truncationMode: TruncationMode.Elide
                                        font.pixelSize: Theme.fontSizeExtraSmall
                                        color: tile.highlighted ? Theme.highlightColor
                                                                : tile.achieved ? Theme.primaryColor : Theme.secondaryColor
                                        text: app.achievementName(modelData.key)
                                    }

                                    Label {
                                        width: parent.width
                                        horizontalAlignment: Text.AlignHCenter
                                        font.pixelSize: Theme.fontSizeExtraSmall
                                        color: tile.achieved ? app.achievementLevelColor(modelData.level)
                                                             : Theme.secondaryColor
                                        opacity: tile.achieved ? 1 : Theme.opacityHigh
                                        text: tile.achieved ? app.achievementLevelName(modelData.level) : qsTr("Not yet")
                                    }
                                }
                            }
                        }
                    }
                }
            }
        }

        VerticalScrollDecorator {}
    }

    // Two lines of a name, for every name to take.
    Label {
        id: twoLines
        visible: false
        font.pixelSize: Theme.fontSizeExtraSmall
        text: "X\nX"
    }

    AchievementOverlay {
        id: overlay
        anchors.fill: parent
    }
}
