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

// One block of the statistics page: a header, the distance and the time side
// by side, each centred in its half, and the land cover below them.
Column {
    id: section

    // One entry of ActivityStatistics.read().
    property var statistics
    property alias title: header.text

    width: parent.width
    spacing: Theme.paddingMedium

    SectionHeader {
        id: header
    }

    Row {
        x: Theme.horizontalPageMargin
        width: parent.width - 2 * Theme.horizontalPageMargin

        ActivityValue {
            width: parent.width / 2
            value: Units.distance(section.statistics.distance)
            horizontalAlignment: Text.AlignHCenter
            unit: "km"
            label: qsTr("Distance")
        }

        ActivityValue {
            width: parent.width / 2
            value: Units.duration(section.statistics.duration)
            horizontalAlignment: Text.AlignHCenter
            label: qsTr("Time")
        }
    }

    // None where the activities went nowhere.
    LandCoverList {
        x: Theme.horizontalPageMargin
        width: parent.width - 2 * Theme.horizontalPageMargin
        visible: entries.length > 0
        entries: section.statistics.landCover
        label: qsTr("Land cover")
    }
}
