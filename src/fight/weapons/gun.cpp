/***************************************************************************
 *  Copyright (C) 2010  Philipp Nordhus                                    *
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

#include "gun.h"
#include "fight/scenario.h"
#include "txt/desfile.h"
#include "fight/effects/effectmanager.h"
#include "fight/effects/billboard.h"
#include "util/random.h"
#include <algorithm>


namespace fight {


Gun::GunState* Gun::GunStates = nullptr;

namespace {
// the original fires the next barrel of an ammo gun's round on the next frame; Vertigo spaces them
// as at 60 frames a second, with a quarter of that for the frames' jitter (docs/ROADMAP.md,
// appendix K)
const float BarrelInterval = 1.0f/60;
const float BarrelTolerance = BarrelInterval/4;
}


Gun::Gun(Scenario *scenario, int model, ActiveObject *owner) :
    m_scenario(scenario),
    m_owner(owner),
    m_model(model),
    m_defect(1.0f),
    m_state(StateReady)
{
    txt::DesFile file("vfx:sobjects/globweap.des");

    if (GunStates == nullptr)
    {
        GunStates = new Gun::GunState[6];
        for (int i = 0; i < 6; i++)
        {
            file.setSection(String("gunstate%1").arg(i));
            GunStates[i].longName = file.value("verbose").toString();
            GunStates[i].shortName = file.value("short").toString();
        }
    }

    file.setSection(String("gun%1").arg(model));
    m_longName = file.value("longname").toString();
    m_shortName = file.value("shortname").toString();
    m_imageName = file.value("imagename").toString();
    m_rearmingTime = file.value("rearmingtime").toFloat();
    m_energyDemand = file.value("energydemand").toFloat();
    m_defectDamping = file.value("defectdamping").toFloat();
    m_defectDelay = file.value("defectdelay").toFloat();

    m_ammo = file.value("defaultammo").toInt();

    static Effects effects[14] = { Effects::Shoot_PlasmaGun, Effects::Shoot_PlasmaGun, Effects::Shoot_PlasmaDeath, Effects::Shoot_PlasmaDeath, Effects::Shoot_PlasmaDeath, Effects::Shoot_DoomMortar, Effects::Shoot_MaximumImpactI, Effects::Shoot_MaximumImpact2, Effects::Shoot_Vendetta, Effects::Shoot_Vendetta, Effects::Shoot_Vendetta, Effects::Shoot_MaximumImpactI, Effects::Shoot_MaximumImpact2, Effects::Shoot_Vendetta };
    m_projectile = effects[model];

    m_range = m_scenario->effectManager().getBillboard(m_projectile)->range();
    m_projectileSpeed = m_scenario->effectManager().getBillboard(m_projectile)->velocity();

    static sfx::Sample samples[14] = { sfx::Sample::Shoot_PlasmaGun, sfx::Sample::Shoot_PlasmaGun, sfx::Sample::Shoot_PlasmaDeath, sfx::Sample::Shoot_PlasmaDeath, sfx::Sample::Shoot_PlasmaDeath, sfx::Sample::Shoot_DoomMortar, sfx::Sample::Shoot_MaximumImpact, sfx::Sample::Shoot_MaximumImpact, sfx::Sample::Shoot_Vendetta, sfx::Sample::Shoot_Vendetta, sfx::Sample::Shoot_Vendetta, sfx::Sample::Shoot_MaximumImpact, sfx::Sample::Shoot_MaximumImpact, sfx::Sample::Shoot_Vendetta };
    m_sample = samples[model];
}


int Gun::barrels(int model)
{
    static const int counts[14] = { 1, 2, 1, 2, 4, 1, 2, 2, 2, 3, 4, 2, 2, 2 };
    return model >= 0 && model < 14 ? counts[model] : 1;
}


void Gun::addMuzzle(const Vector3D &muzzle)
{
    m_muzzles.push_back(muzzle);
}


Vector3D Gun::barrelOffset(int model, int barrel)
{
    // 0.75 m to either side, the lower pair of four 0.8 m down; the Vendetta II's third barrel
    // sits 0.75 m above (docs/ROADMAP.md, appendix K)
    const float side = 0.75f, down = -0.8f;
    switch (barrels(model))
    {
    case 2:
        return Vector3D(barrel == 0 ? side : -side, 0, 0);
    case 3:
        return barrel == 0 ? Vector3D(side, 0, 0) : barrel == 1 ? Vector3D(-side, 0, 0) : Vector3D(0, 0, side);
    case 4:
    {
        static const Vector3D offsets[4] = { Vector3D(side, 0, 0), Vector3D(-side, 0, down), Vector3D(side, 0, down), Vector3D(-side, 0, 0) };
        return offsets[barrel % 4];
    }
    default:
        return Vector3D(0);
    }
}


bool Gun::update(float elapsedTime)
{
    if (m_state == StateJammed && m_stateTime < m_scenario->time())
        m_state = StateReady;
    if (m_state == StateRearming && m_stateTime < m_scenario->time())
        m_state = StateReady;
    // an energy gun is ready again once its rearming time is over and the owner's pool holds more
    // than a shot takes (docs/ROADMAP.md, appendix K)
    if (m_state == StateEnergyDelay && m_stateTime < m_scenario->time() && (!m_owner || m_owner->energy() > m_energyDemand))
        m_state = StateReady;
    if (m_state == StateReady && m_energyDemand == 0.0f && m_ammo == 0)
        m_state = StateEmpty;
    return false;
}


bool Gun::ready() const
{
    return m_state == StateReady && (!usesAmmo() || m_barrel == 0 || m_scenario->time() >= m_nextBarrel - BarrelTolerance);
}


void Gun::fire(const Vector3D &pos, const Vector3D &dir, const Vector3D &up, const Vector3D &right, bool allBarrels)
{
    if (!ready())
        return;
    m_state = StateFiring;
    // a jam is decided before each shot (docs/ROADMAP.md, appendix K): the gun's condition (1 when
    // new, taken as 0.2 at the least) against a random share of its defectdamping, so the chance is
    // 1 - condition/defectdamping, and the jam lasts defectdelay times 0.5 to 1.5
    auto random = []() { return static_cast<float>(util::random())/RAND_MAX; };
    const float condition = std::max(m_defect, 0.2f);
    if (condition < m_defectDamping && random()*m_defectDamping >= condition)
    {
        m_state = StateJammed;
        m_stateTime = m_scenario->time() + m_defectDelay*(0.5f + random());
        return;
    }
    // a round fires the model's barrels in turn around the next mounting (appendix K)
    const int barrelCount = barrels(m_model);
    if (m_barrel == 0 && !m_muzzles.empty())
        m_mounting = (m_mounting + 1) % m_muzzles.size();
    const Vector3D mounting = m_muzzles.empty() ? Vector3D(0) : m_muzzles[m_mounting];
    const int barrelsFired = allBarrels ? barrelCount - m_barrel : 1;
    for (int i = 0; i < barrelsFired; i++)
    {
        const Vector3D muzzle = mounting + barrelOffset(m_model, m_barrel + i);
        m_scenario->effectManager().addProjectile(m_projectile, pos + right*muzzle.x + dir*muzzle.y + up*muzzle.z, dir, m_owner);
    }
    m_barrel = (m_barrel + barrelsFired) % barrelCount;
    m_shots++;
    // the player's guns sound in the cockpit, others where they fire
    if (m_owner && m_owner != m_scenario->player())
        sfx::SampleMap::playAt(m_sample, pos.x, pos.y, pos.z);
    else
        sfx::SampleMap::get(m_sample).playInstance();
    if (usesAmmo())
    {
        // an ammo gun fires the round's barrels without the rearming time between them, and the
        // round takes one of its rounds
        if (m_barrel != 0)
        {
            m_state = StateReady;
            m_nextBarrel = m_scenario->time() + BarrelInterval;
            return;
        }
        m_ammo = std::max(0, m_ammo - 1);
        m_state = StateRearming;
    }
    else
    {
        // a barrel's shot takes energydemand divided by the model's barrels off the owner's pool,
        // however little is left; the gun then waits for its rearming time and for the pool to
        // hold more than the whole demand again
        if (m_owner)
            m_owner->drawEnergy(m_energyDemand*barrelsFired/barrelCount);
        m_state = StateEnergyDelay;
    }
    m_stateTime = m_scenario->time() + m_rearmingTime;
}


} // namespace fight
