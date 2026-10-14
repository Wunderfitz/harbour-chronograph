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

#include "Database.h"
#include "StepCounter.h"
#include "StepHistory.h"

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

    /* All three constructed before the view, so counting starts with the app
     * rather than with whichever page first looks at it - and destroyed after
     * the event loop ends, in reverse, which is when StepHistory makes its last
     * save. */
    Database database(Database::defaultFilePath());
    StepHistory stepHistory(&database);
    StepCounter stepCounter;
    QObject::connect(&stepCounter, &StepCounter::stepsAdded,
                     &stepHistory, &StepHistory::addSteps);

    QScopedPointer<QQuickView> view(SailfishApp::createView());
    view->rootContext()->setContextProperty(QStringLiteral("stepCounter"), &stepCounter);
    view->rootContext()->setContextProperty(QStringLiteral("stepHistory"), &stepHistory);
    view->setSource(SailfishApp::pathToMainQml());
    view->show();

    return app->exec();
}
