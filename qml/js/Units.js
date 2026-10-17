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

// An activity's numbers as the activity page and the list show them, from
// what C++ hands over: seconds, metres and metres per second. The units go
// next to them, separately. Not a .pragma library, so that Qt.locale() has the
// importing file's context to work in.

// What stands in for a value there is none of yet.
var none = "–"

// Below this, in metres per second, there is no pace to speak of: it would be
// over half an hour per kilometre. Track itself calls 0.5 standing.
var slowestPace = 1000 / 1800

function pad(value) {
    return value < 10 ? "0" + value : "" + value
}

// 1:02:03, or 2:03 under the hour.
function duration(seconds) {
    var total = Math.max(0, Math.floor(seconds))
    var hours = Math.floor(total / 3600)
    var minutes = Math.floor(total % 3600 / 60)
    var rest = total % 60
    return hours > 0 ? hours + ":" + pad(minutes) + ":" + pad(rest)
                     : minutes + ":" + pad(rest)
}

// Kilometres, to ten metres.
function distance(metres) {
    return Number(metres / 1000).toLocaleString(Qt.locale(), "f", 2)
}

// Minutes per kilometre as 5:23.
function pace(speed) {
    if (!(speed >= slowestPace))
        return none
    return paceOf(1000 / speed)
}

// The same from seconds per kilometre, as the pace chart's axis has them.
function paceOf(seconds) {
    var whole = Math.round(seconds)
    return Math.floor(whole / 60) + ":" + pad(whole % 60)
}

// Kilometres per hour, to a tenth.
function speed(speed) {
    return Number(speed * 3.6).toLocaleString(Qt.locale(), "f", 1)
}

// Whole metres; none for NaN.
function metres(value) {
    return isNaN(value) ? none : Number(Math.round(value)).toLocaleString(Qt.locale(), "f", 0)
}
