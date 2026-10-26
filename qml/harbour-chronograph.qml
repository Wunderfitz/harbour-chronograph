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

    // The land covers by LandCover's keys, an empty one for the land not
    // known: what the activity page calls them.
    function landCoverName(key) {
        switch (key) {
        case "forest":
            return qsTr("Forest")
        case "farmland":
            return qsTr("Farmland")
        case "grassland":
            return qsTr("Meadows and parks")
        case "builtup":
            return qsTr("Built-up area")
        case "water":
            return qsTr("Water")
        case "natural":
            return qsTr("Heath, scrub and rock")
        case "unmapped":
            return qsTr("Not mapped")
        default:
            return qsTr("Unknown")
        }
    }

    // And the colour the map paints each in; none for the land not known.
    function landCoverColor(key) {
        switch (key) {
        case "forest":
            return "#add19e"
        case "farmland":
            return "#eef0d5"
        case "grassland":
            return "#cdebb0"
        case "builtup":
            return "#e0dfdf"
        case "water":
            return "#aad3df"
        case "natural":
            return "#d6d99f"
        case "unmapped":
            return "#f2efe9"
        default:
            return "transparent"
        }
    }

    // The achievements' levels, 1 to 7, by the sequence of a wedding's
    // anniversaries and a podium's medals; 0 is none.
    function achievementLevelName(level) {
        switch (level) {
        case 1:
            return qsTr("Paper")
        case 2:
            return qsTr("Wood")
        case 3:
            return qsTr("Bronze")
        case 4:
            return qsTr("Silver")
        case 5:
            return qsTr("Gold")
        case 6:
            return qsTr("Platinum")
        case 7:
            return qsTr("Diamond")
        default:
            return ""
        }
    }

    // And the hue a level tints its pictogram and name in: light on a dark
    // ambience, darker on a light one, so that each stands out from either.
    function achievementLevelColor(level) {
        var light = Theme.colorScheme === Theme.LightOnDark
        switch (level) {
        case 1:
            return light ? "#e2cc98" : "#9a8a5c"
        case 2:
            return light ? "#a8835a" : "#7a5838"
        case 3:
            return light ? "#e0904e" : "#a65a1e"
        case 4:
            return light ? "#cdd3dc" : "#6d7682"
        case 5:
            return light ? "#f6c945" : "#a87c00"
        case 6:
            return light ? "#b9c8f0" : "#4f63a0"
        case 7:
            return light ? "#7ce4ff" : "#0083a8"
        default:
            return Theme.secondaryColor
        }
    }

    // The sections of the achievements page, by AchievementCatalogue's
    // groups; those of a single activity type are named after it.
    function achievementGroupName(group) {
        switch (group) {
        case "climbing":
            return qsTr("Climbing")
        case "terrain":
            return qsTr("Land cover")
        case "endurance":
            return qsTr("Endurance")
        case "regularity":
            return qsTr("Regularity")
        case "steps":
            return qsTr("Steps")
        default:
            return activityTypeName(group)
        }
    }

    // The achievements by their keys in AchievementCatalogue: their names,
    // and what each measures. A new category gets both here.
    function achievementName(key) {
        switch (key) {
        case "running-longest":
            return qsTr("Longest run")
        case "running-distance":
            return qsTr("Distance run")
        case "running-pace":
            return qsTr("Fastest run")
        case "walking-longest":
            return qsTr("Longest walk")
        case "walking-distance":
            return qsTr("Distance walked")
        case "walking-pace":
            return qsTr("Fastest walk")
        case "cycling-longest":
            return qsTr("Longest ride")
        case "cycling-distance":
            return qsTr("Distance cycled")
        case "cycling-speed":
            return qsTr("Fastest ride")
        case "rowing-longest":
            return qsTr("Longest row")
        case "rowing-distance":
            return qsTr("Distance rowed")
        case "rowing-speed":
            return qsTr("Fastest row")
        case "climb-single":
            return qsTr("Biggest climb")
        case "climb-total":
            return qsTr("Height climbed")
        case "terrain-forest":
            return landCoverName("forest")
        case "terrain-grassland":
            return landCoverName("grassland")
        case "terrain-farmland":
            return landCoverName("farmland")
        case "terrain-natural":
            return landCoverName("natural")
        case "terrain-water":
            return landCoverName("water")
        case "terrain-builtup":
            return landCoverName("builtup")
        case "duration-single":
            return qsTr("Longest activity")
        case "duration-total":
            return qsTr("Time on the move")
        case "activities-count":
            return qsTr("Activities")
        case "streak-weeks":
            return qsTr("Weeks in a row")
        case "steps-day":
            return qsTr("Steps in a day")
        case "steps-total":
            return qsTr("Steps in all")
        default:
            return key
        }
    }

    // `minimumDistance`, in metres, is what an activity needs to count for
    // the fastest.
    function achievementDescription(key, minimumDistance) {
        var kilometres = Number(minimumDistance / 1000).toLocaleString(Qt.locale(), "f", 0)
        switch (key) {
        case "running-longest":
            return qsTr("The farthest you have run in a single activity.")
        case "running-distance":
            return qsTr("How far you have run, all runs together.")
        case "running-pace":
            return qsTr("Your best average pace over a run of at least %1 km, pauses left out.").arg(kilometres)
        case "walking-longest":
            return qsTr("The farthest you have walked in a single activity.")
        case "walking-distance":
            return qsTr("How far you have walked, all walks together.")
        case "walking-pace":
            return qsTr("Your best average pace over a walk of at least %1 km, pauses left out.").arg(kilometres)
        case "cycling-longest":
            return qsTr("The farthest you have cycled in a single activity.")
        case "cycling-distance":
            return qsTr("How far you have cycled, all rides together.")
        case "cycling-speed":
            return qsTr("Your best average speed over a ride of at least %1 km, pauses left out.").arg(kilometres)
        case "rowing-longest":
            return qsTr("The farthest you have rowed in a single activity.")
        case "rowing-distance":
            return qsTr("How far you have rowed, all outings together.")
        case "rowing-speed":
            return qsTr("Your best average speed over an outing of at least %1 km, pauses left out.").arg(kilometres)
        case "climb-single":
            return qsTr("The most metres you have climbed in a single activity, driving aside.")
        case "climb-total":
            return qsTr("All the metres you have climbed, all activities together, driving aside.")
        case "terrain-forest":
        case "terrain-grassland":
        case "terrain-farmland":
        case "terrain-natural":
        case "terrain-water":
        case "terrain-builtup":
            return qsTr("How far you have gone over this land, all activities together, driving aside. It is read off the map's tiles, so it counts only where the map has been on screen.")
        case "duration-single":
            return qsTr("The longest you have been on the move in a single activity, pauses and driving left out.")
        case "duration-total":
            return qsTr("All the time you have been on the move, all activities together, pauses and driving left out.")
        case "activities-count":
            return qsTr("How many activities you have recorded, driving aside.")
        case "streak-weeks":
            return qsTr("The most calendar weeks in a row, Monday to Sunday, with at least one activity in each, driving aside.")
        case "steps-day":
            return qsTr("The most steps counted in a single day. Steps are counted while Chronograph runs.")
        case "steps-total":
            return qsTr("All the steps counted, all days together.")
        default:
            return ""
        }
    }

    // An achievement's pictogram or badge in images/, white on
    // transparent, for an Icon to tint.
    function achievementIcon(name) {
        return Qt.resolvedUrl("../images/" + name + ".svg")
    }

    initialPage: Component { OverviewPage {} }
    cover: Qt.resolvedUrl("cover/CoverPage.qml")
    allowedOrientations: defaultAllowedOrientations
}
