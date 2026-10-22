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

#ifndef UTIL_PROCESS_HPP
#define UTIL_PROCESS_HPP

#include <string>
#include <vector>

namespace util {

// Another program, run with its output dropped: the upscaler and the tools that fetch it. Not on
// iOS, where apps can't start programs (isSupported).
class Process
{
public:
    Process() = default;
    Process(const Process &) = delete;
    Process& operator=(const Process &) = delete;
    // stops it
    ~Process();

public:
    static bool isSupported();
    // the program (a path, or a name looked up as the system does) and its arguments
    bool start(const std::vector<std::string> &arguments);
    // still running; false once it has ended, then exitCode() tells how
    bool isRunning();
    int exitCode() const { return m_exitCode; }
    void kill();

private:
#if defined(_WIN32)
    void *m_handle = nullptr;
#else
    int m_pid = -1;
#endif
    int m_exitCode = -1;
};

} // namespace util

#endif // UTIL_PROCESS_HPP
