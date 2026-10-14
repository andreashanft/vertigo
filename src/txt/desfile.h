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

#ifndef TXT_DESFILE_H
#define TXT_DESFILE_H


#include "value.h"
#include "util/string.hpp"
#include <map>
#include <set>
#include <string>


namespace txt {


class DesFile
{
public:
    DesFile();
    // a data file ("vfx:scenario/s00101.des") or a path in UTF-8, e.g. of a saved game
    DesFile(const std::string &filename);
    DesFile(const String &filename) : DesFile(filename.toUtf8()) {}
    DesFile(const char *filename) : DesFile(std::string(filename)) {}

public:
    bool load(const std::string &filename);
    bool load(const String &filename) { return load(filename.toUtf8()); }
    bool load(const char *filename) { return load(std::string(filename)); }
    bool save(const std::string &filename) const;
    void setSection(const String &section);
    bool contains(const String &key) const;
    Value value(const String &key, const Value &defaultValue = Value()) const;
    void setValue(const String &key, const Value &value);
    String valueText(const String &key) const;
    StringList sections() const;
    StringList keys() const;

    // for load reports: sections never selected and keys never queried
    bool wasSectionRead(const String &section) const;
    StringList unreadKeys(const String &section) const;

private:
    std::map<String, std::map<String, Value>> m_sections;
    std::map<String, Value> *m_section;
    String m_sectionName;
    mutable std::map<String, std::set<String>> m_read;
};


} // namespace txt


#endif // TXT_DESFILE_H
