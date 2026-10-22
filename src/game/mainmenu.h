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

#ifndef GAME_MAINMENU_H
#define GAME_MAINMENU_H

#include "menu.h"
#include "gfx/texture.h"
#include "sfx/sound.h"
#include "ui/button.h"
#include "util/timer.hpp"
#include <list>
#include "util/string.hpp"

namespace game {

class MainMenu : public Menu
{
private:
    enum State { Invalid, Presents, Title, Loading, Credits };

public:
    MainMenu(std::function<void(String)> funcStartGame, std::function<void(String)> funcLoadGame, std::function<void(int)> funcStartTraining,
             std::function<void(int)> funcStartOpenMission, std::function<void()> funcQuit);

private:
    void showMain();
    void showNew();
    void showLoad();
    void showLoadList();
    void scrollLoad(int step);
    void updateLoadList();
    void confirmLoad(const String &path, const String &name);
    void showTraining();
    void showOpenMission();
    void showOptions();
    void showLoading();
    void showCredits(int page);
    void updateDifficulty();
    void updateOptions();
    void updateUpscaled();
    void clickUpscaled();

    void draw();
    void activate();
    void deactivate();
    void changeState(State state);
    void mousePressEvent(input::MouseEvent *);
    void keyPressEvent(input::KeyEvent *);
    bool wantsTextInput() const { return m_lblNew.isVisible(); }
    Rect textInputArea() const;

private:
    State m_state;
    ui::Label m_presents;
    ui::Label m_title;
    ui::Label m_lblVersion;
    ui::Label m_lblBar1;
    ui::Label m_lblBar2;
    ui::Label m_lblMain;
    ui::Label m_lblMainTitle;
    ui::Button m_btnMainNew;
    ui::Button m_btnMainLoad;
    ui::Button m_btnMainTraining;
    ui::Button m_btnMainCredits;
    ui::Button m_btnMainOptions;
    ui::Button m_btnMainQuit;
    ui::Label m_lblNew;
    ui::Label m_lblNewTitle;
    ui::Button m_btnNewBack;
    ui::Label m_lblLoad;
    ui::Label m_lblLoadTitle;
    // the saved games, LoadRows of them at a time from m_loadFirst, and the question before one
    // is loaded
    static const int LoadRows = 4;
    static const int LoadArrowsX = 465;
    ui::Label m_lblLoadList;
    ui::Button m_btnLoadUp;
    ui::Button m_btnLoadDown;
    ui::Button m_btnLoadBack;
    ui::Label m_lblLoadConfirm;
    ui::Label m_lblLoadQuestion;
    ui::Label m_lblLoadName;
    ui::Button m_btnLoadYes;
    ui::Button m_btnLoadNo;
    std::list<ui::Label> m_lblLoadSave;
    std::list<ui::Button> m_btnLoadSave;
    int m_loadFirst = 0;
    ui::Label m_lblTraining;
    ui::Label m_lblTrainingTitle;
    ui::Button m_btnTrainingBack;
    std::list<ui::Button> m_btnTrainingMissions;
    ui::Button m_btnTrainingOpen;
    // the open mission's boats
    ui::Label m_lblOpen;
    ui::Label m_lblOpenTitle;
    ui::Button m_btnOpenBack;
    std::list<ui::Button> m_btnOpenBoats;
    ui::Label m_lblOptions;
    ui::Label m_lblOptionsTitle;
    ui::Button m_btnOptionsBack;
    ui::Button m_btnOptionsDifficulty;
    // the upscaled graphics: prepared, updated, switched on and off (gfx::Upscaled, Upscaler)
    ui::Button m_btnOptionsUpscaled;
    std::string m_upscaledSource;   // the game data's fingerprint, taken when the page opens
    util::ElapsedTimer m_upscaledTime;
    bool m_upscaledErrorShown = true;
    // the notebook's options and the touch screen's, each a button that switches it
    struct Option
    {
        std::function<bool()> value;
        std::function<void(bool)> setValue;
        String on;
        String off;
    };
    std::vector<Option> m_options;
    std::list<ui::Button> m_btnOptions;
    ui::Label m_lblLoading;
    ui::Label m_lblLoadingTitle;
    ui::Label m_lblLoadingWait;
    // the credits: four full-screen pictures, twelve seconds each
    ui::Label m_credits;
    int m_creditsPage = 0;
    util::ElapsedTimer m_creditsTime;

    sfx::Sound m_backgroundSound;
    String m_name;
    bool m_cursor;
    util::ElapsedTimer m_time;
    std::function<void(String)> m_funcStartGame;
    std::function<void(String)> m_funcLoadGame;
    std::function<void(int)> m_funcStartTraining;
    std::function<void(int)> m_funcStartOpenMission;
    String m_loadingGame;   // its file
};

} // namespace game

#endif // GAME_MAINMENU_H
