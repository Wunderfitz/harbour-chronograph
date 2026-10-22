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

// A map of OpenStreetMap's tiles with a position on it - its accuracy as a
// circle, its heading as an arrow while moving - and an activity's route. Drag
// to move it, pinch or double-tap to zoom. It follows the position until it is
// dragged, and again once the button in its corner is tapped.
//
// Everything is placed in Web Mercator coordinates, 0 to 1 across the world
// from its top left, which zoom turns into pixels: worldSize is the whole
// world's width at the current zoom.
//
// Once a batch of tiles has loaded it says so, tilesLoaded(): they are in
// the tile cache then, for the land cover to be read from.
Item {
    id: map

    property real centerX: 0.5
    property real centerY: 0.5
    property real zoom: 16
    property bool following: true

    property bool hasPosition: false
    property real latitude: 0
    property real longitude: 0
    // Metres; NaN draws no circle.
    property real accuracy: NaN
    // Degrees from north; NaN draws no arrow.
    property real direction: NaN

    property alias activity: trackLine.activity
    // See TrackLine.progress.
    property alias progress: trackLine.progress

    readonly property int minimumZoom: 3
    // The most detailed level OpenStreetMap's servers render.
    readonly property int maximumZoom: 19
    // Tiles are 256 pixels; on a dense screen that is too small to read, so
    // they are drawn as large as 256 would be on a screen of ratio 1.
    readonly property real tileSize: Math.round(256 * Theme.pixelRatio)
    readonly property real worldSize: tileSize * Math.pow(2, zoom)
    readonly property int level: Math.max(minimumZoom, Math.min(maximumZoom, Math.round(zoom)))
    // Whether anyone can see the map, which is when its tiles may be fetched:
    // not with the display off, nor with the app in the background, though a
    // recording still moves the map along.
    readonly property bool viewed: Qt.application.active && visible

    readonly property real positionX: mercatorX(longitude)
    readonly property real positionY: mercatorY(latitude)

    // The layer showing `level`; the other one holds the level before.
    property Item frontLayer: layerA

    signal tilesLoaded()

    function mercatorX(longitude) {
        return (longitude + 180) / 360
    }

    function mercatorY(latitude) {
        var limited = Math.max(-85.05112878, Math.min(85.05112878, latitude))
        var sine = Math.sin(limited * Math.PI / 180)
        return 0.5 - Math.log((1 + sine) / (1 - sine)) / (4 * Math.PI)
    }

    function setCenter(x, y) {
        centerX = x - Math.floor(x)
        centerY = Math.max(0, Math.min(1, y))
    }

    function centerOnPosition() {
        if (hasPosition)
            setCenter(positionX, positionY)
    }

    // Called by the layers for each tile; a second after the last of a batch,
    // the map says it has them.
    function tileLoaded() {
        loadedTimer.restart()
    }

    // Puts a place in the middle without making it the position: following,
    // the map still moves to the position once there is one.
    function centerOn(latitude, longitude) {
        setCenter(mercatorX(longitude), mercatorY(latitude))
    }

    // Zooms so that the point at (x, y) on the map stays where it is.
    function zoomTo(newZoom, x, y) {
        var dx = x - width / 2
        var dy = y - height / 2
        var pointX = centerX + dx / worldSize
        var pointY = centerY + dy / worldSize
        zoom = Math.max(minimumZoom, Math.min(maximumZoom, newZoom))
        setCenter(pointX - dx / worldSize, pointY - dy / worldSize)
    }

    // Shows the area between the two corners whole, on a whole level so its
    // tiles are sharp, and stops following.
    function fit(south, west, north, east) {
        var left = mercatorX(west)
        var right = mercatorX(east)
        var top = mercatorY(north)
        var bottom = mercatorY(south)
        following = false
        setCenter((left + right) / 2, (top + bottom) / 2)
        var spanX = Math.max(right - left, 1e-9)
        var spanY = Math.max(bottom - top, 1e-9)
        var scale = Math.min(width * 0.8 / spanX, height * 0.8 / spanY) / tileSize
        zoom = Math.max(minimumZoom, Math.min(17, Math.floor(Math.log(scale) / Math.LN2)))
    }

    // The level just left stays underneath until the new one has loaded over
    // it, rather than the map going blank at every change of level.
    onLevelChanged: {
        var back = frontLayer === layerA ? layerB : layerA
        frontLayer.frozen = true
        frontLayer.z = 0
        back.level = level
        back.frozen = false
        back.z = 1
        frontLayer = back
    }

    // Set rather than bound: a layer's level changes only as it takes its turn.
    Component.onCompleted: layerA.level = level

    onPositionXChanged: if (following) centerOnPosition()
    onPositionYChanged: if (following) centerOnPosition()
    onHasPositionChanged: if (following) centerOnPosition()
    onFollowingChanged: if (following) centerOnPosition()

    clip: true

    Timer {
        id: loadedTimer
        interval: 1000
        onTriggered: map.tilesLoaded()
    }

    // OpenStreetMap's land colour, for where no tile has come yet.
    Rectangle {
        anchors.fill: parent
        color: "#f2efe9"
    }

    TileLayer {
        id: layerA
        anchors.fill: parent
        map: map
        z: 1
    }

    TileLayer {
        id: layerB
        anchors.fill: parent
        map: map
        frozen: true
        z: 0
    }

    // Painted for the view as it was at the last sync, and moved and scaled
    // with the map until the next, which comes a few times a second at most
    // while the map moves.
    TrackLine {
        id: trackLine

        readonly property real factor: Math.pow(2, map.zoom - zoom)

        z: 2
        width: map.width
        height: map.height
        tileSize: map.tileSize
        // The app icon's orange, which nothing on the map is.
        color: "#e16c31"
        lineWidth: Math.max(3, Math.round(3 * Theme.pixelRatio))
        transformOrigin: Item.TopLeft
        scale: factor
        x: (centerX - map.centerX) * map.worldSize + map.width / 2 * (1 - factor)
        y: (centerY - map.centerY) * map.worldSize + map.height / 2 * (1 - factor)

        function sync() {
            centerX = map.centerX
            centerY = map.centerY
            zoom = map.zoom
        }

        Component.onCompleted: sync()

        Timer {
            interval: 250
            repeat: true
            running: trackLine.centerX !== map.centerX || trackLine.centerY !== map.centerY
                     || trackLine.zoom !== map.zoom
            onTriggered: trackLine.sync()
        }
    }

    // The position, as a point of no size that its parts hang off.
    Item {
        id: marker

        readonly property real metresPerPixel: 40075016.686 * Math.cos(map.latitude * Math.PI / 180)
                                               / map.worldSize

        z: 3
        visible: map.hasPosition
        x: (map.positionX - map.centerX) * map.worldSize + map.width / 2
        y: (map.positionY - map.centerY) * map.worldSize + map.height / 2

        Rectangle {
            width: isNaN(map.accuracy) ? 0 : 2 * map.accuracy / marker.metresPerPixel
            height: width
            x: -width / 2
            y: -height / 2
            radius: width / 2
            color: Qt.rgba(0.078, 0.447, 0.627, 0.15)
            border.color: Qt.rgba(0.078, 0.447, 0.627, 0.6)
            border.width: Math.max(1, Math.round(Theme.pixelRatio))
        }

        // Turned about the position, so the arrow sits just outside the
        // point, on the side it is heading for.
        Item {
            visible: !isNaN(map.direction)
            rotation: isNaN(map.direction) ? 0 : map.direction

            Image {
                width: dot.width
                height: width
                x: -width / 2
                y: -dot.height / 2 - height
                sourceSize.width: width
                sourceSize.height: height
                source: "../../images/position-arrow.svg"
            }
        }

        Rectangle {
            id: dot
            width: Theme.paddingLarge + Theme.paddingSmall
            height: width
            x: -width / 2
            y: -height / 2
            radius: width / 2
            // The deep blue of the app icon's hands.
            color: "#1472a0"
            border.color: "white"
            border.width: Math.max(2, Math.round(2 * Theme.pixelRatio))
        }
    }

    PinchArea {
        id: pinchArea

        property real startZoom

        anchors.fill: parent

        // While following, the zoom keeps the position in the middle;
        // otherwise it keeps what is under the fingers there, and moves along
        // with them.
        onPinchStarted: startZoom = map.zoom
        onPinchUpdated: {
            if (!map.following)
                map.setCenter(map.centerX - (pinch.center.x - pinch.previousCenter.x) / map.worldSize,
                              map.centerY - (pinch.center.y - pinch.previousCenter.y) / map.worldSize)
            var x = map.following ? map.width / 2 : pinch.center.x
            var y = map.following ? map.height / 2 : pinch.center.y
            map.zoomTo(startZoom + Math.log(pinch.scale) / Math.LN2, x, y)
        }

        MouseArea {
            property real lastX
            property real lastY
            property bool dragging

            anchors.fill: parent
            // The page's own swipe must not take a drag on the map from it;
            // the page is left by swiping above the map.
            preventStealing: true

            onPressed: {
                lastX = mouse.x
                lastY = mouse.y
                dragging = false
            }
            onPositionChanged: {
                var dx = mouse.x - lastX
                var dy = mouse.y - lastY
                if (!dragging) {
                    if (Math.abs(dx) < Theme.startDragDistance && Math.abs(dy) < Theme.startDragDistance)
                        return
                    dragging = true
                }
                map.following = false
                map.setCenter(map.centerX - dx / map.worldSize, map.centerY - dy / map.worldSize)
                lastX = mouse.x
                lastY = mouse.y
            }
            onDoubleClicked: map.zoomTo(map.zoom + 1, map.following ? map.width / 2 : mouse.x,
                                        map.following ? map.height / 2 : mouse.y)
        }
    }

    // Back to following the position, once the map has been moved off it.
    Rectangle {
        z: 4
        visible: !map.following && map.hasPosition
        anchors {
            top: parent.top
            right: parent.right
            margins: Theme.paddingMedium
        }
        width: Theme.itemSizeSmall
        height: width
        radius: width / 2
        color: Theme.rgba(Theme.overlayBackgroundColor, Theme.opacityOverlay)

        IconButton {
            anchors.centerIn: parent
            icon.source: "image://theme/icon-m-whereami"
            onClicked: map.following = true
        }
    }

    // OpenStreetMap's licence asks for this wherever its map is shown.
    Rectangle {
        z: 4
        anchors {
            right: parent.right
            bottom: parent.bottom
        }
        width: attribution.width + 2 * Theme.paddingSmall
        height: attribution.height + Theme.paddingSmall
        color: Qt.rgba(1, 1, 1, 0.75)

        Label {
            id: attribution
            anchors.centerIn: parent
            font.pixelSize: Theme.fontSizeTiny
            color: "#333333"
            text: qsTr("© OpenStreetMap contributors")
        }

        MouseArea {
            anchors.fill: parent
            onClicked: Qt.openUrlExternally("https://www.openstreetmap.org/copyright")
        }
    }
}
