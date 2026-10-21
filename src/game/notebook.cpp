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

#include "notebook.h"

#include "chapter.h"

#include "gfx/colortable.h"
#include "gfx/image.h"
#include "hud/touchcontrols.h"
#include "sfx/music.hpp"
#include "util/datetimetext.hpp"

#include <algorithm>

namespace game {

// the pages' width, and where the rows of the lists go: down to the arrows' line
static const int PageWidth = 304;
static const int RowsTop = 50;
static const int RowsBottom = 262;
static const int RowHeight = 10;
static const int ArrowsY = 270;
// the original's arrows are pieces of the notebook's font
static const char *const ArrowUp = "\xe3\xe4\xe5\xe6";
static const char *const ArrowDown = "\xe7\xe8\xe9\xea";

Notebook::Notebook(std::function<void()> funcClose) :
    m_lblBackground(this),
    m_lblMain(&m_lblBackground),
    m_lblMissions(&m_lblBackground, false),
    m_lblOptions(&m_lblBackground, false),
    m_lblSound(&m_lblBackground, false),
    m_lblMovies(&m_lblBackground, false),
    m_lblInput(&m_lblBackground, false),
    m_lblMoviePlayer(&m_lblBackground, false),
    m_lblMap(&m_lblBackground, false),
    m_btnSoundCdMusic([this]() { sfx::Music::setEnabled(!sfx::Music::isEnabled()); updateSound(); }, &m_lblSound),
    m_btnMoviesAutopilot([this]() { Chapter::get()->toggleMovieAutopilot(); updateMovies(); }, &m_lblMovies),
    m_btnMoviesApproach([this]() { Chapter::get()->toggleMovieApproach(); updateMovies(); }, &m_lblMovies),
    m_btnMoviesHarbour([this]() { Chapter::get()->toggleMovieHarbour(); updateMovies(); }, &m_lblMovies),
    m_btnInputTouchSide([this]() { hud::TouchControls::setMirrored(!hud::TouchControls::isMirrored()); updateInput(); }, &m_lblInput),
    m_btnInputTouchStick([this]() { hud::TouchControls::setStickInverted(!hud::TouchControls::isStickInverted()); updateInput(); }, &m_lblInput),
    m_btnMoviePlayerHide([this]() { hideMoviePlayer(); }, &m_lblMoviePlayer),
    m_btnMissionsBack([this]() { hideMissions(); }, &m_lblMissions),
    m_listMissions(&m_lblMissions),
    m_lblLoadSave(&m_lblBackground, false),
    m_lblLoad(&m_lblBackground, false),
    m_lblSave(&m_lblBackground, false),
    m_lblSaveName(&m_lblBackground, false),
    m_lblLoadConfirm(&m_lblBackground, false),
    m_btnLoadUp([this]() { scroll(m_loadRows, m_loadFirst, -1, m_btnLoadUp, m_btnLoadDown); }, &m_lblLoad),
    m_btnLoadDown([this]() { scroll(m_loadRows, m_loadFirst, 1, m_btnLoadUp, m_btnLoadDown); }, &m_lblLoad),
    m_btnSaveUp([this]() { scroll(m_saveRows, m_saveFirst, -1, m_btnSaveUp, m_btnSaveDown); }, &m_lblSave),
    m_btnSaveDown([this]() { scroll(m_saveRows, m_saveFirst, 1, m_btnSaveUp, m_btnSaveDown); }, &m_lblSave),
    m_lblSaveNameText(&m_lblSaveName),
    m_btnSaveSave([this]() { saveGame(); }, &m_lblSaveName),
    m_lblLoadConfirmName(&m_lblLoadConfirm),
    m_funcClose(std::move(funcClose))
{
    const gfx::ColorTable colorTable("gfx:pal/notebook/notebook.pal");
    m_fontGreen.load("gfx:fnt/nfont1a.fnt", colorTable, true);
    m_fontYellow.load("gfx:fnt/nfont1b.fnt", colorTable, true);
    m_background.fromImage(gfx::Image::load("gfx:img/desktop/notebook/noteback.img", colorTable));

    gfx::Image image(640, 480, gfx::Image::Indexed8);
    image.setColorTable(gfx::ColorTable{ gfx::rgba(0, 0, 0, 128) });
    setTexture(image);

    m_lblBackground.setPosition(48, 48);
    m_lblBackground.setTexture(gfx::Image::loadPCX("gfx:pic/notebook/notebook.pcx"));

    {
        m_lblMain.setPosition(162, 73);
        m_lblMain.setTexture(m_background);

        createLabel(&m_lblMain, txt::Notebook_Title, 30);
        createLabel(&m_lblMain, txt::Notebook_TitleLine, 40);

        createButton([this]() { showMissions(); }, &m_lblMain, txt::Notebook_Missions, 90);
        createButton([this]() { showLoadSave(); }, &m_lblMain, txt::Notebook_LoadSave, 110);
        createButton([this]() { showOptions(); }, &m_lblMain, txt::Notebook_Options, 130);
        createButton([this]() { showMap(); }, &m_lblMain, txt::Notebook_Map, 150);
        createButton(m_funcClose, &m_lblMain, txt::Notebook_Back, 170);
        createButton([]() { Chapter::get()->quit(); }, &m_lblMain, txt::Notebook_QuitGame, 210);
    }

    {
        m_lblOptions.setPosition(162, 73);
        m_lblOptions.setTexture(m_background);

        createLabel(&m_lblOptions, txt::Notebook_Options_Title, 30);
        createLabel(&m_lblOptions, txt::Notebook_Options_TitleLine, 40);

        createButton([this]() { showSound(); }, &m_lblOptions, txt::Notebook_Sound, 90);
        createButton([]() {}, &m_lblOptions, txt::Notebook_Graphics, 110);
        createButton([this]() { showMovies(); }, &m_lblOptions, txt::Notebook_Movies, 130);
        createButton([this]() { showInput(); }, &m_lblOptions, txt::Notebook_InputDevices, 150);
        createButton([this]() { showMoviePlayer(); }, &m_lblOptions, txt::Notebook_MoviePlayer, 170);
        createButton([this]() { hideOptions(); }, &m_lblOptions, txt::Notebook_Back, 220);
    }

    {
        m_lblSound.setPosition(162, 73);
        m_lblSound.setTexture(m_background);

        createLabel(&m_lblSound, txt::Notebook_Sound_Title, 30);
        createLabel(&m_lblSound, txt::Notebook_Sound_TitleLine, 40);

        setupButton(m_btnSoundCdMusic, txt::Notebook_Sound_CdMusic_On, 90);

        createButton([this]() { hideSound(); }, &m_lblSound, txt::Notebook_Back, 130);

        updateSound();
    }

    {
        // (the original's were the joystick's; Vertigo's are the touch screen's)
        m_lblInput.setPosition(162, 73);
        m_lblInput.setTexture(m_background);

        createLabel(&m_lblInput, txt::Notebook_InputDevices_Title, 30);
        createLabel(&m_lblInput, txt::Notebook_InputDevices_TitleLine, 40);

        setupButton(m_btnInputTouchSide, txt::Notebook_Back, 90);
        setupButton(m_btnInputTouchStick, txt::Notebook_Back, 110);

        createButton([this]() { hideInput(); }, &m_lblInput, txt::Notebook_Back, 150);

        updateInput();
    }

    {
        m_lblMovies.setPosition(162, 73);
        m_lblMovies.setTexture(m_background);

        createLabel(&m_lblMovies, txt::Notebook_Movies_Title, 30);
        createLabel(&m_lblMovies, txt::Notebook_Movies_TitleLine, 40);

        setupButton(m_btnMoviesAutopilot, txt::Notebook_Movies_Autopilot_Yes, 90);
        setupButton(m_btnMoviesApproach, txt::Notebook_Movies_Approach_Yes, 110);
        setupButton(m_btnMoviesHarbour, txt::Notebook_Movies_Harbour_Yes, 130);

        createButton([this]() { hideMovies(); }, &m_lblMovies, txt::Notebook_Back, 170);

        updateMovies();
    }

    {
        m_lblMap.setPosition(164, 75);
        m_lblMap.setTexture(gfx::Image::load(String("gfx:pic/notebook/%1.r16").arg(Chapter::get()->area()->map()), 304, 284));
    }

    {
        m_lblMoviePlayer.setPosition(162, 73);
        m_lblMoviePlayer.setTexture(m_background);

        createLabel(&m_lblMoviePlayer, txt::Notebook_MoviePlayer_Title, 10);
        createLabel(&m_lblMoviePlayer, txt::Notebook_MoviePlayer_TitleLine, 20);

        setupButton(m_btnMoviePlayerHide, txt::Notebook_Back, 30);
    }

    {
        m_lblMissions.setPosition(162, 73);
        m_lblMissions.setTexture(m_background);

        createLabel(&m_lblMissions, txt::Notebook_Missions_Title, 10);
        createLabel(&m_lblMissions, txt::Notebook_Missions_TitleLine, 20);

        m_listMissions.setFont(m_fontYellow);
        m_listMissions.setPosition(0, 50);
        m_listMissions.setSize(304, 220);

        m_btnMissionsBack.setFont(m_fontGreen);
        m_btnMissionsBack.setText(txt::StringTable::get(txt::Notebook_Back));
        m_btnMissionsBack.setPosition(180, ArrowsY);
    }

    {
        // loading and saving, as in the original (docs/ROADMAP.md, appendix U): the saved games
        // by slot, any number of them, and the arrows and the way back on the bottom line as on
        // the missions' page
        for (ui::Label *page : { &m_lblLoadSave, &m_lblLoad, &m_lblSave, &m_lblSaveName, &m_lblLoadConfirm }) {
            page->setPosition(162, 73);
            page->setTexture(m_background);
        }

        createLabel(&m_lblLoadSave, txt::Notebook_LoadSave_Title, 10);
        createLabel(&m_lblLoadSave, txt::Notebook_LoadSave_TitleLine, 20);
        createButton([this]() { showLoad(); }, &m_lblLoadSave, txt::Notebook_LoadGame, 70);
        createButton([this]() { showSave(); }, &m_lblLoadSave, txt::Notebook_SaveGame, 90);
        createButton([this]() { hideLoadSave(); }, &m_lblLoadSave, txt::Notebook_Back, 110);

        createLabel(&m_lblLoad, txt::Notebook_Load_Title, 10);
        createLabel(&m_lblLoad, txt::Notebook_Load_TitleLine, 20);
        createLabel(&m_lblSave, txt::Notebook_Save_Title, 10);
        createLabel(&m_lblSave, txt::Notebook_Save_TitleLine, 20);
        for (ui::Button *arrow : { &m_btnLoadUp, &m_btnLoadDown, &m_btnSaveUp, &m_btnSaveDown }) {
            const bool up = arrow == &m_btnLoadUp || arrow == &m_btnSaveUp;
            arrow->setFont(m_fontGreen);
            arrow->setText(String::fromLatin1(up ? ArrowUp : ArrowDown));
            arrow->setPosition(up ? 20 : 99, ArrowsY);
            arrow->setDisabledOpacity(0.5f);
        }
        for (ui::Label *page : { &m_lblLoad, &m_lblSave }) {
            m_buttons.emplace_back([this]() { showLoadSave(); }, page);
            m_buttons.back().setFont(m_fontGreen);
            m_buttons.back().setText(txt::StringTable::get(txt::Notebook_Back));
            m_buttons.back().setPosition(180, ArrowsY);
        }

        // a save's name: typed, with a cursor, over a dotted line; it can't be saved without one
        createLabel(&m_lblSaveName, txt::Notebook_Save_Title, 10);
        createLabel(&m_lblSaveName, txt::Notebook_Save_TitleLine, 20);
        createLabel(&m_lblSaveName, txt::Notebook_Save_EnterName, 80);
        m_lblSaveNameText.setFont(m_fontYellow);
        m_lblSaveNameText.setPosition(0, 110);
        m_lblSaveNameText.setWidth(PageWidth);
        m_lblSaveNameText.setAlignment(ui::Label::AlignHCenter);
        m_labels.emplace_back(&m_lblSaveName);
        m_labels.back().setFont(m_fontYellow);
        m_labels.back().setText(String(24, '.'));
        m_labels.back().setPosition(0, 114);
        m_labels.back().setWidth(PageWidth);
        m_labels.back().setAlignment(ui::Label::AlignHCenter);
        setupButton(m_btnSaveSave, txt::Notebook_Save_Save, 170);
        m_btnSaveSave.setDisabledOpacity(0.5f);
        createButton([this]() { m_lblSaveName.hide(); m_lblSave.show(); }, &m_lblSaveName, txt::Notebook_Save_Cancel, 190);

        // the question before a game is loaded
        createLabel(&m_lblLoadConfirm, txt::Notebook_Load_Title, 10);
        createLabel(&m_lblLoadConfirm, txt::Notebook_Load_TitleLine, 20);
        createLabel(&m_lblLoadConfirm, txt::LoadGame_Question, 60);
        m_lblLoadConfirmName.setFont(m_fontYellow);
        m_lblLoadConfirmName.setPosition(0, 80);
        m_lblLoadConfirmName.setWidth(PageWidth);
        m_lblLoadConfirmName.setAlignment(ui::Label::AlignHCenter);
        createButton([this]() { Chapter::get()->loadGame(m_slot.file); }, &m_lblLoadConfirm, txt::Yes, 120);
        createButton([this]() { m_lblLoadConfirm.hide(); m_lblLoad.show(); }, &m_lblLoadConfirm, txt::No, 140);
    }
}

bool Notebook::wantsTextInput() const
{
    return isVisible() && m_lblSaveName.isVisible();
}

Rect Notebook::textInputArea() const
{
    // the name on its line, with some room around it
    return Rect(48 + 162, 48 + 73 + 104, PageWidth, 24);
}

void Notebook::keyPressEvent(input::KeyEvent *event)
{
    if (!wantsTextInput())
        return;

    switch (event->key()) {
    case input::Key_Return:
    case input::Key_Enter:
        saveGame();
        return;

    case input::Key_Escape:
        m_lblSaveName.hide();
        m_lblSave.show();
        return;

    case input::Key_Backspace:
        m_saveName.chop(1);
        break;

    default:
        break;
    }

    // letters, digits, spaces and some punctuation (none that the saved game's file would take
    // for more than text); the umlauts as the game's texts and fonts have them
    static const std::pair<unsigned char, unsigned char> umlauts[] = {
        { 0xe4, 0x84 }, { 0xf6, 0x94 }, { 0xfc, 0x81 }, { 0xc4, 0x8e }, { 0xd6, 0x99 }, { 0xdc, 0x9a }, { 0xdf, 0xe1 }
    };
    for (unsigned char c : event->text().toLatin1()) {
        for (const auto &umlaut : umlauts) {
            if (c == umlaut.first)
                c = umlaut.second;
        }
        if ((c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') || (c >= '0' && c <= '9') || c >= 0x80 || String(" -.,!?'()&+:/").contains(char(c)))
            m_saveName += String(1, char(c));
    }
    if (m_saveName.length() > 20)
        m_saveName.chop(m_saveName.length() - 20);
    updateSaveName();
}

void Notebook::showLoadSave()
{
    m_lblMain.hide();
    m_lblLoad.hide();
    m_lblSave.hide();
    m_lblSaveName.hide();
    m_lblLoadConfirm.hide();
    m_lblLoadSave.show();
}

void Notebook::hideLoadSave()
{
    m_lblLoadSave.hide();
    m_lblMain.show();
}

void Notebook::showLoad()
{
    m_lblLoadSave.hide();
    m_lblLoad.show();

    // as name: station - date time (Auto)
    m_loadRows.clear();
    for (const SavedGame &game : Chapter::savedGames()) {
        String row = String("%1: %2 - %3").arg(game.name, game.station, util::shortDateTime(game.time));
        if (game.autosave)
            row = String("%1 (%2)").arg(row, txt::StringTable::get(txt::AutosaveShort));
        addRow(m_loadRows, &m_lblLoad, row, [this, game]() { confirmLoad(game); });
    }
    m_loadFirst = 0;
    updateRows(m_loadRows, m_loadFirst, m_btnLoadUp, m_btnLoadDown);
}

void Notebook::showSave()
{
    m_lblLoadSave.hide();
    m_lblSave.show();

    // the saves by hand can be saved over, the autosaves not; below them, after a gap, a new one
    m_saveRows.clear();
    for (const SavedGame &game : Chapter::savedGames()) {
        if (game.autosave)
            addRow(m_saveRows, &m_lblSave, String("%1 (%2)").arg(game.name, txt::StringTable::get(txt::AutosaveShort)), []() {}, false);
        else
            addRow(m_saveRows, &m_lblSave, game.name, [this, game]() { showSaveName(game); });
    }
    if (!m_saveRows.empty())
        m_saveRows.emplace_back();
    addRow(m_saveRows, &m_lblSave, txt::StringTable::get(txt::NewSave), [this]() { showSaveName(Chapter::freeSlot()); });
    m_saveFirst = 0;
    updateRows(m_saveRows, m_saveFirst, m_btnSaveUp, m_btnSaveDown);
}

void Notebook::showSaveName(const SavedGame &slot)
{
    // a save's name to change, none for a new one
    m_slot = slot;
    m_saveName = slot.name;
    m_lblSave.hide();
    m_lblSaveName.show();
    m_cursor = true;
    m_cursorTime.start();
    updateSaveName();
}

void Notebook::confirmLoad(const SavedGame &game)
{
    m_slot = game;
    m_lblLoadConfirmName.setText(String("---- %1 ----").arg(game.name));
    m_lblLoad.hide();
    m_lblLoadConfirm.show();
}

void Notebook::saveGame()
{
    const String name = m_saveName.trimmed();
    if (name.isEmpty())
        return;
    Chapter::get()->saveGame(m_slot, name);
    showLoadSave();
}

void Notebook::updateSaveName()
{
    m_lblSaveNameText.setText(m_saveName + (m_cursor ? "_" : " "));
    m_btnSaveSave.setEnabled(!m_saveName.trimmed().isEmpty());
}

void Notebook::addRow(std::list<Row> &rows, ui::Widget *page, const String &text, std::function<void()> funcClick, bool enabled)
{
    // the words that fit on the page on each line
    StringList lines;
    for (const String &word : text.split(" ")) {
        const String line = lines.empty() ? word : lines.back() + " " + word;
        if (lines.empty())
            lines.append(line);
        else if (m_fontGreen.width(line) > PageWidth)
            lines.append(word);
        else
            lines.back() = line;
    }

    rows.emplace_back();
    for (const String &line : lines) {
        rows.back().lines.emplace_back(funcClick, page);
        ui::Button &button = rows.back().lines.back();
        button.setFont(m_fontGreen);
        button.setText(line);
        button.setWidth(PageWidth);
        button.setAlignment(ui::Label::AlignHCenter);
        button.setEnabled(enabled);
        button.setDisabledOpacity(0.5f);
    }
}

void Notebook::scroll(std::list<Row> &rows, int &first, int step, ui::Button &up, ui::Button &down)
{
    first = std::clamp(first + step, 0, std::max(0, int(rows.size()) - 1));
    updateRows(rows, first, up, down);
}

void Notebook::updateRows(std::list<Row> &rows, int first, ui::Button &up, ui::Button &down)
{
    // from the first row shown, as many as fit above the arrows
    int y = RowsTop;
    int i = 0;
    bool full = false;
    for (Row &row : rows) {
        const int height = RowHeight*std::max(1, int(row.lines.size()));
        const bool shown = i++ >= first && !full && y + height <= RowsBottom;
        if (i > first && !shown)
            full = true;
        int lineY = y;
        for (ui::Button &line : row.lines) {
            line.setVisible(shown);
            line.setPosition(0, lineY);
            lineY += RowHeight;
        }
        if (shown)
            y += height;
    }
    up.setEnabled(first > 0);
    down.setEnabled(full);
}

void Notebook::createLabel(ui::Widget *parent, txt::StringId text, float posY)
{
    m_labels.emplace_back(parent);
    setupLabel(m_labels.back(), text, posY);
}

void Notebook::createButton(std::function<void()> funcClick, ui::Widget *parent, txt::StringId text, float posY)
{
    m_buttons.emplace_back(funcClick, parent);
    setupButton(m_buttons.back(), text, posY);
}

void Notebook::setupLabel(ui::Label &label, txt::StringId text, float posY)
{
    label.setFont(m_fontYellow);
    label.setText(txt::StringTable::get(text));
    label.setPosition(0, posY);
    label.setWidth(304);
    label.setAlignment(ui::Label::AlignHCenter);
}

void Notebook::setupButton(ui::Button &button, txt::StringId text, float posY)
{
    setupLabel(button, text, posY);
    button.setFont(m_fontGreen);
}

void Notebook::showMissions()
{
    m_lblMain.hide();
    m_lblMissions.show();

    StringList tasks;
    for (const Task &task : Chapter::get()->tasks()) {
        switch (task.type()) {
        case Task::Tip:
            tasks.append(txt::StringTable::get(txt::Notebook_Tip));
            break;

        case Task::Job:
            tasks.append(txt::StringTable::get(txt::Notebook_Job));
            break;

        case Task::Mission:
            tasks.append(txt::StringTable::get(txt::Notebook_Mission));
            break;
        }

        tasks.append(task.text());
        tasks.append("");
    }

    m_listMissions.setText(tasks);
}

void Notebook::hideMissions()
{
    m_lblMissions.hide();
    m_lblMain.show();
}

void Notebook::showOptions()
{
    m_lblMain.hide();
    m_lblOptions.show();
}

void Notebook::hideOptions()
{
    m_lblOptions.hide();
    m_lblMain.show();
}

void Notebook::showSound()
{
    m_lblOptions.hide();
    m_lblSound.show();
}

void Notebook::hideSound()
{
    m_lblSound.hide();
    m_lblOptions.show();
}

void Notebook::showMovies()
{
    m_lblOptions.hide();
    m_lblMovies.show();
}

void Notebook::hideMovies()
{
    m_lblMovies.hide();
    m_lblOptions.show();
}

void Notebook::showInput()
{
    m_lblOptions.hide();
    m_lblInput.show();
}

void Notebook::hideInput()
{
    m_lblInput.hide();
    m_lblOptions.show();
}

void Notebook::showMoviePlayer()
{
    m_lblOptions.hide();
    m_lblMoviePlayer.show();

    m_movieButtons.clear();
    // in the order of their numbers (they were in the order of a hash before, which changed from
    // run to run)
    const std::set<int>& movies = Chapter::get()->playedMovies();
    int y = 40;
    for (int movie : movies) {
        m_movieButtons.emplace_back([this, movie]() { Chapter::get()->playMovie(movie); }, &m_lblMoviePlayer);
        setupButton(m_movieButtons.back(), txt::StringId(txt::FirstMovie + movie), y);
        y += 10;
    }
    m_btnMoviePlayerHide.setPosition(0, y + 10);
}

void Notebook::hideMoviePlayer()
{
    m_lblMoviePlayer.hide();
    m_lblOptions.show();
}

void Notebook::showMap()
{
    m_lblMain.hide();
    m_lblMap.show();
}

void Notebook::hideMap()
{
    m_lblMap.hide();
    m_lblMain.show();
}

void Notebook::updateSound()
{
    m_btnSoundCdMusic.setText(txt::StringTable::get(sfx::Music::isEnabled() ? txt::Notebook_Sound_CdMusic_On : txt::Notebook_Sound_CdMusic_Off));
}

void Notebook::updateMovies()
{
    txt::StringId autopilot = txt::Notebook_Movies_Autopilot_No;
    if (Chapter::get()->movieAutopilot())
        autopilot = txt::Notebook_Movies_Autopilot_Yes;
    m_btnMoviesAutopilot.setText(txt::StringTable::get(autopilot));

    txt::StringId approach = txt::Notebook_Movies_Approach_No;
    if (Chapter::get()->movieApproach())
        approach = txt::Notebook_Movies_Approach_Yes;
    m_btnMoviesApproach.setText(txt::StringTable::get(approach));

    txt::StringId harbour = txt::Notebook_Movies_Harbour_No;
    if (Chapter::get()->movieHarbour())
        harbour = txt::Notebook_Movies_Harbour_Yes;
    m_btnMoviesHarbour.setText(txt::StringTable::get(harbour));
}

void Notebook::updateInput()
{
    m_btnInputTouchSide.setText(txt::StringTable::get(hud::TouchControls::isMirrored() ? txt::Touch_Stick_Right : txt::Touch_Stick_Left));
    m_btnInputTouchStick.setText(txt::StringTable::get(hud::TouchControls::isStickInverted() ? txt::Touch_Stick_Inverted : txt::Touch_Stick_Normal));
}

void Notebook::draw()
{
    // the cursor blinks while a name is typed
    if (m_lblSaveName.isVisible() && m_cursorTime.elapsed() > 300) {
        m_cursorTime.start();
        m_cursor = !m_cursor;
        updateSaveName();
    }
    ui::Label::draw();
}

bool Notebook::mousePressEvent(const Point &pos, input::MouseButton button)
{
    if (m_lblMap.isVisible()) {
        hideMap();
        return true;
    }

    if (button == input::RightButton) {
        if (m_lblLoadConfirm.isVisible()) {
            m_lblLoadConfirm.hide();
            m_lblLoad.show();
            return true;
        }
        if (m_lblSaveName.isVisible()) {
            m_lblSaveName.hide();
            m_lblSave.show();
            return true;
        }
        if (m_lblLoad.isVisible() || m_lblSave.isVisible()) {
            showLoadSave();
            return true;
        }
        if (m_lblLoadSave.isVisible()) {
            hideLoadSave();
            return true;
        }
        if (m_lblMissions.isVisible()) {
            hideMissions();
            return true;
        }
        if (m_lblOptions.isVisible()) {
            hideOptions();
            return true;
        }
        if (m_lblSound.isVisible()) {
            hideSound();
            return true;
        }
        if (m_lblMovies.isVisible()) {
            hideMovies();
            return true;
        }
        if (m_lblInput.isVisible()) {
            hideInput();
            return true;
        }
        if (m_lblMoviePlayer.isVisible()) {
            hideMoviePlayer();
            return true;
        }
        if (m_lblMain.isVisible()) {
            m_funcClose();
            return true;
        }
    }

    return false;
}

} // namespace game
