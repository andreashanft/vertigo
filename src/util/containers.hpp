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

#ifndef UTIL_CONTAINERS_HPP
#define UTIL_CONTAINERS_HPP

#include <algorithm>
#include <type_traits>
#include <utility>

// What Qt's containers had and the standard library's lack, for the code that used them
namespace util {

// the value of a key in a map, or the default without one
template <class Map>
typename Map::mapped_type value(const Map &map, const typename Map::key_type &key, const typename Map::mapped_type &defaultValue = typename Map::mapped_type())
{
    const auto it = map.find(key);
    return it != map.end() ? it->second : defaultValue;
}


// removes a key from a map and returns its value, or the default without one
template <class Map>
typename Map::mapped_type take(Map &map, const typename Map::key_type &key)
{
    const auto it = map.find(key);
    if (it == map.end())
        return typename Map::mapped_type();
    typename Map::mapped_type value = std::move(it->second);
    map.erase(it);
    return value;
}


template <class Container, class T>
bool contains(const Container &container, const T &value)
{
    return std::find(container.begin(), container.end(), value) != container.end();
}


template <class Container, class T>
void removeAll(Container &container, const T &value)
{
    container.erase(std::remove(container.begin(), container.end(), value), container.end());
}


// deletes what the pointers in a container or the values of a map point to
template <class Container>
void deleteAll(const Container &container)
{
    for (const auto &item : container) {
        if constexpr (std::is_pointer<std::decay_t<decltype(item)>>::value)
            delete item;
        else
            delete item.second;
    }
}

} // namespace util

#endif // UTIL_CONTAINERS_HPP
