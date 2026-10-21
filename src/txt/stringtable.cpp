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

#include "desfile.h"
#include "stringtable.h"
#include "util/datapath.h"
#include "util/path.hpp"


namespace txt {


String StringTable::m_language;
StringList StringTable::m_tableDesktop;
StringList StringTable::m_tableFight;


bool StringTable::load()
{
    if (util::exists(util::DataPath::resolve("txt:german.lan")))
        m_language = "German";
    if (util::exists(util::DataPath::resolve("txt:english.lan")))
        m_language = "English";

    DesFile fileDes("txt:deeptext.des");
    fileDes.setSection("Text");

    if (!fileDes.contains("Text190"))
        return false;

    for (int i = 0; ; i++) {
        const String key = String("Text%1").arg(i);
        if (!fileDes.contains(key))
            break;

        m_tableDesktop.append(fileDes.value(key).toString());
    }

    fileDes.load("txt:fightext.des");
    fileDes.setSection("Text");

    for (int i = 0; ; i++) {
        const String key = String("Text%1").arg(i);
        if (!fileDes.contains(key))
            break;

        m_tableFight.append(fileDes.value(key).toString());
    }

    return true;
}


String StringTable::get(StringId string)
{
    return m_tableDesktop.at(string);
}

String StringTable::get(StringFight string)
{
    return m_tableFight.at(string);
}

String StringTable::get(StringVertigo string)
{
    static const char *const texts[][2] = {
        { "Touch-Stick: LINKS", "Touch stick: LEFT" },
        { "Touch-Stick: RECHTS", "Touch stick: RIGHT" },
        { "Touch-Stick vertikal: NORMAL", "Touch stick vertical: NORMAL" },
        { "Touch-Stick vertikal: UMGEKEHRT", "Touch stick vertical: INVERTED" },
        { "OFFENE MISSION", "OPEN MISSION" },
        { "MISSIONEN", "MISSIONS" },
        { "Neuer Speicherstand", "New saved game" },
        { "Autosave", "Autosave" },
        { "Auto", "Auto" },
    };
    return texts[string][m_language == "English" ? 1 : 0];
}


} // namespace txt
