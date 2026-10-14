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

#include "StepCounter.h"

int main(int argc, char *argv[])
{
    QScopedPointer<QGuiApplication> app(SailfishApp::application(argc, argv));
    /* The same pair as the [X-Sailjail] section of the .desktop file. Sailjail
     * only lets the app write below ~/.local/share/<organization>/<application>
     * and its siblings, which is where QStandardPaths will point once there is
     * something to persist. */
    app->setOrganizationName(QStringLiteral("de.ygriega"));
    app->setApplicationName(QStringLiteral("chronograph"));

    /* Registered only so QML can name the status values (StepCounter.Counting
     * and friends); the instance itself comes in as a context property. */
    qmlRegisterUncreatableType<StepCounter>(
        "de.ygriega.chronograph", 1, 0, "StepCounter",
        QStringLiteral("StepCounter is exposed as the 'stepCounter' context property"));

    /* Constructed before the view, so counting starts with the app rather than
     * with whichever page first looks at it. */
    StepCounter stepCounter;

    QScopedPointer<QQuickView> view(SailfishApp::createView());
    view->rootContext()->setContextProperty(QStringLiteral("stepCounter"), &stepCounter);
    view->setSource(SailfishApp::pathToMainQml());
    view->show();

    return app->exec();
}
