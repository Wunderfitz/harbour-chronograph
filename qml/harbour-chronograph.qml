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

    initialPage: Component { OverviewPage {} }
    cover: Qt.resolvedUrl("cover/CoverPage.qml")
    allowedOrientations: defaultAllowedOrientations
}
