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
#include <thread>
#include <vector>
#include <utility>
#include <mutex>
#include <map>
extern "C"
{
#include <xdo.h>
#include <pulse/simple.h>
}

class MouseCursorTracker
{
public:
    MouseCursorTracker(std::string cfgPath,
                       std::vector<std::pair<std::string, int> > motions = {},
                       std::vector<std::string> expressions = {});
    ~MouseCursorTracker();

    enum class MotionPriority
    {
        // See LAppDefine.cpp in Demo
        none,
        idle,
        normal,
        force
    };

    struct Params
    {
        std::map<std::string, double> live2d;

        bool autoBlink;
        bool autoBreath;
        bool randomIdleMotion;
        bool useLipSync;
        double lipSyncParam;

        MotionPriority motionPriority;
        std::string motionGroup;
        int motionNumber;

        std::string expression;
    };

    Params getParams(void);

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
        bool randomIdleMotion;
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
        Coord origin;
    } m_cfg;

    std::map<std::string, double> m_overrideMap;

    bool m_stop;

    Coord m_curPos;

    xdo_t *m_xdo;

    std::thread m_getVolumeThread;
    std::thread m_parseCommandThread;
    void audioLoop(void);
    void cliLoop(void);
    void processCommand(std::string);
    double m_currentVol;
    pa_simple *m_pulse;

    MotionPriority m_motionPriority;
    std::string m_motionGroup;
    int m_motionNumber;
    std::mutex m_motionMutex;
    std::string m_expression;

    void populateDefaultConfig(void);
    void parseConfig(std::string cfgPath);
};

#endif
