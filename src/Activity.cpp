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

#include "Activity.h"

#include "ActivityHistory.h"
#include "Gpx.h"

#include <QDateTime>
#include <QDebug>
#include <QFile>
#include <QFileInfo>
#include <QGeoPositionInfo>
#include <QGeoPositionInfoSource>
#include <QNmeaPositionInfoSource>
#include <QSettings>

#include <keepalive/backgroundactivity.h>

#include <cmath>

namespace {

/* What the receiver is asked for: a fix a second, the most a phone's gives. */
const int kUpdateIntervalMs = 1000;

/* The signal counts as gone after this many seconds without a fix. */
const double kSignalLostAfter = 5.0;

/* A fix whose own time is older than this is the receiver's last known
 * position from before, which some hand out on start: not a fix at all. */
const double kStaleFixAfter = 10.0;

/* Horizontal accuracy, in metres, up to which a fix is good, and fair. Fixes
 * worse than fair go nowhere: not into the filter, the map or the track. Set
 * generously, so that the filter settles even under a fair sky. */
const double kGoodAccuracy = 15.0;
const double kFairAccuracy = 40.0;

/* Set this to an NMEA log to have the activity page replay it in place of the
 * receiver - for the emulator, which has none, and for trying the page indoors.
 * Off unless asked for; the launcher never sets it. */
const char kNmeaVariable[] = "CHRONOGRAPH_NMEA";

/* What an NMEA log's dilution of precision is multiplied by to give metres. */
const double kNmeaRangeError = 5.0;

/* Sailfish OS keeps its location switches here; the Location permission lets
 * the sandbox read it. */
const char kLocationSettings[] = "/etc/location/location.conf";

const char kDefaultType[] = "running";

bool isType(const QString &type)
{
    return type == QLatin1String("running") || type == QLatin1String("walking")
            || type == QLatin1String("cycling") || type == QLatin1String("rowing")
            || type == QLatin1String("driving");
}

/* How the filter expects each type to move. */
TrackFilter::Mode filterMode(const QString &type)
{
    if (type == QLatin1String("walking"))
        return TrackFilter::Walking;
    if (type == QLatin1String("cycling"))
        return TrackFilter::Cycling;
    if (type == QLatin1String("rowing"))
        return TrackFilter::Rowing;
    if (type == QLatin1String("driving"))
        return TrackFilter::Driving;
    return TrackFilter::Running;
}

} // namespace

Activity::Activity(QObject *parent)
    : QObject(parent)
    , m_type(QString::fromLatin1(kDefaultType))
    , m_direction(std::nan(""))
{
    m_tick.setInterval(1000);
    connect(&m_tick, &QTimer::timeout, this, &Activity::tick);
    m_filter.setMode(filterMode(m_type));
}

/* The page left or the app closed while recording: the activity is stored all
 * the same. Quietly, as the page that shows it is going too. */
Activity::~Activity()
{
    blockSignals(true);
    stop();
    stopPositioning();
}

void Activity::classBegin()
{
}

/* Only now are history and fileName known, which decide between recording and
 * replaying. */
void Activity::componentComplete()
{
    m_completed = true;
    if (m_fileName.isEmpty())
        startPositioning();
    else
        load();
}

ActivityHistory *Activity::history() const
{
    return m_history;
}

void Activity::setHistory(ActivityHistory *history)
{
    if (m_history == history)
        return;
    m_history = history;
    emit historyChanged();
}

QString Activity::fileName() const
{
    return m_fileName;
}

/* Only at creation: an activity does not switch to another one. */
void Activity::setFileName(const QString &fileName)
{
    if (m_completed || m_fileName == fileName)
        return;
    m_fileName = fileName;
    emit fileNameChanged();
}

QString Activity::type() const
{
    return m_type;
}

/* Until it is stopped: what is stored is not changed after the fact. */
void Activity::setType(const QString &type)
{
    if (m_status == Finished || m_type == type || !isType(type))
        return;
    m_type = type;
    m_filter.setMode(filterMode(type));
    emit typeChanged();
}

Activity::Status Activity::status() const
{
    return m_status;
}

Activity::SignalQuality Activity::signalQuality() const
{
    return m_signalQuality;
}

bool Activity::hasPosition() const
{
    return m_hasPosition;
}

double Activity::latitude() const
{
    return m_position.latitude;
}

double Activity::longitude() const
{
    return m_position.longitude;
}

double Activity::accuracy() const
{
    return m_position.accuracy;
}

double Activity::direction() const
{
    return m_direction;
}

double Activity::elapsed() const
{
    return m_state.elapsed;
}

double Activity::distance() const
{
    return m_state.distance;
}

double Activity::speed() const
{
    return m_state.speed;
}

double Activity::averageSpeed() const
{
    return m_state.averageSpeed;
}

double Activity::altitude() const
{
    return m_state.altitude;
}

double Activity::elevationGain() const
{
    return m_state.elevationGain;
}

int Activity::currentPoint() const
{
    return m_state.point;
}

double Activity::duration() const
{
    return m_status == Finished ? m_track.duration() : 0;
}

double Activity::replayPosition() const
{
    return m_replayPosition;
}

void Activity::setReplayPosition(double position)
{
    if (m_status != Finished)
        return;
    position = qBound(0.0, position, m_track.duration());
    if (position == m_replayPosition)
        return;
    m_replayPosition = position;
    emit replayPositionChanged();
    showReplay();
}

const Track &Activity::track() const
{
    return m_track;
}

QRectF Activity::bounds() const
{
    if (m_track.isEmpty())
        return QRectF(0, 0, -1, -1);
    double south = m_track.point(0).latitude;
    double north = south;
    double west = m_track.point(0).longitude;
    double east = west;
    for (std::size_t i = 1; i < m_track.size(); ++i) {
        const TrackPoint &point = m_track.point(i);
        south = qMin(south, point.latitude);
        north = qMax(north, point.latitude);
        west = qMin(west, point.longitude);
        east = qMax(east, point.longitude);
    }
    return QRectF(west, south, east - west, north - south);
}

void Activity::start()
{
    if (m_status != Ready)
        return;
    const double time = now();
    m_track.begin(time);
    setStatus(Recording);
    addEstimate(time);
    showLive();
}

void Activity::pause()
{
    if (m_status != Recording)
        return;
    m_track.end(now());
    setStatus(Paused);
    showLive();
}

void Activity::resume()
{
    if (m_status != Paused)
        return;
    const double time = now();
    m_track.begin(time);
    setStatus(Recording);
    addEstimate(time);
    showLive();
}

void Activity::stop()
{
    if (m_status != Recording && m_status != Paused)
        return;
    m_track.end(now());
    stopPositioning();
    save();
    setStatus(Finished);
    emit durationChanged();
    m_replayPosition = m_track.duration();
    emit replayPositionChanged();
    showReplay();
}

void Activity::handlePosition(const QGeoPositionInfo &info)
{
    if (!info.isValid() || !info.coordinate().isValid())
        return;
    const double time = now();
    // An NMEA log keeps the times it was made at.
    if (!m_simulated && info.timestamp().isValid()
            && info.timestamp().toMSecsSinceEpoch() / 1000.0 < time - kStaleFixAfter)
        return;

    /* Timed by the phone's clock rather than the fix's own: the segments begin
     * and end by it, and a fix must not seem to come before its segment. The
     * receiver's delay is well under a second. */
    TrackPoint fix;
    fix.time = time;
    fix.latitude = info.coordinate().latitude();
    fix.longitude = info.coordinate().longitude();
    if (info.coordinate().type() == QGeoCoordinate::Coordinate3D)
        fix.altitude = info.coordinate().altitude();
    if (info.hasAttribute(QGeoPositionInfo::HorizontalAccuracy))
        fix.accuracy = info.attribute(QGeoPositionInfo::HorizontalAccuracy);
    if (info.hasAttribute(QGeoPositionInfo::VerticalAccuracy))
        fix.verticalAccuracy = info.attribute(QGeoPositionInfo::VerticalAccuracy);
    if (info.hasAttribute(QGeoPositionInfo::GroundSpeed))
        fix.speed = info.attribute(QGeoPositionInfo::GroundSpeed);
    if (info.hasAttribute(QGeoPositionInfo::Direction))
        fix.course = info.attribute(QGeoPositionInfo::Direction);
    m_fix = fix;
    m_hasFix = true;

    // Poor fixes are left out altogether; the filter weighs the others.
    const bool estimated = qualityOf(fix.accuracy) >= Fair && m_filter.add(fix);
    if (estimated && m_status == Recording)
        addEstimate(time);
    showLive();
}

void Activity::handlePositioningError(int error)
{
    qWarning() << "Positioning error" << error;
    if (error == QGeoPositionInfoSource::AccessError || error == QGeoPositionInfoSource::ClosedError) {
        m_positioningFailed = true;
        showLive();
    }
}

void Activity::tick()
{
    if (m_hasFix && now() - m_fix.time > kSignalLostAfter)
        m_hasFix = false;
    showLive();
}

/* Location is not asked for before the page is there, and given up the moment
 * the activity is stopped. A replay never asks. */
void Activity::startPositioning()
{
    const QString nmeaLog = QString::fromLocal8Bit(qgetenv(kNmeaVariable));
    if (!nmeaLog.isEmpty()) {
        QFile *file = new QFile(nmeaLog, this);
        if (file->open(QIODevice::ReadOnly)) {
            QNmeaPositionInfoSource *source =
                    new QNmeaPositionInfoSource(QNmeaPositionInfoSource::SimulationMode, this);
            source->setDevice(file);
            source->setUserEquivalentRangeError(kNmeaRangeError);
            m_source = source;
            m_simulated = true;
            qInfo() << "Positions from" << nmeaLog;
        } else {
            qWarning() << "Cannot read" << nmeaLog << file->errorString();
        }
    }
    if (!m_source)
        m_source = QGeoPositionInfoSource::createDefaultSource(this);
    if (!m_source) {
        qWarning() << "No positioning source available";
        m_positioningFailed = true;
        showLive();
        return;
    }

    m_source->setPreferredPositioningMethods(QGeoPositionInfoSource::SatellitePositioningMethods);
    m_source->setUpdateInterval(kUpdateIntervalMs);
    connect(m_source, &QGeoPositionInfoSource::positionUpdated, this, &Activity::handlePosition);
    // error() is a getter as well as a signal.
    connect(m_source,
            static_cast<void (QGeoPositionInfoSource::*)(QGeoPositionInfoSource::Error)>(
                &QGeoPositionInfoSource::error),
            this, [this](QGeoPositionInfoSource::Error error) { handlePositioningError(error); });
    m_source->startUpdates();
    m_tick.start();
    m_keepAlive = new BackgroundActivity(this);
    m_keepAlive->run();
    showLive();
}

void Activity::stopPositioning()
{
    m_tick.stop();
    if (m_keepAlive) {
        m_keepAlive->stop();
        delete m_keepAlive;
        m_keepAlive = nullptr;
    }
    if (m_source) {
        m_source->stopUpdates();
        delete m_source;
        m_source = nullptr;
    }
    m_hasFix = false;
}

void Activity::load()
{
    const QString path = m_history ? m_history->filePath(m_fileName) : m_fileName;
    QString type;
    if (!Gpx::read(path, &m_track, &type))
        qWarning() << "Cannot replay" << path;
    if (isType(type) && type != m_type) {
        m_type = type;
        emit typeChanged();
    }
    setStatus(Finished);
    emit durationChanged();
    emit trackChanged();
    m_replayPosition = m_track.duration();
    emit replayPositionChanged();
    showReplay();
}

/* An activity in which no fix was good enough to record has nothing to replay,
 * and is not stored. */
void Activity::save()
{
    if (m_track.isEmpty()) {
        qInfo() << "Nothing recorded, so nothing stored";
        return;
    }
    if (!m_history) {
        qWarning() << "No history to store the activity in";
        return;
    }
    const QString fileName = m_history->add(m_type, m_track);
    if (fileName.isEmpty())
        return;
    m_fileName = fileName;
    emit fileNameChanged();
}

/* The filter's latest estimate becomes a track point, if it is fresh. Called
 * for every fix the filter takes while recording, and on start and resume
 * with the estimate in hand, so the track does not wait a second for its first
 * point. Outliers and poor fixes leave a gap of a second or so, which the
 * track bridges with a straight line. */
void Activity::addEstimate(double time)
{
    if (!m_filter.hasEstimate() || time - m_filter.estimate().time > kSignalLostAfter)
        return;
    TrackPoint point = m_filter.estimate();
    point.time = time;
    if (m_track.addPoint(point))
        emit trackChanged();
}

void Activity::setStatus(Status status)
{
    if (m_status == status)
        return;
    m_status = status;
    // For the cover, which shows the activity while it is recorded. Cleared
    // in the destructor too, which stops a recording.
    if (m_history) {
        if (status == Recording || status == Paused)
            m_history->setRecording(this);
        else if (m_history->recording() == this)
            m_history->setRecording(nullptr);
    }
    emit statusChanged();
}

/* While the receiver runs: the values as of now, the position as of the
 * latest estimate. Until the filter has one, there is no position, however
 * many fixes have come. */
void Activity::showLive()
{
    if (m_status == Finished)
        return;
    const double time = now();
    const double elapsed = m_status == Recording ? m_track.elapsedAt(time) : m_track.duration();
    TrackState state = m_track.stateAt(elapsed);
    if (m_status != Recording)
        state.speed = 0;
    const bool hasEstimate = m_filter.hasEstimate();
    const TrackPoint &estimate = m_filter.estimate();
    // Before the track has an altitude, the filter's.
    if (std::isnan(state.altitude) && hasEstimate)
        state.altitude = estimate.altitude;
    showState(state);

    // Fixes coming in settle it, whatever the switches say: an NMEA log, say.
    SignalQuality quality = NoSignal;
    if (m_hasFix)
        quality = qualityOf(m_fix.accuracy);
    else if (m_positioningFailed)
        quality = NoPositioning;
    else if (!locationEnabled())
        quality = LocationOff;
    if (!hasEstimate) {
        showPoint(m_position, 0, quality);
        return;
    }
    // The last position stays on the map when the signal goes, at a standstill.
    m_hasPosition = true;
    TrackPoint point = estimate;
    if (m_hasFix)
        point.accuracy = m_fix.accuracy;
    const bool fresh = time - estimate.time <= kSignalLostAfter;
    showPoint(point, fresh ? estimate.speed : 0, quality);
}

/* Everything as it was at replayPosition. Before the first point, the map
 * shows where the activity started. */
void Activity::showReplay()
{
    const TrackState state = m_track.stateAt(m_replayPosition);
    showState(state);
    if (m_track.isEmpty()) {
        m_hasPosition = false;
        showPoint(m_position, 0, NoSignal);
        return;
    }
    const std::size_t index = state.point < 0 ? 0 : std::size_t(state.point);
    const TrackPoint &point = m_track.point(index);
    m_hasPosition = true;
    showPoint(point, state.speed, qualityOf(point.accuracy));
}

void Activity::showState(const TrackState &state)
{
    m_state = state;
    emit valuesChanged();
}

void Activity::showPoint(const TrackPoint &point, double speed, SignalQuality quality)
{
    m_position = point;
    m_signalQuality = quality;
    m_direction = !std::isnan(speed) && speed >= Track::kMovingSpeed ? point.course : std::nan("");
    emit positionChanged();
}

double Activity::now()
{
    return QDateTime::currentMSecsSinceEpoch() / 1000.0;
}

/* A fix that does not say how accurate it is counts as fair: recorded, but not
 * claimed to be good. */
Activity::SignalQuality Activity::qualityOf(double accuracy)
{
    if (std::isnan(accuracy))
        return Fair;
    if (accuracy <= kGoodAccuracy)
        return Good;
    if (accuracy <= kFairAccuracy)
        return Fair;
    return Poor;
}

/* Both the main switch and GPS. Where the file cannot be read, positioning is
 * left to say for itself whether it works. */
bool Activity::locationEnabled()
{
    if (!QFileInfo::exists(QString::fromLatin1(kLocationSettings)))
        return true;
    QSettings settings(QString::fromLatin1(kLocationSettings), QSettings::IniFormat);
    return settings.value(QStringLiteral("location/enabled"), true).toBool()
            && settings.value(QStringLiteral("location/gps/enabled"), true).toBool();
}
