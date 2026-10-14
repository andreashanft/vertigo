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

#include "audiosession.hpp"
#include "soundsystem.h"

#import <AVFoundation/AVFoundation.h>

namespace sfx {

namespace {

id<NSObject> s_observer;

} // namespace


void setUpAudioSession()
{
    AVAudioSession *session = [AVAudioSession sharedInstance];
    [session setCategory:AVAudioSessionCategoryAmbient error:nil];
    s_observer = [[NSNotificationCenter defaultCenter] addObserverForName:AVAudioSessionInterruptionNotification object:session
                                                                    queue:[NSOperationQueue mainQueue] usingBlock:^(NSNotification *notification) {
        const NSUInteger type = [notification.userInfo[AVAudioSessionInterruptionTypeKey] unsignedIntegerValue];
        SoundSystem::get()->interrupt(SoundSystem::AudioSession, type == AVAudioSessionInterruptionTypeBegan);
    }];
}


void tearDownAudioSession()
{
    if (s_observer)
        [[NSNotificationCenter defaultCenter] removeObserver:s_observer];
    s_observer = nil;
}


void activateAudioSession()
{
    [[AVAudioSession sharedInstance] setActive:YES error:nil];
}

} // namespace sfx
