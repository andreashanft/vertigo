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


// The mission's steps and the frames between them: fight::StepClock and fight::poseAhead

#include "check.hpp"

#include "fight/poseahead.hpp"
#include "fight/stepclock.hpp"

#include <glm/gtc/matrix_transform.hpp>

#include <cmath>


namespace {

const float Step = 1.0f/60;

bool near(float actual, float expected, float tolerance = 1e-4f)
{
    return std::abs(actual - expected) <= tolerance;
}

bool near(const Vector3D &actual, const Vector3D &expected, float tolerance = 1e-4f)
{
    return glm::length(actual - expected) <= tolerance;
}

// The time the picture shows after each of a run of equal frames, in steps: the steps taken so
// far and the part of one the frame is drawn ahead. Returns how far the worst frame is off an
// even pace, as a share of a step.
float unevenness(float frame, int frames)
{
    fight::StepClock clock(Step);
    int steps = 0;
    float first = 0, worst = 0;
    for (int i = 0; i < frames; i++)
    {
        steps += clock.advance(frame);
        const float shown = steps + clock.ahead();
        if (i == 0)
            first = shown;
        worst = std::max(worst, std::abs(shown - (first + i*frame/Step)));
    }
    return worst;
}

// a camera at a place, turned about the vertical: its inverted matrix
Matrix pose(const Vector3D &place, float degrees)
{
    return glm::translate(Matrix(1), place)*glm::rotate(Matrix(1), glm::radians(degrees), Vector3D(0, 0, 1));
}

// the angle a pose is turned about the vertical, in degrees
float turn(const Matrix &pose)
{
    return glm::degrees(std::atan2(pose[0].y, pose[0].x));
}

} // namespace


int main()
{
    // at 60 Hz every frame is one step, with nothing left over
    {
        fight::StepClock clock(Step);
        for (int i = 0; i < 600; i++)
        {
            CHECK_EQUAL(clock.advance(Step), 1);
            CHECK_EQUAL(clock.ahead(), 0.0f);
        }
    }
    // at 120 Hz every other frame is a step, and the frames between are half a step ahead
    {
        fight::StepClock clock(Step);
        for (int i = 0; i < 600; i++)
        {
            CHECK_EQUAL(clock.advance(Step/2), i % 2);
            CHECK(near(clock.ahead(), i % 2 == 0 ? 0.5f : 0.0f));
        }
    }
    // at 30 Hz every frame is two steps
    {
        fight::StepClock clock(Step);
        for (int i = 0; i < 100; i++)
        {
            CHECK_EQUAL(clock.advance(2*Step), 2);
            CHECK(near(clock.ahead(), 0.0f));
        }
    }
    // frames a little short or long (here by a twentieth) still get a step each
    {
        fight::StepClock clock(Step);
        for (int i = 0; i < 600; i++)
            CHECK_EQUAL(clock.advance(i % 2 ? 1.05f*Step : 0.95f*Step), 1);
    }
    // the mission keeps the frames' time: 6 steps to 5 frames at 50 Hz, 4 to 5 at 75 Hz
    {
        fight::StepClock clock(Step);
        int steps = 0;
        for (int i = 0; i < 500; i++)
            steps += clock.advance(1.0f/50);
        CHECK_EQUAL(steps, 600);
        clock.reset();
        steps = 0;
        for (int i = 0; i < 750; i++)
            steps += clock.advance(1.0f/75);
        CHECK_EQUAL(steps, 600);
    }
    // a frame is never drawn more than a quarter of a step before the last step or three quarters
    // past it, and a paused mission (no time) takes no step and stays where it is
    {
        fight::StepClock clock(Step);
        for (int i = 0; i < 1000; i++)
        {
            clock.advance(0.001f*(1 + i % 37));
            CHECK(clock.ahead() >= -0.25f && clock.ahead() < 0.75f);
        }
        const float ahead = clock.ahead();
        CHECK_EQUAL(clock.advance(0), 0);
        CHECK_EQUAL(clock.ahead(), ahead);
    }
    // with the part of a step ahead, the picture's time goes on evenly whatever the refresh
    for (float hertz : { 50.0f, 60.0f, 75.0f, 90.0f, 120.0f, 144.0f, 165.0f, 240.0f })
        CHECK(unevenness(1/hertz, 2000) < 0.01f);

    // a frame's time is the display's when it is about a whole number of its refreshes
    {
        const float period = 1.0f/60;
        CHECK_EQUAL(fight::StepClock::displayTime(0.016f, period), period);
        CHECK_EQUAL(fight::StepClock::displayTime(0.0175f, period), period);
        CHECK_EQUAL(fight::StepClock::displayTime(0.0335f, period), 2*period);
        // too far off a refresh, less than one, or a display that isn't known: as measured
        CHECK_EQUAL(fight::StepClock::displayTime(0.020f, period), 0.020f);
        CHECK_EQUAL(fight::StepClock::displayTime(0.008f, period), 0.008f);
        CHECK_EQUAL(fight::StepClock::displayTime(0.0f, period), 0.0f);
        CHECK_EQUAL(fight::StepClock::displayTime(0.0163f, 0), 0.0163f);
        // 60 Hz frames measured in whole milliseconds are one step each, with nothing left over
        fight::StepClock clock(Step);
        for (int i = 0; i < 600; i++)
        {
            CHECK_EQUAL(clock.advance(fight::StepClock::displayTime(i % 3 ? 0.017f : 0.016f, period)), 1);
            CHECK_EQUAL(clock.ahead(), 0.0f);
        }
    }

    // a camera that moved 2 m and turned 10 degrees in the last step
    {
        const Matrix before = pose(Vector3D(10, 20, 30), 40);
        const Matrix after = pose(Vector3D(12, 20, 30), 50);
        // drawn at the step it is there, bit for bit
        CHECK(fight::poseAhead(before, after, 0) == after);
        // half a step on: another metre and 5 degrees
        const Matrix half = fight::poseAhead(before, after, 0.5f);
        CHECK(near(Vector3D(half[3]), Vector3D(13, 20, 30)));
        CHECK(near(turn(half), 55, 1e-3f));
        CHECK_EQUAL(half[3].w, 1.0f);
        // a step that came a quarter early: back towards where it was
        const Matrix early = fight::poseAhead(before, after, -0.25f);
        CHECK(near(Vector3D(early[3]), Vector3D(11.5f, 20, 30)));
        CHECK(near(turn(early), 47.5f, 1e-3f));
        // its axes stay at right angles and of length 1
        const glm::mat3 axes(half);
        CHECK(near(glm::transpose(axes)*axes*Vector3D(1, 2, 3), Vector3D(1, 2, 3)));
        // a camera that stood still stays
        const Matrix still = fight::poseAhead(after, after, 0.5f);
        CHECK(near(Vector3D(still[3]), Vector3D(12, 20, 30)));
        CHECK(near(turn(still), 50, 1e-3f));
    }
    return test::result();
}
