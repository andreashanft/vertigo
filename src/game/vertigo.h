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

#ifndef GAME_VERTIGO_H
#define GAME_VERTIGO_H

#include "chapter.h"
#include "mainmenu.h"
#include "movie.h"
#include "window.h"

#include "gfx/fontmanager.h"
#include "sfx/soundsystem.h"

#include <QKeySequence>

namespace game {

class Vertigo
{
public:
    struct Options
    {
        QString scenario;                               // start this mission directly (test mode)
        int boat = 0;                                   // test mode boat type, 0 = chapter 1 boat
        std::vector<std::pair<QString, int>> equipment; // test mode items: mounting, model
        int timeout = 0;                                // test mode time limit in seconds
        bool debugOverlay = false;
        std::vector<int> destroy;                       // test mode: destroy these units after one second
        std::vector<std::pair<float, QKeySequence>> keys; // test mode: press these keys at these times (seconds)
        struct Drive { int id; float speed; float turn; };
        std::vector<Drive> drive;                       // test mode: let these boats move straight or in a circle
        bool check = false;                             // report what the mission loader ignores
        QStringList checkScenarios;                     // empty = all missions
    };

    enum ExitCode
    {
        ExitSuccess = 0,
        ExitFailed = 1,
        ExitAborted = 2,
        ExitTimeout = 3,
        ExitDied = 4,
        ExitUsage = 64,
    };

public:
    Vertigo();

public:
    int start(const Options &options);

private:
    int check(const QStringList &names);

    void startGame(const QString &name);
    void loadGame(const QString &name);
    void endGame();
    void introFinished();
    void createMainMenu(bool skipToTitle);
    void createChapter();

private:
    Window m_window;
    sfx::SoundSystem m_soundSystem;
    gfx::FontManager m_fontManager;
    MainMenu m_mainMenu;
    std::unique_ptr<Movie> m_intro;
    std::unique_ptr<Chapter> m_chapter;
    QString m_name;
};

} // namespace game

#endif // GAME_VERTIGO_H
