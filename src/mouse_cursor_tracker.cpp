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
#include <utility>
#include <algorithm>
#include <cstdlib>
#include <cmath>
#include <cassert>

#include <iostream>
#include <cctype>
#include <mutex>

extern "C"
{
#include <xdo.h>
#include <pulse/simple.h>
#include <readline/readline.h>
#include <readline/history.h>
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

static std::vector<std::string> split(std::string s)
{
    std::vector<std::string> v;
    std::string tmp;

    for (std::size_t i = 0; i < s.length(); i++)
    {
        char c = s[i];
        if (std::isspace(c))
        {
            if (tmp != "")
            {
                v.push_back(tmp);
                tmp = "";
            }
        }
        else
        {
            tmp += c;
        }
    }
    if (tmp != "")
    {
        v.push_back(tmp);
    }
    return v;
}


/* Using readline callback functions means that we need to pass
 * information from our class to the callback, and the only way
 * to do so is using globals.
 */

// Taken from https://github.com/eliben/code-for-blog/blob/master/2016/readline-samples/utils.cpp
static std::string longest_common_prefix(std::string s,
                                         const std::vector<std::string>& candidates) {
    assert(candidates.size() > 0);
    if (candidates.size() == 1) {
        return candidates[0];
    }

    std::string prefix(s);
    while (true) {
        // Each iteration of this loop advances to the next location in all the
        // candidates and sees if they match up to it.
        size_t nextloc = prefix.size();
        auto i = candidates.begin();
        if (i->size() <= nextloc) {
            return prefix;
        }
        char nextchar = (*(i++))[nextloc];
        for (; i != candidates.end(); ++i) {
            if (i->size() <= nextloc || (*i)[nextloc] != nextchar) {
                // Bail out if there's a mismatch for this candidate.
                return prefix;
            }
        }
        // All candidates have contents[nextloc] == nextchar, so we can safely
        // extend the prefix.
        prefix.append(1, nextchar);
    }

    assert(0 && "unreachable");
}

std::vector<std::string> commands =
{
    "help", "motion", "expression", "set", "clear"
};

std::vector<std::string> live2dParams =
{ // https://docs.live2d.com/cubism-editor-manual/standard-parametor-list/?locale=ja
    "ParamAngleX", "ParamAngleY", "ParamAngleZ",
    "ParamEyeLOpen", "ParamEyeLSmile", "ParamEyeROpen", "ParamEyeRSmile",
    "ParamEyeBallX", "ParamEyeBallY", "ParamEyeBallForm",
    "ParamBrowLY", "ParamBrowRY", "ParamBrowLX", "ParamBrowRX",
    "ParamBrowLAngle", "ParamBrowRAngle", "ParamBrowLForm", "ParamBrowRForm",
    "ParamMouthForm", "ParamMouthOpenY", "ParamTere",
    "ParamBodyAngleX", "ParamBodyAngleY", "ParamBodyAngleZ", "ParamBreath",
    "ParamArmLA", "ParamArmRA", "ParamArmLB", "ParamArmRB",
    "ParamHandL", "ParamHandR",
    "ParamHairFront", "ParamHairSide", "ParamHairBack", "ParamHairFluffy",
    "ParamShoulderY", "ParamBustX", "ParamBustY",
    "ParamBaseX", "ParamBaseY"
};
std::vector<std::pair<std::string, int> > MCT_motions;
std::vector<std::string> MCT_expressions;
std::map<std::string, double> *MCT_overrideMap;

static char **cliCompletionFunction(const char *textCStr, int start, int end)
{
    // Reference: https://github.com/eliben/code-for-blog/blob/master/2016/readline-samples/readline-complete-subcommand.cpp
    rl_attempted_completion_over = 1;

    std::string line(rl_line_buffer);

    std::vector<std::string> cmdline = split(line);

    std::vector<std::string> constructed;
    std::vector<std::string> *vocab = nullptr;

    if (cmdline.size() == 0 ||
        (cmdline.size() == 1 && line.back() != ' ') ||
        cmdline[0] == "help")
    {
        vocab = &commands;
    }
    else if (cmdline[0] == "motion")
    {
        for (auto it = MCT_motions.begin(); it != MCT_motions.end(); ++it)
        {
            if ((cmdline.size() == 1 && line.back() == ' ') ||
                (cmdline.size() == 2 && line.back() != ' '))
            { // motionGroup
                {
                    constructed.push_back(it->first);
                }
            }
            else if ((cmdline.size() == 2 && line.back() == ' ') ||
                     (cmdline.size() == 3 && line.back() != ' '))
            { // motionNumber
                if (it->first == cmdline[1])
                {
                    for (int i = 0; i < it->second; i++)
                    {
                        constructed.push_back(std::to_string(i));
                    }
                    break;
                }
            }
            else if (cmdline.size() <= 4)
            { // priority
                for (int i = 0; i < 4; i++)
                {
                    constructed.push_back(std::to_string(i));
                }
            }
        }

        vocab = &constructed;
    }
    else if (cmdline[0] == "expression")
    {
        vocab = &MCT_expressions;
    }
    else if (cmdline[0] == "set")
    {
        if ((cmdline.size() % 2 == 0 && line.back() != ' ') ||
            (cmdline.size() % 2 == 1 && line.back() == ' '))
        {
            vocab = &live2dParams;
        }
    }
    else if (cmdline[0] == "clear")
    {
        constructed.push_back("all");
        for (auto const &entry : *MCT_overrideMap)
        {
            constructed.push_back(entry.first);
        }
        vocab = &constructed;
    }

    if (!vocab)
    {
        return nullptr;
    }

    std::string text(textCStr);
    std::vector<std::string> matches;
    std::copy_if(vocab->begin(), vocab->end(), std::back_inserter(matches),
                 [&text](const std::string &s)
                 {
                     return (s.size() >= text.size() &&
                             s.compare(0, text.size(), text) == 0);
                 });

    if (matches.empty())
    {
        return nullptr;
    }

    char** array =
        static_cast<char**>(malloc((2 + matches.size()) * sizeof(*array)));
    array[0] = strdup(longest_common_prefix(text, matches).c_str());
    size_t ptr = 1;
    for (const auto& m : matches) {
        array[ptr++] = strdup(m.c_str());
    }
    array[ptr] = nullptr;
    return array;
}

MouseCursorTracker::MouseCursorTracker(std::string cfgPath,
                                       std::vector<std::pair<std::string, int> > motions,
                                       std::vector<std::string> expressions)
    : m_stop(false)
{
    m_motionPriority = MotionPriority::none;
    m_motionNumber = 0;


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
    m_parseCommandThread = std::thread(&MouseCursorTracker::cliLoop, this);

    MCT_motions = motions;
    MCT_expressions = expressions;
    MCT_overrideMap = &m_overrideMap;
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

void MouseCursorTracker::cliLoop(void)
{
    rl_catch_signals = 0;
    rl_attempted_completion_function = cliCompletionFunction;
    while (!m_stop)
    {
        char *buf = readline(">> ");

        if (buf)
        {
            std::string cmdline(buf);
            free(buf);
            processCommand(cmdline);
        }
        else
        {
            std::cout << "Exiting CLI loop. Use Ctrl+C to exit the whole process." << std::endl;
            stop();
        }
    }
}


void MouseCursorTracker::processCommand(std::string cmdline)
{
    auto cmdSplit = split(cmdline);

    if (cmdSplit.size() > 0)
    {
        add_history(cmdline.c_str());

        if (cmdSplit[0] == "help")
        {
            if (cmdSplit.size() == 1)
            {
                std::cout << "Available commands: motion set clear\n"
                          << "Type \"help <command>\" for more help" << std::endl;
            }
            else if (cmdSplit[1] == "motion")
            {
                std::cout << "motion <motionGroup> <motionNumber> [<priority>]\n"
                          << "motionGroup: The motion name in the .model3.json file\n"
                          << "motionNumber: The index of this motion in the .model3.json file, 0-indexed\n"
                          << "priority: 0 = none, 1 = idle, 2 = normal, 3 = force (default normal)" << std::endl;
            }
            else if (cmdSplit[1] == "expression")
            {
                std::cout << "expression <expressionName>\n"
                          << "expressionName: Name of expression in the .model3.json file" << std::endl;
            }
            else if (cmdSplit[1] == "set")
            {
                std::cout << "set <param1> <value1> [<param2> <value2> ...]\n"
                          << "Set parameter value. Overrides any tracking."
                          << "See live2D documentation for full list of params" << std::endl;
            }
            else if (cmdSplit[1] == "clear")
            {
                std::cout << "clear <param1> [<param2> ...]\n"
                          << "Clear parameter value. Re-enables tracking if it was overridden by \"set\"\n"
                          << "You can also use \"clear all\" to clear everything" << std::endl;
            }
            else
            {
                std::cout << "Unrecognized command" << std::endl;
            }
        }
        else if (cmdSplit[0] == "motion")
        {
            if (cmdSplit.size() == 3 || cmdSplit.size() == 4)
            {
                std::unique_lock<std::mutex> lock(m_motionMutex, std::defer_lock);
                lock.lock();
                m_motionGroup = cmdSplit[1];
                try
                {
                    m_motionNumber = std::stoi(cmdSplit[2]);
                    if (cmdSplit.size() == 4)
                    {
                        m_motionPriority = static_cast<MotionPriority>(std::stoi(cmdSplit[3]));
                    }
                    else
                    {
                        m_motionPriority = MotionPriority::normal;
                    }
                }
                catch (const std::exception &e)
                {
                    std::cerr << "std::stoi failed" << std::endl;
                }
                lock.unlock();
            }
            else
            {
                std::cerr << "Incorrect command, expecting 2 or 3 arguments" << std::endl;
                std::cerr << "motion motionGroup motionNumber [motionPriority]" << std::endl;
            }
        }
        else if (cmdSplit[0] == "expression")
        {
            if (cmdSplit.size() == 2)
            {
                std::unique_lock<std::mutex> lock(m_motionMutex, std::defer_lock);
                lock.lock();
                m_expression = cmdSplit[1];
                lock.unlock();
            }
            else
            {
                std::cerr << "Incorrect command, expecting 1 argument: expressionName" << std::endl;
            }
        }
        else if (cmdSplit[0] == "set")
        {
            if (cmdSplit.size() % 2 != 1)
            {
                // "set param1 value1 param2 value2 ..."
                std::cerr << "Incorrect number of arguments for command 'set'" << std::endl;
            }
            for (std::size_t i = 1; i < cmdSplit.size(); i += 2)
            {
                try
                {
                    m_overrideMap[cmdSplit[i]] = std::stod(cmdSplit[i + 1]);
                }
                catch (const std::exception &e)
                {
                    std::cerr << "std::stod failed" << std::endl;
                }

                std::cerr << "Debug: setting " << cmdSplit[i] << std::endl;
            }
        }
        else if (cmdSplit[0] == "clear")
        {
            for (std::size_t i = 1; i < cmdSplit.size(); i++)
            {
                if (cmdSplit[i] == "all")
                {
                    m_overrideMap.clear();
                    break;
                }
                std::size_t removed = m_overrideMap.erase(cmdSplit[i]);
                if (removed == 0)
                {
                    std::cerr << "Warning: key " << cmdSplit[i] << " not found" << std::endl;
                }
            }
        }
        else
        {
            std::cerr << "Unknown command" << std::endl;
        }
    }
}

MouseCursorTracker::~MouseCursorTracker()
{
    xdo_free(m_xdo);
    m_getVolumeThread.join();
    m_parseCommandThread.join();
    pa_simple_free(m_pulse);
}

void MouseCursorTracker::stop(void)
{
    m_stop = true;
}

MouseCursorTracker::Params MouseCursorTracker::getParams(void)
{
    Params params = Params();

    int xOffset = m_curPos.x - m_cfg.origin.x;
    int leftRange = m_cfg.origin.x - m_cfg.left;
    int rightRange = m_cfg.right - m_cfg.origin.x;

    if (xOffset > 0) // i.e. to the right
    {
        params.live2d["ParamAngleX"] = 30.0 * xOffset / rightRange;
    }
    else // to the left
    {
        params.live2d["ParamAngleX"] = 30.0 * xOffset / leftRange;
    }

    int yOffset = m_curPos.y - m_cfg.origin.y;
    int topRange = m_cfg.origin.y - m_cfg.top;
    int bottomRange = m_cfg.bottom - m_cfg.origin.y;

    if (yOffset > 0) // downwards
    {
        params.live2d["ParamAngleY"] = -30.0 * yOffset / bottomRange;
    }
    else // upwards
    {
        params.live2d["ParamAngleY"] = -30.0 * yOffset / topRange;
    }

    params.autoBlink = m_cfg.autoBlink;
    params.autoBreath = m_cfg.autoBreath;
    params.randomIdleMotion = m_cfg.randomIdleMotion;
    params.useLipSync = m_cfg.useLipSync;

    params.live2d["ParamMouthForm"] = m_cfg.mouthForm;

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

    // Don't block in getParams()
    std::unique_lock<std::mutex> lock(m_motionMutex, std::try_to_lock);
    if (lock.owns_lock())
    {
        if (m_motionPriority != MotionPriority::none)
        {
            params.motionPriority = m_motionPriority;
            params.motionGroup = m_motionGroup;
            params.motionNumber = m_motionNumber;

            m_motionPriority = MotionPriority::none;
            m_motionGroup = "";
            m_motionNumber = 0;
        }
        params.expression = m_expression;
        m_expression = "";
        lock.unlock();
    }

    // Leave everything else as zero


    // Process overrides
    for (auto const &x : m_overrideMap)
    {
        std::string key = x.first;
        double val = x.second;

        params.live2d[key] = val;
    }


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
                else if (paramName == "randomIdleMotion")
                {
                    if (!(ss >> m_cfg.randomIdleMotion))
                    {
                        throw std::runtime_error("Error parsing randomIdleMotion");
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
                else if (paramName == "origin_x")
                {
                    if (!(ss >> m_cfg.origin.x))
                    {
                        throw std::runtime_error("Error parsing origin_x");
                    }
                }
                else if (paramName == "origin_y")
                {
                    if (!(ss >> m_cfg.origin.y))
                    {
                        throw std::runtime_error("Error parsing origin_y");
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
    m_cfg.randomIdleMotion = false;
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
    m_cfg.origin = {1600, 870}; // Somewhere near the bottom right
}

