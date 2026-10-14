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

#include "StepCounter.h"

#include <QAccelerometer>
#include <QAccelerometerReading>
#include <QDateTime>
#include <QDebug>
#include <QDir>
#include <QFile>
#include <QStandardPaths>

#include <keepalive/backgroundactivity.h>

#include <cstdlib>

namespace {

/* What the accelerometer is asked for. StepDetector counts the same at 20, 30
 * and 100 Hz in its tests; 30 leaves headroom over a running cadence of about
 * 3 Hz while waking the phone a third as often as 100 would. */
const int kPreferredDataRateHz = 30;

/* Set this in the environment to have every raw sample written to a CSV file
 * below the app's data directory - the material for tuning StepDetector against
 * real walks rather than synthetic ones. Off unless asked for. */
const char kRecordVariable[] = "CHRONOGRAPH_RECORD";

/* A recording is flushed this often, in samples, so a walk survives the app
 * being killed rather than closed. About a second at the preferred rate. */
const int kRecordFlushInterval = 30;

/* What a recording is called, and the pattern that finds all of them. */
const char kRecordingNameFormat[] = "recording-%1.csv";
const char kRecordingNamePattern[] = "recording-*.csv";

QString recordingDirectory()
{
    return QStandardPaths::writableLocation(QStandardPaths::AppDataLocation);
}

} // namespace

StepCounter::StepCounter(QObject *parent)
    : QObject(parent)
    , m_sensor(new QAccelerometer(this))
    , m_keepAlive(new BackgroundActivity(this))
{
    m_clock.start();

    connect(m_sensor, &QSensor::readingChanged, this, &StepCounter::handleReading);
    connect(m_sensor, &QSensor::sensorError, this, &StepCounter::handleSensorError);

    deleteRecordings();
    if (qEnvironmentVariableIsSet(kRecordVariable))
        startRecording();

    start();
}

StepCounter::~StepCounter()
{
    m_sensor->stop();
    m_keepAlive->stop();
    if (m_recording)
        m_recording->flush();
}

int StepCounter::steps() const
{
    return m_steps;
}

StepCounter::Status StepCounter::status() const
{
    return m_status;
}

void StepCounter::handleReading()
{
    const QAccelerometerReading *reading = m_sensor->reading();
    if (!reading)
        return;

    setStatus(Counting);

    /* sensorfw stamps its readings in microseconds, which also keeps samples it
     * delivers in a batch apart. A backend that leaves the stamp empty gets the
     * time of arrival instead. */
    quint64 timestampUs = reading->timestamp();
    if (timestampUs == 0)
        timestampUs = quint64(m_clock.nsecsElapsed() / 1000);

    const int added = m_detector.addSample(timestampUs, reading->x(), reading->y(), reading->z());
    if (added > 0) {
        m_steps += added;
        emit stepsChanged();
    }

    if (m_recording)
        record(timestampUs, reading->x(), reading->y(), reading->z());
}

void StepCounter::handleSensorError(int error)
{
    qWarning() << "Accelerometer reported error" << error;
    // Not every error stops the sensor; only give up on one that did.
    if (!m_sensor->isActive())
        giveUp();
}

void StepCounter::start()
{
    if (!m_sensor->connectToBackend()) {
        qWarning() << "No accelerometer backend available";
        giveUp();
        return;
    }

    /* The standby override: without it sensorfw stops delivering the moment the
     * display goes off, which while walking is nearly always. */
    m_sensor->setAlwaysOn(true);

    const int dataRate = chooseDataRate();
    if (dataRate > 0)
        m_sensor->setDataRate(dataRate);
    qInfo() << "Accelerometer" << m_sensor->identifier() << "at" << dataRate
            << "Hz; offers" << m_sensor->availableDataRates();

    if (!m_sensor->start()) {
        qWarning() << "Accelerometer did not start";
        giveUp();
        return;
    }

    /* The sensor staying on is no use if the CPU that reads it suspends. This is
     * what costs the battery, and it is held for as long as the app runs. */
    m_keepAlive->run();
}

void StepCounter::giveUp()
{
    m_sensor->stop();
    m_keepAlive->stop();
    setStatus(Unavailable);
}

void StepCounter::setStatus(Status status)
{
    if (m_status == status)
        return;
    m_status = status;
    emit statusChanged();
}

/* The preferred rate if the sensor offers it, otherwise the closest one it does.
 * 0 leaves the choice to the backend, for a sensor that does not say. */
int StepCounter::chooseDataRate() const
{
    const qrangelist ranges = m_sensor->availableDataRates();
    int best = 0;
    for (const qrange &range : ranges) {
        const int candidate = qBound(range.first, kPreferredDataRateHz, range.second);
        if (best == 0 || std::abs(candidate - kPreferredDataRateHz)
                < std::abs(best - kPreferredDataRateHz))
            best = candidate;
    }
    return best;
}

/* Recordings are for debugging there and then: whatever is worth keeping is
 * copied off the phone (to tests/raw-data/, which git ignores). Every start
 * clears out those of earlier runs, recording or not, so that a few megabytes a
 * walk do not pile up on the device. */
void StepCounter::deleteRecordings()
{
    QDir directory(recordingDirectory());
    const QStringList names = directory.entryList(
                QStringList() << QString::fromLatin1(kRecordingNamePattern), QDir::Files);
    for (const QString &name : names) {
        if (directory.remove(name))
            qInfo() << "Deleted earlier recording" << directory.filePath(name);
        else
            qWarning() << "Cannot delete earlier recording" << directory.filePath(name);
    }
}

void StepCounter::startRecording()
{
    const QString directory = recordingDirectory();
    QDir().mkpath(directory);
    const QString name = QString::fromLatin1(kRecordingNameFormat)
            .arg(QDateTime::currentDateTime().toString(QStringLiteral("yyyyMMdd-HHmmss")));

    m_recording = new QFile(QDir(directory).filePath(name), this);
    if (!m_recording->open(QIODevice::WriteOnly | QIODevice::Text)) {
        qWarning() << "Cannot record to" << m_recording->fileName() << m_recording->errorString();
        delete m_recording;
        m_recording = nullptr;
        return;
    }
    m_recording->write("timestamp_us,x,y,z,steps\n");
    qInfo() << "Recording raw accelerometer samples to" << m_recording->fileName();
}

/* One line per sample, with the count as it stood after it, so a replay can be
 * checked against what the app showed at the time. */
void StepCounter::record(quint64 timestampUs, qreal x, qreal y, qreal z)
{
    QByteArray line = QByteArray::number(timestampUs);
    line += ',' + QByteArray::number(x, 'f', 4);
    line += ',' + QByteArray::number(y, 'f', 4);
    line += ',' + QByteArray::number(z, 'f', 4);
    line += ',' + QByteArray::number(m_steps) + '\n';
    m_recording->write(line);

    if (++m_recordedSamples % kRecordFlushInterval == 0)
        m_recording->flush();
}
