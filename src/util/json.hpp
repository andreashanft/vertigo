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

#ifndef UTIL_JSON_HPP
#define UTIL_JSON_HPP

#include <cstdint>
#include <map>
#include <memory>
#include <string>
#include <vector>

namespace util {

// A JSON value, as the glTF files of the 3D cockpits need it. Reading what isn't there (a missing
// key, an index past the end, a value of another type) gives an undefined value or the default,
// as QJsonValue did.
class Json
{
public:
    enum Type { Undefined, Null, Bool, Number, String, Array, Object };

public:
    Json() = default;

    // a document; on a syntax error an undefined value, and the reason and where
    static Json parse(const std::string &text, std::string *error = nullptr);

public:
    Type type() const { return m_type; }
    bool isUndefined() const { return m_type == Undefined; }

    bool toBool(bool defaultValue = false) const;
    double toDouble(double defaultValue = 0) const;
    // whole numbers in range, else the default
    int toInt(int defaultValue = 0) const;
    // whole numbers exactly, others rounded; 0 for anything but a number
    std::int64_t toInt64() const;
    // UTF-8
    const std::string& toString() const;

    // the elements of an array or the values of an object
    std::size_t size() const;
    const Json& operator[](std::size_t index) const;
    const Json& operator[](int index) const { return index < 0 ? (*this)[size()] : (*this)[std::size_t(index)]; }
    const Json& operator[](const std::string &key) const;
    const Json& operator[](const char *key) const { return (*this)[std::string(key)]; }
    bool contains(const std::string &key) const;
    const std::vector<Json>& elements() const;
    const std::map<std::string, Json>& members() const;

private:
    class Parser;

    Type m_type = Undefined;
    bool m_bool = false;
    double m_number = 0;
    // for numbers written without a fraction or an exponent, which fit
    bool m_integral = false;
    std::int64_t m_integer = 0;
    std::string m_string;
    std::shared_ptr<std::vector<Json>> m_elements;
    std::shared_ptr<std::map<std::string, Json>> m_members;
};

} // namespace util

#endif // UTIL_JSON_HPP
