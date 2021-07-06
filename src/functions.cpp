/*
 *  BatchDownloader is a tool to download URLs
 *  Copyright (C) 2013-2018  Cedric OCHS
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
#include "functions.h"

#ifdef Q_OS_WIN32
	#include <windows.h>
#elif defined(Q_OS_MAC)
	#include <sys/mount.h>
	#include <sys/stat.h>
#else
	#include <sys/vfs.h>
	#include <sys/stat.h>
#endif

#define USE_JPEGCHECKER

#ifdef DEBUG_NEW
	#define new DEBUG_NEW
#endif

/// Return a readable text according to the error code submited
QString formatErrorMessage(int errorCode)
{
#ifdef _WIN32
	wchar_t *lpMsgBuf = NULL;
	DWORD len = FormatMessageW(FORMAT_MESSAGE_ALLOCATE_BUFFER | FORMAT_MESSAGE_FROM_SYSTEM | FORMAT_MESSAGE_IGNORE_INSERTS, NULL, errorCode, MAKELANGID(LANG_NEUTRAL, SUBLANG_DEFAULT), (LPWSTR)&lpMsgBuf, 0, NULL);

	// empty buffer, an error occurred
	if (len == 0) return QString("FormatMessage returned error %1").arg(GetLastError());

	// convert wchar_t* to std::string
	QString ret = QString::fromWCharArray(lpMsgBuf, len);

	// Free the buffer.
	LocalFree(lpMsgBuf);

	return ret;
#else
	return strerror(errorCode);
#endif
}

qint64 getFreeDiskSpace(const QString &path)
{
#ifdef _WIN32
	ULARGE_INTEGER free = {0};

	int size = path.length() + 1;

	wchar_t* buffer = new wchar_t[size];

	int len = QDir::toNativeSeparators(path).toWCharArray(buffer);
	if (len < 1) return 0;

	buffer[len] = 0;

	BOOL bRes = ::GetDiskFreeSpaceExW(buffer, &free, NULL, NULL);

	delete[] buffer;

	if (!bRes)
	{
		qDebug() << "GetDiskFreeSpaceEx returned error: " << formatErrorMessage(GetLastError());
		return 0;
	}

	return free.QuadPart;
#else
	struct stat stst;
	struct statfs stfs;

	if (::stat(path.toLocal8Bit(), &stst) == -1) return 0;
	if (::statfs(path.toLocal8Bit(), &stfs) == -1) return 0;

	return stfs.f_bavail * stst.st_blksize;
#endif
}

#ifdef Q_OS_WIN
/** Return the offset in 10th of micro sec between the windows base time (
*	01-01-1601 0:0:0 UTC) and the unix base time (01-01-1970 0:0:0 UTC).
*	This value is used to convert windows system and file time back and
*	forth to unix time (aka epoch)
*/
quint64 getWindowsToUnixBaseTimeOffset()
{
	static bool init = false;

	static quint64 offset = 0;

	if (!init)
	{
		// compute the offset to convert windows base time into unix time (aka epoch)
		// build a WIN32 system time for jan 1, 1970
		SYSTEMTIME baseTime;
		baseTime.wYear = 1970;
		baseTime.wMonth = 1;
		baseTime.wDayOfWeek = 0;
		baseTime.wDay = 1;
		baseTime.wHour = 0;
		baseTime.wMinute = 0;
		baseTime.wSecond = 0;
		baseTime.wMilliseconds = 0;

		FILETIME baseFileTime = { 0,0 };
		// convert it into a FILETIME value
		SystemTimeToFileTime(&baseTime, &baseFileTime);
		offset = baseFileTime.dwLowDateTime | (quint64(baseFileTime.dwHighDateTime) << 32);

		init = true;
	}

	return offset;
}
#endif

bool setFileModificationDate(const QString &filename, const QDateTime &modTime)
{
#if defined (Q_OS_WIN)
	// Use the WIN32 API to set the file times in UTC
	wchar_t wFilename[256];
	int res = filename.toWCharArray(wFilename);

	if (res < filename.size()) return 0;

	wFilename[res] = L'\0';

	// create a file handle (this does not open the file)
	HANDLE h = CreateFileW(wFilename, GENERIC_WRITE | GENERIC_READ, FILE_SHARE_WRITE | FILE_SHARE_READ, NULL, OPEN_EXISTING, 0, 0);
	if (h == INVALID_HANDLE_VALUE)
	{
		qDebug() << QString("Can't set modification date on file '%1' (error accessing file)").arg(filename);
		return false;
	}

	FILETIME creationFileTime;
	FILETIME accessFileTime;
	FILETIME modFileTime;

	// read the current file time
	if (GetFileTime(h, &creationFileTime, &accessFileTime, &modFileTime) == 0)
	{
		qDebug() << QString("Can't set modification date on file '%1'").arg(filename);
		CloseHandle(h);
		return false;
	}

	// win32 file times are in 10th of micro sec (100ns resolution), starting at jan 1, 1601
	// hey Mr Gates, why 1601 ?

	// convert the unix time in ms to a windows file time
	quint64 t = modTime.toMSecsSinceEpoch();
	// convert to 10th of microsec
	t *= 1000;	// microsec
	t *= 10;	// 10th of micro sec (rez of windows file time is 100ns <=> 1/10 us

				// apply the windows to unix base time offset
	t += getWindowsToUnixBaseTimeOffset();

	// update the windows modTime structure
	modFileTime.dwLowDateTime = quint32(t & 0xffffffff);
	modFileTime.dwHighDateTime = quint32(t >> 32);

	// update the file time on disk
	BOOL rez = SetFileTime(h, &creationFileTime, &accessFileTime, &modFileTime);
	if (rez == 0)
	{
		qDebug() << QString("Can't set modification date on file '%1'").arg(filename);

		CloseHandle(h);
		return false;
	}

	// close the handle
	CloseHandle(h);

	return true;

#else
	const char *fn = filename.toUtf8().constData();

	// first, read the current time of the file
	struct stat buf;
	int result = stat(fn, &buf);
	if (result != 0)
		return false;

	// prepare the new time to apply
	utimbuf tb;
	tb.actime = buf.st_atime;
	tb.modtime = modTime.toMSecsSinceEpoch() / 1000;

	// set the new time
	int res = utime(fn, &tb);
	if (res == -1)
	{
		qDebug() << QString("Can't set modification date on file '%1'").arg(filename);
	}

	return res != -1;
#endif
}

QString removeLastPoints(const QString &str)
{
	QString res = str;

	// remove all points at the end, invalid under Windows
	while (!res.isEmpty() && res.right(1) == '.')
	{
		res.resize(res.length() - 1);
	}

	return res;
}

QString stripParameters(const QString& url)
{
	int pos = url.indexOf('?');

	if (pos > -1) return url.left(pos);

	return url;
}

QString getFilenameFromUrl(const QString& url)
{
	QString baseUrl = stripParameters(url);

	// check if there is a directory
	int pos = baseUrl.lastIndexOf('/');

	if (pos > -1)
	{
		baseUrl = baseUrl.mid(pos + 1);
	}

	// check if there is an extension
	pos = baseUrl.lastIndexOf('.');

	if (pos > -1) return baseUrl;

	// empty filename
	return "";
}

bool parseUrl(const QString &url, QString *basename, QString *ext)
{
	QString baseUrl = stripParameters(url);

	// check if there is a directory
	int pos = baseUrl.lastIndexOf('/');

	// no folder
	if (pos == -1) return false;

	// only keep filename
	QString filename = baseUrl.mid(pos + 1);

	// check if there is an extension
	pos = filename.lastIndexOf('.');

	if (pos > -1)
	{
		if (ext) *ext = filename.mid(pos);

		// return base name
		if (basename) *basename = filename.left(pos);
	}
	else
	{
		// return base name
		if (basename) *basename = filename;
	}

	return true;
}

QString getChecksumFromUrl(const QString& url)
{
	// https://c10.patreonusercontent.com/3/eyJwIjoxfQ%3D%3D/patreon-media/p/post/41522731/005fcd89d29e42718988a147b7da83d2/1.jpg

	QRegularExpression reg("/([a-f0-9]{32})/");

	QRegularExpressionMatch match = reg.match(url);

	if (match.hasMatch())
	{
		return match.captured(1);
	}

	return QString();
}

QString fixFilename(const QString& filename)
{
	QString tmp = filename;

	// replace invalid characters by spaces
	tmp.replace(QRegularExpression("[" + QRegularExpression::escape("\n\r\\/:*$?\"<>|") + "]"), QString(" "));

	for (int i = 0; i < tmp.size(); ++i)
	{
		ushort c = tmp[i].unicode();

		if (c > 1000)
		{
			tmp[i] = ' ';
		}
	}

	// remove all dots from the end
	for (int i = tmp.size() - 1; i >= 0; --i)
	{
		ushort c = tmp[i].unicode();

		if (c != '.') break;

		tmp[i] = ' ';
	}

	// replace several spaces by one
	tmp.replace(QRegularExpression(" +"), QString(" "));

	// maximum 100 characters
	if (tmp.length() > 100)
	{
		// search last space
		int pos = tmp.lastIndexOf(' ', 100);

		if (pos > -1)
		{
			// truncate until space
			tmp = tmp.left(pos);
		}
		else
		{
			// truncate at 100 characters
			tmp = tmp.left(100);
		}
	}

	return tmp.trimmed();
}

QString makeFilenameFromUrl(const QString& url, const QString &mediaId)
{
	QString ext, basename;

	if (parseUrl(url, &basename, &ext))
	{
		if (basename != "1")
		{
			qDebug() << "Different from 1.jpg" << basename;
		}
		else
		{
			// don't use .jpg
			basename.clear();
		}
	}

	// no filename, use media ID
	if (basename.isEmpty()) basename = mediaId;

	// no extension, consider it's a JPEG image
	if (ext.isEmpty()) ext = ".jpg";

	return basename + ext;
}

bool saveFile(const QString& filename, const QByteArray& data, const QDateTime& date)
{
	QFile file(filename);

	if (file.open(QIODevice::WriteOnly))
	{
		qint64 len = file.write(data);
		file.close();

		if (!date.isNull() && date.isValid()) setFileModificationDate(filename, date);

		return true;
	}

	return false;
}
