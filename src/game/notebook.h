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

#ifndef GAME_NOTEBOOK_H
#define GAME_NOTEBOOK_H

#include "savedgame.h"
#include "gfx/font.h"
#include "txt/stringtable.h"
#include "ui/button.h"
#include "ui/list.h"
#include "util/timer.hpp"

#include <list>

namespace game {

class Notebook : public ui::Label
{
public:
    Notebook(std::function<void()> funcClose);

public:
    // typing a saved game's name
    bool wantsTextInput() const;
    Rect textInputArea() const;
    void keyPressEvent(input::KeyEvent *event);

private:
    void showLoadSave();
    void hideLoadSave();
    void showLoad();
    void showSave();
    void showSaveName(const SavedGame &slot);
    void confirmLoad(const SavedGame &game);
    void saveGame();
    // a row of the lists: a line, or more where it is too wide for the page, each centred; none
    // for a gap
    struct Row
    {
        std::list<ui::Button> lines;
    };
    void addRow(std::list<Row> &rows, ui::Widget *page, const String &text, std::function<void()> funcClick, bool enabled = true);
    void scroll(std::list<Row> &rows, int &first, int step, ui::Button &up, ui::Button &down);
    void updateRows(std::list<Row> &rows, int first, ui::Button &up, ui::Button &down);
    void updateSaveName();
    void showMissions();
    void hideMissions();
    void showOptions();
    void hideOptions();
    void showSound();
    void hideSound();
    void showMovies();
    void hideMovies();
    void showInput();
    void hideInput();
    void showMoviePlayer();
    void hideMoviePlayer();
    void showMap();
    void hideMap();
    void updateSound();
    void updateMovies();
    void updateInput();

private:
    void createLabel(ui::Widget *parent, txt::StringId text, float posY);
    void createButton(std::function<void()> funcClick, ui::Widget *parent, txt::StringId text, float posY);
    void setupLabel(ui::Label &label, txt::StringId text, float posY);
    void setupButton(ui::Button &button, txt::StringId text, float posY);

private:
    void draw();
    bool mousePressEvent(const Point &pos, input::MouseButton button);

private:
    gfx::Font m_fontGreen;
    gfx::Font m_fontYellow;
    gfx::Texture m_background;
    ui::Label m_lblBackground;
    ui::Label m_lblMain;
    ui::Label m_lblMissions;
    ui::Label m_lblOptions;
    ui::Label m_lblSound;
    ui::Label m_lblMovies;
    ui::Label m_lblInput;
    ui::Label m_lblMoviePlayer;
    ui::Label m_lblMap;
    ui::Button m_btnSoundCdMusic;
    ui::Button m_btnMoviesAutopilot;
    ui::Button m_btnMoviesApproach;
    ui::Button m_btnMoviesHarbour;
    ui::Button m_btnInputTouchSide;
    ui::Button m_btnInputTouchStick;
    ui::Button m_btnMoviePlayerHide;
    ui::Button m_btnMissionsBack;
    ui::List m_listMissions;
    // loading and saving: the saved games to load and the slots to save in, scrolled by the
    // arrows from the first row shown, then a save's name or the question before loading
    ui::Label m_lblLoadSave;
    ui::Label m_lblLoad;
    ui::Label m_lblSave;
    ui::Label m_lblSaveName;
    ui::Label m_lblLoadConfirm;
    ui::Button m_btnLoadUp;
    ui::Button m_btnLoadDown;
    ui::Button m_btnSaveUp;
    ui::Button m_btnSaveDown;
    ui::Label m_lblSaveNameText;
    ui::Button m_btnSaveSave;
    ui::Label m_lblLoadConfirmName;
    std::list<Row> m_loadRows;
    std::list<Row> m_saveRows;
    int m_loadFirst = 0;
    int m_saveFirst = 0;
    SavedGame m_slot;   // to save in, or to load
    String m_saveName;
    bool m_cursor = false;
    util::ElapsedTimer m_cursorTime;
    std::list<ui::Label> m_labels;
    std::list<ui::Button> m_buttons;
    std::list<ui::Button> m_movieButtons;
    std::function<void()> m_funcClose;
};

} // namespace game

#endif // GAME_NOTEBOOK_H
