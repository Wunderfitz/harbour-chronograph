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

#ifndef LANDCOVER_H
#define LANDCOVER_H

#include <cstdint>
#include <string>

/**
 * The ground an activity crossed, read off OpenStreetMap's standard map
 * tiles - the ones the map has shown anyway. The tiles hold no data, only
 * pixels, but OSM Carto paints each kind of land in a colour of its own; so
 * the colours around a place, looked up in a table of Carto's, say what is
 * there. Only plain fills count: roads, labels, symbols, patterns and the
 * edges between areas match no colour and are left out.
 *
 * The kinds are coarse on purpose. Carto's finer ones lie a few shades apart
 * - grass, park and campsite, say - and a tile's palette shifts them; the
 * groups below are what the colours tell apart reliably.
 *
 * Plain C++ without Qt, like Track, so tests/tst_landcover.cpp drives it on
 * the host with generated tiles. Where the tiles come from is up to the
 * caller: see Tiles.
 */
namespace LandCover {

enum Kind {
    /* No tile at hand, or too little on it to tell. */
    Unknown = 0,
    /* Forest and wood. */
    Forest,
    /* Farmland and farmyards, orchards and vineyards. */
    Farmland,
    /* Grass and meadows, parks, gardens, pitches, cemeteries. */
    Grassland,
    /* Residential, commercial and industrial land, buildings, car parks. */
    BuiltUp,
    /* Lakes, rivers and the sea. */
    Water,
    /* Heath, scrub, bare rock and ground, sand, beaches, glaciers. */
    Natural,
    /* Land without any land use mapped: Carto's background. */
    Unmapped,
    KindCount
};

/* A tile's width and height in pixels. */
const int kTileSize = 256;

/* Zoom levels a kind is read at: the finest OSM renders, down to the
 * coarsest at which Carto still paints the land use. */
const int kFinestLevel = 19;
const int kCoarsestLevel = 10;

/* What the GPX file says: "forest", "farmland", "grassland", "builtup",
 * "water", "natural", "unmapped"; empty for Unknown. */
const char *key(Kind kind);
/* Unknown for anything else. */
Kind fromKey(const std::string &key);

/* What a pixel of 0xRRGGBB (the alpha is ignored) stands for: Unknown for
 * anything but the fills. */
Kind ofColour(std::uint32_t colour);

/* The tiles to read from, kTileSize square, a 0xAARRGGBB pixel after
 * another, row by row. tile() may give null where one is not at hand; a
 * pointer it gave stays good for at least the next three calls. */
class Tiles
{
public:
    virtual ~Tiles() {}
    virtual const std::uint32_t *tile(int level, int x, int y) = 0;
};

/* What most of the land within kRadius metres of the place is, at one zoom
 * level - read from the tiles of that level the circle touches. Unknown if
 * the tile with the place itself is not at hand, or if too little of the
 * circle has the colour of any kind. */
Kind classify(Tiles &tiles, int level, double latitude, double longitude);

/* The same at the finest level that tells, from `finest` down to `coarsest`;
 * that level goes into `level`, 0 if none did. `untelling`, if given, has a
 * bit, 1 << level, for each level whose tile was at hand and told nothing:
 * those are set here and skipped from then on, as a tile does not change. */
Kind classifyFinest(Tiles &tiles, int finest, int coarsest, double latitude, double longitude,
                    int *level, std::uint32_t *untelling = nullptr);

/* Metres around the place that are looked at: about what a receiver's
 * position is off by, so that a path along a forest's edge counts as both. */
extern const double kRadius;

} // namespace LandCover

#endif // LANDCOVER_H
