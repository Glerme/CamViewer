#pragma once

#include <QCoreApplication>
#include <QFileInfo>
#include <QString>

// O instalador do Windows traz o ffmpeg junto do app; nos outros sistemas vale
// o que estiver no PATH.
inline QString ffmpegProgram()
{
#ifdef Q_OS_WIN
    const QString bundled = QCoreApplication::applicationDirPath() + QStringLiteral("/ffmpeg/ffmpeg.exe");
    if (QFileInfo::exists(bundled)) {
        return bundled;
    }
#endif
    return QStringLiteral("ffmpeg");
}
