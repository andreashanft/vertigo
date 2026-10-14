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

#include <QDir>
#include <QFile>
#include <QRegularExpression>


// also used by Items
String trimEnd(const String &str)
{
    int i = str.size();
    while (i > 0 && String::isSpace(str[i - 1]))
        i--;
    return str.left(i);
}


namespace txt {


DesFile::DesFile()
{
    setSection("Global");
}


DesFile::DesFile(const QString &filename)
{
    load(filename);
}


bool DesFile::load(const QString &filename)
{
    m_sections.clear();
    setSection("Global");

    QFile file(util::DataPath::resolve(filename));
    if (!file.open(QFile::ReadOnly))
        return false;

    const QRegularExpression regSection("^\\s*\\[(.*)\\]");
    // values may be followed by a ';' comment, which can contain '=' itself
    const QRegularExpression regKey("([^=]*)=([^;]*)");

    while (!file.atEnd()) {
        // the game data is 8-bit (DOS codepage 437) text; keep the raw bytes, the fonts are indexed by them
        const QByteArray bytes = file.readLine();
        const QString line = QString::fromLatin1(bytes);

        const QRegularExpressionMatch section = regSection.match(line);
        if (section.hasMatch())
            setSection(String(section.captured(1)).trimmed().toLower());

        const QRegularExpressionMatch keyValue = regKey.match(line);
        if (keyValue.hasMatch()) {
            const String key = String(keyValue.captured(1)).trimmed().toLower();
            if (!key.startsWith(";") && !key.startsWith("/"))
                setValue(key, String(keyValue.captured(2)).trimmed());
        }

        if (String::fromLatin1(bytes.constData(), int(bytes.size())).trimmed() == "{")
        {
            StringList text;
            QByteArray next = file.readLine();
            String textLine = trimEnd(String::fromLatin1(next.constData(), int(next.size())));
            while (!file.atEnd() && textLine != "}")
            {
                text.append(textLine);
                next = file.readLine();
                textLine = trimEnd(String::fromLatin1(next.constData(), int(next.size())));
            }
            setValue("{text}", text);
        }
    }

    // sections selected while parsing don't count as read
    m_read.clear();
    setSection("Global");
    return true;
}

bool DesFile::save(const QString &filename) const
{
    QDir dir;
    dir.mkpath(QFileInfo(filename).path());

    QFile file(filename);
    if (!file.open(QFile::WriteOnly))
        return false;

    // 8-bit text like the game data (see load())
    String text;
    for (const auto &section : m_sections) {
        if (section.second.empty())
            continue;

        text += "[" + section.first + "]\n";
        for (const auto &value : section.second)
            text += value.first + " = " + value.second.toString() + "\n";
        text += "\n";
    }

    return file.write(text.constData(), text.size()) == text.size();
}


void DesFile::setSection(const String &section)
{
    m_sectionName = section.toLower();
    m_section = &m_sections[m_sectionName];
    m_read[m_sectionName];
}


bool DesFile::contains(const String &key) const
{
    m_read[m_sectionName].insert(key.toLower());
    return m_section->count(key.toLower()) > 0;
}


Value DesFile::value(const String &key, const Value &defaultValue) const
{
    m_read[m_sectionName].insert(key.toLower());
    const auto it = m_section->find(key.toLower());
    return it != m_section->end() ? it->second : defaultValue;
}


void DesFile::setValue(const String &key, const Value &value)
{
    (*m_section)[key.toLower()] = value;
}


String DesFile::valueText(const String &key) const
{
    // the other languages' variants of a text count as read, too
    std::set<String> &read = m_read[m_sectionName];
    read.insert(key.toLower());
    read.insert(key.toLower() + "e");
    read.insert(key.toLower() + "f");
    if (StringTable::language() == "English")
        return value(key + "e").toString();
    return value(key).toString();
}


StringList DesFile::sections() const
{
    StringList sections;
    for (const auto &section : m_sections)
        sections << section.first;
    return sections;
}


StringList DesFile::keys() const
{
    StringList keys;
    for (const auto &value : *m_section)
        keys << value.first;
    return keys;
}


bool DesFile::wasSectionRead(const String &section) const
{
    return m_read.count(section.toLower()) > 0;
}


StringList DesFile::unreadKeys(const String &section) const
{
    const auto read = m_read.find(section.toLower());
    const auto values = m_sections.find(section.toLower());
    StringList keys;
    if (values == m_sections.end())
        return keys;
    for (const auto &value : values->second) {
        if (read == m_read.end() || read->second.count(value.first) == 0)
            keys << value.first;
    }
    return keys;
}


} // namespace txt
