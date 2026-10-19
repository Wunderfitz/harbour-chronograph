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
// Without it OverviewPage does not resolve, and the app shows a blank window
// without a word of complaint.
import "pages"

ApplicationWindow {
    id: app

    // The activity types as Activity and the database name them, in the order
    // the activity page offers them: running first, the default.
    readonly property var activityTypes: ["running", "walking", "cycling", "rowing", "driving"]

    // How long a deleted activity can be kept, on the list and on its page:
    // longer than Silica's 4 s, as a recording cannot be made again.
    readonly property int deleteRemorseTimeout: 10000

    function activityTypeName(type) {
        switch (type) {
        case "walking":
            return qsTr("Walking")
        case "cycling":
            return qsTr("Cycling")
        case "rowing":
            return qsTr("Rowing")
        case "driving":
            return qsTr("Driving")
        default:
            return qsTr("Running")
        }
    }

    // The type's pictogram in images/, white on transparent: an Icon tints it
    // for the ambience. An unknown type is shown as running, as by its name.
    function activityTypeIcon(type) {
        var known = activityTypes.indexOf(type) !== -1
        return Qt.resolvedUrl("../images/activity-" + (known ? type : "running") + ".svg")
    }

    initialPage: Component { OverviewPage {} }
    cover: Qt.resolvedUrl("cover/CoverPage.qml")
    allowedOrientations: defaultAllowedOrientations
}
