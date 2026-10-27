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

#ifndef ACHIEVEMENTCATALOGUE_H
#define ACHIEVEMENTCATALOGUE_H

#include <cstddef>
#include <string>
#include <vector>

/**
 * The achievements: what there is to achieve, and how far the activities and
 * the steps have come towards it.
 *
 * Each category measures one thing - the longest run, the distance cycled in
 * all, the forest walked through - and has kLevels levels, from Paper to
 * Diamond, each reached at a threshold of that measure. The first comes with
 * the first try or nearly; the last takes years, or more than most can.
 *
 * Plain C++ without Qt, like Track, so tests/tst_achievements.cpp drives it
 * on the host. Achievements reads the database for it and keeps what it
 * says - and keeps a level once reached, whatever is deleted later: see
 * merge().
 *
 * The calendar categories collect days instead: the national days, the
 * days wars began, the birthdays of composers - each a list of occasions,
 * collected by an activity on one of them, in any year. Their levels count
 * the occasions ever collected, which collect() keeps.
 *
 * A new category is an entry in kCategories (AchievementCatalogue.cpp), with
 * its name and description in harbour-chronograph.qml; a new kind of
 * measure is a Metric as well, worked out in evaluate(). A calendar
 * category's occasions are named in qml/js/Occasions.js.
 */
namespace AchievementCatalogue {

/* Paper, Wood, Bronze, Silver, Gold, Platinum, Diamond: 1 to 7. 0 is none. */
const int kLevels = 7;

/* The most dates a moving feast lists: one a year, seven for a lunar year's
 * feast in six solar years. */
const int kOccasionDates = 8;

enum Metric {
    /* Metres, in a single activity. */
    LongestDistance,
    /* Metres, all activities together. */
    TotalDistance,
    /* Metres per second on average, pauses left out, in a single activity
     * of at least the category's minimum distance. */
    FastestSpeed,
    /* Metres climbed, in a single activity. */
    BiggestClimb,
    /* Metres climbed, all activities together. */
    TotalClimb,
    /* Metres over the category's land cover, all activities together. */
    LandCoverDistance,
    /* Seconds, pauses left out, in a single activity. */
    LongestDuration,
    /* Seconds, pauses left out, all activities together. */
    TotalDuration,
    /* Activities. */
    ActivityCount,
    /* Calendar weeks in a row, Monday to Sunday, with an activity in each. */
    WeekStreak,
    /* Steps, in a single day. */
    MostDailySteps,
    /* Steps, all days together. */
    TotalSteps,
    /* The category's occasions with an activity on their day. */
    CalendarDays
};

/* A day a calendar category collects: the same date every year, or a feast
 * that moves, on the dates listed. */
struct Occasion {
    /* Stable: the database keeps what was collected by it, and QML names
     * it by it. Unique across all categories. */
    const char *key;
    /* 1 to 12 and 1 to 31, every year; 0 for a feast that moves. */
    int month;
    int day;
    /* The year it happened, negative before Christ; 0 for none, as for a
     * feast or most national days. */
    int year;
    /* For a feast that moves: its dates as yyyymmdd, rising, then 0. */
    int dates[kOccasionDates];
};

struct Category {
    /* Stable: the database keeps the levels by it, and QML names it by it. */
    const char *key;
    /* Which section of the page it is shown in, in the order they come. */
    const char *group;
    Metric metric;
    /* The activity type it counts, or null for every type a body moves:
     * all but driving. */
    const char *type;
    /* For LandCoverDistance: a LandCover::Kind. */
    int landCover;
    /* For FastestSpeed: metres an activity must have to count. */
    double minimumDistance;
    /* How the page shows its values: "distance", "pace", "speed", "climb",
     * "duration", "count", "weeks", "steps" or "days". */
    const char *unit;
    /* Its pictogram, images/<icon>.svg, and the badge on it, if any:
     * "total" or "fastest" (images/achievement-badge-<badge>.svg). */
    const char *icon;
    const char *badge;
    /* What each level takes, rising: metres, metres per second, seconds,
     * activities, weeks, steps or occasions - as the metric counts. */
    double thresholds[kLevels];
    /* For CalendarDays: what there is to collect. The last level takes
     * all of it. */
    const Occasion *occasions;
    std::size_t occasionCount;
};

/* Every category, grouped and in the order the page shows them. */
std::size_t categoryCount();
const Category &category(std::size_t index);

/* One stored activity, as evaluate() needs it. */
struct ActivityRecord {
    /* The type's key: "running", "walking", "cycling", "rowing", "driving". */
    std::string type;
    /* The local date it started on, as a Julian day number - a Monday when
     * divisible by 7. */
    int day = 0;
    /* Seconds, pauses left out. */
    double duration = 0;
    /* Metres. */
    double distance = 0;
    /* Metres climbed; 0 while not known. */
    double climb = 0;
    /* Metres per LandCover::Kind; empty while not known. */
    std::vector<double> landCover;
};

/* One day's steps. */
struct DayRecord {
    /* The local date, as a Julian day number. */
    int day = 0;
    double steps = 0;
};

/* An occasion of a calendar category, collected. */
struct Collected {
    /* Its index in the category's occasions. */
    std::size_t occasion = 0;
    /* The local date of the first activity on it, as a Julian day number. */
    int day = 0;
};

/* How far one category has come. */
struct Progress {
    /* As the metric counts. */
    double value = 0;
    int level = 0;
    /* The day the level was reached, as a Julian day number: that of the
     * activity or the day that took it there. Meaningless at level 0. */
    int day = 0;
    /* For CalendarDays: the occasions collected, by index. */
    std::vector<Collected> collected;
};

/* The level a value comes to. */
int levelOf(const Category &category, double value);

/* Every category's progress, in catalogue order, from the activities and the
 * days in the order they happened, oldest first. */
std::vector<Progress> evaluate(const std::vector<ActivityRecord> &activities,
                               const std::vector<DayRecord> &days);

/* A calendar category's progress with the occasions collected before as
 * well: those of activities since deleted, or with the clock set
 * differently. Each occasion counts once, from the earliest day it was
 * collected on, and the level is dated by the occasion that reached it -
 * so what was collected is never lost, and the level comes with the
 * occasions themselves rather than the activities now stored. */
Progress collect(const Category &category, const Progress &progress, const std::vector<Collected> &kept);

/* The occasions of a calendar category on a local date, a Julian day
 * number: their indices, rising. */
std::vector<std::size_t> occasionsOn(const Category &category, int day);

/* What is kept of a category: the level and when it was reached, and the
 * best value ever. */
struct Award {
    int level = 0;
    int day = 0;
    double best = 0;
};

/* What is kept once the progress is known: a level is never taken away,
 * nor the best value lowered, should activities be deleted or their land
 * cover be read anew. A level that rises is dated by the progress. */
Award merge(const Award &kept, const Progress &progress);

/* Whether an activity type counts for a category. */
bool counts(const Category &category, const std::string &type);

} // namespace AchievementCatalogue

#endif // ACHIEVEMENTCATALOGUE_H
