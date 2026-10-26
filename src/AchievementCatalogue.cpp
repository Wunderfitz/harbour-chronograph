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

namespace AchievementCatalogue {

namespace {

constexpr double km(double kilometres) { return kilometres * 1000; }
/* Minutes and seconds per kilometre, as metres per second. */
constexpr double pace(int minutes, int seconds) { return 1000.0 / (minutes * 60 + seconds); }
constexpr double kmh(double kilometresPerHour) { return kilometresPerHour / 3.6; }
constexpr double minutes(double count) { return count * 60; }
constexpr double hours(double count) { return count * 3600; }

/* None tuned on anyone's activities. The first level comes with a first
 * try, or nearly; the last is out of reach for most: a 100 km run, 3:20 per
 * km over 5 km, round the world by bike, eleven Everests. The distances of
 * a single activity stop short of the half and the full marathon's 21.1
 * and 42.2 km, as a receiver's distance may fall a little short of a
 * course's. */
const Category kCategories[] = {
    { "running-longest", "running", LongestDistance, "running", 0, 0, "distance",
      "activity-running", "", { km(1), km(3), km(5), km(10), km(21), km(42), km(100) } },
    { "running-distance", "running", TotalDistance, "running", 0, 0, "distance",
      "activity-running", "total", { km(5), km(25), km(100), km(500), km(1000), km(5000), km(10000) } },
    { "running-pace", "running", FastestSpeed, "running", 0, km(5), "pace",
      "activity-running", "fastest",
      { pace(8, 0), pace(7, 0), pace(6, 0), pace(5, 20), pace(4, 40), pace(4, 0), pace(3, 20) } },

    { "walking-longest", "walking", LongestDistance, "walking", 0, 0, "distance",
      "activity-walking", "", { km(1), km(3), km(5), km(10), km(20), km(42), km(100) } },
    { "walking-distance", "walking", TotalDistance, "walking", 0, 0, "distance",
      "activity-walking", "total", { km(5), km(25), km(100), km(500), km(1000), km(5000), km(10000) } },
    { "walking-pace", "walking", FastestSpeed, "walking", 0, km(3), "pace",
      "activity-walking", "fastest",
      { pace(15, 0), pace(13, 0), pace(12, 0), pace(11, 0), pace(10, 0), pace(9, 0), pace(8, 0) } },

    { "cycling-longest", "cycling", LongestDistance, "cycling", 0, 0, "distance",
      "activity-cycling", "", { km(5), km(10), km(25), km(50), km(100), km(200), km(400) } },
    { "cycling-distance", "cycling", TotalDistance, "cycling", 0, 0, "distance",
      "activity-cycling", "total", { km(25), km(100), km(500), km(2500), km(10000), km(25000), km(40000) } },
    { "cycling-speed", "cycling", FastestSpeed, "cycling", 0, km(10), "speed",
      "activity-cycling", "fastest", { kmh(12), kmh(16), kmh(20), kmh(25), kmh(30), kmh(35), kmh(40) } },

    { "rowing-longest", "rowing", LongestDistance, "rowing", 0, 0, "distance",
      "activity-rowing", "", { km(1), km(2), km(5), km(10), km(21), km(42), km(100) } },
    { "rowing-distance", "rowing", TotalDistance, "rowing", 0, 0, "distance",
      "activity-rowing", "total", { km(5), km(25), km(100), km(250), km(1000), km(2500), km(5000) } },
    { "rowing-speed", "rowing", FastestSpeed, "rowing", 0, km(2), "speed",
      "activity-rowing", "fastest", { kmh(4), kmh(6), kmh(8), kmh(10), kmh(12), kmh(14), kmh(16) } },

    { "climb-single", "climbing", BiggestClimb, nullptr, 0, 0, "climb",
      "achievement-mountain", "", { 25, 50, 100, 250, 500, 1000, 2000 } },
    { "climb-total", "climbing", TotalClimb, nullptr, 0, 0, "climb",
      "achievement-mountain", "total", { 50, 250, 1000, 2500, 8849, 25000, 100000 } },

    { "terrain-forest", "terrain", LandCoverDistance, nullptr, LandCover::Forest, 0, "distance",
      "achievement-forest", "", { km(1), km(5), km(25), km(100), km(250), km(1000), km(2500) } },
    { "terrain-grassland", "terrain", LandCoverDistance, nullptr, LandCover::Grassland, 0, "distance",
      "achievement-meadow", "", { km(1), km(5), km(25), km(100), km(250), km(1000), km(2500) } },
    { "terrain-farmland", "terrain", LandCoverDistance, nullptr, LandCover::Farmland, 0, "distance",
      "achievement-field", "", { km(1), km(5), km(25), km(100), km(250), km(1000), km(2500) } },
    { "terrain-natural", "terrain", LandCoverDistance, nullptr, LandCover::Natural, 0, "distance",
      "achievement-heath", "", { km(1), km(5), km(25), km(100), km(250), km(1000), km(2500) } },
    { "terrain-water", "terrain", LandCoverDistance, nullptr, LandCover::Water, 0, "distance",
      "achievement-water", "", { km(1), km(5), km(25), km(100), km(250), km(1000), km(2500) } },
    { "terrain-builtup", "terrain", LandCoverDistance, nullptr, LandCover::BuiltUp, 0, "distance",
      "achievement-town", "", { km(1), km(5), km(25), km(100), km(250), km(1000), km(2500) } },

    { "duration-single", "endurance", LongestDuration, nullptr, 0, 0, "duration",
      "achievement-stopwatch", "",
      { minutes(15), minutes(30), hours(1), hours(2), hours(4), hours(8), hours(24) } },
    { "duration-total", "endurance", TotalDuration, nullptr, 0, 0, "duration",
      "achievement-stopwatch", "total",
      { hours(1), hours(10), hours(50), hours(150), hours(500), hours(1500), hours(5000) } },

    { "activities-count", "regularity", ActivityCount, nullptr, 0, 0, "count",
      "achievement-flag", "", { 1, 10, 25, 100, 250, 500, 1000 } },
    { "streak-weeks", "regularity", WeekStreak, nullptr, 0, 0, "weeks",
      "achievement-calendar", "", { 2, 4, 8, 13, 26, 52, 104 } },

    { "steps-day", "steps", MostDailySteps, nullptr, 0, 0, "steps",
      "achievement-steps", "", { 2500, 5000, 7500, 10000, 15000, 25000, 40000 } },
    { "steps-total", "steps", TotalSteps, nullptr, 0, 0, "steps",
      "achievement-steps", "total", { 1e4, 1e5, 5e5, 1e6, 2.5e6, 5e6, 1e7 } },
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
    for (std::size_t i = 0; i < kCategoryCount; ++i) {
        const Category &category = kCategories[i];
        Progress &result = progress[i];

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
