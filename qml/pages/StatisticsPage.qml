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

// What the activities came to - distance, time and land cover - for all of
// them together, then for each type. Only read here: ActivityStatistics works
// them out as activities are stored. Read as the page opens, and again should
// one be stored, deleted or learn its land cover while it is open.
Page {
    id: statisticsPage

    allowedOrientations: Orientation.All

    property var statistics: activityStatistics.read()

    // All activities together; none at all without a database.
    readonly property var overall: statistics.length > 0
                                   ? statistics[0]
                                   : { "activities": 0, "duration": 0, "distance": 0, "landCover": [] }
    readonly property bool hasActivities: overall.activities > 0

    // The types that have any, in the order the activity page offers them.
    readonly property var types: {
        var types = statistics.slice(1)
        types.sort(function(a, b) { return typeOrder(a.type) - typeOrder(b.type) })
        return types
    }

    function typeOrder(type) {
        var index = app.activityTypes.indexOf(type)
        return index === -1 ? app.activityTypes.length : index
    }

    Connections {
        target: activityStatistics
        onUpdated: statisticsPage.statistics = activityStatistics.read()
    }

    SilicaFlickable {
        anchors.fill: parent
        contentHeight: content.height + Theme.paddingLarge

        Column {
            id: content
            width: parent.width
            spacing: Theme.paddingLarge

            PageHeader {
                title: qsTr("Statistics")
            }

            ActivityStatisticsSection {
                visible: statisticsPage.hasActivities
                title: qsTr("All activities")
                statistics: statisticsPage.overall
            }

            Repeater {
                model: statisticsPage.types

                ActivityStatisticsSection {
                    title: app.activityTypeName(modelData.type)
                    statistics: modelData
                }
            }
        }

        ViewPlaceholder {
            enabled: !statisticsPage.hasActivities
            text: qsTr("No activities yet")
        }

        VerticalScrollDecorator {}
    }
}
