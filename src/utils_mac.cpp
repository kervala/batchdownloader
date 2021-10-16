/*
 *  kTimer is a timers manager
 *  Copyright (C) 2021  Cedric OCHS
 *
 *  This program is free software: you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation, either version 3 of the License, or
 *  (at your option) any later version.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program.  If not, see <http://www.gnu.org/licenses/>.
 *
 */

#include "common.h"
#include "utils.h"

#ifdef Q_OS_MAC

#include <Carbon/Carbon.h>

void mouseLeftClickUp(const QPoint& pos)
{
	CGEventRef click = CGEventCreateMouseEvent(NULL, kCGEventLeftMouseUp, CGPointMake(pos.x(), pos.y()), kCGMouseButtonLeft);

	CGEventPost(kCGHIDEventTap, click);

	CFRelease(click);
}

void mouseLeftClickDown(const QPoint& pos)
{
	CGEventRef click = CGEventCreateMouseEvent(NULL, kCGEventLeftMouseDown, CGPointMake(pos.x(), pos.y()), kCGMouseButtonLeft);

	CGEventPost(kCGHIDEventTap, click);

	CFRelease(click);
}

int QKeySequenceToVK(const QKeySequence& seq)
{
	/*
	kVK_Return = 0x24,
		kVK_Tab = 0x30,
		kVK_Space = 0x31,
		kVK_Delete = 0x33,
		kVK_Escape = 0x35,
		kVK_Command = 0x37,
		kVK_Shift = 0x38,
		kVK_CapsLock = 0x39,
		kVK_Option = 0x3A,
		kVK_Control = 0x3B,
		kVK_RightShift = 0x3C,
		kVK_RightOption = 0x3D,
		kVK_RightControl = 0x3E,
		kVK_Function = 0x3F,
		kVK_F17 = 0x40,
		kVK_VolumeUp = 0x48,
		kVK_VolumeDown = 0x49,
		kVK_Mute = 0x4A,
		kVK_F18 = 0x4F,
		kVK_F19 = 0x50,
		kVK_F20 = 0x5A,
		kVK_F5 = 0x60,
		kVK_F6 = 0x61,
		kVK_F7 = 0x62,
		kVK_F3 = 0x63,
		kVK_F8 = 0x64,
		kVK_F9 = 0x65,
		kVK_F11 = 0x67,
		kVK_F13 = 0x69,
		kVK_F16 = 0x6A,
		kVK_F14 = 0x6B,
		kVK_F10 = 0x6D,
		kVK_F12 = 0x6F,
		kVK_F15 = 0x71,
		kVK_Help = 0x72,
		kVK_Home = 0x73,
		kVK_PageUp = 0x74,
		kVK_ForwardDelete = 0x75,
		kVK_F4 = 0x76,
		kVK_End = 0x77,
		kVK_F2 = 0x78,
		kVK_PageDown = 0x79,
		kVK_F1 = 0x7A,
		kVK_LeftArrow = 0x7B,
		kVK_RightArrow = 0x7C,
		kVK_DownArrow = 0x7D,
		kVK_UpArrow = 0x7E
*/
	return 0;
}

bool isKeyPressed(int key)
{
	unsigned char keyMap[16];
	GetKeys((BigEndianUInt32*)&keyMap);
	return (0 != ((keyMap[key >> 3] >> (key & 7)) & 1));
}

bool isSameWindowAtPos(Window window, const QPoint& pos)
{
	return false;
}

void createWindowsList(Windows& windows)
{
}

void createWindowsList(QAbstractItemModel* model)
{
}

bool isWindowMinimized(WId id)
{
	return false;
}

bool RestoreMinimizedWindow(WId &id)
{
	return false;
}

void MinimizeWindow(WId id)
{
}

bool IsUsingComposition()
{
	return true;
}

void PutForegroundWindow(WId id)
{
}

bool IsOS64bits()
{
	return true;
}

#endif
