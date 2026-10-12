#pragma once

#include <QObject>
#include <QProcess>
#include <QTimer>

// Detecta movimento rodando um ffmpeg leve (2 fps, 320px) e lendo a pontuação
// de diferença entre quadros ("scene score") que ele imprime.
class MotionDetector : public QObject
{
    Q_OBJECT

public:
    explicit MotionDetector(QObject *parent = nullptr);
    ~MotionDetector() override;

    void start(const QStringList &inputArgs);
    void stop();
    // 1 (pouco sensível) .. 10 (muito sensível)
    void setSensitivity(int sensitivity);

Q_SIGNALS:
    void motion(double score);

private:
    void launch();
    void readOutput();

    QProcess m_process;
    QTimer m_restartTimer;
    QStringList m_inputArgs;
    QByteArray m_buffer;
    double m_threshold = 0.02;
    int m_framesSeen = 0;
    int m_hits = 0;
    bool m_running = false;
};
