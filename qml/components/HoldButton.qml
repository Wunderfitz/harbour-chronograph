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

// A button that tells a tap from a hold: tapped() for a short press, held()
// once it has been kept down for holdSeconds. While it is down it counts the
// seconds left, so it is clear how long to keep at it; letting go early, or
// sliding off it, gives up.
Button {
    id: button

    property string idleText
    // "%1" is the seconds left.
    property string holdText
    property int holdSeconds: 3

    signal tapped()
    signal held()

    // Milliseconds since the press, -1 when not down.
    property int heldFor: -1
    readonly property bool counting: heldFor >= countdownDelay
    readonly property int remaining: Math.ceil((holdSeconds * 1000 - heldFor) / 1000)

    // A press shorter than this is a tap. Qt's own press-and-hold time.
    readonly property int tapLimit: 800
    // The countdown shows only after this, so a tap does not flash it.
    readonly property int countdownDelay: 250
    property double pressedAt

    text: counting ? holdText.arg(remaining) : idleText

    onPressedChanged: {
        if (pressed) {
            pressedAt = Date.now()
            heldFor = 0
            clock.start()
            return
        }
        var duration = heldFor
        clock.stop()
        heldFor = -1
        if (duration >= 0 && duration < tapLimit && containsMouse)
            tapped()
    }
    onContainsMouseChanged: {
        if (!containsMouse) {
            clock.stop()
            heldFor = -1
        }
    }
    onCanceled: {
        clock.stop()
        heldFor = -1
    }

    Timer {
        id: clock
        interval: 100
        repeat: true
        onTriggered: {
            button.heldFor = Date.now() - button.pressedAt
            if (button.heldFor >= button.holdSeconds * 1000) {
                stop()
                button.heldFor = -1
                button.held()
            }
        }
    }
}
