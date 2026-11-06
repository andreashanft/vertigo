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
        { "Hochskalierte Grafik: AN", "Upscaled graphics: ON" },
        { "Hochskalierte Grafik: AUS", "Upscaled graphics: OFF" },
        { "Hochskalierte Grafik: VORBEREITEN", "Upscaled graphics: PREPARE" },
        { "Hochskalierte Grafik: AKTUALISIEREN", "Upscaled graphics: UPDATE" },
        { "Grafik wird hochskaliert: %1 %", "Upscaling graphics: %1 %" },
        // (in the game's 8-bit text: \x8e Ä, \x99 Ö, \x9a Ü; split where a letter follows that
        // would extend the escape)
        { "TORPEDO ABFEUERN", "FIRE TORPEDO" },
        { "HALTEN: ZWEITE BELEGUNG", "HOLD: SECOND SET" },
        { "GESCH\x9aTZTURM-MEN\x9a", "TURRET MENU" },
        { "AUTOPILOT", "AUTOPILOT" },
        { "SCHNELLER (+10 KM/H)", "FASTER (+10 KM/H)" },
        { "WAFFENSTATUS (F1-F4)", "WEAPON STATUS (F1-F4)" },
        { "SONAR AN / AUS", "SONAR ON / OFF" },
        { "ZOOM SONARANZEIGE", "SONAR DISPLAY ZOOM" },
        { "LANGSAMER (-10 KM/H)", "SLOWER (-10 KM/H)" },
        { "PANZERUNG / SYSTEME", "ARMOR / SYSTEMS" },
        { "STOPP (0 KM/H)", "STOP (0 KM/H)" },
        { "MOTOR AN / AUS", "ENGINE ON / OFF" },
        { "STEUERN", "STEER" },
        { "BORDKANONE ABFEUERN", "FIRE GUN" },
        { "ZIEL IM VISIER AUFFASSEN", "LOCK TARGET IN SIGHT" },
        { "N\x8e" "CHSTEN GEGNER AUFFASSEN", "LOCK NEAREST ENEMY" },
        { "MISSIONSMEN\x9a", "MISSION MENU" },
        { "PAUSE", "PAUSE" },
        { "N\x8e" "CHSTER NAV-PUNKT", "NEXT NAV POINT" },
        { "NAV-PUNKT AUFFASSEN", "LOCK NAV POINT" },
        { "N\x8e" "CHSTES TORPEDO", "NEXT TORPEDO" },
        { "VOLLE FAHRT ZUR\x9a" "CK", "FULL ASTERN" },
        { "BUZZER ABWERFEN", "DROP BUZZER" },
        { "VOLLE FAHRT VORAUS", "FULL AHEAD" },
        { "BOOSTER", "BOOSTER" },
        { "UMSEHEN / KAMERA", "LOOK AROUND / CAMERA" },
        { "KAMERA-ZOOM", "CAMERA ZOOM" },
        { "ANSICHT WECHSELN", "CHANGE VIEW" },
    };
    return String::fromLatin1(texts[string][m_language == "English" ? 1 : 0]);
}


} // namespace txt
