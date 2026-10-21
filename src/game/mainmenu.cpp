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

#include "chapter.h"
#include "fight/scenario.h"
#include "hud/touchcontrols.h"
#include "sfx/music.hpp"
#include "mainmenu.h"
#include "openmission.h"
#include "training.h"
#include "gfx/colortable.h"
#include "gfx/image.h"
#include "txt/stringtable.h"
#include "ui/button.h"
#include "window.h"
#include "util/error.h"
#include "gfx/gl.hpp"
#include "util/datetimetext.hpp"
#include "util/settings.hpp"
#include <algorithm>

namespace game {

MainMenu::MainMenu(std::function<void(String)> funcStartGame, std::function<void(String)> funcLoadGame, std::function<void(int)> funcStartTraining,
                   std::function<void(int)> funcStartOpenMission, std::function<void()> funcQuit) :
    m_state(Invalid),
    m_lblVersion(&m_title),
    m_lblBar1(&m_title),
    m_lblBar2(&m_title),
    m_lblMain(&m_title),
    m_lblMainTitle(&m_lblMain),
    m_btnMainNew([this]() { showNew(); }, &m_lblMain),
    m_btnMainLoad([this]() { showLoad(); }, &m_lblMain),
    m_btnMainTraining([this]() { showTraining(); }, &m_lblMain),
    m_btnMainCredits([this]() { changeState(Credits); }, &m_lblMain),
    m_btnMainOptions([this]() { showOptions(); }, &m_lblMain),
    m_btnMainQuit(funcQuit, &m_lblMain),
    m_lblNew(&m_title, false),
    m_lblNewTitle(&m_lblNew),
    m_btnNewBack([this]() { showMain(); }, &m_lblNew),
    m_lblLoad(&m_title, false),
    m_lblLoadTitle(&m_lblLoad),
    m_lblLoadList(&m_lblLoad),
    m_btnLoadUp([this]() { scrollLoad(-1); }, &m_lblLoadList),
    m_btnLoadDown([this]() { scrollLoad(1); }, &m_lblLoadList),
    m_btnLoadBack([this]() { showMain(); }, &m_lblLoadList),
    m_lblLoadConfirm(&m_lblLoad, false),
    m_lblLoadQuestion(&m_lblLoadConfirm),
    m_lblLoadName(&m_lblLoadConfirm),
    m_btnLoadYes([this]() { changeState(Loading); }, &m_lblLoadConfirm),
    m_btnLoadNo([this]() { showLoadList(); }, &m_lblLoadConfirm),
    m_lblTraining(&m_title, false),
    m_lblTrainingTitle(&m_lblTraining),
    m_btnTrainingBack([this]() { showMain(); }, &m_lblTraining),
    m_btnTrainingOpen([this]() { showOpenMission(); }, &m_lblTraining),
    m_lblOpen(&m_title, false),
    m_lblOpenTitle(&m_lblOpen),
    m_btnOpenBack([this]() { showTraining(); }, &m_lblOpen),
    m_lblOptions(&m_title, false),
    m_lblOptionsTitle(&m_lblOptions),
    m_btnOptionsBack([this]() { showMain(); }, &m_lblOptions),
    m_btnOptionsDifficulty([this]() { fight::Scenario::setDifficulty((fight::Scenario::difficulty() + 1)%3); updateDifficulty(); }, &m_lblOptions),
    m_lblLoading(&m_title, false),
    m_lblLoadingTitle(&m_lblLoading),
    m_lblLoadingWait(&m_lblLoading),
    m_cursor(false),
    m_funcStartGame(std::move(funcStartGame)),
    m_funcLoadGame(std::move(funcLoadGame)),
    m_funcStartTraining(std::move(funcStartTraining)),
    m_funcStartOpenMission(std::move(funcStartOpenMission))
{
    const gfx::ColorTable colorTable("gfx:pal/gui/border.pal");

    gfx::Texture texBar(gfx::Image::load("gfx:img/title/horline.img", colorTable));
    gfx::Font fontSmall("gfx:fnt/dpsmall.fnt", colorTable);
    gfx::Font fontMedium("gfx:fnt/dpmedium.fnt", colorTable);
    gfx::Font fontLarge("gfx:fnt/dplarge.fnt", colorTable);

    m_presents.setTexture(gfx::Image::loadPCX("gfx:pic/title/present.pcx"));
    m_title.setTexture(gfx::Image::loadPCX("gfx:pic/title/title.pcx"));

    m_backgroundSound.load("sfx:snd/bground/b02.pcl", "sfx:snd/bground/b02.pcr");

    m_lblVersion.setFont(fontSmall);
    m_lblVersion.setPosition(2, 2);
    m_lblVersion.setText("Vertigo " VERTIGO_VERSION);
    m_lblBar1.setTexture(texBar);
    m_lblBar1.setPosition(140, 256);
    m_lblBar2.setTexture(texBar);
    m_lblBar2.setPosition(140, 448);

    m_lblMainTitle.setFont(fontMedium);
    m_lblMainTitle.setPosition(145, 256 - fontMedium.height() - 2);
    m_lblMainTitle.setText(txt::StringTable::get(txt::MainMenu));

    m_btnMainNew.setFont(fontLarge);
    m_btnMainNew.setPosition(0, 288);
    m_btnMainNew.setWidth(640);
    m_btnMainNew.setAlignment(ui::Label::AlignHCenter);
    m_btnMainNew.setText(txt::StringTable::get(txt::MainMenu_NewGame));

    m_btnMainLoad.setFont(fontLarge);
    m_btnMainLoad.setPosition(0, 308);
    m_btnMainLoad.setWidth(640);
    m_btnMainLoad.setAlignment(ui::Label::AlignHCenter);
    m_btnMainLoad.setText(txt::StringTable::get(txt::MainMenu_Load));

    // the training missions the data has, and below them the open mission
    const StringList trainings = Training::names();
    m_btnMainTraining.setFont(fontLarge);
    m_btnMainTraining.setPosition(0, 328);
    m_btnMainTraining.setWidth(640);
    m_btnMainTraining.setAlignment(ui::Label::AlignHCenter);
    m_btnMainTraining.setText(txt::StringTable::get(txt::MainMenu_Training));

    m_lblTrainingTitle.setFont(fontMedium);
    m_lblTrainingTitle.setPosition(145, 256 - fontMedium.height() - 2);
    m_lblTrainingTitle.setText(txt::StringTable::get(txt::MainMenu_Training));
    for (int i = 0; i < trainings.size(); i++) {
        m_btnTrainingMissions.emplace_back([this, i]() { m_funcStartTraining(i); }, &m_lblTraining);
        ui::Button &button = m_btnTrainingMissions.back();
        button.setFont(fontLarge);
        button.setPosition(0, 288 + 20*i);
        button.setWidth(640);
        button.setAlignment(ui::Label::AlignHCenter);
        button.setText(trainings[i].toUpper());
    }
    m_btnTrainingOpen.setFont(fontLarge);
    m_btnTrainingOpen.setPosition(0, 308 + 20*trainings.size());
    m_btnTrainingOpen.setWidth(640);
    m_btnTrainingOpen.setAlignment(ui::Label::AlignHCenter);
    m_btnTrainingOpen.setText(txt::StringTable::get(txt::OpenMission));
    m_btnTrainingBack.setPosition(0, 428);
    m_btnTrainingBack.setWidth(640);
    m_btnTrainingBack.setAlignment(ui::Button::AlignHCenter);
    m_btnTrainingBack.setFont(fontLarge);
    m_btnTrainingBack.setText(txt::StringTable::get(txt::MainMenu));

    // the open mission: one of the four boats, then its depot and the missions; as in the
    // original, a page below a submenu has the path in its title, e.g. TRAINING\OPEN MISSION
    m_lblOpenTitle.setFont(fontMedium);
    m_lblOpenTitle.setPosition(145, 256 - fontMedium.height() - 2);
    m_lblOpenTitle.setText(txt::StringTable::get(txt::MainMenu_Training) + "\\" + txt::StringTable::get(txt::OpenMission));
    int y = 288;
    for (const auto &boat : OpenMission::boats()) {
        const int type = boat.first;
        m_btnOpenBoats.emplace_back([this, type]() { m_funcStartOpenMission(type); }, &m_lblOpen);
        ui::Button &button = m_btnOpenBoats.back();
        button.setFont(fontLarge);
        button.setPosition(0, y);
        button.setWidth(640);
        button.setAlignment(ui::Label::AlignHCenter);
        button.setText(boat.second.toUpper());
        y += 20;
    }
    m_btnOpenBack.setPosition(0, 428);
    m_btnOpenBack.setWidth(640);
    m_btnOpenBack.setAlignment(ui::Button::AlignHCenter);
    m_btnOpenBack.setFont(fontLarge);
    m_btnOpenBack.setText(txt::StringTable::get(txt::MainMenu_Training));

    // as in the original, below the training
    m_btnMainCredits.setFont(fontLarge);
    m_btnMainCredits.setPosition(0, 348);
    m_btnMainCredits.setWidth(640);
    m_btnMainCredits.setAlignment(ui::Label::AlignHCenter);
    m_btnMainCredits.setText(txt::StringTable::get(txt::MainMenu_Credits));

    m_btnMainOptions.setFont(fontLarge);
    m_btnMainOptions.setPosition(0, 368);
    m_btnMainOptions.setWidth(640);
    m_btnMainOptions.setAlignment(ui::Label::AlignHCenter);
    m_btnMainOptions.setText(txt::StringTable::get(txt::Notebook_Options));

    // the difficulty (easy, normal, hard: cycles on click), the notebook's sound and movie
    // options (which a chapter takes when it starts) and the touch screen's, on one page
    m_lblOptionsTitle.setFont(fontMedium);
    m_lblOptionsTitle.setPosition(145, 256 - fontMedium.height() - 2);
    m_lblOptionsTitle.setText(txt::StringTable::get(txt::Notebook_Options));
    auto setting = [](const char *key) {
        return Option{ [key]() { return util::Settings::boolValue(key, true); }, [key](bool value) { util::Settings::setValue(key, value); } };
    };
    m_options = {
        { []() { return sfx::Music::isEnabled(); }, [](bool value) { sfx::Music::setEnabled(value); },
          txt::StringTable::get(txt::Notebook_Sound_CdMusic_On), txt::StringTable::get(txt::Notebook_Sound_CdMusic_Off) },
        setting("Movie/autopilot"), setting("Movie/approach"), setting("Movie/harbour"),
        { []() { return hud::TouchControls::isMirrored(); }, [](bool value) { hud::TouchControls::setMirrored(value); },
          txt::StringTable::get(txt::Touch_Stick_Right), txt::StringTable::get(txt::Touch_Stick_Left) },
        { []() { return hud::TouchControls::isStickInverted(); }, [](bool value) { hud::TouchControls::setStickInverted(value); },
          txt::StringTable::get(txt::Touch_Stick_Inverted), txt::StringTable::get(txt::Touch_Stick_Normal) },
    };
    const txt::StringId movies[][2] = { { txt::Notebook_Movies_Autopilot_Yes, txt::Notebook_Movies_Autopilot_No },
                                        { txt::Notebook_Movies_Approach_Yes, txt::Notebook_Movies_Approach_No },
                                        { txt::Notebook_Movies_Harbour_Yes, txt::Notebook_Movies_Harbour_No } };
    for (int i = 0; i < 3; i++) {
        m_options[1 + i].on = txt::StringTable::get(movies[i][0]);
        m_options[1 + i].off = txt::StringTable::get(movies[i][1]);
    }
    // as on the original's option pages: the medium font, 15 pixels apart
    m_btnOptionsDifficulty.setFont(fontMedium);
    m_btnOptionsDifficulty.setPosition(0, 293);
    m_btnOptionsDifficulty.setWidth(640);
    m_btnOptionsDifficulty.setAlignment(ui::Label::AlignHCenter);
    updateDifficulty();
    for (std::size_t i = 0; i < m_options.size(); i++) {
        m_btnOptions.emplace_back([this, i]() { m_options[i].setValue(!m_options[i].value()); updateOptions(); }, &m_lblOptions);
        ui::Button &button = m_btnOptions.back();
        button.setFont(fontMedium);
        button.setPosition(0, 308 + 15*int(i));
        button.setWidth(640);
        button.setAlignment(ui::Label::AlignHCenter);
    }
    m_btnOptionsBack.setPosition(0, 428);
    m_btnOptionsBack.setWidth(640);
    m_btnOptionsBack.setAlignment(ui::Button::AlignHCenter);
    m_btnOptionsBack.setFont(fontLarge);
    m_btnOptionsBack.setText(txt::StringTable::get(txt::MainMenu));

    m_btnMainQuit.setFont(fontLarge);
    m_btnMainQuit.setPosition(0, 408);
    m_btnMainQuit.setWidth(640);
    m_btnMainQuit.setAlignment(ui::Label::AlignHCenter);
    m_btnMainQuit.setText(txt::StringTable::get(txt::MainMenu_QuitGame));

    m_lblNewTitle.setFont(gfx::Font::Medium);
    m_lblNewTitle.setPosition(145, 256 - gfx::Font(gfx::Font::Medium).height() - 2);
    m_lblNewTitle.setText(txt::StringTable::get(txt::MainMenu_NewGame));

    m_btnNewBack.setPosition(0, 428);
    m_btnNewBack.setWidth(640);
    m_btnNewBack.setAlignment(ui::Button::AlignHCenter);
    m_btnNewBack.setFont(gfx::Font::Large);
    m_btnNewBack.setText(txt::StringTable::get(txt::MainMenu));

    m_lblLoadTitle.setFont(gfx::Font::Medium);
    m_lblLoadTitle.setPosition(145, 256 - gfx::Font(gfx::Font::Medium).height() - 2);
    m_lblLoadTitle.setText(txt::StringTable::get(txt::MainMenu_Load));

    m_btnLoadBack.setPosition(0, 428);
    m_btnLoadBack.setWidth(640);
    m_btnLoadBack.setAlignment(ui::Button::AlignHCenter);
    m_btnLoadBack.setFont(gfx::Font::Large);
    m_btnLoadBack.setText(txt::StringTable::get(txt::MainMenu));

    // as in the original: the saved games by slot with the arrows at the bar's right end (arr1 up,
    // arr2 down, each _up and pressed _do), half transparent when they have nowhere to go, and a
    // question before one is loaded
    m_btnLoadUp.setTexture(gfx::Image::load("gfx:img/title/arr1_up.img", colorTable));
    m_btnLoadUp.setPressedTexture(gfx::Image::load("gfx:img/title/arr1_do.img", colorTable));
    m_btnLoadUp.setDisabledOpacity(0.5f);
    m_btnLoadUp.setPosition(LoadArrowsX, 304);
    m_btnLoadDown.setTexture(gfx::Image::load("gfx:img/title/arr2_up.img", colorTable));
    m_btnLoadDown.setPressedTexture(gfx::Image::load("gfx:img/title/arr2_do.img", colorTable));
    m_btnLoadDown.setDisabledOpacity(0.5f);
    m_btnLoadDown.setPosition(LoadArrowsX, 345);

    const std::pair<ui::Label *, int> confirm[] = { { &m_lblLoadQuestion, 288 }, { &m_lblLoadName, 318 }, { &m_btnLoadYes, 348 }, { &m_btnLoadNo, 368 } };
    for (const auto &line : confirm) {
        line.first->setFont(fontLarge);
        line.first->setPosition(0, line.second);
        line.first->setWidth(640);
        line.first->setAlignment(ui::Label::AlignHCenter);
    }
    m_lblLoadQuestion.setText(txt::StringTable::get(txt::LoadGame_Question));
    m_btnLoadYes.setText(txt::StringTable::get(txt::Yes));
    m_btnLoadNo.setText(txt::StringTable::get(txt::No));

    m_lblLoadingTitle.setFont(gfx::Font::Large);
    m_lblLoadingTitle.setPosition(0, 316);
    m_lblLoadingTitle.setWidth(640);
    m_lblLoadingTitle.setAlignment(ui::Label::AlignHCenter);
    m_lblLoadingTitle.setText(txt::StringTable::get(txt::MainMenu_Loading));

    m_lblLoadingWait.setFont(gfx::Font::Large);
    m_lblLoadingWait.setPosition(0, 336);
    m_lblLoadingWait.setWidth(640);
    m_lblLoadingWait.setAlignment(ui::Label::AlignHCenter);
    m_lblLoadingWait.setText(txt::StringTable::get(txt::MainMenu_PleaseWait));

    changeState(Presents);
}

void MainMenu::draw()
{
    // each picture of the credits for twelve seconds, then back to the menu
    if (m_state == Credits && m_creditsTime.elapsed() >= 12000) {
        if (m_creditsPage < 3)
            showCredits(m_creditsPage + 1);
        else
            changeState(Title);
    }
    Menu::draw();

    if (m_lblNew.isVisible()) {
        if (!m_time.isValid() || m_time.elapsed() > 300) {
            m_time.start();
            m_cursor = !m_cursor;
        }

        Rect rect = gfx::Font(gfx::Font::Large).draw(m_name, Rect(0, 328, 640, -1), true, false);
        if (m_cursor)
            gfx::Font(gfx::Font::Large).draw("_", rect.pos() + Point(rect.width, 2));
    }
    if (m_state == Loading)
    {
        // show the loading screen during the load; the game takes over the window, so the menu
        // has none afterwards
        Window *win = window();
        win->swapBuffers();
        m_funcLoadGame(m_loadingGame);
        changeState(Title);
        win->swapBuffers();
    }
}

Rect MainMenu::textInputArea() const
{
    // the name, as draw() shows it, with some room around it
    return Rect(0, 322, 640, gfx::Font(gfx::Font::Large).height() + 12);
}

void MainMenu::activate()
{
    m_backgroundSound.playLoop();
}

void MainMenu::deactivate()
{
    m_backgroundSound.stop();
}

void MainMenu::changeState(State state)
{
    switch (state) {
    case Presents:
        setRootWidget(&m_presents);
        m_state = state;
        break;

    case Title:
        setRootWidget(&m_title);
        showMain();
        m_state = state;
        break;

    case Loading:
        setRootWidget(&m_title);
        showLoading();
        m_state = state;
        break;

    case Credits:
        setRootWidget(&m_credits);
        showCredits(0);
        m_state = state;
        break;

    default:
        util::fatal("Cannot change state!");
    }
}

void MainMenu::mousePressEvent(input::MouseEvent *event)
{
    Menu::mousePressEvent(event);

    switch (m_state) {
    case Presents:
    case Credits:
        changeState(Title);
        return;

    default:
        ; // nothing to do
    }

    if (event->button() == input::RightButton) {
        // from the question back to the saved games
        if (m_lblLoad.isVisible() && m_lblLoadConfirm.isVisible())
            showLoadList();
        else
            showMain();
    }
}

void MainMenu::keyPressEvent(input::KeyEvent *event)
{
    // any key ends the credits
    if (m_state == Credits) {
        changeState(Title);
        return;
    }
    Menu::keyPressEvent(event);

    if (m_lblNew.isVisible()) {
        if (event->key() == input::Key_Enter || event->key() == input::Key_Return) {
            m_funcStartGame(m_name.trimmed());
            showMain();
        }

        if (event->key() == input::Key_Backspace)
            m_name.chop(1);

        // only lower-case letters, digits, '-' and spaces
        const String typed = String(event->text()).toLower();
        String text;
        for (const char c : typed)
            if ((c >= 'a' && c <= 'z') || (c >= '0' && c <= '9') || c == '-' || c == ' ')
                text += c;
        if (!text.isEmpty())
            m_name += text;

        if (m_name.length() > 20)
            m_name.chop(m_name.length() - 20);
    }
}

void MainMenu::showMain()
{
    m_lblMain.show();
    m_lblNew.hide();
    m_lblLoad.hide();
    m_lblTraining.hide();
    m_lblOpen.hide();
    m_lblOptions.hide();
    m_lblLoading.hide();
}

void MainMenu::showNew()
{
    m_lblMain.hide();
    m_lblNew.show();
    m_name = "dead-eye";
}

void MainMenu::showLoad()
{
    m_lblMain.hide();
    m_lblLoad.show();

    m_lblLoadSave.clear();
    m_btnLoadSave.clear();

    // each with its station, and its date; a line too wide for the room left of the arrows ends
    // its station with "..."
    const gfx::Font fontLarge(gfx::Font::Large);
    for (const Chapter::SavedGame &game : Chapter::savedGames()) {
        const String path = String::fromUtf8(game.file);
        const String name = game.name;
        String line = String("%1: %2").arg(game.name, game.station);
        for (int keep = game.station.length() - 1; keep > 0 && fontLarge.width(line) > 2*(LoadArrowsX - 320); keep--)
            line = String("%1: %2...").arg(game.name, game.station.left(keep).trimmed());
        String date = util::shortDateTime(game.time);
        if (game.autosave)
            date = String("%1 (%2)").arg(date, txt::StringTable::get(txt::Autosave));

        m_btnLoadSave.emplace_back([this, path, name]() { confirmLoad(path, name); }, &m_lblLoadList);
        ui::Button &button = m_btnLoadSave.back();
        button.setWidth(640);
        button.setAlignment(ui::Button::AlignHCenter);
        button.setFont(gfx::Font::Large);
        button.setText(line);

        m_lblLoadSave.emplace_back(&m_lblLoadList);
        ui::Label &label = m_lblLoadSave.back();
        label.setWidth(640);
        label.setAlignment(ui::Button::AlignHCenter);
        label.setFont(gfx::Font::Small);
        label.setText(date);
    }

    m_loadFirst = 0;
    showLoadList();
}


void MainMenu::showLoadList()
{
    m_lblLoadConfirm.hide();
    m_lblLoadList.show();
    updateLoadList();
}


void MainMenu::scrollLoad(int step)
{
    m_loadFirst = std::clamp(m_loadFirst + step, 0, std::max(0, int(m_btnLoadSave.size()) - LoadRows));
    updateLoadList();
}


void MainMenu::updateLoadList()
{
    // the rows from the first one shown
    auto label = m_lblLoadSave.begin();
    int i = 0;
    for (ui::Button &button : m_btnLoadSave) {
        const int row = i++ - m_loadFirst;
        const bool shown = row >= 0 && row < LoadRows;
        button.setVisible(shown);
        label->setVisible(shown);
        button.setPosition(0, 282 + 33*row);
        (label++)->setPosition(0, 282 + 33*row + 13);
    }
    m_btnLoadUp.setEnabled(m_loadFirst > 0);
    m_btnLoadDown.setEnabled(m_loadFirst + LoadRows < int(m_btnLoadSave.size()));
}


void MainMenu::confirmLoad(const String &path, const String &name)
{
    m_loadingGame = path;
    m_lblLoadName.setText(String("--- %1 ---").arg(name));
    m_lblLoadList.hide();
    m_lblLoadConfirm.show();
}


void MainMenu::showTraining()
{
    m_lblMain.hide();
    m_lblOpen.hide();
    m_lblTraining.show();
}


void MainMenu::showOpenMission()
{
    m_lblTraining.hide();
    m_lblOpen.show();
}


void MainMenu::showOptions()
{
    m_lblMain.hide();
    m_lblOptions.show();
    updateOptions();
}


void MainMenu::updateOptions()
{
    auto button = m_btnOptions.begin();
    for (const Option &option : m_options)
        (button++)->setText(option.value() ? option.on : option.off);
}


void MainMenu::showLoading()
{
    m_lblLoad.hide();
    m_lblLoading.show();
}


// The credits as the original shows them (docs/ROADMAP.md, appendix E): CREDITS0 to CREDITS3, each
// for twelve seconds; a key or a click ends them
void MainMenu::showCredits(int page)
{
    m_creditsPage = page;
    m_credits.setTexture(gfx::Image::load(String("gfx:pic/title/credits%1.r16").arg(page), 640, 480));
    m_creditsTime.start();
}


void MainMenu::updateDifficulty()
{
    m_btnOptionsDifficulty.setText(txt::StringTable::get(txt::StringId(txt::Text107 + fight::Scenario::difficulty())));
}


} // namespace game
