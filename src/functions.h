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

#ifndef FUNCTIONS_H
#define FUNCTIONS_H

qint64 getFreeDiskSpace(const QString &path);

bool setFileModificationDate(const QString &filename, const QDateTime &modTime);
QString removeLastPoints(const QString &str);

QString stripParameters(const QString& url);
QString getFilenameFromUrl(const QString& url);
bool parseUrl(const QString& url, QString* basename, QString* ext);
QString getChecksumFromUrl(const QString& url);
QString fixFilename(const QString& url);
QString makeFilenameFromUrl(const QString& url, const QString& mediaId);

bool saveFile(const QString& filename, const QByteArray& data, const QDateTime& date);

#endif
