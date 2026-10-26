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

#include <QGuiApplication>
#include <QQmlContext>
#include <QQmlEngine>
#include <QQuickView>
#include <QScopedPointer>

#include <sailfishapp.h>

#include "Achievements.h"
#include "Activity.h"
#include "ActivityHistory.h"
#include "ActivityStatistics.h"
#include "ChartLine.h"
#include "Database.h"
#include "StepCounter.h"
#include "StepHistory.h"
#include "TileNetworkFactory.h"
#include "TrackLine.h"

int main(int argc, char *argv[])
{
    QScopedPointer<QGuiApplication> app(SailfishApp::application(argc, argv));
    /* The same pair as the [X-Sailjail] section of the .desktop file. Sailjail
     * only lets the app write below ~/.local/share/<organization>/<application>
     * and its siblings, which is where QStandardPaths points the database. */
    app->setOrganizationName(QStringLiteral("de.ygriega"));
    app->setApplicationName(QStringLiteral("chronograph"));

    /* Registered only so QML can name the status values (StepCounter.Counting
     * and friends); the instance itself comes in as a context property. */
    qmlRegisterUncreatableType<StepCounter>(
        "de.ygriega.chronograph", 1, 0, "StepCounter",
        QStringLiteral("StepCounter is exposed as the 'stepCounter' context property"));
    qmlRegisterUncreatableType<ActivityHistory>(
        "de.ygriega.chronograph", 1, 0, "ActivityHistory",
        QStringLiteral("ActivityHistory is exposed as the 'activityHistory' context property"));
    // One per activity page, which decides whether it records or replays.
    qmlRegisterType<Activity>("de.ygriega.chronograph", 1, 0, "Activity");
    qmlRegisterType<TrackLine>("de.ygriega.chronograph", 1, 0, "TrackLine");
    qmlRegisterType<ChartLine>("de.ygriega.chronograph", 1, 0, "ChartLine");

    /* All constructed before the view, so counting starts with the app rather
     * than with whichever page first looks at it - and destroyed after the view,
     * in reverse: the view takes an activity page with it, whose Activity
     * stores a recording still going in activityHistory, and StepHistory makes
     * its last save after that. */
    Database database(Database::defaultFilePath());
    StepHistory stepHistory(&database);
    StepCounter stepCounter;
    QObject::connect(&stepCounter, &StepCounter::stepsAdded,
                     &stepHistory, &StepHistory::addSteps);
    ActivityHistory activityHistory(&database, ActivityHistory::defaultDirectory());
    // Worked out whenever an activity is stored, deleted or changed, and only
    // read by the statistics page.
    ActivityStatistics activityStatistics(&database);
    QObject::connect(&activityHistory, &ActivityHistory::activitiesChanged,
                     &activityStatistics, &ActivityStatistics::update);
    /* Likewise, and as the steps are saved; once as well when the app has
     * started, should it bring categories new to the activities and the
     * steps stored before. */
    Achievements achievements(&database);
    QObject::connect(&activityHistory, &ActivityHistory::activitiesChanged,
                     &achievements, &Achievements::update);
    QObject::connect(&stepHistory, &StepHistory::saved, &achievements, &Achievements::update);
    QMetaObject::invokeMethod(&achievements, "update", Qt::QueuedConnection);
    TileNetworkFactory tileNetworkFactory;

    QScopedPointer<QQuickView> view(SailfishApp::createView());
    // Before anything is loaded: the engine asks for it once, on first use.
    view->engine()->setNetworkAccessManagerFactory(&tileNetworkFactory);
    view->rootContext()->setContextProperty(QStringLiteral("stepCounter"), &stepCounter);
    view->rootContext()->setContextProperty(QStringLiteral("stepHistory"), &stepHistory);
    view->rootContext()->setContextProperty(QStringLiteral("activityHistory"), &activityHistory);
    view->rootContext()->setContextProperty(QStringLiteral("activityStatistics"), &activityStatistics);
    view->rootContext()->setContextProperty(QStringLiteral("achievements"), &achievements);
    view->setSource(SailfishApp::pathToMainQml());
    view->show();

    return app->exec();
}
