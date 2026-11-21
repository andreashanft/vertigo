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


#ifndef HUD_COCKPITPREPARER_H
#define HUD_COCKPITPREPARER_H


#include "util/string.hpp"

#include <atomic>
#include <thread>


namespace hud {


// Paints the textures of the 3D cockpits ahead of the missions, in the background, when the 3D
// cockpit is switched on: for every cockpit that has a kit, from the cockpit art in use (the
// upscaled one with the upscaled graphics), into the cache where a mission finds it (Cockpit3D).
// A mission then shows its 3D cockpit as soon as the model is loaded.
//
// It also is the option "3D cockpit" of the main menu's options and of the notebook: whether
// there is anything to switch on, the text of the option's line and what a click on it does.
class CockpitPreparer
{
public:
    static CockpitPreparer& get();
    ~CockpitPreparer();

public:
    // there is a model or a kit for at least one cockpit
    static bool isOptionAvailable();
    // the option's line: on, off, or the progress while the textures are painted
    static String optionText();
    // Switches the 3D cockpit on, which starts the painting, or off, which stops it. Saved in
    // the setting "cockpit/mode".
    static void toggleOption();
    // The cockpit art in use has changed, because the upscaled graphics went on or off: paints
    // the textures from it if the 3D cockpit is on.
    static void artChanged();

    // Paints the textures that the cache doesn't have yet. Call it on the game's thread with an
    // OpenGL context. A run that is going on starts again.
    void start();
    // Asks a run to stop, which it does at its next step. The textures painted so far are kept.
    void stop();
    bool isRunning() const { return m_running && !m_stop; }
    // of the whole run, 0 to 100
    int percent() const;

private:
    CockpitPreparer() = default;

private:
    std::thread m_thread;
    std::atomic<bool> m_running{ false };
    std::atomic<bool> m_stop{ false };
    // the cockpits of the run, those that are done and how far the one being painted is (0 to 100)
    std::atomic<int> m_total{ 0 };
    std::atomic<int> m_done{ 0 };
    std::atomic<int> m_current{ 0 };
};


} // namespace hud


#endif // HUD_COCKPITPREPARER_H
