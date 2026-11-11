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

#ifndef FIGHT_STEPCLOCK_HPP
#define FIGHT_STEPCLOCK_HPP

#include <cmath>

namespace fight {


// Turns the time of the frames into the mission's steps, which are all of one length whatever the
// display's refresh. A frame takes as many steps as its time asks for. What is left over is the
// part of a step the frame is ahead of the last step: the cameras and whatever moves are drawn
// that far further on (Scenario::setDrawAhead()), so the picture moves evenly on every display.
class StepClock
{
public:
    explicit StepClock(float step) : m_step(step) { }

    // The time a frame took as the display counts it. The measured time is a little off, so a
    // time of about a whole number of the display's refreshes counts as exactly that many
    // (period: the seconds per refresh, 0 when unknown). The steps then keep in time with the
    // display: at 60 Hz every frame is one step, with nothing left over.
    static float displayTime(float measured, float period)
    {
        if (period <= 0)
            return measured;
        const float refreshes = std::round(measured/period);
        const bool whole = refreshes >= 1 && std::abs(measured - refreshes*period) <= PeriodTolerance*period;
        return whole ? refreshes*period : measured;
    }

    // A frame of this time: returns how many steps the mission takes before the frame is drawn.
    // A step may come up to a quarter of one early, so that frames a little short or long still
    // get one each.
    int advance(float elapsed)
    {
        m_ahead += elapsed;
        int steps = 0;
        for (; m_ahead >= (1 - EarlyShare)*m_step; m_ahead -= m_step)
            steps++;
        return steps;
    }

    // the part of a step the frame is ahead of the mission's last step, -0.25 to 0.75: below 0
    // when that step came early
    float ahead() const { return m_ahead/m_step; }

    void reset() { m_ahead = 0; }

private:
    static constexpr float EarlyShare = 0.25f;
    // how far a frame's time may be off a whole number of refreshes, as a share of one
    static constexpr float PeriodTolerance = 0.1f;

    float m_step;
    float m_ahead = 0;  // the time the frame is ahead of the last step
};


} // namespace fight

#endif // FIGHT_STEPCLOCK_HPP
