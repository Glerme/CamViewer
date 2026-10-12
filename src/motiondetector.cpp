#include "motiondetector.h"

#include <cmath>

namespace {
// Os primeiros quadros depois de conectar costumam ter pontuação alta.
constexpr int WarmupFrames = 6;
// Quadros seguidos acima do limite para contar como movimento.
constexpr int HitsNeeded = 2;
constexpr int RestartDelayMs = 10000;
}

MotionDetector::MotionDetector(QObject *parent)
    : QObject(parent)
{
    m_process.setProcessChannelMode(QProcess::SeparateChannels);
    m_process.setStandardErrorFile(QProcess::nullDevice());
    connect(&m_process, &QProcess::readyReadStandardOutput, this, &MotionDetector::readOutput);
    connect(&m_process, &QProcess::finished, this, [this] {
        if (m_running) {
            m_restartTimer.start(RestartDelayMs);
        }
    });
    connect(&m_process, &QProcess::errorOccurred, this, [this](QProcess::ProcessError error) {
        if (m_running && error == QProcess::FailedToStart) {
            m_restartTimer.start(RestartDelayMs);
        }
    });

    m_restartTimer.setSingleShot(true);
    connect(&m_restartTimer, &QTimer::timeout, this, &MotionDetector::launch);
}

MotionDetector::~MotionDetector()
{
    stop();
}

void MotionDetector::start(const QStringList &inputArgs)
{
    if (m_running && inputArgs == m_inputArgs) {
        return;
    }
    stop();
    m_inputArgs = inputArgs;
    m_running = true;
    launch();
}

void MotionDetector::stop()
{
    m_running = false;
    m_restartTimer.stop();
    if (m_process.state() != QProcess::NotRunning) {
        m_process.kill();
        m_process.waitForFinished(2000);
    }
}

void MotionDetector::setSensitivity(int sensitivity)
{
    const double s = std::clamp(sensitivity, 1, 10);
    // 1 -> 0.04, 5 -> ~0.01, 10 -> 0.0016 (escala logarítmica)
    m_threshold = 0.04 * std::pow(0.04, (s - 1.0) / 9.0);
}

void MotionDetector::launch()
{
    if (!m_running || m_process.state() != QProcess::NotRunning) {
        return;
    }
    m_buffer.clear();
    m_framesSeen = 0;
    m_hits = 0;

    QStringList args = m_inputArgs;
    args << QStringLiteral("-an")
         << QStringLiteral("-vf")
         << QStringLiteral("fps=2,scale=320:-2,select='gte(scene,0)',metadata=print:file=-:direct=1")
         << QStringLiteral("-f") << QStringLiteral("null") << QStringLiteral("-");
    m_process.start(QStringLiteral("ffmpeg"), args);
}

void MotionDetector::readOutput()
{
    m_buffer += m_process.readAllStandardOutput();

    static const QByteArray key = QByteArrayLiteral("lavfi.scene_score=");
    qsizetype newline;
    while ((newline = m_buffer.indexOf('\n')) >= 0) {
        const QByteArray line = m_buffer.left(newline).trimmed();
        m_buffer.remove(0, newline + 1);

        if (!line.startsWith(key)) {
            continue;
        }
        const double score = line.mid(key.size()).toDouble();
        if (++m_framesSeen <= WarmupFrames) {
            continue;
        }
        if (score >= m_threshold) {
            // O intervalo mínimo entre alertas fica por conta de quem escuta.
            if (++m_hits >= HitsNeeded) {
                Q_EMIT motion(score);
            }
        } else {
            m_hits = 0;
        }
    }
}
