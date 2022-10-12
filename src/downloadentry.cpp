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
#include "downloadentry.h"

#ifdef DEBUG_NEW
#define new DEBUG_NEW
#endif

DownloadEntry::DownloadEntry():reply(NULL), method(Method::Get), offset(0), count(0), type(0), fileoffset(0), filesize(0), supportsAcceptRanges(false), supportsContentRange(false), file(nullptr)
{
}

DownloadEntry::DownloadEntry(const DownloadEntry& entry) : reply(nullptr), url(entry.url), filename(entry.filename),
referer(entry.referer), method(entry.method), headers(entry.headers), parameters(entry.parameters),
offset(entry.offset), offsetParameter(entry.offsetParameter), count(entry.count), countParameter(entry.countParameter),
type(entry.type), error(entry.error), data(entry.data), time(entry.time), downloadStart(entry.downloadStart),
fileoffset(entry.fileoffset), filesize(entry.filesize),
supportsAcceptRanges(entry.supportsAcceptRanges), supportsContentRange(entry.supportsContentRange),
fullPath(entry.fullPath), file(entry.file)
{
}

DownloadEntry::~DownloadEntry()
{
	if (reply)
	{
		reply->deleteLater();
	}

	closeFile();
}

bool DownloadEntry::operator == (const DownloadEntry& entry) const
{
	return url == entry.url && method == entry.method && parameters == entry.parameters && offset == entry.offset && count == entry.count;
}

DownloadEntry& DownloadEntry::operator = (const DownloadEntry& entry)
{
	reply = nullptr;
	url = entry.url;
	filename = entry.filename;
	referer = entry.referer;
	method = entry.method;
	headers = entry.headers;
	parameters = entry.parameters;
	offset = entry.offset;
	offsetParameter = entry.offsetParameter;
	count = entry.count;
	countParameter = entry.countParameter;
	type = entry.type;
	error = entry.error;
	time = entry.time;
	downloadStart = entry.downloadStart;

	fileoffset = entry.fileoffset;
	filesize = entry.filesize;

	supportsAcceptRanges = entry.supportsAcceptRanges;
	supportsContentRange = entry.supportsContentRange;

	fullPath = entry.fullPath;

	file = nullptr;

	return *this;
}

void DownloadEntry::reset()
{
	reply = nullptr;
	url.clear();
	filename.clear();
	referer.clear();
	method = Method::None;
	headers.clear();
	parameters.clear();
	offset = 0;
	offsetParameter.clear();
	count = 0;
	countParameter.clear();
	type = 0;
	error.clear();
	time = QDateTime();
	downloadStart = QDateTime();

	fileoffset = 0;
	filesize = 0;

	supportsAcceptRanges = false;
	supportsContentRange = false;;

	fullPath.clear();

	file = nullptr;
}

bool DownloadEntry::checkDownloadedFile() const
{
	if (fullPath.isEmpty()) return false;

	QFileInfo file2(fullPath);

	return file2.exists() && file2.size() > 0 && file2.size() == filesize && file2.lastModified().toUTC() == time;
}

bool DownloadEntry::openFile()
{
	closeFile();

	if (fullPath.isEmpty()) return false;

	file.reset(new QFile(fullPath));

	if (file->open(QFile::Append)) return true;

	closeFile();

	return false;
}

void DownloadEntry::closeFile()
{
	if (!file) return;

	file->flush();
	file->close();

	file.clear();
}

bool DownloadEntry::supportsResume() const
{
	return supportsAcceptRanges && supportsContentRange;
}
