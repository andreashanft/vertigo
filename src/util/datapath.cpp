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

#include "datapath.h"

#include <QDir>
#include <QDirIterator>
#include <QHash>

namespace util {

namespace {

// lower-case relative path -> relative path as on disk, for every file and
// directory in the data directory (about 4,300 entries)
QHash<QString, QString> s_entries;

const char *const s_parts[] = { "dat", "gfx", "sfx", "txt", "vfx" };

} // namespace

QString DataPath::m_directory;

void DataPath::set(const QString &directory)
{
    m_directory = QDir::cleanPath(directory);
    s_entries.clear();
    if (m_directory.isEmpty())
        return;

    const QDir root(m_directory);
    QDirIterator it(m_directory, QDir::AllEntries | QDir::NoDotAndDotDot | QDir::Hidden, QDirIterator::Subdirectories);
    while (it.hasNext()) {
        const QString relative = root.relativeFilePath(it.next());
        s_entries.insert(relative.toLower(), relative);
    }
}

QString DataPath::resolve(const QString &name)
{
    QString relative;
    for (const char *part : s_parts) {
        const QString prefix = QString(part);
        if (name.startsWith(prefix) && name.length() > prefix.length() && (name[prefix.length()] == ':' || name[prefix.length()] == '/')) {
            relative = prefix + '/' + name.mid(prefix.length() + 1);
            break;
        }
    }
    if (relative.isEmpty())
        return name;

    relative = QDir::cleanPath(relative.replace('\\', '/'));
    return m_directory + '/' + s_entries.value(relative.toLower(), relative);
}

} // namespace util
