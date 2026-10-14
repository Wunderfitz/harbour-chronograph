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

// Body copy for AboutPage: page margins, wrapped, dimmed against the section
// headers. Several paragraphs of it, so it is a component rather than copies
// of the same five properties.
Label {
    x: Theme.horizontalPageMargin
    width: parent.width - 2 * Theme.horizontalPageMargin
    wrapMode: Text.WordWrap
    font.pixelSize: Theme.fontSizeSmall
    color: Theme.secondaryColor
}
