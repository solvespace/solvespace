//-----------------------------------------------------------------------------
// An abstraction for platform-dependent GUI functionality.
//
// Copyright 2018 whitequark
//-----------------------------------------------------------------------------

#ifndef SOLVESPACE_GUI_H
#define SOLVESPACE_GUI_H

namespace Platform {

//-----------------------------------------------------------------------------
// Interfaces
//-----------------------------------------------------------------------------

// A native single-shot timer.
class Timer {
public:
    std::function<void()>   onTimeout;

    virtual ~Timer() {}

    virtual void WindUp(unsigned milliseconds) = 0;
};

typedef std::unique_ptr<Timer> TimerRef;

TimerRef CreateTimer();

}

#endif
