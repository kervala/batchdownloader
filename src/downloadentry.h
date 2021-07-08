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

#ifndef DOWNLOADENTRY_H
#define DOWNLOADENTRY_H

struct DownloadEntry
{
	enum class Method
	{
		None,
		Get, // for small files
		Head, // for big files
		Post // for forms
	};

	DownloadEntry();
	DownloadEntry(const DownloadEntry& entry);
	~DownloadEntry();

	bool operator == (const DownloadEntry& entry) const;
	DownloadEntry& operator = (const DownloadEntry& entry);

	void reset();
	bool checkDownloadedFile() const;
	bool supportsResume() const;

	bool openFile();
	void closeFile();

	QNetworkReply* reply;
	QString url;
	QString filename;
	QString referer;
	Method method;
	QMap<QString, QString> headers;
	QMap<QString, QString> parameters;
	int offset;
	QString offsetParameter;
	int count;
	QString countParameter;
	int type; // custom type of request
	QString error;
	QString data;
	QDateTime time;
	QDateTime downloadStart;

	qint64 fileoffset;
	qint64 filesize;

	bool supportsAcceptRanges;
	bool supportsContentRange;

	QString fullPath;
	QSharedPointer<QFile> file; // for head
};

#endif
