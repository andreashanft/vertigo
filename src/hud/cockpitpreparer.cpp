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


#include "cockpitpreparer.h"
#include "cockpit3d.h"
#include "hud.h"
#include "gfx/gl.hpp"
#include "gfx/glbmodel.h"
#include "txt/stringtable.h"
#include "util/datapath.h"
#include "util/path.hpp"

#include <algorithm>
#include <iostream>
#include <utility>
#include <vector>


namespace hud {


CockpitPreparer& CockpitPreparer::get()
{
    static CockpitPreparer preparer;
    return preparer;
}


CockpitPreparer::~CockpitPreparer()
{
    stop();
    if (m_thread.joinable())
        m_thread.join();
}


bool CockpitPreparer::isOptionAvailable()
{
    return !Cockpit3D::cockpits().empty();
}


String CockpitPreparer::optionText()
{
    const bool on = HUD::cockpitMode() == HUD::CockpitMode::Model;
    if (on && get().isRunning())
        return txt::StringTable::get(txt::Cockpit3D_Running).arg(get().percent());
    return txt::StringTable::get(on ? txt::Cockpit3D_On : txt::Cockpit3D_Off);
}


void CockpitPreparer::toggleOption()
{
    const bool on = HUD::cockpitMode() != HUD::CockpitMode::Model;
    HUD::setCockpitMode(on ? HUD::CockpitMode::Model : HUD::CockpitMode::Art, true);
    if (on)
        get().start();
    else
        get().stop();
}


void CockpitPreparer::artChanged()
{
    if (HUD::cockpitMode() == HUD::CockpitMode::Model)
        get().start();
}


void CockpitPreparer::start()
{
    // (the run before this one ends at its next step)
    stop();
    if (m_thread.joinable())
        m_thread.join();
    // (the size of the textures depends on what OpenGL takes)
    if (!gfx::gl::hasContext())
        return;

    // what the game's thread has to gather: each kit's file and what it is painted from
    struct Job
    {
        std::string file;
        Cockpit3D::PaintSource source;
    };
    std::vector<Job> jobs;
    for (const String &cockpit : Cockpit3D::cockpits()) {
        const std::string file = Cockpit3D::find(cockpit);
        // (a model for a cockpit that the game data doesn't have is none of the game's)
        if (file.empty() || !util::exists(util::DataPath::resolve(String("vfx:cockpit/%1/cockpit.r16").arg(cockpit).toUtf8())))
            continue;
        jobs.push_back({ file, Cockpit3D::paintSource(cockpit, Cockpit3D::artCamera(cockpit)) });
    }
    if (jobs.empty())
        return;

    m_total = int(jobs.size());
    m_done = 0;
    m_current = 0;
    m_stop = false;
    m_running = true;
    m_thread = std::thread([this, jobs = std::move(jobs)]() {
        for (const Job &job : jobs) {
            if (m_stop)
                break;
            gfx::GlbModel kit;
            if (!kit.load(job.file))
                std::cerr << "3D cockpit: " << kit.error() << std::endl;
            // (a finished model has its texture)
            else if (!kit.extras()["vertigo_kit"].isUndefined()) {
                const Cockpit3D::Painted painted = Cockpit3D::paint(kit, job.file, job.source, &m_stop, &m_current, false);
                if (!painted.cached && !painted.texture.isNull())
                    std::cout << "3D cockpit: the texture painted ahead, " << painted.file << std::endl;
            }
            m_current = 0;
            m_done++;
        }
        m_running = false;
    });
}


void CockpitPreparer::stop()
{
    m_stop = true;
}


int CockpitPreparer::percent() const
{
    const int total = m_total;
    if (total <= 0)
        return 0;
    return std::min((100*m_done + m_current)/total, 100);
}


} // namespace hud
