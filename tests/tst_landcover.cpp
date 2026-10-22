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

/*
 * Host-side checks for LandCover, which has no Qt in it for exactly this
 * reason: the colours and the kinds, and the vote around a place - over a
 * road, along an edge between two kinds, across a tile's border and the
 * antimeridian, with tiles missing, and at the finest level that tells. Every
 * tile here is generated: OSM Carto's fills, with a tree pattern, roads and
 * labels painted over them. Build and run from the project root:
 *
 *   g++ -std=c++11 -Wall -Wextra -O2 -Isrc tests/tst_landcover.cpp \
 *       src/LandCover.cpp -o /tmp/tst_landcover && /tmp/tst_landcover
 */

#include "LandCover.h"

#include <cmath>
#include <cstdio>
#include <map>
#include <random>
#include <set>
#include <tuple>
#include <vector>

using LandCover::Kind;

namespace {

const double kPi = 3.14159265358979323846;
const int kSize = LandCover::kTileSize;

const std::uint32_t kForest = 0xadd19e;
const std::uint32_t kFarmland = 0xeef0d5;
const std::uint32_t kWater = 0xaad3df;
const std::uint32_t kLand = 0xf2efe9;
const std::uint32_t kResidential = 0xe0dfdf;
const std::uint32_t kRoad = 0xffffff;
const std::uint32_t kCasing = 0xbbbbbb;

int g_failures = 0;

void expect(const char *name, bool ok)
{
    std::printf("%s %s\n", ok ? "PASS" : "FAIL", name);
    if (!ok)
        ++g_failures;
}

void expectKind(const char *name, Kind actual, Kind expected)
{
    const bool ok = actual == expected;
    std::printf("%s %-62s %-10s (expected %s)\n", ok ? "PASS" : "FAIL", name,
                actual == LandCover::Unknown ? "unknown" : LandCover::key(actual),
                expected == LandCover::Unknown ? "unknown" : LandCover::key(expected));
    if (!ok)
        ++g_failures;
}

typedef std::vector<std::uint32_t> Pixels;

/* Tiles by level and place, and which were asked for. */
class FakeTiles : public LandCover::Tiles
{
public:
    Pixels &add(int level, int x, int y, std::uint32_t fill)
    {
        Pixels &pixels = m_tiles[std::make_tuple(level, x, y)];
        pixels.assign(std::size_t(kSize * kSize), 0xff000000u | fill);
        return pixels;
    }

    const std::uint32_t *tile(int level, int x, int y) override
    {
        m_asked.insert(std::make_tuple(level, x, y));
        const auto found = m_tiles.find(std::make_tuple(level, x, y));
        return found == m_tiles.end() ? nullptr : found->second.data();
    }

    std::size_t asked() const { return m_asked.size(); }
    bool askedFor(int level, int x, int y) const { return m_asked.count(std::make_tuple(level, x, y)) > 0; }
    void forget() { m_asked.clear(); }

private:
    std::map<std::tuple<int, int, int>, Pixels> m_tiles;
    std::set<std::tuple<int, int, int>> m_asked;
};

/* A place in the world's pixels at a level, and back. */
double worldPixels(int level)
{
    return double(1LL << level) * kSize;
}

void place(int level, double x, double y, double *latitude, double *longitude)
{
    const double world = worldPixels(level);
    *longitude = x / world * 360.0 - 180.0;
    *latitude = std::atan(std::sinh(kPi * (1.0 - 2.0 * y / world))) * 180.0 / kPi;
}

void pixelOf(int level, double latitude, double longitude, double *x, double *y)
{
    const double world = worldPixels(level);
    const double sine = std::sin(latitude * kPi / 180.0);
    *x = (longitude + 180.0) / 360.0 * world;
    *y = (0.5 - std::log((1 + sine) / (1 - sine)) / (4 * kPi)) * world;
}

void paint(Pixels &pixels, int left, int top, int right, int bottom, std::uint32_t colour)
{
    for (int y = std::max(0, top); y < std::min(kSize, bottom); ++y) {
        for (int x = std::max(0, left); x < std::min(kSize, right); ++x)
            pixels[std::size_t(y * kSize + x)] = 0xff000000u | colour;
    }
}

/* Carto's tree symbols: darker greens over a third of the forest, none of
 * them a fill. */
void treePattern(Pixels &pixels, unsigned seed)
{
    std::mt19937 random(seed);
    std::uniform_real_distribution<double> share(0, 1);
    const std::uint32_t greens[] = { 0x93c586, 0x7dba73, 0x9dc18f, 0xa1c492, 0xb6d6a9 };
    std::uniform_int_distribution<int> pick(0, 4);
    for (std::uint32_t &pixel : pixels) {
        if ((pixel & 0xffffff) == kForest && share(random) < 0.35)
            pixel = 0xff000000u | greens[pick(random)];
    }
}

/* A road across the tile, top to bottom: white with a grey casing. */
void road(Pixels &pixels, int middle, int width)
{
    paint(pixels, middle - width / 2 - 1, 0, middle + width / 2 + 1, kSize, kCasing);
    paint(pixels, middle - width / 2, 0, middle + width / 2, kSize, kRoad);
}

/* Level 16 at about 50 degrees north; the place given in pixels within the tile. */
const int kLevel = 16;
const int kTileX = 34308;
const int kTileY = 22144;

void at(double inX, double inY, double *latitude, double *longitude, int level = kLevel,
        int tileX = kTileX, int tileY = kTileY)
{
    place(level, double(tileX) * kSize + inX, double(tileY) * kSize + inY, latitude, longitude);
}

void testColours()
{
    const std::uint32_t fills[] = { kForest, kFarmland, 0xcdebb0, kResidential, kWater, 0xd6d99f,
                                    kLand };
    const Kind kinds[] = { LandCover::Forest, LandCover::Farmland, LandCover::Grassland,
                           LandCover::BuiltUp, LandCover::Water, LandCover::Natural,
                           LandCover::Unmapped };
    bool all = true;
    for (int i = 0; i < 7; ++i)
        all = all && LandCover::ofColour(0xff000000u | fills[i]) == kinds[i];
    expect("colours: a fill of each kind", all);
    expectKind("colours: two shades off still counts", LandCover::ofColour(kForest + 0x020202 - 0x000400),
               LandCover::Forest);
    expectKind("colours: three shades off does not", LandCover::ofColour(kForest + 0x030000),
               LandCover::Unknown);
    expectKind("colours: a road", LandCover::ofColour(kRoad), LandCover::Unknown);
    expectKind("colours: a motorway", LandCover::ofColour(0xe892a2), LandCover::Unknown);
    expectKind("colours: a tree symbol", LandCover::ofColour(0x93c586), LandCover::Unknown);
    expectKind("colours: a label", LandCover::ofColour(0x46673b), LandCover::Unknown);
    expectKind("colours: a building is built-up", LandCover::ofColour(0xd9d0c9), LandCover::BuiltUp);

    bool keys = true;
    for (int kind = LandCover::Unknown + 1; kind < LandCover::KindCount; ++kind)
        keys = keys && LandCover::fromKey(LandCover::key(Kind(kind))) == kind;
    expect("keys: each kind read back from its key", keys);
    expectKind("keys: an unknown key", LandCover::fromKey("swamp"), LandCover::Unknown);
    expectKind("keys: none", LandCover::fromKey(""), LandCover::Unknown);
}

void testForestRoad()
{
    FakeTiles tiles;
    Pixels &pixels = tiles.add(kLevel, kTileX, kTileY, kForest);
    treePattern(pixels, 1);
    road(pixels, 128, 8);
    paint(pixels, 60, 120, 120, 130, 0x46673b);
    double latitude, longitude;
    at(128.5, 100.5, &latitude, &longitude);
    expectKind("a road through a forest, with trees and a label", LandCover::classify(tiles, kLevel,
                                                                                    latitude, longitude),
               LandCover::Forest);
}

/* 20 m is 13 pixels at level 16 there: a few pixels to either side of an
 * edge, the side the place is on has more of the circle. */
void testEdge()
{
    FakeTiles tiles;
    Pixels &pixels = tiles.add(kLevel, kTileX, kTileY, kFarmland);
    paint(pixels, 128, 0, kSize, kSize, kForest);
    treePattern(pixels, 2);
    double latitude, longitude;
    at(124.5, 100.5, &latitude, &longitude);
    expectKind("an edge: four pixels into the field", LandCover::classify(tiles, kLevel, latitude, longitude),
               LandCover::Farmland);
    at(132.5, 100.5, &latitude, &longitude);
    expectKind("an edge: four pixels into the forest", LandCover::classify(tiles, kLevel, latitude, longitude),
               LandCover::Forest);
}

void testAcrossTiles()
{
    FakeTiles tiles;
    tiles.add(kLevel, kTileX, kTileY, kRoad);
    double latitude, longitude;
    at(253.5, 100.5, &latitude, &longitude);
    expectKind("across a border: the neighbour not at hand", LandCover::classify(tiles, kLevel, latitude, longitude),
               LandCover::Unknown);
    tiles.add(kLevel, kTileX + 1, kTileY, kWater);
    expectKind("across a border: only the neighbour has a fill", LandCover::classify(tiles, kLevel, latitude, longitude),
               LandCover::Water);

    FakeTiles below;
    below.add(kLevel, kTileX, kTileY, kRoad);
    below.add(kLevel, kTileX, kTileY + 1, kResidential);
    at(100.5, 254.5, &latitude, &longitude);
    expectKind("across a border: the tile below", LandCover::classify(below, kLevel, latitude, longitude),
               LandCover::BuiltUp);

    FakeTiles corner;
    corner.add(kLevel, kTileX, kTileY, kLand);
    corner.add(kLevel, kTileX + 1, kTileY, kLand);
    corner.add(kLevel, kTileX, kTileY + 1, kLand);
    corner.add(kLevel, kTileX + 1, kTileY + 1, kLand);
    at(255.5, 255.5, &latitude, &longitude);
    corner.forget();
    expectKind("across a corner: four tiles", LandCover::classify(corner, kLevel, latitude, longitude),
               LandCover::Unmapped);
    expect("across a corner: no more than four asked for", corner.asked() == 4);
}

void testMissing()
{
    FakeTiles tiles;
    tiles.add(kLevel, kTileX + 1, kTileY, kWater);
    double latitude, longitude;
    at(253.5, 100.5, &latitude, &longitude);
    expectKind("missing: the place's own tile, its neighbour at hand",
               LandCover::classify(tiles, kLevel, latitude, longitude), LandCover::Unknown);
}

void testTooLittle()
{
    FakeTiles tiles;
    Pixels &pixels = tiles.add(kLevel, kTileX, kTileY, kRoad);
    paint(pixels, 128, 90, 131, 111, kForest);
    double latitude, longitude;
    at(128.5, 100.5, &latitude, &longitude);
    expectKind("too little: a sliver of forest among roads",
               LandCover::classify(tiles, kLevel, latitude, longitude), LandCover::Unknown);
    paint(pixels, 128, 85, 136, 116, kForest);
    expectKind("enough: a strip of forest beside the road",
               LandCover::classify(tiles, kLevel, latitude, longitude), LandCover::Forest);
}

/* At level 19 the circle is about 100 pixels wide and sampled sparsely; at
 * level 10 it is below a pixel, and two are taken. */
void testLevels()
{
    double latitude, longitude;
    at(128.5, 128.5, &latitude, &longitude);
    for (int level : { 19, 10 }) {
        double x, y;
        pixelOf(level, latitude, longitude, &x, &y);
        const int tileX = int(x) / kSize;
        const int tileY = int(y) / kSize;
        FakeTiles tiles;
        Pixels &pixels = tiles.add(level, tileX, tileY, kWater);
        // A pier through the middle, a fifth of the circle at either level.
        const int inX = int(x) % kSize;
        paint(pixels, inX - (level == 19 ? 12 : 0), 0, inX + (level == 19 ? 12 : 1), kSize, kRoad);
        for (int dx = -1; dx <= 1; ++dx) {
            for (int dy = -1; dy <= 1; ++dy) {
                if (dx || dy)
                    tiles.add(level, tileX + dx, tileY + dy, kWater);
            }
        }
        expectKind(level == 19 ? "level 19: water around a pier" : "level 10: water around a pier",
                   LandCover::classify(tiles, level, latitude, longitude), LandCover::Water);
    }
}

void testFinest()
{
    double latitude, longitude;
    at(128.5, 128.5, &latitude, &longitude);
    FakeTiles tiles;
    for (int level : { 13, 11 }) {
        double x, y;
        pixelOf(level, latitude, longitude, &x, &y);
        tiles.add(level, int(x) / kSize, int(y) / kSize, level == 13 ? kForest : kFarmland);
    }
    int level = -1;
    expectKind("finest: the finest level at hand",
               LandCover::classifyFinest(tiles, LandCover::kFinestLevel, LandCover::kCoarsestLevel,
                                         latitude, longitude, &level),
               LandCover::Forest);
    expect("finest: its level", level == 13);

    tiles.add(kLevel, kTileX, kTileY, kRoad);
    expectKind("finest: past a level of roads only",
               LandCover::classifyFinest(tiles, LandCover::kFinestLevel, LandCover::kCoarsestLevel,
                                         latitude, longitude, &level),
               LandCover::Forest);
    expectKind("finest: none finer than asked",
               LandCover::classifyFinest(tiles, 12, LandCover::kCoarsestLevel, latitude, longitude,
                                         &level),
               LandCover::Farmland);
    expect("finest: that level", level == 11);
    expectKind("finest: none at all",
               LandCover::classifyFinest(tiles, 10, 10, latitude, longitude, &level), LandCover::Unknown);
    expect("finest: no level then", level == 0);

    // The level of roads at hand told nothing, the levels without tiles were
    // not at hand: only the former is not asked again.
    std::uint32_t untelling = 0;
    LandCover::classifyFinest(tiles, LandCover::kFinestLevel, LandCover::kCoarsestLevel, latitude,
                              longitude, &level, &untelling);
    expect("untelling: the level of roads marked", untelling == (std::uint32_t(1) << kLevel));
    tiles.forget();
    LandCover::classifyFinest(tiles, LandCover::kFinestLevel, LandCover::kCoarsestLevel, latitude,
                              longitude, &level, &untelling);
    expect("untelling: its tile not asked for again", !tiles.askedFor(kLevel, kTileX, kTileY)
                                                     && tiles.askedFor(15, kTileX / 2, kTileY / 2));
    double x17, y17;
    pixelOf(17, latitude, longitude, &x17, &y17);
    tiles.add(17, int(x17) / kSize, int(y17) / kSize, kWater);
    expectKind("untelling: a finer tile come since still counts",
               LandCover::classifyFinest(tiles, LandCover::kFinestLevel, LandCover::kCoarsestLevel,
                                         latitude, longitude, &level, &untelling),
               LandCover::Water);
}

/* The world repeats sideways: a place by the antimeridian looks into the
 * first column of tiles. */
void testAntimeridian()
{
    const int last = (1 << kLevel) - 1;
    FakeTiles tiles;
    tiles.add(kLevel, last, kTileY, kRoad);
    tiles.add(kLevel, 0, kTileY, kWater);
    double latitude, longitude;
    at(253.5, 100.5, &latitude, &longitude, kLevel, last, kTileY);
    expectKind("antimeridian: the water beyond it", LandCover::classify(tiles, kLevel, latitude, longitude),
               LandCover::Water);
    expectKind("poles: beyond the map", LandCover::classify(tiles, kLevel, 89.0, longitude),
               LandCover::Unknown);
}

} // namespace

int main()
{
    testColours();
    testForestRoad();
    testEdge();
    testAcrossTiles();
    testMissing();
    testTooLittle();
    testLevels();
    testFinest();
    testAntimeridian();

    std::printf("%s\n", g_failures ? "FAILED" : "All passed");
    return g_failures ? 1 : 0;
}
