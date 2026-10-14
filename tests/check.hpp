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


// The tests' checks: each compares a value with the one expected, and reports where they differ.

#ifndef TESTS_CHECK_HPP
#define TESTS_CHECK_HPP

#include <iostream>

namespace test {

inline int g_checks = 0;
inline int g_failures = 0;

template <typename Actual, typename Expected>
void equal(const Actual &actual, const Expected &expected, const char *what, int line)
{
    g_checks++;
    if (actual == expected)
        return;
    g_failures++;
    std::cerr << "line " << line << ": " << what << " is " << actual << ", expected " << expected << std::endl;
}

// the exit code
inline int result()
{
    std::cout << g_checks << " checks, " << g_failures << " failures" << std::endl;
    return g_failures == 0 ? 0 : 1;
}

} // namespace test

#define CHECK_EQUAL(actual, expected) test::equal((actual), (expected), #actual, __LINE__)
#define CHECK(condition) test::equal(bool(condition), true, #condition, __LINE__)

#endif // TESTS_CHECK_HPP
