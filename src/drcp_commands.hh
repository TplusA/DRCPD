/*
 * Copyright (C) 2015, 2016  T+A elektroakustik GmbH & Co. KG
 *
 * This file is part of DRCPD.
 *
 * DRCPD is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License, version 3 as
 * published by the Free Software Foundation.
 *
 * DRCPD is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with DRCPD.  If not, see <http://www.gnu.org/licenses/>.
 */

#ifndef DRCP_COMMANDS_HH
#define DRCP_COMMANDS_HH

enum class DrcpCommand
{
    UNDEFINED_COMMAND              = 0x00,
    POWER_ON                       = 0x02,
    POWER_OFF                      = 0x03,
    POWER_TOGGLE                   = 0x04,
    ALARM_CANCEL                   = 0x06,
    ALARM_SNOOZE                   = 0x07,
    SLEEP_TIMER_START              = 0x08,
    PLAYBACK_PAUSE                 = 0x13,
    KEY_CAPS_LOCK_TOGGLE           = 0x14,
    ACCEPT                         = 0x1e,
    UPNP_START_SSDP_DISCOVERY      = 0x1f,
    SCROLL_UP_MANY                 = 0x21,
    SCROLL_DOWN_MANY               = 0x22,
    GOTO_LINE                      = 0x23,
    GO_BACK_ONE_LEVEL              = 0x25,
    SCROLL_UP_ONE                  = 0x26,
    SELECT_ITEM                    = 0x27,
    SCROLL_DOWN_ONE                = 0x28,
    KEY_OK_ENTER                   = 0x29,
    CANCEL_JUMP_COMMAND            = 0x2a,
    KEY_EXECUTE                    = 0x2b,
    FAVORITES_STORE                = 0x2c,
    FAVORITES_ADD_ITEM             = 0x2d,
    FAVORITES_REMOVE_ITEM          = 0x2e,
    FAVORITES_CLEAR                = 0x2f,
    KEY_DIGIT_0                    = '0',       /* 0x30 */
    KEY_DIGIT_1                    = '1',
    KEY_DIGIT_2                    = '2',
    KEY_DIGIT_3                    = '3',
    KEY_DIGIT_4                    = '4',
    KEY_DIGIT_5                    = '5',
    KEY_DIGIT_6                    = '6',
    KEY_DIGIT_7                    = '7',
    KEY_DIGIT_8                    = '8',
    KEY_DIGIT_9                    = '9',       /* 0x39 */
    KEY_LETTER_A                   = 'A',       /* 0x41 */
    KEY_LETTER_B                   = 'B',
    KEY_LETTER_C                   = 'C',
    KEY_LETTER_D                   = 'D',
    KEY_LETTER_E                   = 'E',
    KEY_LETTER_F                   = 'F',
    KEY_LETTER_G                   = 'G',
    KEY_LETTER_H                   = 'H',
    KEY_LETTER_I                   = 'I',
    KEY_LETTER_J                   = 'J',
    KEY_LETTER_K                   = 'K',
    KEY_LETTER_L                   = 'L',
    KEY_LETTER_M                   = 'M',
    KEY_LETTER_N                   = 'N',
    KEY_LETTER_O                   = 'O',
    KEY_LETTER_P                   = 'P',
    KEY_LETTER_Q                   = 'Q',
    KEY_LETTER_R                   = 'R',
    KEY_LETTER_S                   = 'S',
    KEY_LETTER_T                   = 'T',
    KEY_LETTER_U                   = 'U',
    KEY_LETTER_V                   = 'V',
    KEY_LETTER_W                   = 'W',
    KEY_LETTER_X                   = 'X',
    KEY_LETTER_Y                   = 'Y',
    KEY_LETTER_Z                   = 'Z',       /* 0x5a */
    SCROLL_PAGE_UP                 = 0x97,
    SCROLL_PAGE_DOWN               = 0x98,
    JUMP_TO_LETTER                 = 0x99,
    BROWSE_VIEW_OPEN_SOURCE        = 0x9a,
    SEARCH                         = 0x9b,
    X_TA_SEARCH_PARAMETERS         = 0x9c,
    JUMP_TO_NEXT                   = 0x9d,
    X_TA_SET_STREAM_INFO           = 0xa0,
    GOTO_INTERNET_RADIO            = 0xaa,
    GOTO_FAVORITES                 = 0xab,
    GOTO_HOME                      = 0xac,
    VOLUME_DOWN                    = 0xae,
    VOLUME_UP                      = 0xaf,
    PLAYBACK_NEXT                  = 0xb0,
    PLAYBACK_PREVIOUS              = 0xb1,
    PLAYBACK_STOP                  = 0xb2,
    PLAYBACK_START                 = 0xb3,
    PLAYBACK_SELECTED_FILE_ONESHOT = 0xb4,
    BROWSE_PLAY_VIEW_TOGGLE        = 0xba,
    BROWSE_PLAY_VIEW_SET           = 0xbb,
    REPEAT_MODE_TOGGLE             = 0xc0,
    FAST_WIND_FORWARD              = 0xc1,
    FAST_WIND_REVERSE              = 0xc2,
    FAST_WIND_STOP                 = 0xc3,
    FAST_WIND_SET_SPEED            = 0xc4,
    REPEAT_MODE_SET                = 0xc6,
    AUDIOBOOK_SET_SPEED            = 0xc5,
    SHUFFLE_MODE_SET               = 0xc7,
    GOTO_CONFIGURATION             = 0xdb,
    SHUFFLE_MODE_TOGGLE            = 0xdc,
    GOTO_SOURCE_SELECTION          = 0xde,
    GOTO_FM_TUNER                  = 0xf0,
    PLAYBACK_MUTE_TOGGLE           = 0xf1,
    VIDEO_MODE_SET_NTSC            = 0xf4,
    VIDEO_MODE_SET_PAL             = 0xf5,
    PLAYBACK_START_OR_RESUME       = 0xfa,
};

#endif /* !DRCP_COMMANDS_HH */
