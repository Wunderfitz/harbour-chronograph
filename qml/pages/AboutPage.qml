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

Page {
    id: aboutPage

    allowedOrientations: Orientation.All

    readonly property string sourcesUrl: "https://github.com/Wunderfitz/harbour-chronograph"

    SilicaFlickable {
        anchors.fill: parent
        contentHeight: column.height + Theme.paddingLarge

        Column {
            id: column
            width: aboutPage.width
            spacing: Theme.paddingMedium

            PageHeader { title: qsTr("About Chronograph") }

            // The app icon's own master, which is installed with the cover
            // artwork; drawn from the SVG so it stays sharp at this size.
            Image {
                id: logo
                anchors.horizontalCenter: parent.horizontalCenter
                source: "../../images/harbour-chronograph.svg"
                width: Math.min(2 * Theme.itemSizeHuge,
                                Math.min(aboutPage.width, aboutPage.height) / 2)
                height: width
                // Named rather than a sourceSize group block: inside one, a
                // bare width reads ambiguously, and rasterising the SVG at
                // zero would leave the page looking simply empty.
                sourceSize.width: logo.width
                sourceSize.height: logo.width
                fillMode: Image.PreserveAspectFit
                asynchronous: true
            }

            Label {
                anchors.horizontalCenter: parent.horizontalCenter
                // Bump together with rpm/harbour-chronograph.spec.
                text: qsTr("Chronograph %1").arg("0.3")
                font.pixelSize: Theme.fontSizeExtraLarge
            }

            AboutParagraph {
                horizontalAlignment: Text.AlignHCenter
                text: qsTr("Activity tracker for Sailfish OS")
            }

            Item { width: 1; height: Theme.paddingMedium }

            AboutParagraph {
                horizontalAlignment: Text.AlignHCenter
                color: Theme.primaryColor
                text: qsTr("By Sebastian J. Wolf and Claude")
            }

            AboutLink {
                url: "mailto:sebastian@ygriega.de"
                label: qsTr("Send e-mail")
            }

            Separator {
                width: parent.width
                color: Theme.primaryColor
                horizontalAlignment: Qt.AlignHCenter
            }

            AboutParagraph {
                horizontalAlignment: Text.AlignHCenter
                color: Theme.primaryColor
                text: qsTr("Licensed under GNU GPLv3 or later")
            }

            AboutLink {
                url: aboutPage.sourcesUrl
                label: qsTr("Sources on GitHub")
            }

            SectionHeader { text: qsTr("Good to know") }

            AboutParagraph {
                text: qsTr("Running, walking, cycling, rowing and driving are recorded with GPS on the activity page, which the pulley menu opens. Tap Pause to pause, and hold it for three seconds to stop. Leaving the page stops the activity as well; either way it is stored, and the list on the main page replays it.")
            }

            AboutParagraph {
                text: qsTr("With Auto-pause on, the recording pauses on its own when you stand still for a few seconds, or move faster than the activity allows - on a bus, say - and resumes when you move on. Walking and running, your steps tell it too. The time you stood is not counted. Paused like this for half an hour, the activity is stopped and stored. A pause by hand is only resumed by hand.")
            }

            AboutParagraph {
                text: qsTr("Each activity is kept on the phone as a GPX file, which other apps and sport sites read, in .local/share/de.ygriega/chronograph/gpx of your home directory.")
            }

            AboutParagraph {
                text: qsTr("The map's tiles are downloaded from OpenStreetMap while the map is on screen, which uses mobile data; tiles already seen are kept on the phone for a while. GPS costs battery for as long as the activity page is open.")
            }

            AboutParagraph {
                text: qsTr("Swiped to after the charts, the activity page shows how far you went through forest, farmland, water and other land. It is read off the map's tiles, and only off those shown on screen, during the activity or after it: nothing is downloaded for it. Land whose tiles were never shown counts as unknown until they are.")
            }

            AboutParagraph {
                text: qsTr("Steps are counted with the phone's accelerometer from the moment Chronograph starts, also while the display is off. Closing the app stops counting.")
            }

            AboutParagraph {
                text: qsTr("The steps of each day are kept on the phone and nowhere else. The main page shows today's and those of the three days before; at midnight the count starts over.")
            }

            AboutParagraph {
                text: qsTr("To keep counting with the display off, Chronograph keeps the phone from going to sleep while it runs. That costs battery, so close it when you do not need it.")
            }

            AboutParagraph {
                text: qsTr("A walk is only recognised after a few regular steps; they are then counted all at once. A handful of steps around the room, or shaking the phone, does not count.")
            }

            SectionHeader { text: qsTr("Credits") }

            AboutParagraph {
                text: qsTr("The app icon follows Jolla's apps icon story for Sailfish OS.")
            }

            AboutParagraph {
                text: qsTr("Map data © OpenStreetMap contributors, available under the Open Database License. Map tiles by the OpenStreetMap Foundation.")
            }

            AboutLink {
                url: "https://www.openstreetmap.org/copyright"
                label: qsTr("Open OpenStreetMap's copyright page")
            }

            AboutParagraph {
                text: qsTr("Chronograph was written by its author together with Claude, Anthropic's assistant, in Claude Code.")
            }

            AboutLink {
                url: "https://claude.com/claude-code"
                label: qsTr("Open the Claude Code website")
            }
        }

        VerticalScrollDecorator {}
    }
}
