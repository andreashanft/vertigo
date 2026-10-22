/***************************************************************************
 *  Copyright (C) 2026  Andreas Hanft                                      *
 *                                                                         *
 *  This program is free software: you can redistribute it and/or modify   *
 *  it under the terms of the GNU General Public License as published by   *
 *  the Free Software Foundation, either version 3 of the License, or      *
 *  (at your option) any later version.                                    *
 *                                                                         *
 *  This program is distributed in the hope that it will be useful,        *
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of         *
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the          *
 *  GNU General Public License for more details.                           *
 *                                                                         *
 *  You should have received a copy of the GNU General Public License      *
 *  along with this program.  If not, see <http://www.gnu.org/licenses/>.  *
 ***************************************************************************/

#ifndef GAME_UPSCALER_H
#define GAME_UPSCALER_H

#include <atomic>
#include <mutex>
#include <string>
#include <thread>

namespace game {

// Makes the upscaled graphics (gfx::Upscaled) from the game data, in the background: fetches the
// Real-ESRGAN upscaler once (its official release, checked against the hash it should have),
// decodes the game's pictures, model and effect textures and the cursor as the game does, has the upscaler make them four times as large, and keeps each in "Game
// Data Upscaled" as soon as it is done, so a stopped run goes on where it was. Desktop systems only.
class Upscaler
{
public:
    enum class State { Idle, Running, Finished, Failed };

    static Upscaler& get();
    ~Upscaler();

public:
    // the upscaler can be fetched and run here
    static bool isSupported();
    // it has been fetched
    static bool hasTool();
    // the size of the download, for asking first
    static int downloadMegabytes();

    void start();
    // stops a run; what is done is kept
    void stop();
    State state() const { return m_state; }
    bool isRunning() const { return m_state == State::Running; }
    // of the whole run, 0 to 100
    int percent() const { return m_permille/10; }
    // why it failed
    std::string error() const;
    // true once after a run has finished, for the game to switch to the set (on its thread)
    bool takeFinished();

private:
    Upscaler() = default;
    void run();
    bool fetchTool();
    bool upscale();
    void fail(const std::string &error);

private:
    std::thread m_thread;
    std::atomic<State> m_state{ State::Idle };
    std::atomic<int> m_permille{ 0 };
    std::atomic<bool> m_stop{ false };
    std::atomic<bool> m_finished{ false };
    mutable std::mutex m_mutex;
    std::string m_error;
};

} // namespace game

#endif // GAME_UPSCALER_H
