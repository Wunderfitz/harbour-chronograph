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

// One zoom level of TileMap's tiles, scaled to the map's zoom, which is within
// half a level of it.
//
// A fixed set of slots covers the map, each one showing the tile at its place
// modulo the number of slots. Moving the map by a tile changes the tiles of a
// single row or column of slots; the others just move along, without loading
// anything again. A frozen layer stops taking new tiles altogether and only
// moves - it is the level before, kept underneath while the new one loads.
Item {
    id: tileLayer

    property Item map
    // -1 shows nothing.
    property int level: -1
    property bool frozen: false

    readonly property real tileScale: Math.pow(2, map.zoom - level)
    readonly property real size: map.tileSize * tileScale
    readonly property int tiles: level >= 0 ? Math.pow(2, level) : 0
    // Where the top left corner of the world is, on the map.
    readonly property real originX: map.width / 2 - map.centerX * tiles * size
    readonly property real originY: map.height / 2 - map.centerY * tiles * size
    // Enough slots for the smallest the tiles get before the map changes
    // level, 1 / sqrt(2), plus one for a row cut in two at either edge.
    readonly property int columns: Math.ceil(map.width * Math.SQRT2 / map.tileSize) + 1
    readonly property int rows: Math.ceil(map.height * Math.SQRT2 / map.tileSize) + 1

    // The first tile in view, which the slots count from; kept while frozen.
    property int firstColumn: 0
    property int firstRow: 0

    function modulo(value, divisor) {
        return ((value % divisor) + divisor) % divisor
    }

    function refresh() {
        if (frozen || level < 0)
            return
        firstColumn = Math.floor(-originX / size)
        firstRow = Math.floor(-originY / size)
    }

    onOriginXChanged: refresh()
    onOriginYChanged: refresh()
    onFrozenChanged: refresh()
    onLevelChanged: refresh()

    Repeater {
        model: tileLayer.level >= 0 ? tileLayer.columns * tileLayer.rows : 0

        Image {
            readonly property int column: tileLayer.firstColumn
                                          + tileLayer.modulo(index % tileLayer.columns - tileLayer.firstColumn, tileLayer.columns)
            readonly property int row: tileLayer.firstRow
                                       + tileLayer.modulo(Math.floor(index / tileLayer.columns) - tileLayer.firstRow, tileLayer.rows)

            // Rounded edge by edge, so neighbours meet without a hairline.
            x: Math.round(tileLayer.originX + column * tileLayer.size)
            y: Math.round(tileLayer.originY + row * tileLayer.size)
            width: Math.round(tileLayer.originX + (column + 1) * tileLayer.size) - x
            height: Math.round(tileLayer.originY + (row + 1) * tileLayer.size) - y
            // The world repeats sideways but not up and down.
            readonly property bool inWorld: row >= 0 && row < tileLayer.tiles

            visible: inWorld
            asynchronous: true
            smooth: true
            source: inWorld ? "https://tile.openstreetmap.org/" + tileLayer.level + "/"
                              + tileLayer.modulo(column, tileLayer.tiles) + "/" + row + ".png"
                            : ""
        }
    }
}
