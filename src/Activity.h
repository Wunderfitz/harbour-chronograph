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

#include <QDateTime>
#include <QObject>
#include <QPointer>
#include <QQmlParserStatus>
#include <QRectF>
#include <QTimer>
#include <QVariantList>

#include <cstdint>
#include <vector>

#include "AutoPause.h"
#include "LandCoverReader.h"
#include "Track.h"
#include "TrackReader.h"
#include "TrackFilter.h"

class ActivityHistory;
class BackgroundActivity;
class StepCounter;
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
 * From start to stop the history names it as the activity being recorded, so
 * that the cover can show it and pause or resume it.
 *
 * The receiver's fixes are never shown or recorded as they come: they go
 * through a TrackFilter first, set to the activity's type, and only its
 * estimates reach the map, the values and the track - so the GPX file holds
 * them too, and a replay shows what was shown while recording.
 *
 * With autoPause on, an AutoPause watches the filter's speed and, walking or
 * running, the pedometer's steps, and pauses and resumes the recording on its
 * own - a pause as of the moment the movement ended, so the points since are
 * dropped again - and stops it after half an hour paused. A pause by hand is
 * never resumed on its own.
 *
 * While the receiver runs, a keepalive of its own stops the phone from
 * suspending, so a recording carries on with the display off - whatever
 * StepCounter's keepalive does.
 *
 * Each point's land cover is read off the map's tiles - only those the map
 * has shown, from its cache: as the point is recorded, and again whenever the
 * map says it has loaded tiles, updateLandCover(), which also finds a finer
 * level for points read from a coarse one. That goes on after the stop, when
 * the map shows the whole activity, and in a replay; a stored activity's
 * file is then written anew, a few seconds after the last change or as the
 * page goes. The tiles are read in a LandCoverReader's thread, one batch at a
 * time, and landCoverChanged() comes once a batch has found something.
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
    /* "running", "walking", "cycling", "rowing" or "driving". */
    Q_PROPERTY(QString type READ type WRITE setType NOTIFY typeChanged)
    Q_PROPERTY(Status status READ status NOTIFY statusChanged)
    /* A stored activity's file is being read: it is Finished from the start,
     * but its track, values and start time come once the file is read. */
    Q_PROPERTY(bool loading READ loading NOTIFY loadingChanged)
    /* Whether the recording pauses and resumes on its own. On by default, and
     * switched at any time: switched off while paused on its own, it resumes. */
    Q_PROPERTY(bool autoPause READ autoPause WRITE setAutoPause NOTIFY autoPauseChanged)
    /* Paused on its own rather than by hand. */
    Q_PROPERTY(bool autoPaused READ autoPaused NOTIFY statusChanged)
    /* Whose steps tell auto-pause that the user walks or runs. */
    Q_PROPERTY(StepCounter *pedometer READ pedometer WRITE setPedometer NOTIFY pedometerChanged)
    /* When it was started, local time, to the second; invalid before. */
    Q_PROPERTY(QDateTime startTime READ startTime NOTIFY startTimeChanged)

    /* The receiver now; in a replay, what it said at that moment. The
     * position is the filter's estimate, the accuracy the receiver's. */
    Q_PROPERTY(SignalQuality signalQuality READ signalQuality NOTIFY positionChanged)
    Q_PROPERTY(bool hasPosition READ hasPosition NOTIFY positionChanged)
    Q_PROPERTY(double latitude READ latitude NOTIFY positionChanged)
    Q_PROPERTY(double longitude READ longitude NOTIFY positionChanged)
    /* Metres, NaN if unknown. */
    Q_PROPERTY(double accuracy READ accuracy NOTIFY positionChanged)
    /* Degrees from north while moving, NaN while standing. */
    Q_PROPERTY(double direction READ direction NOTIFY positionChanged)
    /* No position good enough to record for a while, though recording. */
    Q_PROPERTY(bool signalLost READ signalLost NOTIFY signalLostChanged)

    /* Where the receiver's last position was, the last time it ran - for the
     * map of a new activity to start at before the first fix. As it was when
     * this activity was created; hasLastPosition is false before the first. */
    Q_PROPERTY(bool hasLastPosition READ hasLastPosition CONSTANT)
    Q_PROPERTY(double lastLatitude READ lastLatitude CONSTANT)
    Q_PROPERTY(double lastLongitude READ lastLongitude CONSTANT)

    /* Seconds, metres, metres per second: see TrackState. */
    Q_PROPERTY(double elapsed READ elapsed NOTIFY valuesChanged)
    Q_PROPERTY(double distance READ distance NOTIFY valuesChanged)
    Q_PROPERTY(double speed READ speed NOTIFY valuesChanged)
    Q_PROPERTY(double averageSpeed READ averageSpeed NOTIFY valuesChanged)
    Q_PROPERTY(double altitude READ altitude NOTIFY valuesChanged)
    Q_PROPERTY(double elevationGain READ elevationGain NOTIFY valuesChanged)
    /* The track point the values are from, -1 for none. */
    Q_PROPERTY(int currentPoint READ currentPoint NOTIFY valuesChanged)
    /* The distance so far over each land cover, the most first: maps of the
     * LandCover key ("" for the land not known) and the metres. Only those
     * covered at all. */
    Q_PROPERTY(QVariantList landCover READ landCover NOTIFY landCoverChanged)
    /* The land cover at the point the values are from, a LandCover key: ""
     * where it is not known, and before the first point. */
    Q_PROPERTY(QString currentLandCover READ currentLandCover NOTIFY landCoverChanged)

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
    bool loading() const;
    bool autoPause() const;
    void setAutoPause(bool autoPause);
    bool autoPaused() const;
    StepCounter *pedometer() const;
    void setPedometer(StepCounter *pedometer);
    QDateTime startTime() const;

    SignalQuality signalQuality() const;
    bool hasPosition() const;
    double latitude() const;
    double longitude() const;
    double accuracy() const;
    double direction() const;
    bool signalLost() const;

    bool hasLastPosition() const;
    double lastLatitude() const;
    double lastLongitude() const;

    double elapsed() const;
    double distance() const;
    double speed() const;
    double averageSpeed() const;
    double altitude() const;
    double elevationGain() const;
    int currentPoint() const;
    QVariantList landCover() const;
    QString currentLandCover() const;

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

    /* Reads the land cover of every point it is not known for, or known only
     * from a coarser level than the tiles the map has now: for the map to
     * call once it has loaded tiles. In the background: the values follow
     * once it is done. */
    Q_INVOKABLE void updateLandCover();

signals:
    void historyChanged();
    void fileNameChanged();
    void typeChanged();
    void statusChanged();
    void loadingChanged();
    void autoPauseChanged();
    void pedometerChanged();
    void signalLostChanged();
    void startTimeChanged();
    void positionChanged();
    void valuesChanged();
    void durationChanged();
    void replayPositionChanged();
    /* Points were added, or the whole track loaded. */
    void trackChanged();
    void landCoverChanged();

private slots:
    void handlePosition(const QGeoPositionInfo &info);
    void handlePositioningError(int error);
    void tick();
    void handleSteps();
    void updateStepsCounted();
    void handleTrack();
    void handleLandCover();

private:
    void startPositioning();
    void stopPositioning();
    void readLastPosition();
    void writeLastPosition();
    void load();
    void save();
    void addEstimate(double now);
    void applyAutoPause(double time);
    void setStatus(Status status);
    void showLive();
    void showReplay();
    void showState(const TrackState &state);
    void showPoint(const TrackPoint &point, double speed, SignalQuality quality);
    void readLandCover(std::size_t index);
    void readLandCover();
    void rewrite();

    static double now();
    static SignalQuality qualityOf(double accuracy);
    static bool locationEnabled();
    static QString settingsPath();

    QPointer<ActivityHistory> m_history;
    QString m_fileName;
    QString m_type;
    Status m_status = Ready;
    bool m_completed = false;
    QPointer<StepCounter> m_pedometer;

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
    /* What the fixes come to: the only positions shown or recorded. */
    TrackFilter m_filter;

    bool m_autoPauseOn = true;
    bool m_autoPaused = false;
    AutoPause m_autoPause;
    /* Recording since, as far as the signal is concerned: the start, or a
     * resume by hand. */
    double m_watchedSince = 0;
    bool m_signalLost = false;

    bool m_hasLastPosition = false;
    double m_lastLatitude = 0;
    double m_lastLongitude = 0;

    Track m_track;
    /* Reads a stored activity's file. */
    TrackReader m_trackReader;
    bool m_loading = false;
    double m_replayPosition = 0;

    /* Reads the points' land cover off the map's cached tiles. */
    LandCoverReader m_landCoverReader;
    /* What it is to read once it is done with the batch it reads: all points,
     * or those recorded since. */
    bool m_landCoverOfAll = false;
    std::vector<std::size_t> m_landCoverOf;
    /* Per point, the levels whose tiles told nothing about it: see
     * LandCover::classifyFinest(). */
    std::vector<std::uint32_t> m_untelling;
    /* Writes a stored activity's file anew once its land cover has settled. */
    QTimer m_rewrite;

    SignalQuality m_signalQuality = NoSignal;
    bool m_hasPosition = false;
    TrackPoint m_position;
    double m_direction;
    TrackState m_state;
};

#endif // ACTIVITY_H
