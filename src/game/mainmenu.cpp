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
                   std::function<void()> funcQuit) :
    m_state(Invalid),
    m_lblVersion(&m_title),
    m_lblBar1(&m_title),
    m_lblBar2(&m_title),
    m_lblMain(&m_title),
    m_lblMainTitle(&m_lblMain),
    m_btnMainNew([this]() { showNew(); }, &m_lblMain),
    m_btnMainLoad([this]() { showLoad(); }, &m_lblMain),
    m_btnMainDifficulty([this]() { fight::Scenario::setDifficulty((fight::Scenario::difficulty() + 1)%3); updateDifficulty(); }, &m_lblMain),
    m_btnMainTraining([this]() { showTraining(); }, &m_lblMain),
    m_btnMainOptions([this]() { showOptions(); }, &m_lblMain),
    m_btnMainQuit(funcQuit, &m_lblMain),
    m_lblNew(&m_title, false),
    m_lblNewTitle(&m_lblNew),
    m_btnNewBack([this]() { showMain(); }, &m_lblNew),
    m_lblLoad(&m_title, false),
    m_lblLoadTitle(&m_lblLoad),
    m_btnLoadBack([this]() { showMain(); }, &m_lblLoad),
    m_lblTraining(&m_title, false),
    m_lblTrainingTitle(&m_lblTraining),
    m_btnTrainingBack([this]() { showMain(); }, &m_lblTraining),
    m_lblOptions(&m_title, false),
    m_lblOptionsTitle(&m_lblOptions),
    m_btnOptionsBack([this]() { showMain(); }, &m_lblOptions),
    m_lblLoading(&m_title, false),
    m_lblLoadingTitle(&m_lblLoading),
    m_lblLoadingWait(&m_lblLoading),
    m_cursor(false),
    m_funcStartGame(std::move(funcStartGame)),
    m_funcLoadGame(std::move(funcLoadGame)),
    m_funcStartTraining(std::move(funcStartTraining))
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

    // easy, normal, hard: cycles on click
    m_btnMainDifficulty.setFont(fontLarge);
    m_btnMainDifficulty.setPosition(0, 328);
    m_btnMainDifficulty.setWidth(640);
    m_btnMainDifficulty.setAlignment(ui::Label::AlignHCenter);
    updateDifficulty();

    // the training missions, if the data has them
    const StringList trainings = Training::names();
    m_btnMainTraining.setFont(fontLarge);
    m_btnMainTraining.setPosition(0, 348);
    m_btnMainTraining.setWidth(640);
    m_btnMainTraining.setAlignment(ui::Label::AlignHCenter);
    m_btnMainTraining.setText(txt::StringTable::get(txt::MainMenu_Training));
    m_btnMainTraining.setVisible(!trainings.isEmpty());

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
    m_btnTrainingBack.setPosition(0, 428);
    m_btnTrainingBack.setWidth(640);
    m_btnTrainingBack.setAlignment(ui::Button::AlignHCenter);
    m_btnTrainingBack.setFont(fontLarge);
    m_btnTrainingBack.setText(txt::StringTable::get(txt::MainMenu));

    // as in the original, below the training (where it had CREDITS between them)
    m_btnMainOptions.setFont(fontLarge);
    m_btnMainOptions.setPosition(0, 368);
    m_btnMainOptions.setWidth(640);
    m_btnMainOptions.setAlignment(ui::Label::AlignHCenter);
    m_btnMainOptions.setText(txt::StringTable::get(txt::Notebook_Options));

    // the notebook's sound and movie options (which a chapter takes when it starts) and the
    // touch screen's, on one page
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
    for (std::size_t i = 0; i < m_options.size(); i++) {
        m_btnOptions.emplace_back([this, i]() { m_options[i].setValue(!m_options[i].value()); updateOptions(); }, &m_lblOptions);
        ui::Button &button = m_btnOptions.back();
        button.setFont(fontLarge);
        button.setPosition(0, 288 + 20*int(i));
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

    default:
        util::fatal("Cannot change state!");
    }
}

void MainMenu::mousePressEvent(input::MouseEvent *event)
{
    Menu::mousePressEvent(event);

    switch (m_state) {
    case Presents:
        changeState(Title);
        break;

    default:
        ; // nothing to do
    }

    if (event->button() == input::RightButton) {
        showMain();
    }
}

void MainMenu::keyPressEvent(input::KeyEvent *event)
{
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

    int y = 282;

    m_lblLoadSave.clear();
    m_btnLoadSave.clear();

    std::vector<Chapter::SavedGame> games = Chapter::savedGames();
    std::sort(games.begin(), games.end());
    for (const Chapter::SavedGame &game : games) {
        String name = game.name;

        m_btnLoadSave.emplace_back([this, name]() { changeState(Loading); m_loadingGame = name; }, &m_lblLoad);
        ui::Button &button = m_btnLoadSave.back();
        button.setPosition(0, y);
        button.setWidth(640);
        button.setAlignment(ui::Button::AlignHCenter);
        button.setFont(gfx::Font::Large);
        button.setText(String("%1: %2").arg(game.name, game.station));

        y += 13;

        m_lblLoadSave.emplace_back(&m_lblLoad);
        ui::Label &label = m_lblLoadSave.back();
        label.setPosition(0, y);
        label.setWidth(640);
        label.setAlignment(ui::Button::AlignHCenter);
        label.setFont(gfx::Font::Small);
        label.setText(util::shortDateTime(game.time));

        y += 20;
    }
}


void MainMenu::showTraining()
{
    m_lblMain.hide();
    m_lblTraining.show();
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


void MainMenu::updateDifficulty()
{
    m_btnMainDifficulty.setText(txt::StringTable::get(txt::StringId(txt::Text107 + fight::Scenario::difficulty())));
}


} // namespace game
