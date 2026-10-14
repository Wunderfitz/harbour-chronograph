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

#ifndef STEPCOUNTER_H
#define STEPCOUNTER_H

#include <QElapsedTimer>
#include <QObject>

#include "StepDetector.h"

class BackgroundActivity;
class QAccelerometer;
class QFile;

/**
 * QML-facing step counter.
 *
 * Starts counting as soon as it is constructed - the app has no start button -
 * and keeps at it while the display is off: the accelerometer is asked to stay
 * on through sensorfw's standby override, and a keepalive stops the phone from
 * suspending underneath it. Both last exactly as long as this object, which is
 * as long as the app runs. The steps themselves are found by StepDetector.
 */
class StepCounter : public QObject
{
    Q_OBJECT

    Q_PROPERTY(int steps READ steps NOTIFY stepsChanged)
    Q_PROPERTY(Status status READ status NOTIFY statusChanged)

public:
    enum Status {
        /* Asked for the accelerometer, no reading yet. */
        Starting,
        /* Readings are arriving and being counted. */
        Counting,
        /* No accelerometer, or sensorfw refused it. */
        Unavailable
    };
    Q_ENUM(Status)

    explicit StepCounter(QObject *parent = nullptr);
    ~StepCounter() override;

    int steps() const;
    Status status() const;

signals:
    void stepsChanged();
    void statusChanged();

private slots:
    void handleReading();
    void handleSensorError(int error);

private:
    void start();
    void giveUp();
    void setStatus(Status status);
    int chooseDataRate() const;
    void deleteRecordings();
    void startRecording();
    void record(quint64 timestampUs, qreal x, qreal y, qreal z);

    QAccelerometer *m_sensor = nullptr;
    BackgroundActivity *m_keepAlive = nullptr;
    StepDetector m_detector;
    QElapsedTimer m_clock;
    int m_steps = 0;
    Status m_status = Starting;
    QFile *m_recording = nullptr;
    int m_recordedSamples = 0;
};

#endif // STEPCOUNTER_H
