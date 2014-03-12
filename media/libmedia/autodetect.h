/*
 * Copyright (C) 2008 The Android Open Source Project
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *      http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#ifndef AUTODETECT_H
#define AUTODETECT_H

#include <inttypes.h>

// flags used for native encoding detection
enum {
    kEncodingNone               = 0,
    kEncodingShiftJIS           = (1 << 0),
    kEncodingGBK                = (1 << 1),
    kEncodingBig5               = (1 << 2),
    kEncodingEUCKR              = (1 << 3),
    kEncodingUTF8               = (1<<4),
	kEncodingWin1251           = (1<<5),
	kEncodingWin1252           = (1<<6),
    kEncodingAll                = (kEncodingShiftJIS | kEncodingGBK | kEncodingBig5 | kEncodingEUCKR|kEncodingUTF8|kEncodingWin1251|kEncodingWin1252),
};

typedef struct win1251tounicode {
    unsigned char win1251;
    unsigned short unicode;
    }win1251tounicode;

typedef struct win1252tounicode {
    unsigned char win1252;
    unsigned short unicode;
    }win1252tounicode;
#define MEM_FAIL -1
#define NO_MEM -2
#define MEM_NOT_ENOUGH  -3
//pin :1251 character

extern int Win1251ToUtf8(unsigned char *pin,int inlen,unsigned char *pout,int outlen);

extern int Win1252ToUtf8(unsigned char *pin,int inlen,unsigned char *pout,int outlen);

// returns a bitfield containing the possible native encodings for the given character
extern uint32_t findPossibleEncodings(int ch);

#endif // AUTODETECT_H
