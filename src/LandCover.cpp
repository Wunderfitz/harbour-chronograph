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

#include "LandCover.h"

#include <algorithm>
#include <cmath>
#include <cstdlib>

namespace LandCover {

const double kRadius = 20.0;

namespace {

/* OSM Carto's fills, from its style/landcover.mss and style.mss, and the
 * kind each belongs to. A tile's palette may shift a colour by a shade or
 * two, hence kTolerance. Should Carto change a colour, its kind goes silent
 * - counted as unknown, never as another one. */
struct Fill {
    std::uint32_t colour;
    Kind kind;
};

const Fill kFills[] = {
    { 0xadd19e, Forest },       // forest, wood
    { 0xeef0d5, Farmland },     // farmland
    { 0xf5dcba, Farmland },     // farmyard
    { 0xaedfa3, Farmland },     // orchard, vineyard
    { 0xcdebb0, Grassland },    // grass, meadow, village green
    { 0xc8facc, Grassland },    // park
    { 0xdffce2, Grassland },    // other leisure: playgrounds, gardens
    { 0xdef6c0, Grassland },    // campsite, golf course
    { 0x88e0be, Grassland },    // pitch, running track
    { 0xaacbaf, Grassland },    // cemetery
    { 0xc9e1bf, Grassland },    // allotments
    { 0xe0dfdf, BuiltUp },      // residential
    { 0xdddddd, BuiltUp },      // residential and the like at level 12
    { 0xd0d0d0, BuiltUp },      // the same below it; place of worship
    { 0xffd6d1, BuiltUp },      // retail
    { 0xf2dad9, BuiltUp },      // commercial
    { 0xebdbe8, BuiltUp },      // industrial, railway, waterworks
    { 0xe2cbde, BuiltUp },      // power
    { 0xdfddce, BuiltUp },      // garages
    { 0xeeeeee, BuiltUp },      // parking
    { 0xe9e7e2, BuiltUp },      // transportation area
    { 0xdadae0, BuiltUp },      // apron
    { 0xd9d0c9, BuiltUp },      // building
    { 0xc7c7b4, BuiltUp },      // construction
    { 0xc5c3c3, BuiltUp },      // quarry
    { 0xdddde8, BuiltUp },      // pedestrian area
    { 0xffffe5, BuiltUp },      // school, hospital, university
    { 0xefc8c8, BuiltUp },      // rest area
    { 0xaad3df, Water },        // water, the sea
    { 0xd6d99f, Natural },      // heath
    { 0xc8d7ab, Natural },      // scrub
    { 0xeee5dc, Natural },      // bare rock and ground, scree
    { 0xf5e9c6, Natural },      // sand
    { 0xfff1ba, Natural },      // beach
    { 0xddecec, Natural },      // glacier
    { 0xf2efe9, Unmapped },     // the land itself
};

/* How far, per channel, a pixel may be off a fill and still count as it. The
 * closest two fills of different kinds lie 5 apart - parking and the land -
 * so no pixel matches two. */
const int kTolerance = 2;

/* The circle is never smaller than this, in pixels: at coarse levels the
 * radius is less than one. */
const double kMinRadiusPixels = 2.0;

/* Across the circle, at most this many samples on either side of the
 * middle; at fine levels the pixels are taken a few apart. */
const double kMaxSamples = 12.0;

/* At least this share of the samples must have the colour of a kind, or
 * there is too little to tell - roads and labels everywhere. */
const double kMinShare = 0.2;

const double kPi = 3.14159265358979323846;

/* Metres along a parallel per pixel at level 0, at the equator. */
const double kEquatorMetresPerPixel = 40075016.686 / kTileSize;

int channel(std::uint32_t colour, int shift)
{
    return int((colour >> shift) & 0xff);
}

/* Up to four tiles at once: a circle never touches more of one level. */
class Neighbourhood
{
public:
    Neighbourhood(Tiles &tiles, int level) : m_tiles(tiles), m_level(level) {}

    const std::uint32_t *tile(int x, int y)
    {
        for (int i = 0; i < m_count; ++i) {
            if (m_x[i] == x && m_y[i] == y)
                return m_pixels[i];
        }
        const std::uint32_t *pixels = m_tiles.tile(m_level, x, y);
        if (m_count < 4) {
            m_x[m_count] = x;
            m_y[m_count] = y;
            m_pixels[m_count] = pixels;
            ++m_count;
        }
        return pixels;
    }

private:
    Tiles &m_tiles;
    int m_level;
    int m_count = 0;
    int m_x[4] = {};
    int m_y[4] = {};
    const std::uint32_t *m_pixels[4] = {};
};

} // namespace

const char *key(Kind kind)
{
    switch (kind) {
    case Forest:
        return "forest";
    case Farmland:
        return "farmland";
    case Grassland:
        return "grassland";
    case BuiltUp:
        return "builtup";
    case Water:
        return "water";
    case Natural:
        return "natural";
    case Unmapped:
        return "unmapped";
    case Unknown:
    case KindCount:
        break;
    }
    return "";
}

Kind fromKey(const std::string &text)
{
    for (int kind = Unknown + 1; kind < KindCount; ++kind) {
        if (text == key(Kind(kind)))
            return Kind(kind);
    }
    return Unknown;
}

Kind ofColour(std::uint32_t colour)
{
    for (const Fill &fill : kFills) {
        if (std::abs(channel(colour, 16) - channel(fill.colour, 16)) <= kTolerance
                && std::abs(channel(colour, 8) - channel(fill.colour, 8)) <= kTolerance
                && std::abs(channel(colour, 0) - channel(fill.colour, 0)) <= kTolerance)
            return fill.kind;
    }
    return Unknown;
}

namespace {

/* In Web Mercator, as the tiles are: the place in pixels of the whole world
 * at that level, and a circle around it as many pixels wide as kRadius is at
 * its latitude. `read` says whether the place's own tile was at hand. */
Kind classifyAt(Tiles &tiles, int level, double latitude, double longitude, bool *read)
{
    *read = false;
    if (level < 0 || level > kFinestLevel || !(std::fabs(latitude) < 85.05112878))
        return Unknown;
    const long long tiles1D = 1LL << level;
    const double worldPixels = double(tiles1D) * kTileSize;
    const double sine = std::sin(latitude * kPi / 180.0);
    const double x = (longitude + 180.0) / 360.0 * worldPixels;
    const double y = (0.5 - std::log((1 + sine) / (1 - sine)) / (4 * kPi)) * worldPixels;

    const double metresPerPixel = kEquatorMetresPerPixel * std::cos(latitude * kPi / 180.0)
            / double(tiles1D);
    const double radius = std::max(kMinRadiusPixels, kRadius / metresPerPixel);
    const double step = std::max(1.0, std::floor(radius / kMaxSamples));
    const int reach = int(std::floor(radius / step));

    Neighbourhood neighbourhood(tiles, level);
    const long long centreX = (long long)std::floor(x);
    const long long centreY = (long long)std::floor(y);
    if (!neighbourhood.tile(int(centreX / kTileSize % tiles1D), int(centreY / kTileSize)))
        return Unknown;
    *read = true;

    int counts[KindCount] = {};
    int samples = 0;
    const long long width = tiles1D * kTileSize;
    for (int j = -reach; j <= reach; ++j) {
        for (int i = -reach; i <= reach; ++i) {
            if ((i * i + j * j) * step * step > radius * radius)
                continue;
            const long long pixelY = (long long)std::floor(y + j * step);
            if (pixelY < 0 || pixelY >= width)
                continue;
            // The world repeats sideways.
            const long long pixelX = (((long long)std::floor(x + i * step)) % width + width) % width;
            const std::uint32_t *pixels = neighbourhood.tile(int(pixelX / kTileSize),
                                                             int(pixelY / kTileSize));
            if (!pixels)
                continue;
            ++samples;
            ++counts[ofColour(pixels[(pixelY % kTileSize) * kTileSize + pixelX % kTileSize])];
        }
    }

    int known = 0;
    int best = Unknown;
    for (int kind = Unknown + 1; kind < KindCount; ++kind) {
        known += counts[kind];
        if (counts[kind] > 0 && (best == Unknown || counts[kind] > counts[best]))
            best = kind;
    }
    if (known == 0 || known < kMinShare * samples)
        return Unknown;
    return Kind(best);
}

} // namespace

Kind classify(Tiles &tiles, int level, double latitude, double longitude)
{
    bool read = false;
    return classifyAt(tiles, level, latitude, longitude, &read);
}

Kind classifyFinest(Tiles &tiles, int finest, int coarsest, double latitude, double longitude,
                    int *level, std::uint32_t *untelling)
{
    for (int candidate = std::min(finest, kFinestLevel); candidate >= std::max(coarsest, 0); --candidate) {
        const std::uint32_t bit = std::uint32_t(1) << candidate;
        if (untelling && (*untelling & bit))
            continue;
        bool read = false;
        const Kind kind = classifyAt(tiles, candidate, latitude, longitude, &read);
        if (kind != Unknown) {
            *level = candidate;
            return kind;
        }
        if (untelling && read)
            *untelling |= bit;
    }
    *level = 0;
    return Unknown;
}

} // namespace LandCover
