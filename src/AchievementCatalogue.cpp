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

#include "AchievementCatalogue.h"

#include "LandCover.h"

#include <algorithm>
#include <utility>

namespace AchievementCatalogue {

namespace {

constexpr double km(double kilometres) { return kilometres * 1000; }
/* Minutes and seconds per kilometre, as metres per second. */
constexpr double pace(int minutes, int seconds) { return 1000.0 / (minutes * 60 + seconds); }
constexpr double kmh(double kilometresPerHour) { return kilometresPerHour / 3.6; }
constexpr double minutes(double count) { return count * 60; }
constexpr double hours(double count) { return count * 3600; }

/* The calendar categories' occasions, each in the order of the year, the
 * feasts that move by their first date. 42 each, which is the answer, and
 * the most a category takes: the last level is all of them. Each date only
 * once within a category, so that every activity collects one occasion at
 * most - but for a feast that moves onto another's day in some year.
 *
 * The dates of the feasts that move are listed for 2026 to 2031, for the
 * main day where there are several; a later version lists more. Easter and
 * the feasts it moves are by the Gregorian and Julian computus, the Jewish
 * holidays by the Hebrew calendar's rules (the first day; they begin on the
 * evening before), the Islamic ones by the Umm al-Qura calendar, the Hindu,
 * Sikh and Buddhist ones by Drik Panchang's for New Delhi - where the moon
 * is sighted, or the place is far from Delhi, a day may differ.
 *
 * Dates before 1582, and in Russia before 1918, are as history books give
 * them, mostly in the Julian calendar of the time. Birthdays are those
 * usually celebrated. */
const Occasion kPatriot[] = {
    { "australia-day", 1, 26, 1788, {} },
    { "new-zealand-waitangi-day", 2, 6, 1840, {} },
    { "japan-foundation-day", 2, 11, 0, {} },
    { "estonia-independence-day", 2, 24, 1918, {} },
    { "korea-independence-movement-day", 3, 1, 1919, {} },
    { "ghana-independence-day", 3, 6, 1957, {} },
    { "ireland-st-patricks-day", 3, 17, 0, {} },
    { "greece-independence-day", 3, 25, 1821, {} },
    { "south-africa-freedom-day", 4, 27, 1994, {} },
    { "norway-constitution-day", 5, 17, 1814, {} },
    { "argentina-may-revolution", 5, 25, 1810, {} },
    { "italy-republic-day", 6, 2, 1946, {} },
    { "denmark-constitution-day", 6, 5, 1849, {} },
    { "sweden-national-day", 6, 6, 1523, {} },
    { "portugal-day", 6, 10, 0, {} },
    { "russia-day", 6, 12, 1990, {} },
    { "canada-day", 7, 1, 1867, {} },
    { "united-states-independence-day", 7, 4, 1776, {} },
    { "france-bastille-day", 7, 14, 1789, {} },
    { "colombia-independence-day", 7, 20, 1810, {} },
    { "belgium-national-day", 7, 21, 1831, {} },
    { "egypt-revolution-day", 7, 23, 1952, {} },
    { "peru-independence-day", 7, 28, 1821, {} },
    { "switzerland-national-day", 8, 1, 1291, {} },
    { "pakistan-independence-day", 8, 14, 1947, {} },
    { "india-independence-day", 8, 15, 1947, {} },
    { "indonesia-independence-day", 8, 17, 1945, {} },
    { "hungary-st-stephens-day", 8, 20, 0, {} },
    { "ukraine-independence-day", 8, 24, 1991, {} },
    { "vietnam-national-day", 9, 2, 1945, {} },
    { "brazil-independence-day", 9, 7, 1822, {} },
    { "mexico-independence-day", 9, 16, 1810, {} },
    { "chile-independence-day", 9, 18, 1810, {} },
    { "china-national-day", 10, 1, 1949, {} },
    { "germany-unity-day", 10, 3, 1990, {} },
    { "spain-national-day", 10, 12, 0, {} },
    { "austria-national-day", 10, 26, 1955, {} },
    { "czechia-independence-day", 10, 28, 1918, {} },
    { "turkey-republic-day", 10, 29, 1923, {} },
    { "poland-independence-day", 11, 11, 1918, {} },
    { "finland-independence-day", 12, 6, 1917, {} },
    { "kenya-jamhuri-day", 12, 12, 1963, {} },
};

const Occasion kWorshipper[] = {
    { "epiphany", 1, 6, 0, {} },
    { "orthodox-christmas", 1, 7, 0, {} },
    { "makar-sankranti", 0, 0, 0, { 20260114, 20270115, 20280115, 20290114, 20300114, 20310115 } },
    { "nirvana-day", 2, 15, 0, {} },
    { "ramadan", 0, 0, 0, { 20260218, 20270208, 20280128, 20290116, 20300105, 20301226, 20311216 } },
    { "maha-shivaratri", 0, 0, 0, { 20260215, 20270306, 20280223, 20290211, 20300302, 20310220 } },
    { "ash-wednesday", 0, 0, 0, { 20260218, 20270210, 20280301, 20290214, 20300306, 20310226 } },
    { "purim", 0, 0, 0, { 20260303, 20270323, 20280312, 20290301, 20300319, 20310309 } },
    { "holi", 0, 0, 0, { 20260304, 20270322, 20280311, 20290301, 20300320, 20310309 } },
    { "laylat-al-qadr", 0, 0, 0, { 20260316, 20270306, 20280223, 20290211, 20300131, 20310121 } },
    { "eid-al-fitr", 0, 0, 0, { 20260320, 20270309, 20280226, 20290214, 20300204, 20310124 } },
    { "nowruz", 3, 21, 0, {} },
    { "passover", 0, 0, 0, { 20260402, 20270422, 20280411, 20290331, 20300418, 20310408 } },
    { "good-friday", 0, 0, 0, { 20260403, 20270326, 20280414, 20290330, 20300419, 20310411 } },
    { "easter", 0, 0, 0, { 20260405, 20270328, 20280416, 20290401, 20300421, 20310413 } },
    { "orthodox-easter", 0, 0, 0, { 20260412, 20270502, 20280416, 20290408, 20300428, 20310413 } },
    { "vaisakhi", 0, 0, 0, { 20260414, 20270414, 20280413, 20290414, 20300414, 20310414 } },
    { "vesak", 0, 0, 0, { 20260501, 20270520, 20280508, 20290527, 20300517, 20310507 } },
    { "ascension", 0, 0, 0, { 20260514, 20270506, 20280525, 20290510, 20300530, 20310522 } },
    { "shavuot", 0, 0, 0, { 20260522, 20270611, 20280531, 20290520, 20300607, 20310528 } },
    { "pentecost", 0, 0, 0, { 20260524, 20270516, 20280604, 20290520, 20300609, 20310601 } },
    { "day-of-arafah", 0, 0, 0, { 20260526, 20270515, 20280504, 20290423, 20300412, 20310401 } },
    { "eid-al-adha", 0, 0, 0, { 20260527, 20270516, 20280505, 20290424, 20300413, 20310402 } },
    { "corpus-christi", 0, 0, 0, { 20260604, 20270527, 20280615, 20290531, 20300620, 20310612 } },
    { "islamic-new-year", 0, 0, 0, { 20260616, 20270606, 20280525, 20290514, 20300504, 20310423 } },
    { "ashura", 0, 0, 0, { 20260625, 20270615, 20280603, 20290523, 20300513, 20310502 } },
    { "mawlid", 0, 0, 0, { 20260825, 20270814, 20280803, 20290724, 20300713, 20310702 } },
    { "assumption", 8, 15, 0, {} },
    { "janmashtami", 0, 0, 0, { 20260904, 20270825, 20280813, 20290901, 20300821, 20310809 } },
    { "ganesh-chaturthi", 0, 0, 0, { 20260914, 20270904, 20280823, 20290911, 20300901, 20310920 } },
    { "rosh-hashanah", 0, 0, 0, { 20260912, 20271002, 20280921, 20290910, 20300928, 20310918 } },
    { "yom-kippur", 0, 0, 0, { 20260921, 20271011, 20280930, 20290919, 20301007, 20310927 } },
    { "sukkot", 0, 0, 0, { 20260926, 20271016, 20281005, 20290924, 20301012, 20311002 } },
    { "navaratri", 0, 0, 0, { 20261011, 20270930, 20280919, 20291008, 20300928, 20311017 } },
    { "dussehra", 0, 0, 0, { 20261020, 20271009, 20280927, 20291016, 20301006, 20311025 } },
    { "reformation-day", 10, 31, 0, {} },
    { "all-saints", 11, 1, 0, {} },
    { "diwali", 0, 0, 0, { 20261108, 20271029, 20281017, 20291105, 20301026, 20311114 } },
    { "guru-nanak-gurpurab", 0, 0, 0, { 20261124, 20271114, 20281102, 20291121, 20301110, 20311128 } },
    { "hanukkah", 0, 0, 0, { 20261205, 20271225, 20281213, 20291202, 20301221, 20311210 } },
    { "bodhi-day", 12, 8, 0, {} },
    { "christmas", 12, 25, 0, {} },
};

const Occasion kFreedomFighter[] = {
    { "death-galtieri", 1, 12, 2003, {} },
    { "death-lenin", 1, 21, 1924, {} },
    { "death-suharto", 1, 27, 2008, {} },
    { "death-metaxas", 1, 29, 1941, {} },
    { "death-mubarak", 2, 25, 2020, {} },
    { "death-stalin", 3, 5, 1953, {} },
    { "death-caesar", 3, 15, -44, {} },
    { "death-primo-de-rivera", 3, 16, 1930, {} },
    { "death-chiang-kai-shek", 4, 5, 1975, {} },
    { "death-hoxha", 4, 11, 1985, {} },
    { "death-pol-pot", 4, 15, 1998, {} },
    { "death-francois-duvalier", 4, 21, 1971, {} },
    { "death-mussolini", 4, 28, 1945, {} },
    { "death-hitler", 4, 30, 1945, {} },
    { "death-tito", 5, 4, 1980, {} },
    { "death-videla", 5, 17, 2013, {} },
    { "death-honecker", 5, 29, 1994, {} },
    { "death-trujillo", 5, 30, 1961, {} },
    { "death-antonescu", 6, 1, 1946, {} },
    { "death-abacha", 6, 8, 1998, {} },
    { "death-hafez-al-assad", 6, 10, 2000, {} },
    { "death-kim-il-sung", 7, 8, 1994, {} },
    { "death-salazar", 7, 27, 1970, {} },
    { "death-batista", 8, 6, 1973, {} },
    { "death-idi-amin", 8, 16, 2003, {} },
    { "death-mugabe", 9, 6, 2019, {} },
    { "death-mobutu", 9, 7, 1997, {} },
    { "death-mao", 9, 9, 1976, {} },
    { "death-somoza", 9, 17, 1980, {} },
    { "death-marcos", 9, 28, 1989, {} },
    { "death-gaddafi", 10, 20, 2011, {} },
    { "death-park-chung-hee", 10, 26, 1979, {} },
    { "death-bokassa", 11, 3, 1996, {} },
    { "death-franco", 11, 20, 1975, {} },
    { "death-castro", 11, 25, 2016, {} },
    { "death-ne-win", 12, 5, 2002, {} },
    { "death-pinochet", 12, 10, 2006, {} },
    { "death-kim-jong-il", 12, 17, 2011, {} },
    { "death-niyazov", 12, 21, 2006, {} },
    { "death-ceausescu", 12, 25, 1989, {} },
    { "death-pavelic", 12, 28, 1959, {} },
    { "death-saddam-hussein", 12, 30, 2006, {} },
};

const Occasion kWarmonger[] = {
    { "russo-japanese-war", 2, 8, 1904, {} },
    { "invasion-of-ukraine", 2, 24, 2022, {} },
    { "syrian-civil-war", 3, 15, 2011, {} },
    { "iraq-war", 3, 20, 2003, {} },
    { "kosovo-war", 3, 24, 1999, {} },
    { "falklands-war", 4, 2, 1982, {} },
    { "war-of-the-pacific", 4, 5, 1879, {} },
    { "bosnian-war", 4, 6, 1992, {} },
    { "american-civil-war", 4, 12, 1861, {} },
    { "american-revolutionary-war", 4, 19, 1775, {} },
    { "spanish-american-war", 4, 21, 1898, {} },
    { "mexican-american-war", 4, 25, 1846, {} },
    { "war-of-the-spanish-succession", 5, 15, 1702, {} },
    { "napoleonic-wars", 5, 18, 1803, {} },
    { "thirty-years-war", 5, 23, 1618, {} },
    { "six-day-war", 6, 5, 1967, {} },
    { "austro-prussian-war", 6, 14, 1866, {} },
    { "war-of-1812", 6, 18, 1812, {} },
    { "operation-barbarossa", 6, 22, 1941, {} },
    { "korean-war", 6, 25, 1950, {} },
    { "nigerian-civil-war", 7, 6, 1967, {} },
    { "second-sino-japanese-war", 7, 7, 1937, {} },
    { "spanish-civil-war", 7, 17, 1936, {} },
    { "franco-prussian-war", 7, 19, 1870, {} },
    { "first-world-war", 7, 28, 1914, {} },
    { "chinese-civil-war", 8, 1, 1927, {} },
    { "gulf-war", 8, 2, 1990, {} },
    { "seven-years-war", 8, 29, 1756, {} },
    { "second-world-war", 9, 1, 1939, {} },
    { "first-opium-war", 9, 4, 1839, {} },
    { "iran-iraq-war", 9, 22, 1980, {} },
    { "yom-kippur-war", 10, 6, 1973, {} },
    { "war-in-afghanistan", 10, 7, 2001, {} },
    { "first-balkan-war", 10, 8, 1912, {} },
    { "second-boer-war", 10, 11, 1899, {} },
    { "vietnam-war", 11, 1, 1955, {} },
    { "winter-war", 11, 30, 1939, {} },
    { "indo-pakistani-war", 12, 3, 1971, {} },
    { "pacific-war", 12, 7, 1941, {} },
    { "war-of-the-austrian-succession", 12, 16, 1740, {} },
    { "first-indochina-war", 12, 19, 1946, {} },
    { "soviet-afghan-war", 12, 24, 1979, {} },
};

const Occasion kPeacemaker[] = {
    { "treaty-of-kiel", 1, 14, 1814, {} },
    { "elysee-treaty", 1, 22, 1963, {} },
    { "treaty-of-karlowitz", 1, 26, 1699, {} },
    { "paris-peace-accords", 1, 27, 1973, {} },
    { "treaty-of-guadalupe-hidalgo", 2, 2, 1848, {} },
    { "treaty-of-paris-1763", 2, 10, 1763, {} },
    { "treaty-of-brest-litovsk", 3, 3, 1918, {} },
    { "moscow-peace-treaty", 3, 12, 1940, {} },
    { "treaty-of-paris-1856", 3, 30, 1856, {} },
    { "end-of-spanish-civil-war", 4, 1, 1939, {} },
    { "appomattox", 4, 9, 1865, {} },
    { "good-friday-agreement", 4, 10, 1998, {} },
    { "treaty-of-utrecht", 4, 11, 1713, {} },
    { "ve-day", 5, 8, 1945, {} },
    { "schuman-declaration", 5, 9, 1950, {} },
    { "treaty-of-frankfurt", 5, 10, 1871, {} },
    { "treaty-of-vereeniging", 5, 31, 1902, {} },
    { "treaty-of-trianon", 6, 4, 1920, {} },
    { "congress-of-vienna", 6, 9, 1815, {} },
    { "un-charter", 6, 26, 1945, {} },
    { "treaty-of-versailles", 6, 28, 1919, {} },
    { "treaty-of-lausanne", 7, 24, 1923, {} },
    { "korean-armistice", 7, 27, 1953, {} },
    { "peace-of-breda", 7, 31, 1667, {} },
    { "partial-test-ban-treaty", 8, 5, 1963, {} },
    { "peace-of-prague-1866", 8, 23, 1866, {} },
    { "kellogg-briand-pact", 8, 27, 1928, {} },
    { "treaty-of-nanking", 8, 29, 1842, {} },
    { "japan-surrenders", 9, 2, 1945, {} },
    { "treaty-of-paris-1783", 9, 3, 1783, {} },
    { "treaty-of-portsmouth", 9, 5, 1905, {} },
    { "two-plus-four-treaty", 9, 12, 1990, {} },
    { "oslo-accords", 9, 13, 1993, {} },
    { "camp-david-accords", 9, 17, 1978, {} },
    { "peace-of-augsburg", 9, 25, 1555, {} },
    { "treaty-of-aix-la-chapelle", 10, 18, 1748, {} },
    { "peace-of-westphalia", 10, 24, 1648, {} },
    { "armistice-1918", 11, 11, 1918, {} },
    { "inf-treaty", 12, 8, 1987, {} },
    { "universal-declaration-of-human-rights", 12, 10, 1948, {} },
    { "dayton-agreement", 12, 14, 1995, {} },
    { "treaty-of-ghent", 12, 24, 1814, {} },
};

const Occasion kRevolutionary[] = {
    { "cuban-revolution", 1, 1, 1959, {} },
    { "prague-spring", 1, 5, 1968, {} },
    { "bloody-sunday-1905", 1, 22, 1905, {} },
    { "egyptian-revolution-2011", 1, 25, 2011, {} },
    { "iranian-revolution", 2, 11, 1979, {} },
    { "french-revolution-1848", 2, 24, 1848, {} },
    { "people-power-revolution", 2, 25, 1986, {} },
    { "february-revolution-1917", 3, 8, 1917, {} },
    { "salt-march", 3, 12, 1930, {} },
    { "hungarian-revolution-1848", 3, 15, 1848, {} },
    { "march-revolution-berlin", 3, 18, 1848, {} },
    { "easter-rising", 4, 24, 1916, {} },
    { "carnation-revolution", 4, 25, 1974, {} },
    { "indian-rebellion", 5, 10, 1857, {} },
    { "magna-carta", 6, 15, 1215, {} },
    { "uprising-17-june", 6, 17, 1953, {} },
    { "stonewall", 6, 28, 1969, {} },
    { "bastille", 7, 14, 1789, {} },
    { "july-revolution", 7, 27, 1830, {} },
    { "defenestration-1419", 7, 30, 1419, {} },
    { "warsaw-uprising", 8, 1, 1944, {} },
    { "haitian-revolution", 8, 22, 1791, {} },
    { "baltic-way", 8, 23, 1989, {} },
    { "belgian-revolution", 8, 25, 1830, {} },
    { "march-on-washington", 8, 28, 1963, {} },
    { "gdansk-agreement", 8, 31, 1980, {} },
    { "leipzig-monday-demonstration", 10, 9, 1989, {} },
    { "xinhai-revolution", 10, 10, 1911, {} },
    { "hungarian-revolution-1956", 10, 23, 1956, {} },
    { "ninety-five-theses", 10, 31, 1517, {} },
    { "kiel-mutiny", 11, 3, 1918, {} },
    { "glorious-revolution", 11, 5, 1688, {} },
    { "october-revolution", 11, 7, 1917, {} },
    { "fall-of-the-berlin-wall", 11, 9, 1989, {} },
    { "velvet-revolution", 11, 17, 1989, {} },
    { "mexican-revolution", 11, 20, 1910, {} },
    { "orange-revolution", 11, 22, 2004, {} },
    { "november-uprising", 11, 29, 1830, {} },
    { "rosa-parks", 12, 1, 1955, {} },
    { "boston-tea-party", 12, 16, 1773, {} },
    { "arab-spring", 12, 17, 2010, {} },
    { "romanian-revolution", 12, 22, 1989, {} },
};

const Occasion kConqueror[] = {
    { "fall-of-granada", 1, 2, 1492, {} },
    { "crossing-the-rubicon", 1, 10, -49, {} },
    { "overthrow-of-hawaii", 1, 17, 1893, {} },
    { "fall-of-paris-1871", 1, 28, 1871, {} },
    { "fall-of-baghdad", 2, 10, 1258, {} },
    { "fall-of-singapore", 2, 15, 1942, {} },
    { "fall-of-the-alamo", 3, 6, 1836, {} },
    { "anschluss", 3, 12, 1938, {} },
    { "occupation-of-prague", 3, 15, 1939, {} },
    { "battle-of-yamen", 3, 19, 1279, {} },
    { "invasion-of-denmark-and-norway", 4, 9, 1940, {} },
    { "sack-of-constantinople", 4, 13, 1204, {} },
    { "battle-of-panipat", 4, 21, 1526, {} },
    { "fall-of-saigon", 4, 30, 1975, {} },
    { "fall-of-berlin", 5, 2, 1945, {} },
    { "fall-of-acre", 5, 18, 1291, {} },
    { "conquest-of-toledo", 5, 25, 1085, {} },
    { "fall-of-constantinople", 5, 29, 1453, {} },
    { "fall-of-paris-1940", 6, 14, 1940, {} },
    { "battle-of-plassey", 6, 23, 1757, {} },
    { "conquest-of-cordoba", 6, 29, 1236, {} },
    { "conquest-of-algiers", 7, 5, 1830, {} },
    { "siege-of-jerusalem-1099", 7, 15, 1099, {} },
    { "fall-of-tenochtitlan", 8, 13, 1521, {} },
    { "sack-of-rome-410", 8, 24, 410, {} },
    { "battle-of-mohacs", 8, 29, 1526, {} },
    { "fall-of-rome-476", 9, 4, 476, {} },
    { "battle-of-the-plains-of-abraham", 9, 13, 1759, {} },
    { "napoleon-in-moscow", 9, 14, 1812, {} },
    { "capture-of-rome-1870", 9, 20, 1870, {} },
    { "fall-of-warsaw", 9, 28, 1939, {} },
    { "battle-of-gaugamela", 10, 1, -331, {} },
    { "saladin-takes-jerusalem", 10, 2, 1187, {} },
    { "fall-of-babylon", 10, 12, -539, {} },
    { "battle-of-hastings", 10, 14, 1066, {} },
    { "conquest-of-lisbon", 10, 25, 1147, {} },
    { "napoleon-in-vienna", 11, 13, 1805, {} },
    { "battle-of-cajamarca", 11, 16, 1532, {} },
    { "conquest-of-seville", 11, 23, 1248, {} },
    { "conquest-of-goa", 11, 25, 1510, {} },
    { "fall-of-kyiv", 12, 6, 1240, {} },
    { "sack-of-delhi", 12, 17, 1398, {} },
};

const Occasion kExplorer[] = {
    { "discovery-of-ceres", 1, 1, 1801, {} },
    { "antarctic-circle", 1, 17, 1773, {} },
    { "cook-reaches-hawaii", 1, 18, 1778, {} },
    { "landing-on-antarctica", 1, 24, 1895, {} },
    { "cape-of-good-hope", 2, 3, 1488, {} },
    { "discovery-of-pluto", 2, 18, 1930, {} },
    { "discovery-of-uranus", 3, 13, 1781, {} },
    { "terracotta-army", 3, 29, 1974, {} },
    { "ponce-de-leon-florida", 4, 2, 1513, {} },
    { "easter-island", 4, 5, 1722, {} },
    { "farthest-north", 4, 8, 1895, {} },
    { "cabral-brazil", 4, 22, 1500, {} },
    { "botany-bay", 4, 29, 1770, {} },
    { "de-soto-mississippi", 5, 8, 1541, {} },
    { "north-pole-flight", 5, 12, 1926, {} },
    { "vasco-da-gama-india", 5, 20, 1498, {} },
    { "chimborazo", 6, 23, 1802, {} },
    { "founding-of-quebec", 7, 3, 1608, {} },
    { "rosetta-stone", 7, 15, 1799, {} },
    { "mackenzie-pacific", 7, 22, 1793, {} },
    { "machu-picchu", 7, 24, 1911, {} },
    { "source-of-the-nile", 7, 28, 1862, {} },
    { "petra", 8, 22, 1812, {} },
    { "orellana-amazon", 8, 26, 1542, {} },
    { "northwest-passage", 8, 31, 1906, {} },
    { "titanic-wreck", 9, 1, 1985, {} },
    { "first-circumnavigation", 9, 6, 1522, {} },
    { "lascaux", 9, 12, 1940, {} },
    { "darwin-galapagos", 9, 15, 1835, {} },
    { "otzi", 9, 19, 1991, {} },
    { "discovery-of-neptune", 9, 23, 1846, {} },
    { "balboa-pacific", 9, 25, 1513, {} },
    { "first-exoplanet", 10, 6, 1995, {} },
    { "columbus-americas", 10, 12, 1492, {} },
    { "tutankhamun", 11, 4, 1922, {} },
    { "lewis-and-clark-pacific", 11, 7, 1805, {} },
    { "stanley-livingstone", 11, 10, 1871, {} },
    { "victoria-falls", 11, 16, 1855, {} },
    { "tasmania", 11, 24, 1642, {} },
    { "magellan-pacific", 11, 28, 1520, {} },
    { "new-zealand", 12, 13, 1642, {} },
    { "south-pole", 12, 14, 1911, {} },
};

const Occasion kFirstOne[] = {
    { "jupiter-moons", 1, 7, 1610, {} },
    { "deepest-dive", 1, 23, 1960, {} },
    { "dolly-the-sheep", 2, 22, 1997, {} },
    { "first-telephone-call", 3, 10, 1876, {} },
    { "first-spacewalk", 3, 18, 1965, {} },
    { "black-hole-image", 4, 10, 2019, {} },
    { "first-human-in-space", 4, 12, 1961, {} },
    { "splitting-the-atom", 4, 14, 1932, {} },
    { "first-space-station", 4, 19, 1971, {} },
    { "dna-double-helix", 4, 25, 1953, {} },
    { "four-minute-mile", 5, 6, 1954, {} },
    { "diamond-sutra", 5, 11, 868, {} },
    { "first-vaccination", 5, 14, 1796, {} },
    { "first-solo-transatlantic-flight", 5, 21, 1927, {} },
    { "everest", 5, 29, 1953, {} },
    { "first-nonstop-transatlantic-flight", 6, 15, 1919, {} },
    { "first-woman-in-space", 6, 16, 1963, {} },
    { "first-rocket-in-space", 6, 20, 1944, {} },
    { "human-genome", 6, 26, 2000, {} },
    { "special-relativity", 6, 30, 1905, {} },
    { "principia", 7, 5, 1687, {} },
    { "moon-landing", 7, 20, 1969, {} },
    { "first-ivf-baby", 7, 25, 1978, {} },
    { "mont-blanc", 8, 8, 1786, {} },
    { "first-jet-flight", 8, 27, 1939, {} },
    { "luna-2", 9, 13, 1959, {} },
    { "gravitational-waves", 9, 14, 2015, {} },
    { "penicillin", 9, 28, 1928, {} },
    { "sputnik", 10, 4, 1957, {} },
    { "sound-barrier", 10, 14, 1947, {} },
    { "first-anaesthesia", 10, 16, 1846, {} },
    { "x-rays", 11, 8, 1895, {} },
    { "first-crewed-balloon-flight", 11, 21, 1783, {} },
    { "origin-of-species", 11, 24, 1859, {} },
    { "first-nuclear-reactor", 12, 2, 1942, {} },
    { "first-heart-transplant", 12, 3, 1967, {} },
    { "first-nobel-prizes", 12, 10, 1901, {} },
    { "first-transatlantic-radio", 12, 12, 1901, {} },
    { "first-powered-flight", 12, 17, 1903, {} },
    { "first-rocket-landing", 12, 21, 2015, {} },
    { "first-kidney-transplant", 12, 23, 1954, {} },
    { "apollo-8", 12, 24, 1968, {} },
};

const Occasion kInnovator[] = {
    { "steam-engine", 1, 5, 1769, {} },
    { "daguerreotype", 1, 7, 1839, {} },
    { "jet-engine", 1, 16, 1930, {} },
    { "television", 1, 26, 1926, {} },
    { "automobile", 1, 29, 1886, {} },
    { "eniac", 2, 14, 1946, {} },
    { "phonograph", 2, 19, 1878, {} },
    { "steam-locomotive", 2, 21, 1804, {} },
    { "gps", 2, 22, 1978, {} },
    { "diesel-engine", 2, 23, 1893, {} },
    { "nylon", 2, 28, 1935, {} },
    { "telephone", 3, 7, 1876, {} },
    { "world-wide-web", 3, 12, 1989, {} },
    { "cotton-gin", 3, 14, 1794, {} },
    { "liquid-fuel-rocket", 3, 16, 1926, {} },
    { "battery", 3, 20, 1800, {} },
    { "mobile-phone", 4, 3, 1973, {} },
    { "safety-pin", 4, 10, 1849, {} },
    { "dynamite", 5, 7, 1867, {} },
    { "z3-computer", 5, 12, 1941, {} },
    { "laser", 5, 16, 1960, {} },
    { "blue-jeans", 5, 20, 1873, {} },
    { "telegraph", 5, 24, 1844, {} },
    { "radio", 6, 2, 1896, {} },
    { "bicycle", 6, 12, 1817, {} },
    { "typewriter", 6, 23, 1868, {} },
    { "zeppelin", 7, 2, 1900, {} },
    { "telstar", 7, 10, 1962, {} },
    { "aspirin", 8, 10, 1897, {} },
    { "sewing-machine", 9, 10, 1846, {} },
    { "integrated-circuit", 9, 12, 1958, {} },
    { "barcode", 10, 7, 1952, {} },
    { "microwave-oven", 10, 8, 1945, {} },
    { "light-bulb", 10, 22, 1879, {} },
    { "internet", 10, 29, 1969, {} },
    { "microprocessor", 11, 15, 1971, {} },
    { "assembly-line", 12, 1, 1913, {} },
    { "text-message", 12, 3, 1992, {} },
    { "pneumatic-tyre", 12, 7, 1888, {} },
    { "computer-mouse", 12, 9, 1968, {} },
    { "transistor", 12, 16, 1947, {} },
    { "cinematograph", 12, 28, 1895, {} },
};

const Occasion kVirtuoso[] = {
    { "birth-elvis-presley", 1, 8, 1935, {} },
    { "birth-mozart", 1, 27, 1756, {} },
    { "birth-schubert", 1, 31, 1797, {} },
    { "birth-mendelssohn", 2, 3, 1809, {} },
    { "birth-bob-marley", 2, 6, 1945, {} },
    { "birth-handel", 2, 23, 1685, {} },
    { "birth-chopin", 3, 1, 1810, {} },
    { "birth-smetana", 3, 2, 1824, {} },
    { "birth-vivaldi", 3, 4, 1678, {} },
    { "birth-ravel", 3, 7, 1875, {} },
    { "birth-bach", 3, 21, 1685, {} },
    { "birth-bartok", 3, 25, 1881, {} },
    { "birth-haydn", 3, 31, 1732, {} },
    { "birth-rachmaninoff", 4, 1, 1873, {} },
    { "birth-duke-ellington", 4, 29, 1899, {} },
    { "birth-tchaikovsky", 5, 7, 1840, {} },
    { "birth-monteverdi", 5, 15, 1567, {} },
    { "birth-wagner", 5, 22, 1813, {} },
    { "birth-elgar", 6, 2, 1857, {} },
    { "birth-schumann", 6, 8, 1810, {} },
    { "birth-richard-strauss", 6, 11, 1864, {} },
    { "birth-grieg", 6, 15, 1843, {} },
    { "birth-stravinsky", 6, 17, 1882, {} },
    { "birth-mahler", 7, 7, 1860, {} },
    { "birth-louis-armstrong", 8, 4, 1901, {} },
    { "birth-debussy", 8, 22, 1862, {} },
    { "birth-bernstein", 8, 25, 1918, {} },
    { "birth-bruckner", 9, 4, 1824, {} },
    { "birth-freddie-mercury", 9, 5, 1946, {} },
    { "birth-dvorak", 9, 8, 1841, {} },
    { "birth-clara-schumann", 9, 13, 1819, {} },
    { "birth-shostakovich", 9, 25, 1906, {} },
    { "birth-gershwin", 9, 26, 1898, {} },
    { "birth-john-lennon", 10, 9, 1940, {} },
    { "birth-verdi", 10, 10, 1813, {} },
    { "birth-liszt", 10, 22, 1811, {} },
    { "birth-johann-strauss", 10, 25, 1825, {} },
    { "birth-paganini", 10, 27, 1782, {} },
    { "birth-sibelius", 12, 8, 1865, {} },
    { "birth-berlioz", 12, 11, 1803, {} },
    { "birth-beethoven", 12, 16, 1770, {} },
    { "birth-puccini", 12, 22, 1858, {} },
};
/* The number of occasions in a category's list. */
template <std::size_t N>
constexpr std::size_t countOf(const Occasion (&)[N]) { return N; }

/* None tuned on anyone's activities. The first level comes with a first
 * try, or nearly; the last is out of reach for most: a 100 km run, 3:20 per
 * km over 5 km, round the world by bike, eleven Everests. The distances of
 * a single activity stop short of the half and the full marathon's 21.1
 * and 42.2 km, as a receiver's distance may fall a little short of a
 * course's. */
const Category kCategories[] = {
    { "running-longest", "running", LongestDistance, "running", 0, 0, "distance",
      "activity-running", "", { km(1), km(3), km(5), km(10), km(21), km(42), km(100) }, nullptr, 0 },
    { "running-distance", "running", TotalDistance, "running", 0, 0, "distance",
      "activity-running", "total", { km(5), km(25), km(100), km(500), km(1000), km(5000), km(10000) }, nullptr, 0 },
    { "running-pace", "running", FastestSpeed, "running", 0, km(5), "pace",
      "activity-running", "fastest",
      { pace(8, 0), pace(7, 0), pace(6, 0), pace(5, 20), pace(4, 40), pace(4, 0), pace(3, 20) }, nullptr, 0 },

    { "walking-longest", "walking", LongestDistance, "walking", 0, 0, "distance",
      "activity-walking", "", { km(1), km(3), km(5), km(10), km(20), km(42), km(100) }, nullptr, 0 },
    { "walking-distance", "walking", TotalDistance, "walking", 0, 0, "distance",
      "activity-walking", "total", { km(5), km(25), km(100), km(500), km(1000), km(5000), km(10000) }, nullptr, 0 },
    { "walking-pace", "walking", FastestSpeed, "walking", 0, km(3), "pace",
      "activity-walking", "fastest",
      { pace(15, 0), pace(13, 0), pace(12, 0), pace(11, 0), pace(10, 0), pace(9, 0), pace(8, 0) }, nullptr, 0 },

    { "cycling-longest", "cycling", LongestDistance, "cycling", 0, 0, "distance",
      "activity-cycling", "", { km(5), km(10), km(25), km(50), km(100), km(200), km(400) }, nullptr, 0 },
    { "cycling-distance", "cycling", TotalDistance, "cycling", 0, 0, "distance",
      "activity-cycling", "total",
      { km(25), km(100), km(500), km(2500), km(10000), km(25000), km(40000) }, nullptr, 0 },
    { "cycling-speed", "cycling", FastestSpeed, "cycling", 0, km(10), "speed",
      "activity-cycling", "fastest", { kmh(12), kmh(16), kmh(20), kmh(25), kmh(30), kmh(35), kmh(40) }, nullptr, 0 },

    { "rowing-longest", "rowing", LongestDistance, "rowing", 0, 0, "distance",
      "activity-rowing", "", { km(1), km(2), km(5), km(10), km(21), km(42), km(100) }, nullptr, 0 },
    { "rowing-distance", "rowing", TotalDistance, "rowing", 0, 0, "distance",
      "activity-rowing", "total", { km(5), km(25), km(100), km(250), km(1000), km(2500), km(5000) }, nullptr, 0 },
    { "rowing-speed", "rowing", FastestSpeed, "rowing", 0, km(2), "speed",
      "activity-rowing", "fastest", { kmh(4), kmh(6), kmh(8), kmh(10), kmh(12), kmh(14), kmh(16) }, nullptr, 0 },

    { "climb-single", "climbing", BiggestClimb, nullptr, 0, 0, "climb",
      "achievement-mountain", "", { 25, 50, 100, 250, 500, 1000, 2000 }, nullptr, 0 },
    { "climb-total", "climbing", TotalClimb, nullptr, 0, 0, "climb",
      "achievement-mountain", "total", { 50, 250, 1000, 2500, 8849, 25000, 100000 }, nullptr, 0 },

    { "terrain-forest", "terrain", LandCoverDistance, nullptr, LandCover::Forest, 0, "distance",
      "achievement-forest", "", { km(1), km(5), km(25), km(100), km(250), km(1000), km(2500) }, nullptr, 0 },
    { "terrain-grassland", "terrain", LandCoverDistance, nullptr, LandCover::Grassland, 0, "distance",
      "achievement-meadow", "", { km(1), km(5), km(25), km(100), km(250), km(1000), km(2500) }, nullptr, 0 },
    { "terrain-farmland", "terrain", LandCoverDistance, nullptr, LandCover::Farmland, 0, "distance",
      "achievement-field", "", { km(1), km(5), km(25), km(100), km(250), km(1000), km(2500) }, nullptr, 0 },
    { "terrain-natural", "terrain", LandCoverDistance, nullptr, LandCover::Natural, 0, "distance",
      "achievement-heath", "", { km(1), km(5), km(25), km(100), km(250), km(1000), km(2500) }, nullptr, 0 },
    { "terrain-water", "terrain", LandCoverDistance, nullptr, LandCover::Water, 0, "distance",
      "achievement-water", "", { km(1), km(5), km(25), km(100), km(250), km(1000), km(2500) }, nullptr, 0 },
    { "terrain-builtup", "terrain", LandCoverDistance, nullptr, LandCover::BuiltUp, 0, "distance",
      "achievement-town", "", { km(1), km(5), km(25), km(100), km(250), km(1000), km(2500) }, nullptr, 0 },

    { "duration-single", "endurance", LongestDuration, nullptr, 0, 0, "duration",
      "achievement-stopwatch", "",
      { minutes(15), minutes(30), hours(1), hours(2), hours(4), hours(8), hours(24) }, nullptr, 0 },
    { "duration-total", "endurance", TotalDuration, nullptr, 0, 0, "duration",
      "achievement-stopwatch", "total",
      { hours(1), hours(10), hours(50), hours(150), hours(500), hours(1500), hours(5000) }, nullptr, 0 },

    { "activities-count", "regularity", ActivityCount, nullptr, 0, 0, "count",
      "achievement-flag", "", { 1, 10, 25, 100, 250, 500, 1000 }, nullptr, 0 },
    { "streak-weeks", "regularity", WeekStreak, nullptr, 0, 0, "weeks",
      "achievement-calendar", "", { 2, 4, 8, 13, 26, 52, 104 }, nullptr, 0 },

    { "steps-day", "steps", MostDailySteps, nullptr, 0, 0, "steps",
      "achievement-steps", "", { 2500, 5000, 7500, 10000, 15000, 25000, 40000 }, nullptr, 0 },
    { "steps-total", "steps", TotalSteps, nullptr, 0, 0, "steps",
      "achievement-steps", "total", { 1e4, 1e5, 5e5, 1e6, 2.5e6, 5e6, 1e7 }, nullptr, 0 },

    /* One occasion, and all 42 at last; Diamond takes a year of activities
     * every day, and some feasts until 2031. */
    { "days-patriot", "holidays", CalendarDays, nullptr, 0, 0, "days",
      "achievement-patriot", "", { 1, 3, 6, 12, 20, 30, 42 }, kPatriot, countOf(kPatriot) },
    { "days-worshipper", "holidays", CalendarDays, nullptr, 0, 0, "days",
      "achievement-worshipper", "", { 1, 3, 6, 12, 20, 30, 42 }, kWorshipper, countOf(kWorshipper) },
    { "days-freedom-fighter", "history", CalendarDays, nullptr, 0, 0, "days",
      "achievement-freedom-fighter", "", { 1, 3, 6, 12, 20, 30, 42 }, kFreedomFighter, countOf(kFreedomFighter) },
    { "days-warmonger", "history", CalendarDays, nullptr, 0, 0, "days",
      "achievement-warmonger", "", { 1, 3, 6, 12, 20, 30, 42 }, kWarmonger, countOf(kWarmonger) },
    { "days-peacemaker", "history", CalendarDays, nullptr, 0, 0, "days",
      "achievement-peacemaker", "", { 1, 3, 6, 12, 20, 30, 42 }, kPeacemaker, countOf(kPeacemaker) },
    { "days-revolutionary", "history", CalendarDays, nullptr, 0, 0, "days",
      "achievement-revolutionary", "", { 1, 3, 6, 12, 20, 30, 42 }, kRevolutionary, countOf(kRevolutionary) },
    { "days-conqueror", "history", CalendarDays, nullptr, 0, 0, "days",
      "achievement-conqueror", "", { 1, 3, 6, 12, 20, 30, 42 }, kConqueror, countOf(kConqueror) },
    { "days-explorer", "history", CalendarDays, nullptr, 0, 0, "days",
      "achievement-explorer", "", { 1, 3, 6, 12, 20, 30, 42 }, kExplorer, countOf(kExplorer) },
    { "days-first-one", "history", CalendarDays, nullptr, 0, 0, "days",
      "achievement-first-one", "", { 1, 3, 6, 12, 20, 30, 42 }, kFirstOne, countOf(kFirstOne) },
    { "days-innovator", "history", CalendarDays, nullptr, 0, 0, "days",
      "achievement-innovator", "", { 1, 3, 6, 12, 20, 30, 42 }, kInnovator, countOf(kInnovator) },
    { "days-virtuoso", "history", CalendarDays, nullptr, 0, 0, "days",
      "achievement-virtuoso", "", { 1, 3, 6, 12, 20, 30, 42 }, kVirtuoso, countOf(kVirtuoso) },
};
const std::size_t kCategoryCount = sizeof(kCategories) / sizeof(kCategories[0]);

/* Whether the metric goes by the steps rather than the activities. */
bool bySteps(Metric metric)
{
    return metric == MostDailySteps || metric == TotalSteps;
}

/* What one activity adds to a category's value, as of the value so far;
 * the value itself where it is the best of single activities. */
double valueWith(const Category &category, double value, const ActivityRecord &activity)
{
    switch (category.metric) {
    case LongestDistance:
        return std::max(value, activity.distance);
    case TotalDistance:
        return value + activity.distance;
    case FastestSpeed:
        if (activity.distance >= category.minimumDistance && activity.duration > 0)
            return std::max(value, activity.distance / activity.duration);
        return value;
    case BiggestClimb:
        return std::max(value, activity.climb);
    case TotalClimb:
        return value + activity.climb;
    case LandCoverDistance:
        if (category.landCover >= 0 && std::size_t(category.landCover) < activity.landCover.size())
            return value + activity.landCover[std::size_t(category.landCover)];
        return value;
    case LongestDuration:
        return std::max(value, activity.duration);
    case TotalDuration:
        return value + activity.duration;
    case ActivityCount:
        return value + 1;
    case WeekStreak:
    case MostDailySteps:
    case TotalSteps:
    case CalendarDays:
        break;
    }
    return value;
}

/* Raises the progress to the level its value has reached, dated `day`. */
void raise(const Category &category, Progress *progress, int day)
{
    const int level = levelOf(category, progress->value);
    if (level > progress->level) {
        progress->level = level;
        progress->day = day;
    }
}

/* A Julian day number's week: Julian day 0 was a Monday. */
int weekOf(int day)
{
    return day >= 0 ? day / 7 : (day - 6) / 7;
}

/* A Julian day number's Gregorian date as yyyymmdd, by Fliegel and Van
 * Flandern's algorithm. */
int dateOf(int day)
{
    int l = day + 68569;
    const int n = 4 * l / 146097;
    l -= (146097 * n + 3) / 4;
    const int i = 4000 * (l + 1) / 1461001;
    l = l - 1461 * i / 4 + 31;
    const int j = 80 * l / 2447;
    const int date = l - 2447 * j / 80;
    l = j / 11;
    return (100 * (n - 49) + i + l) * 10000 + (j + 2 - 12 * l) * 100 + date;
}

/* A category's occasions by date, sorted: those of every year as mmdd,
 * the dates of a feast that moves as yyyymmdd, so the two never meet. */
typedef std::vector<std::pair<int, std::size_t>> Calendar;

/* Every category's, empty for all but the calendar ones. Made once, not
 * at every evaluate(). */
const std::vector<Calendar> &calendars()
{
    static const std::vector<Calendar> calendars = [] {
        std::vector<Calendar> all(kCategoryCount);
        for (std::size_t i = 0; i < kCategoryCount; ++i) {
            const Category &category = kCategories[i];
            for (std::size_t j = 0; j < category.occasionCount; ++j) {
                const Occasion &occasion = category.occasions[j];
                if (occasion.month > 0)
                    all[i].push_back(std::make_pair(occasion.month * 100 + occasion.day, j));
                for (int k = 0; k < kOccasionDates && occasion.dates[k] != 0; ++k)
                    all[i].push_back(std::make_pair(occasion.dates[k], j));
            }
            std::sort(all[i].begin(), all[i].end());
        }
        return all;
    }();
    return calendars;
}

/* Calls `function` with each occasion of a calendar on a date, yyyymmdd. */
template <typename Function>
void forEachOccasion(const Calendar &calendar, int date, Function function)
{
    for (int key : { date % 10000, date }) {
        for (auto i = std::lower_bound(calendar.begin(), calendar.end(), std::make_pair(key, std::size_t(0)));
             i != calendar.end() && i->first == key; ++i)
            function(i->second);
    }
}

/* What the occasions collected come to: one each, the level dated by the
 * occasion that reached it. Sorted by occasion. */
Progress settle(const Category &category, std::vector<Collected> collected)
{
    std::sort(collected.begin(), collected.end(), [](const Collected &a, const Collected &b) {
        return a.day < b.day || (a.day == b.day && a.occasion < b.occasion);
    });
    Progress progress;
    for (const Collected &occasion : collected) {
        progress.value += 1;
        raise(category, &progress, occasion.day);
    }
    std::sort(collected.begin(), collected.end(), [](const Collected &a, const Collected &b) {
        return a.occasion < b.occasion;
    });
    progress.collected = collected;
    return progress;
}

/* Adds an occasion collected on `day`, or moves it to `day` if that is
 * earlier: `first` holds each occasion's day, or 0 while not collected. */
void collectOn(std::vector<int> *first, std::size_t occasion, int day)
{
    if (occasion < first->size() && ((*first)[occasion] == 0 || day < (*first)[occasion]))
        (*first)[occasion] = day;
}

std::vector<Collected> collectedOf(const std::vector<int> &first)
{
    std::vector<Collected> collected;
    for (std::size_t i = 0; i < first.size(); ++i) {
        if (first[i] != 0) {
            Collected occasion;
            occasion.occasion = i;
            occasion.day = first[i];
            collected.push_back(occasion);
        }
    }
    return collected;
}

} // namespace

std::size_t categoryCount()
{
    return kCategoryCount;
}

const Category &category(std::size_t index)
{
    return kCategories[index];
}

bool counts(const Category &category, const std::string &type)
{
    return category.type ? type == category.type : type != "driving";
}

int levelOf(const Category &category, double value)
{
    int level = 0;
    while (level < kLevels && value >= category.thresholds[level])
        ++level;
    return level;
}

std::vector<Progress> evaluate(const std::vector<ActivityRecord> &activities,
                               const std::vector<DayRecord> &days)
{
    std::vector<Progress> progress(kCategoryCount);
    // The activities' dates, once for all calendar categories.
    std::vector<int> dates;
    dates.reserve(activities.size());
    for (const ActivityRecord &activity : activities)
        dates.push_back(dateOf(activity.day));

    for (std::size_t i = 0; i < kCategoryCount; ++i) {
        const Category &category = kCategories[i];
        Progress &result = progress[i];

        if (category.metric == CalendarDays) {
            std::vector<int> first(category.occasionCount, 0);
            for (std::size_t j = 0; j < activities.size(); ++j) {
                const int day = activities[j].day;
                if (counts(category, activities[j].type))
                    forEachOccasion(calendars()[i], dates[j], [&first, day](std::size_t occasion) {
                        collectOn(&first, occasion, day);
                    });
            }
            result = settle(category, collectedOf(first));
            continue;
        }

        if (bySteps(category.metric)) {
            for (const DayRecord &day : days) {
                result.value = category.metric == TotalSteps ? result.value + day.steps
                                                             : std::max(result.value, day.steps);
                raise(category, &result, day.day);
            }
            continue;
        }

        // For the streak: the week of the last activity, and how many in a
        // row up to it.
        int lastWeek = 0;
        int run = 0;
        for (const ActivityRecord &activity : activities) {
            if (!counts(category, activity.type))
                continue;
            if (category.metric == WeekStreak) {
                const int week = weekOf(activity.day);
                if (run == 0 || week > lastWeek + 1)
                    run = 1;
                else if (week == lastWeek + 1)
                    ++run;
                lastWeek = std::max(lastWeek, week);
                result.value = std::max(result.value, double(run));
            } else {
                result.value = valueWith(category, result.value, activity);
            }
            raise(category, &result, activity.day);
        }
    }
    return progress;
}

std::vector<std::size_t> occasionsOn(const Category &category, int day)
{
    std::vector<std::size_t> occasions;
    forEachOccasion(calendars()[std::size_t(&category - kCategories)], dateOf(day),
                    [&occasions](std::size_t occasion) { occasions.push_back(occasion); });
    std::sort(occasions.begin(), occasions.end());
    occasions.erase(std::unique(occasions.begin(), occasions.end()), occasions.end());
    return occasions;
}

Progress collect(const Category &category, const Progress &progress, const std::vector<Collected> &kept)
{
    std::vector<int> first(category.occasionCount, 0);
    for (const Collected &occasion : progress.collected)
        collectOn(&first, occasion.occasion, occasion.day);
    // A key no longer in the catalogue was left out before this.
    for (const Collected &occasion : kept)
        collectOn(&first, occasion.occasion, occasion.day);
    return settle(category, collectedOf(first));
}

Award merge(const Award &kept, const Progress &progress)
{
    Award award = kept;
    if (progress.level > kept.level) {
        award.level = progress.level;
        award.day = progress.day;
    }
    award.best = std::max(kept.best, progress.value);
    return award;
}

} // namespace AchievementCatalogue
