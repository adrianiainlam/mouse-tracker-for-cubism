// -*- mode: c++ -*-

#ifndef __MOUSE_CURSOR_TRACKER_H__
#define __MOUSE_CURSOR_TRACKER_H__

/****
Copyright (c) 2020 Adrian I. Lam

Permission is hereby granted, free of charge, to any person obtaining a copy
of this software and associated documentation files (the "Software"), to deal
in the Software without restriction, including without limitation the rights
to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
copies of the Software, and to permit persons to whom the Software is
furnished to do so, subject to the following conditions:

The above copyright notice and this permission notice shall be included in all
copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
SOFTWARE.
****/

#include <string>
#include <map>
#include <thread>
extern "C"
{
#include <xdo.h>
#include <pulse/simple.h>
}

class MouseCursorTracker
{
public:
    MouseCursorTracker(std::string cfgPath);
    ~MouseCursorTracker();

    struct Params
    {
        double leftEyeOpenness;
        double rightEyeOpenness;
        double leftEyeSmile;
        double rightEyeSmile;
        double mouthOpenness;
        double mouthForm;
        double faceXAngle;
        double faceYAngle;
        double faceZAngle;
        bool autoBlink;
        bool autoBreath;
        bool randomMotion;
        bool useLipSync;
        double lipSyncParam;
    };

    Params getParams(void) const;

    void stop(void);

    void mainLoop(void);

private:
    struct Coord
    {
        int x;
        int y;
    };

    struct Config
    {
        int sleepMs;
        bool autoBlink;
        bool autoBreath;
        bool randomMotion;
        bool useLipSync;
        double lipSyncGain;
        double lipSyncCutOff;
        unsigned int audioBufSize;
        double mouthForm;
        int top;
        int bottom;
        int left;
        int right;
        int screen;
        Coord middle;
    } m_cfg;

    bool m_stop;

    Coord m_curPos;

    xdo_t *m_xdo;

    std::thread m_getVolumeThread;
    void audioLoop(void);
    double m_currentVol;
    pa_simple *m_pulse;

    void populateDefaultConfig(void);
    void parseConfig(std::string cfgPath);
};

#endif
