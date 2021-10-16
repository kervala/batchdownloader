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

#ifdef Q_OS_WIN

enum HBitmapFormat
{
	HBitmapNoAlpha,
	HBitmapPremultipliedAlpha,
	HBitmapAlpha
};

QPixmap qt_pixmapFromWinHBITMAP(HBITMAP bitmap, int hbitmapFormat = 0);

#include <QtWin>
#include <windows.h>
#include <tlhelp32.h>
#include <tchar.h>
#include <stdio.h>
#include <ShellAPI.h>
#include <sdkddkver.h>

#ifdef DEBUG_NEW
#define new DEBUG_NEW
#endif

void mouseLeftClickUp(const QPoint& pos)
{
	mouse_event(MOUSEEVENTF_LEFTUP, pos.x(), pos.y(), 0, 0);
}

void mouseLeftClickDown(const QPoint& pos)
{
	mouse_event(MOUSEEVENTF_LEFTDOWN, pos.x(), pos.y(), 0, 0);
}

int QKeySequenceToVK(const QKeySequence& seq)
{
	QString str = seq.toString();

	if (str.isEmpty()) return 0;

	static QMap<QString, qint16> s_keyArray;

	if (s_keyArray.isEmpty())
	{
		// special characters
		s_keyArray["Space"] = VK_SPACE;
		s_keyArray["Ins"] = VK_INSERT;
		s_keyArray["Del"] = VK_DELETE;
		s_keyArray["Esc"] = VK_ESCAPE;
		s_keyArray["Tab"] = VK_TAB;
		// s_keyArray["Backtab"] = VK_BACK;
		s_keyArray["Backspace"] = VK_BACK;
		s_keyArray["Return"] = VK_RETURN;
		// s_keyArray["Enter"] = VK_ENTER;
		s_keyArray["Pause"] = VK_PAUSE;
		s_keyArray["Print"] = VK_PRINT;
		// s_keyArray["SysReq"] = VK_SYSREQ;
		s_keyArray["Home"] = VK_HOME;
		s_keyArray["End"] = VK_END;
		s_keyArray["Left"] = VK_LEFT;
		s_keyArray["Up"] = VK_UP;
		s_keyArray["Right"] = VK_RIGHT;
		s_keyArray["Down"] = VK_DOWN;
		s_keyArray["PgUp"] = VK_PRIOR;
		s_keyArray["PgDown"] = VK_NEXT;
		s_keyArray["Help"] = VK_HELP;
		s_keyArray["Menu"] = VK_MENU;
		s_keyArray["NumLock"] = VK_CAPITAL;
		s_keyArray["CapsLock"] = VK_NUMLOCK;
		s_keyArray["ScrollLock"] = VK_SCROLL;
		s_keyArray["+"] = VK_ADD;
		s_keyArray["-"] = VK_SUBTRACT;
		s_keyArray["*"] = VK_MULTIPLY;
		s_keyArray["/"] = VK_DIVIDE;
		s_keyArray["/"] = VK_SNAPSHOT;
		s_keyArray["\xC2\xB2"] = VK_OEM_7;
		
		// modifiers
		s_keyArray["Shift"] = VK_SHIFT;
		s_keyArray["Control"] = VK_CONTROL;
		s_keyArray["Alt"] = VK_MENU;

		// numbers numpad
		for (int i = '0'; i <= '9'; ++i) s_keyArray[QString(i)] = VK_NUMPAD0 + i;

		// letters
		for (int i = 'A'; i <= 'Z'; ++i) s_keyArray[QString(i)] = i;

		// function keys
		for (int i = 1; i <= 24; ++i) s_keyArray[QString("F%1").arg(i)] = VK_F1 + i - 1;
	}

	QMap<QString, qint16>::iterator it = s_keyArray.find(str);

	if (it != s_keyArray.end()) return *it;

	qDebug() << "unable to find" << str;

	return -1;
}

bool isKeyPressed(int key)
{
	SHORT res = GetAsyncKeyState(key);

	if (res)
	{
		qDebug() << "key" << res;
	}

	// only take current keypresses (0x8000) and not previous ones (0x0001)
	return res & 0x8000;
}

QPixmap grabWindow(WId window)
{
	return QPixmap();
}

bool isSameWindowAtPos(Window window, const QPoint& pos)
{
	POINT p;
	p.x = pos.x();
	p.y = pos.y();

	HWND underCursorWindowId = WindowFromPoint(p);

#ifdef _DEBUG
	QString underCursorWindowTitle;

	if (underCursorWindowId)
	{
		wchar_t buffer[1025];

		int len = GetWindowTextW(underCursorWindowId, buffer, 1024);

		if (len > 0) underCursorWindowTitle = QString::fromWCharArray(buffer);
	}
#endif

	if (((HWND)window.id == underCursorWindowId) || IsChild((HWND)window.id, underCursorWindowId))
	{
#ifdef _DEBUG
		qDebug() << "same window" << underCursorWindowTitle;
#endif

		return true;
	}

#ifdef _DEBUG
	qDebug() << "different window" << underCursorWindowTitle;
#endif

	return false;
}

static QPixmap fancyPants( ICONINFO const &icon_info )
{
	int result;

	HBITMAP h_bitmap = icon_info.hbmColor;

	/// adapted from qpixmap_win.cpp so we can have a _non_ premultiplied alpha
	/// conversion and also apply the icon mask to bitmaps with no alpha channel
	/// remaining comments are Trolltech originals


	////// get dimensions
	BITMAP bitmap;
	memset( &bitmap, 0, sizeof(BITMAP) );

	result = GetObjectW( h_bitmap, sizeof(BITMAP), &bitmap );

	if (!result) return QPixmap();

	int const w = bitmap.bmWidth;
	int const h = bitmap.bmHeight;

	//////
	BITMAPINFO info;
	memset( &info, 0, sizeof(info) );

	info.bmiHeader.biSize = sizeof(BITMAPINFOHEADER);
	info.bmiHeader.biWidth = w;

	info.bmiHeader.biHeight = -h;
	info.bmiHeader.biPlanes = 1;

	info.bmiHeader.biBitCount = 32;
	info.bmiHeader.biCompression = BI_RGB;

	info.bmiHeader.biSizeImage = w * h * 4;

	// Get bitmap bits
	uchar *data = new uchar[info.bmiHeader.biSizeImage];

	result = GetDIBits(GetDC(0), h_bitmap, 0, h, data, &info, DIB_RGB_COLORS );

	QPixmap p;
	if (result)
	{
		// test for a completely invisible image
		// we need to do this because there is apparently no way to determine

		// if an icon's bitmaps have alpha channels or not. If they don't the
		// alpha bits are set to 0 by default and the icon becomes invisible
		// so we do this long check. I've investigated everything, the bitmaps
		// don't seem to carry the BITMAPV4HEADER as they should, that would tell
		// us what we want to know if it was there, but apparently MS are SHIT SHIT
		const int N = info.bmiHeader.biSizeImage;

		int x;
		for (x = 3; x < N; x += 4)

			if (data[x] != 0)
				break;

		if (x < N)
		{
			p = QPixmap::fromImage( QImage( data, w, h, QImage::Format_ARGB32 ) );
		}
		else
		{
			QImage image( data, w, h, QImage::Format_RGB32 );

			QImage mask = image.createHeuristicMask();
			mask.invertPixels(); //prolly efficient as is a 1bpp bitmap really

			image.setAlphaChannel( mask );
			p = QPixmap::fromImage( image );
		}

		// force the pixmap to make a deep copy of the image data
		// otherwise `delete data` will corrupt the pixmap
		QPixmap copy = p;
		copy.detach();

		p = copy;
	}

	delete [] data;

	return p;
}

static QPixmap pixmap( const HICON &icon, bool alpha = true )
{
	try
	{
		ICONINFO info;
		::GetIconInfo(icon, &info);

		QPixmap pixmap = alpha ? fancyPants( info )	: qt_pixmapFromWinHBITMAP(info.hbmColor, HBitmapNoAlpha);

		// gah Win32 is annoying!
		::DeleteObject( info.hbmColor );
		::DeleteObject( info.hbmMask );

		::DestroyIcon( icon );

		return pixmap;
	}
	catch (...)
	{
		return QPixmap();
	}
}

QPixmap associatedIcon( const QString &path )
{
	// performance tuned using:
	// http://www.codeguru.com/Cpp/COM-Tech/shell/article.php/c4511/

	SHFILEINFOW file_info;
	memset(&file_info, 0, sizeof(file_info));
	::SHGetFileInfoW((wchar_t*)path.utf16(), FILE_ATTRIBUTE_NORMAL, &file_info, sizeof(SHFILEINFOW), SHGFI_USEFILEATTRIBUTES | SHGFI_ICON | SHGFI_LARGEICON );

	return pixmap( file_info.hIcon );
}

static BOOL CALLBACK EnumWindowsProc(HWND hWnd, LPARAM inst)
{
	if (IsWindowVisible(hWnd) && IsWindowEnabled(hWnd))
	{
		LONG style = GetWindowLong(hWnd, GWL_STYLE);

		if (style & (WS_THICKFRAME|WS_DLGFRAME|WS_POPUP))
		{
			wchar_t WindowTitle[80];

			int len = GetWindowTextW(hWnd, WindowTitle, 80);

			if (len > 0)
			{
				Windows *windows = (Windows*)inst;

				Window window;

				// define minimum information because some of these windows won't be processed
				window.id = (WId)hWnd;
				window.title = QString::fromWCharArray(WindowTitle);

				windows->push_back(window);
			}
		}
	}

	return TRUE;
}

// Windows 2000 = GetModuleFileName()
// Windows XP x32 = GetProcessImageFileName()
// Windows XP x64 = GetProcessImageFileName()

typedef BOOL (WINAPI *QueryFullProcessImageNamePtr)(HANDLE hProcess, DWORD dwFlags, LPWSTR lpExeName, PDWORD lpdwSize);
typedef DWORD (WINAPI *GetProcessImageFileNamePtr)(HANDLE hProcess, LPWSTR lpImageFileName, DWORD nSize);

static QueryFullProcessImageNamePtr pQueryFullProcessImageName = NULL;
static GetProcessImageFileNamePtr pGetProcessImageFileName = NULL;

void createWindowsList(Windows &windows)
{
	if (pQueryFullProcessImageName == NULL)
	{
		pQueryFullProcessImageName = (QueryFullProcessImageNamePtr) QLibrary::resolve("kernel32", "QueryFullProcessImageNameW");
	}

	if (pGetProcessImageFileName == NULL)
	{
		pGetProcessImageFileName = (GetProcessImageFileNamePtr) QLibrary::resolve("psapi", "GetProcessImageFileNameW");
	}

	HMODULE module = GetModuleHandle(NULL);

	Windows currentWindows;

	// list hWnd
	THREADENTRY32 te32;

	// Fill in the size of the structure before using it.
	te32.dwSize = sizeof(THREADENTRY32);

	// Take a snapshot of all running threads
	HANDLE hThreadSnap = CreateToolhelp32Snapshot(TH32CS_SNAPTHREAD, 0);

	// Retrieve information about the first thread,
	if ((hThreadSnap != INVALID_HANDLE_VALUE) && Thread32First(hThreadSnap, &te32))
	{
		// Now walk the thread list of the system,
		// and display information about each thread
		// associated with the specified process
		do
		{
			currentWindows.clear();

			EnumThreadWindows(te32.th32ThreadID, EnumWindowsProc, (LPARAM)&currentWindows);

			if (!currentWindows.empty() && te32.th32OwnerProcessID)
			{
				for(int i = 0; i < currentWindows.size(); ++i)
				{
					HWND hWnd = (HWND)currentWindows[i].id;

					// get process handle
					DWORD pidwin;
					GetWindowThreadProcessId(hWnd, &pidwin);
					HANDLE hProcess = OpenProcess(PROCESS_QUERY_INFORMATION | PROCESS_VM_READ, FALSE, pidwin);

					// get process path
					wchar_t szProcessPath[MAX_PATH];
					DWORD bufSize = MAX_PATH;

					if (pQueryFullProcessImageName != NULL)
					{
						if (pQueryFullProcessImageName(hProcess, 0, (LPWSTR)&szProcessPath, &bufSize) == 0)
						{
							DWORD error = GetLastError();

							qDebug() << "Error" << error;
						}
					}
					else if (pGetProcessImageFileName != NULL)
					{
						bufSize = pGetProcessImageFileName(hProcess, (LPWSTR)&szProcessPath, bufSize);
					}

					currentWindows[i].path = QString::fromWCharArray(szProcessPath, bufSize);

					// icon
					HICON hIcon = NULL;
					UINT count = ExtractIconExW(szProcessPath, -1, NULL, NULL, 1);
					if (count < 1) continue;

					UINT res = ExtractIconExW(szProcessPath, 0, &hIcon, NULL, 1);
					QPixmap pixmap = ::pixmap(hIcon);
					DestroyIcon(hIcon);

					currentWindows[i].icon = pixmap;

					// rectangle
					RECT r;
					BOOL res2 = GetWindowRect(hWnd, &r);

					if (res) currentWindows[i].rect = QRect(QPoint(r.left, r.top), QPoint(r.right, r.bottom));

					windows << currentWindows[i];
				}
			}
		}
		while(Thread32Next(hThreadSnap, &te32));

		CloseHandle(hThreadSnap);
	}
}

void createWindowsList(QAbstractItemModel* model)
{
	QFileIconProvider icon;
	QPixmap filePixmap = icon.icon(QFileIconProvider::File).pixmap(32, 32);
	QPixmap desktopPixmap = icon.icon(QFileIconProvider::Desktop).pixmap(32, 32);

	Windows currentWindows;

	createWindowsList(currentWindows);

	for (int i = 0; i < currentWindows.size(); ++i)
	{
		HWND hWnd = (HWND)currentWindows[i].id;

		if (model->insertRow(0))
		{
			QModelIndex index = model->index(0, 0);

			model->setData(index, currentWindows[i].title);
			model->setData(index, currentWindows[i].icon.isNull() ? filePixmap: currentWindows[i].icon, Qt::DecorationRole);
			model->setData(index, QVariant::fromValue(currentWindows[i]), Qt::UserRole);
		}
	}

	model->sort(0);

	if (model->insertRow(0))
	{
		QModelIndex index = model->index(0, 0);

		model->setData(index, QObject::tr("Whole screen"));
		model->setData(index, desktopPixmap, Qt::DecorationRole);
		model->setData(index, QVariant::fromValue((void*)NULL), Qt::UserRole);
	}
}

bool isWindowMinimized(WId id)
{
	if (!id) return false;

	WINDOWPLACEMENT placement;
	memset(&placement, 0, sizeof(placement));

	if (GetWindowPlacement((HWND)id, &placement))
	{
		if (placement.showCmd == SW_SHOWMINIMIZED)
		{
			return true;
		}
	}

	return false;
}

bool RestoreMinimizedWindow(WId id)
{
	if (id)
	{
		if (isWindowMinimized(id))
		{
			ShowWindow((HWND)id, SW_RESTORE);

			// time needed to restore window
			Sleep(500);

			return true;
		}
	}
	else
	{
		id = QApplication::desktop()->winId();
		// time needed to hide capture dialog
		Sleep(500);
	}

	return false;
}

void MinimizeWindow(WId id)
{
	ShowWindow((HWND)id, SW_MINIMIZE);
}

bool IsUsingComposition()
{
	typedef BOOL (*voidfuncPtr)(void);

	HINSTANCE hInst = LoadLibraryA("UxTheme.dll");

	bool ret = false;

	if (hInst)
	{
		voidfuncPtr fctIsCompositionActive = (voidfuncPtr)GetProcAddress(hInst, "IsCompositionActive");

		if (fctIsCompositionActive)
		{
			// only if compositing is not activated
			if (fctIsCompositionActive())
				ret = true;
		}

		FreeLibrary(hInst);
	}

	return ret;
}

void PutForegroundWindow(WId id)
{
	SetForegroundWindow((HWND)id);
	Sleep(500);
}

typedef BOOL (WINAPI *LPFN_ISWOW64PROCESS) (HANDLE, PBOOL);

bool IsOS64bits()
{
	bool res;

#ifdef _WIN644
	res = true;
#else
	res = false;

	// IsWow64Process is not available on all supported versions of Windows.
	// Use GetModuleHandle to get a handle to the DLL that contains the function
	// and GetProcAddress to get a pointer to the function if available.
	HMODULE module = GetModuleHandleA("kernel32");

	if (module)
	{
		LPFN_ISWOW64PROCESS fnIsWow64Process = (LPFN_ISWOW64PROCESS)GetProcAddress(module, "IsWow64Process");

		if (fnIsWow64Process)
		{
			BOOL bIsWow64 = FALSE;

			if (fnIsWow64Process(GetCurrentProcess(), &bIsWow64))
			{
				res = bIsWow64 == TRUE;
			}
		}
	}
#endif
	return res;
}

#endif
