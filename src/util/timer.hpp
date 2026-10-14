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

#ifndef UTIL_TIMER_HPP
#define UTIL_TIMER_HPP

#include <chrono>
#include <cstdint>

namespace util {

// Milliseconds since it was started, from a monotonic clock
class ElapsedTimer
{
public:
    void start() { m_start = std::chrono::steady_clock::now(); m_valid = true; }
    // the milliseconds until now, and starts again
    std::int64_t restart() { const std::int64_t ms = elapsed(); start(); return ms; }
    // 0 before it was started
    std::int64_t elapsed() const
    {
        return m_valid ? std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::steady_clock::now() - m_start).count() : 0;
    }
    bool isValid() const { return m_valid; }

private:
    std::chrono::steady_clock::time_point m_start;
    bool m_valid = false;
};

} // namespace util

#endif // UTIL_TIMER_HPP
