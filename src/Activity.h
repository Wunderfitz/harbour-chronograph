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

#ifndef ACTIVITY_H
#define ACTIVITY_H

#include <QObject>
#include <QPointer>
#include <QQmlParserStatus>
#include <QRectF>
#include <QTimer>

#include "Track.h"

class ActivityHistory;
class BackgroundActivity;
class QGeoPositionInfo;
class QGeoPositionInfoSource;

/**
 * One activity, for the activity page: recorded live, or replayed.
 *
 * Created by QML, one per page. With no fileName it records: the receiver is
 * started as soon as the page is built, start(), pause(), resume() and stop()
 * follow the buttons, and stop() hands the track to the history, which stores
 * it. Destroying it while recording - the page left, or the app closed - stops
 * and stores it as well. With a fileName it replays that stored activity
 * instead, and never touches the receiver: replayPosition picks the moment.
 *
 * Either way the values - time, distance, speeds, altitude, climb, position -
 * are the Track's at one moment: now while recording, replayPosition after.
 * While recording they are refreshed with every fix and at least once a second.
 *
 * While the receiver runs, a keepalive of its own stops the phone from
 * suspending, so a recording carries on with the display off - whatever
 * StepCounter's keepalive does.
 */
class Activity : public QObject, public QQmlParserStatus
{
    Q_OBJECT
    Q_INTERFACES(QQmlParserStatus)

    /* Where a finished activity goes, and where a replayed one comes from. */
    Q_PROPERTY(ActivityHistory *history READ history WRITE setHistory NOTIFY historyChanged)
    /* The GPX file to replay, relative to the history's directory. Set it at
     * creation to replay; after a recording it names the file just stored. */
    Q_PROPERTY(QString fileName READ fileName WRITE setFileName NOTIFY fileNameChanged)
    /* "running", "walking" or "cycling". */
    Q_PROPERTY(QString type READ type WRITE setType NOTIFY typeChanged)
    Q_PROPERTY(Status status READ status NOTIFY statusChanged)

    /* The receiver now; in a replay, what it said at that moment. */
    Q_PROPERTY(SignalQuality signalQuality READ signalQuality NOTIFY positionChanged)
    Q_PROPERTY(bool hasPosition READ hasPosition NOTIFY positionChanged)
    Q_PROPERTY(double latitude READ latitude NOTIFY positionChanged)
    Q_PROPERTY(double longitude READ longitude NOTIFY positionChanged)
    /* Metres, NaN if unknown. */
    Q_PROPERTY(double accuracy READ accuracy NOTIFY positionChanged)
    /* Degrees from north while moving, NaN while standing. */
    Q_PROPERTY(double direction READ direction NOTIFY positionChanged)

    /* Seconds, metres, metres per second: see TrackState. */
    Q_PROPERTY(double elapsed READ elapsed NOTIFY valuesChanged)
    Q_PROPERTY(double distance READ distance NOTIFY valuesChanged)
    Q_PROPERTY(double speed READ speed NOTIFY valuesChanged)
    Q_PROPERTY(double averageSpeed READ averageSpeed NOTIFY valuesChanged)
    Q_PROPERTY(double altitude READ altitude NOTIFY valuesChanged)
    Q_PROPERTY(double elevationGain READ elevationGain NOTIFY valuesChanged)
    /* The track point the values are from, -1 for none. */
    Q_PROPERTY(int currentPoint READ currentPoint NOTIFY valuesChanged)

    /* The whole activity's length, once finished. */
    Q_PROPERTY(double duration READ duration NOTIFY durationChanged)
    /* The moment a replay shows, 0 to duration. Starts at the end. */
    Q_PROPERTY(double replayPosition READ replayPosition WRITE setReplayPosition NOTIFY replayPositionChanged)

public:
    enum Status {
        /* Not started yet: the receiver runs, nothing is recorded. */
        Ready,
        Recording,
        Paused,
        /* Stopped, or loaded from a file: replayed. */
        Finished
    };
    Q_ENUM(Status)

    enum SignalQuality {
        /* No receiver, or none that may be used. */
        NoPositioning,
        /* Location, or its GPS, is switched off in the settings. */
        LocationOff,
        /* No fix for a few seconds. */
        NoSignal,
        /* Too far off to record. */
        Poor,
        Fair,
        Good
    };
    Q_ENUM(SignalQuality)

    explicit Activity(QObject *parent = nullptr);
    ~Activity() override;

    void classBegin() override;
    void componentComplete() override;

    ActivityHistory *history() const;
    void setHistory(ActivityHistory *history);
    QString fileName() const;
    void setFileName(const QString &fileName);
    QString type() const;
    void setType(const QString &type);
    Status status() const;

    SignalQuality signalQuality() const;
    bool hasPosition() const;
    double latitude() const;
    double longitude() const;
    double accuracy() const;
    double direction() const;

    double elapsed() const;
    double distance() const;
    double speed() const;
    double averageSpeed() const;
    double altitude() const;
    double elevationGain() const;
    int currentPoint() const;

    double duration() const;
    double replayPosition() const;
    void setReplayPosition(double position);

    const Track &track() const;

    /* The area the track covers: x and width are longitudes, y and height
     * latitudes, from the south-west corner. Negative sizes for no track. */
    Q_INVOKABLE QRectF bounds() const;

    Q_INVOKABLE void start();
    Q_INVOKABLE void pause();
    Q_INVOKABLE void resume();
    Q_INVOKABLE void stop();

signals:
    void historyChanged();
    void fileNameChanged();
    void typeChanged();
    void statusChanged();
    void positionChanged();
    void valuesChanged();
    void durationChanged();
    void replayPositionChanged();
    /* Points were added, or the whole track loaded. */
    void trackChanged();

private slots:
    void handlePosition(const QGeoPositionInfo &info);
    void handlePositioningError(int error);
    void tick();

private:
    void startPositioning();
    void stopPositioning();
    void load();
    void save();
    void addCurrentFix(double now);
    void setStatus(Status status);
    void showLive();
    void showReplay();
    void showState(const TrackState &state);
    void showPoint(const TrackPoint &point, double speed, SignalQuality quality);

    static double now();
    static SignalQuality qualityOf(double accuracy);
    static bool locationEnabled();

    QPointer<ActivityHistory> m_history;
    QString m_fileName;
    QString m_type;
    Status m_status = Ready;
    bool m_completed = false;

    QGeoPositionInfoSource *m_source = nullptr;
    /* Keeps the phone awake while the receiver runs, display off or not. */
    BackgroundActivity *m_keepAlive = nullptr;
    bool m_positioningFailed = false;
    /* Positions from an NMEA log rather than the receiver. */
    bool m_simulated = false;
    /* Once a second while the receiver runs: the clock, and a signal gone. */
    QTimer m_tick;
    /* The latest fix, and when it came; time is 0 before the first. */
    TrackPoint m_fix;
    bool m_hasFix = false;

    Track m_track;
    double m_replayPosition = 0;

    SignalQuality m_signalQuality = NoSignal;
    bool m_hasPosition = false;
    TrackPoint m_position;
    double m_direction;
    TrackState m_state;
};

#endif // ACTIVITY_H
