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

#include <stdexcept>
#include <string>
#include <chrono>
#include <thread>
#include <fstream>
#include <sstream>
#include <vector>
#include <cstdlib>
#include <cmath>

#include <iostream>

extern "C"
{
#include <xdo.h>
#include <pulse/simple.h>
}
#include "mouse_cursor_tracker.h"

static double rms(float *buf, std::size_t count)
{
    double sum = 0;
    for (std::size_t i = 0; i < count; i++)
    {
        sum += buf[i] * buf[i];
    }
    return std::sqrt(sum / count);
}

MouseCursorTracker::MouseCursorTracker(std::string cfgPath)
    : m_stop(false)
{
    parseConfig(cfgPath);
    m_xdo = xdo_new(nullptr);

    const pa_sample_spec ss =
    {
        .format = PA_SAMPLE_FLOAT32NE,
        .rate = 44100,
        .channels = 2
    };
    m_pulse = pa_simple_new(nullptr, "MouseCursorTracker", PA_STREAM_RECORD,
                            nullptr, "LipSync", &ss, nullptr, nullptr, nullptr);
    if (!m_pulse)
    {
        throw std::runtime_error("Unable to create pulse");
    }

    m_getVolumeThread = std::thread(&MouseCursorTracker::audioLoop, this);
}

void MouseCursorTracker::audioLoop(void)
{
    float *buf = new float[m_cfg.audioBufSize];

    std::size_t audioBufByteSize = m_cfg.audioBufSize * sizeof *buf;

    while (!m_stop)
    {
        if (pa_simple_read(m_pulse, buf, audioBufByteSize, nullptr) < 0)
        {
            throw std::runtime_error("Unable to get audio data");
        }
        m_currentVol = rms(buf, m_cfg.audioBufSize);
    }

    delete[] buf;
}

MouseCursorTracker::~MouseCursorTracker()
{
    xdo_free(m_xdo);
    m_getVolumeThread.join();
    pa_simple_free(m_pulse);
}

void MouseCursorTracker::stop(void)
{
    m_stop = true;
}

MouseCursorTracker::Params MouseCursorTracker::getParams(void) const
{
    Params params = Params();

    int xOffset = m_curPos.x - m_cfg.middle.x;
    int leftRange = m_cfg.middle.x - m_cfg.left;
    int rightRange = m_cfg.right - m_cfg.middle.x;

    if (xOffset > 0) // i.e. to the right
    {
        params.faceXAngle = 30.0 * xOffset / rightRange;
    }
    else // to the left
    {
        params.faceXAngle = 30.0 * xOffset / leftRange;
    }

    int yOffset = m_curPos.y - m_cfg.middle.y;
    int topRange = m_cfg.middle.y - m_cfg.top;
    int bottomRange = m_cfg.bottom - m_cfg.middle.y;

    if (yOffset > 0) // downwards
    {
        params.faceYAngle = -30.0 * yOffset / bottomRange;
    }
    else // upwards
    {
        params.faceYAngle = -30.0 * yOffset / topRange;
    }

    params.faceZAngle = 0;

    params.leftEyeOpenness = 1;
    params.rightEyeOpenness = 1;

    params.autoBlink = m_cfg.autoBlink;
    params.autoBreath = m_cfg.autoBreath;
    params.randomMotion = m_cfg.randomMotion;
    params.useLipSync = m_cfg.useLipSync;

    params.mouthForm = m_cfg.mouthForm;

    if (m_cfg.useLipSync)
    {
        params.lipSyncParam = m_currentVol * m_cfg.lipSyncGain;
        if (params.lipSyncParam < m_cfg.lipSyncCutOff)
        {
            params.lipSyncParam = 0;
        }
        else if (params.lipSyncParam > 1)
        {
            params.lipSyncParam = 1;
        }
    }


    // Leave everything else as zero


    return params;
}

void MouseCursorTracker::mainLoop(void)
{
    while (!m_stop)
    {
        int x;
        int y;
        int screenNum;

        xdo_get_mouse_location(m_xdo, &x, &y, &screenNum);

        if (screenNum == m_cfg.screen)
        {
            m_curPos.x = x;
            m_curPos.y = y;
        }
        // else just silently ignore for now
        std::this_thread::sleep_for(std::chrono::milliseconds(m_cfg.sleepMs));
    }
}

void MouseCursorTracker::parseConfig(std::string cfgPath)
{
    populateDefaultConfig();

    if (cfgPath != "")
    {
        std::ifstream file(cfgPath);
        if (!file)
        {
            throw std::runtime_error("Failed to open config file");
        }
        std::string line;
        unsigned int lineNum = 0;

        while (std::getline(file, line))
        {
            if (line[0] == '#')
            {
                continue;
            }

            std::istringstream ss(line);
            std::string paramName;

            if (ss >> paramName)
            {
                if (paramName == "sleep_ms")
                {
                    if (!(ss >> m_cfg.sleepMs))
                    {
                        throw std::runtime_error("Error parsing sleep_ms");
                    }
                }
                else if (paramName == "autoBlink")
                {
                    if (!(ss >> m_cfg.autoBlink))
                    {
                        throw std::runtime_error("Error parsing autoBlink");
                    }
                }
                else if (paramName == "autoBreath")
                {
                    if (!(ss >> m_cfg.autoBreath))
                    {
                        throw std::runtime_error("Error parsing autoBreath");
                    }
                }
                else if (paramName == "randomMotion")
                {
                    if (!(ss >> m_cfg.randomMotion))
                    {
                        throw std::runtime_error("Error parsing randomMotion");
                    }
                }
                else if (paramName == "useLipSync")
                {
                    if (!(ss >> m_cfg.useLipSync))
                    {
                        throw std::runtime_error("Error parsing useLipSync");
                    }
                }
                else if (paramName == "lipSyncGain")
                {
                    if (!(ss >> m_cfg.lipSyncGain))
                    {
                        throw std::runtime_error("Error parsing lipSyncGain");
                    }
                }
                else if (paramName == "lipSyncCutOff")
                {
                    if (!(ss >> m_cfg.lipSyncCutOff))
                    {
                        throw std::runtime_error("Error parsing lipSyncCutOff");
                    }
                }
                else if (paramName == "audioBufSize")
                {
                    if (!(ss >> m_cfg.audioBufSize))
                    {
                        throw std::runtime_error("Error parsing audioBufSize");
                    }
                }
                else if (paramName == "mouthForm")
                {
                    if (!(ss >> m_cfg.mouthForm))
                    {
                        throw std::runtime_error("Error parsing mouthForm");
                    }
                }
                else if (paramName == "screen")
                {
                    if (!(ss >> m_cfg.screen))
                    {
                        throw std::runtime_error("Error parsing screen");
                    }
                }
                else if (paramName == "middle_x")
                {
                    if (!(ss >> m_cfg.middle.x))
                    {
                        throw std::runtime_error("Error parsing middle_x");
                    }
                }
                else if (paramName == "middle_y")
                {
                    if (!(ss >> m_cfg.middle.y))
                    {
                        throw std::runtime_error("Error parsing middle_y");
                    }
                }
                else if (paramName == "top")
                {
                    if (!(ss >> m_cfg.top))
                    {
                        throw std::runtime_error("Error parsing top");
                    }
                }
                else if (paramName == "bottom")
                {
                    if (!(ss >> m_cfg.bottom))
                    {
                        throw std::runtime_error("Error parsing bottom");
                    }
                }
                else if (paramName == "left")
                {
                    if (!(ss >> m_cfg.left))
                    {
                        throw std::runtime_error("Error parsing left");
                    }
                }
                else if (paramName == "right")
                {
                    if (!(ss >> m_cfg.right))
                    {
                        throw std::runtime_error("Error parsing right");
                    }
                }
                else
                {
                    throw std::runtime_error("Unrecognized config parameter");
                }
            }
        }
    }
}

void MouseCursorTracker::populateDefaultConfig(void)
{
    m_cfg.sleepMs = 5;
    m_cfg.autoBlink = true;
    m_cfg.autoBreath = true;
    m_cfg.randomMotion = false;
    m_cfg.useLipSync = true;
    m_cfg.lipSyncGain = 10;
    m_cfg.lipSyncCutOff = 0.15;
    m_cfg.audioBufSize = 4096;
    m_cfg.mouthForm = 0;
    m_cfg.top = 0;
    m_cfg.bottom = 1079;
    m_cfg.left = 0;
    m_cfg.right = 1919;  // These will be the full screen for 1920x1080

    m_cfg.screen = 0;
    m_cfg.middle = {1600, 870}; // Somewhere near the bottom right
}

