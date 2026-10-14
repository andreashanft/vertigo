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
#include <QStringList>
#include <QRegularExpression>


QString trimEnd(const QString& str)
{
    int i;
    for (i = str.length() - 1; i >= 0 && str[i].isSpace(); i--);
    QString res = str;
    res.truncate(i + 1);
    return res;
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
        const QString line = QString::fromLatin1(file.readLine());

        const QRegularExpressionMatch section = regSection.match(line);
        if (section.hasMatch())
            setSection(section.captured(1).trimmed().toLower());

        const QRegularExpressionMatch keyValue = regKey.match(line);
        if (keyValue.hasMatch()) {
            const QString key = keyValue.captured(1).trimmed().toLower();
            if (!key.startsWith(";") && !key.startsWith("/"))
                setValue(key, keyValue.captured(2).trimmed());
        }

        if (line.trimmed() == "{")
        {
            QStringList text;
            QString textLine = trimEnd(QString::fromLatin1(file.readLine()));
            while (!file.atEnd() && textLine != "}")
            {
                text.append(textLine);
                textLine = trimEnd(QString::fromLatin1(file.readLine()));
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
    QString text;
    QMapIterator<QString, QVariantMap> itSections(m_sections);
    while (itSections.hasNext()) {
        itSections.next();
        if (itSections.value().isEmpty())
            continue;

        text += "[" + itSections.key() + "]\n";

        QMapIterator<QString, QVariant> itValues(itSections.value());
        while (itValues.hasNext()) {
            itValues.next();

            text += itValues.key() + " = " + itValues.value().toString() + "\n";
        }

        text += "\n";
    }

    const QByteArray bytes = text.toLatin1();
    return file.write(bytes) == bytes.size();
}


void DesFile::setSection(const QString &section)
{
    m_sectionName = section.toLower();
    m_section = &m_sections[m_sectionName];
    m_read[m_sectionName];
}


bool DesFile::contains(const QString &key) const
{
    m_read[m_sectionName].insert(key.toLower());
    return m_section->contains(key.toLower());
}


QVariant DesFile::value(const QString &key, const QVariant &defaultValue) const
{
    m_read[m_sectionName].insert(key.toLower());
    return m_section->value(key.toLower(), defaultValue);
}


void DesFile::setValue(const QString &key, const QVariant &value)
{
    m_section->insert(key.toLower(), value);
}


QString DesFile::valueText(const QString &key) const
{
    // the other languages' variants of a text count as read, too
    m_read[m_sectionName] << key.toLower() << key.toLower() + "e" << key.toLower() + "f";
    if (StringTable::language() == "English")
        return value(key + "e").toString();
    return value(key).toString();
}


QStringList DesFile::sections() const
{
    return m_sections.keys();
}


QStringList DesFile::keys() const
{
    return m_section->keys();
}


bool DesFile::wasSectionRead(const QString &section) const
{
    return m_read.contains(section.toLower());
}


QStringList DesFile::unreadKeys(const QString &section) const
{
    const QSet<QString> read = m_read.value(section.toLower());
    QStringList keys;
    foreach (const QString &key, m_sections.value(section.toLower()).keys()) {
        if (!read.contains(key))
            keys << key;
    }
    return keys;
}


} // namespace txt
