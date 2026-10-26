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

// An achievement's pictogram on a round medal, in its level's hue, with the
// badge of a total or the fastest at the lower right. Greyed out while no
// level is reached. Square: give it a width.
Item {
    id: medal

    // One entry of Achievements.read().
    property var achievement
    property bool highlighted

    readonly property bool achieved: achievement.level > 0
    readonly property color color: achieved ? app.achievementLevelColor(achievement.level)
                                            : Theme.secondaryColor

    height: width
    opacity: achieved ? 1 : Theme.opacityHigh

    Rectangle {
        anchors.fill: parent
        radius: width / 2
        color: Theme.rgba(medal.color, medal.achieved ? 0.15 : 0.05)
        border.width: Math.max(2, Math.round(medal.width / 40))
        border.color: medal.highlighted ? Theme.highlightColor : medal.color
    }

    Icon {
        anchors.centerIn: parent
        width: Math.round(parent.width * 0.56)
        height: width
        sourceSize.width: width
        sourceSize.height: height
        color: medal.color
        highlighted: medal.highlighted
        source: medal.achievement.icon ? app.achievementIcon(medal.achievement.icon) : ""
    }

    // Filled with the hue, the sign cut out of it in the ambience's
    // opposite, so that it reads on the medal's edge.
    Rectangle {
        visible: medal.achievement.badge !== ""
        anchors {
            right: parent.right
            bottom: parent.bottom
        }
        width: Math.round(parent.width * 0.36)
        height: width
        radius: width / 2
        color: medal.highlighted ? Theme.highlightColor : medal.color

        Icon {
            anchors.centerIn: parent
            width: Math.round(parent.width * 0.6)
            height: width
            sourceSize.width: width
            sourceSize.height: height
            color: Theme.colorScheme === Theme.LightOnDark ? "#000000" : "#ffffff"
            source: medal.achievement.badge !== ""
                    ? app.achievementIcon("achievement-badge-" + medal.achievement.badge) : ""
        }
    }
}
