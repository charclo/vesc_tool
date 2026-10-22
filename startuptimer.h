/*
    Temporary startup profiling. Set VT_STARTUP_TIMING=1 to print
    "STARTUP <epoch ms> <ms since main> <label>" lines to stderr.
 */

#ifndef STARTUPTIMER_H
#define STARTUPTIMER_H

#include <QDateTime>
#include <QElapsedTimer>
#include <cstdio>

inline void startupMark(const char *label)
{
    static const bool enabled = qEnvironmentVariableIsSet("VT_STARTUP_TIMING");
    static QElapsedTimer timer;

    if (!enabled) {
        return;
    }

    if (!timer.isValid()) {
        timer.start();
    }

    fprintf(stderr, "STARTUP %lld %lld %s\n",
            static_cast<long long>(QDateTime::currentMSecsSinceEpoch()),
            static_cast<long long>(timer.elapsed()), label);
    fflush(stderr);
}

#endif // STARTUPTIMER_H
