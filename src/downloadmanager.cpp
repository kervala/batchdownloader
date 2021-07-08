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
#include "downloadmanager.h"
#include "moc_downloadmanager.cpp"

#include "functions.h"
#include "downloadentry.h"
#include "qzipreader.h"

#ifdef DEBUG_NEW
#define new DEBUG_NEW
#endif

DownloadManager::DownloadManager(QObject *parent) : QObject(parent), m_mustStop(false), m_stopOnError(true), m_stopOnExpired(false), m_queueInitialSize(0)
{
	m_manager = new QNetworkAccessManager(this);

	connect(m_manager, &QNetworkAccessManager::proxyAuthenticationRequired, this, &DownloadManager::onAuthentication);

	m_timerConnection = new QTimer(this);
	m_timerConnection->setSingleShot(true);
	m_timerConnection->setInterval(60000);
	connect(m_timerConnection, &QTimer::timeout, this, &DownloadManager::onTimeout);

	m_timerDownload = new QTimer(this);
	m_timerDownload->setSingleShot(true);
	m_timerDownload->setInterval(300000);
	connect(m_timerDownload, &QTimer::timeout, this, &DownloadManager::onTimeout);
}

DownloadManager::~DownloadManager()
{
	reset();
}

int DownloadManager::count() const
{
	return (int)m_entries.size();
}

bool DownloadManager::isEmpty() const
{
	return m_entries.empty();
}

bool DownloadManager::saveFile(DownloadEntry* entry, const QByteArray &data)
{
	// if no filename has been specified or invalid, don't save it
	if (entry->fullPath.isEmpty() || entry->fullPath.startsWith("?")) return false;

	// don't save empty files
	if (data.size() < 1) return false;

	if (!QFile::exists(entry->fullPath))
	{
		QString directory = QFileInfo(entry->fullPath).absolutePath();

		// create directory if not exists
		if (!QFile::exists(directory)) QDir().mkpath(directory);

		if (getFreeDiskSpace(directory) < data.size())
		{
			emit downloadFailed(tr("Not enough disk space to save %1").arg(directory), *entry);

			stop();

			return false;
		}

		if (!::saveFile(entry->fullPath, data, entry->time))
		{
			processError(entry, tr("Unable to save %1").arg(entry->fullPath));

			return false;
		}
	}

	emit downloadSaved(*entry);

	return true;
}

DownloadEntry* DownloadManager::findEntry(const DownloadEntry &entry) const
{
	foreach(DownloadEntry *e, m_entries)
	{
		if (*e == entry) return e;
	}

	return NULL;
}

DownloadEntry* DownloadManager::findEntryByNetworkReply(QNetworkReply *reply) const
{
	foreach(DownloadEntry *entry, m_entries)
	{
		if (entry->reply == reply) return entry;
	}

	return NULL;
}

void DownloadManager::addToQueue(const DownloadEntry &lentry)
{
	if (findEntry(lentry)) return;

	m_entries << new DownloadEntry(lentry);

	emit downloadQueued(lentry.url);
}

void DownloadManager::removeFromQueue(const QString &url)
{
	DownloadEntry *entry = NULL;
	int index = -1;

	for (int i = 0, len = m_entries.size(); i < len; ++i)
	{
		entry = m_entries[i];

		if (entry && entry->url == url)
		{
			index = i;
			break;
		}
	}

	if (index != -1)
	{
		m_entries.removeAt(index);

		if (entry)
		{
			delete entry;
		}
	}
}

void DownloadManager::removeFromQueue(DownloadEntry *entry)
{
	m_entries.removeAll(entry);

	delete entry;
}

void DownloadManager::onAuthentication(const QNetworkProxy &/* proxy */, QAuthenticator * /* auth */)
{
//	auth->setUser("");
//	auth->setPassword("");
}

void DownloadManager::onReplyError(QNetworkReply::NetworkError error)
{
	QNetworkReply* reply = qobject_cast<QNetworkReply*>(sender());

	qDebug() << "Network error when downloading" << reply->errorString();

	if (error == QNetworkReply::OperationCanceledError)
	{
		//m_listener->operationStop();
	}
}

void DownloadManager::onTimeout()
{
	if (!m_entries.isEmpty())
	{
		DownloadEntry *entry = m_entries[0];

		if (entry->reply)
		{
			entry->reply->abort();
		}
	}
}

void DownloadManager::onMetaDataChanged()
{
}

void DownloadManager::reset()
{
	m_mustStop = false;

	foreach(DownloadEntry *entry, m_entries)
	{
		if (entry)
		{
			delete entry;
		}
	}

	m_entries.clear();
}

void DownloadManager::start()
{
	m_queueInitialSize = m_entries.size();;

	downloadNextFile();

	emit queueStarted(m_queueInitialSize);
}

void DownloadManager::stop()
{
	m_mustStop = true;
}

void DownloadManager::downloadNextFile()
{
	// download aborted, queue finished and reset all items
	if (m_mustStop)
	{
		emit queueFinished(true);

		reset();

		return;
	}

	// queue finished
	if (isEmpty())
	{
		emit queueProgress(m_queueInitialSize, m_queueInitialSize);
		emit queueFinished(false);

		return;
	}

	// process next item
	DownloadEntry *entry = m_entries.front();

	if (!entry) return;

	if (downloadEntry(entry))
	{
		emit queueProgress(m_queueInitialSize - count(), m_queueInitialSize);
	}
}

bool DownloadManager::download(const DownloadEntry &entry)
{
	// already in queue
	if (findEntry(entry)) return false;

	// make a copy of entry
	DownloadEntry *e = new DownloadEntry(entry);

	// add entry in queue
	m_entries << e;

	emit downloadQueued(entry.url);

	// begins download
	return downloadEntry(e);
}

bool DownloadManager::downloadEntry(DownloadEntry *entry)
{
	if (entry->reply)
	{
		entry->reply->deleteLater();
		entry->reply = NULL;
	}

	if (m_mustStop)
	{
		return false;
	}

	QUrl url(entry->url);

	if (!url.isValid() || url.scheme().left(4) != "http")
	{
		// invalid entry
		removeFromQueue(entry);

		downloadNextFile();

		return false;
	}

	QNetworkRequest request;

	// set referer header
	if (!entry->referer.isEmpty()) request.setRawHeader("Referer", entry->referer.toLatin1());

	// user agent is always global
	if (!m_userAgent.isEmpty()) request.setRawHeader("User-Agent", m_userAgent.toLatin1());

	if (entry->headers.contains("Accept"))
	{
		// custom Accept value
		request.setRawHeader("Accept", entry->headers["Accept"].toLatin1());
	}
	else
	{
		// default Accept value
		request.setRawHeader("Accept", "text/html,application/xhtml+xml,application/xml;q=0.9,*/*;q=0.8");
	}

	request.setRawHeader("Accept-Language", "fr-FR,fr;q=0.9,en-US;q=0.8,en;q=0.7");

	// append custom headers
	QMap<QString, QString>::ConstIterator it = entry->headers.constBegin(), iend = entry->headers.constEnd();

	while (it != iend)
	{
		// Accept already processed before
		if (it.key() != "Accept") request.setRawHeader(it.key().toUtf8(), it.value().toUtf8());

		++it;
	}

	QNetworkReply *reply = NULL;

	if (entry->method == DownloadEntry::Method::Post)
	{
		request.setUrl(url);

		if (entry->headers.contains("Content-Type") && !entry->data.isEmpty())
		{
			// build parameters
			reply = m_manager->post(request, entry->data.toUtf8());
		}
		else
		{
			// required
			request.setHeader(QNetworkRequest::ContentTypeHeader, "application/x-www-form-urlencoded; charset=UTF-8");

			QUrlQuery params;

			// reset iterators
			it = entry->parameters.constBegin();
			iend = entry->parameters.constEnd();

			while (it != iend)
			{
				params.addQueryItem(it.key(), it.value());

				++it;
			}

			// append offset
			if (!entry->offsetParameter.isEmpty()) params.addQueryItem(entry->offsetParameter, QString::number(entry->offset));

			// append count
			if (!entry->countParameter.isEmpty()) params.addQueryItem(entry->countParameter, QString::number(entry->count));

			// build parameters
			reply = m_manager->post(request, params.query().toUtf8());

			connect(reply, &QNetworkReply::finished, this, &DownloadManager::onPostFinished);
		}
	}
	else if (entry->method == DownloadEntry::Method::Get || entry->method == DownloadEntry::Method::Head)
	{
		QString query = url.query();

		// reset iterators
		it = entry->parameters.constBegin();
		iend = entry->parameters.constEnd();

		while (it != iend)
		{
			// and ampersand if existing parameters
			if (!query.isEmpty()) query += "&";

			query += it.key() + "=" + it.value();

			++it;
		}

		// append offset
		if (!entry->offsetParameter.isEmpty())
		{
			// and ampersand if existing parameters
			if (!query.isEmpty()) query += "&";

			query += entry->offsetParameter + "=" + QString::number(entry->offset);
		}

		// append count
		if (!entry->countParameter.isEmpty())
		{
			// and ampersand if existing parameters
			if (!query.isEmpty()) query += "&";

			query += entry->countParameter + "=" + QString::number(entry->count);
		}

		url.setQuery(query);

		request.setUrl(url);

		if (entry->method == DownloadEntry::Method::Head)
		{
			if (entry->supportsAcceptRanges)
			{
				QFileInfo fileInfo(entry->fullPath);

				if (fileInfo.exists())
				{
					entry->fileoffset = fileInfo.size();
				}
				else
				{
					entry->fileoffset = 0;
				}

				// continue if offset less than size
				if (entry->fileoffset >= entry->filesize)
				{
					if (entry->checkDownloadedFile())
					{
//						emit downloadDone();
						qDebug() << "File is complete";
					}
					else
					{
						// or has wrong size
						qDebug() << "File is larger than expected";
					}

					return true;
				}
			}

			if (entry->supportsAcceptRanges)
			{
				request.setRawHeader("Range", QString("bytes=%1-").arg(entry->fileoffset).toLatin1());
			}

			reply = m_manager->head(request);

			connect(reply, &QNetworkReply::finished, this, &DownloadManager::onHeadFinished);
		}
		else
		{
			if (!entry->fullPath.isEmpty() && !entry->file)
			{
				QString directory = QFileInfo(entry->fullPath).absolutePath();

				// create directory if not exists
				if (!QFile::exists(directory)) QDir().mkpath(directory);

				if (getFreeDiskSpace(directory) < (entry->filesize - entry->fileoffset))
				{
					emit downloadFailed(tr("Not enough disk space to save %1").arg(directory), *entry);

					stop();

					return false;
				}

				if (!entry->openFile())
				{
					qDebug() << "Unable to write file";

					return false;
				}
			}

			if (entry->supportsResume())
			{
				request.setRawHeader("Range", QString("bytes=%1-%2").arg(entry->fileoffset).arg(entry->filesize - 1).toLatin1());
			}

			// start download
			entry->downloadStart = QDateTime::currentDateTime();

			reply = m_manager->get(request);

			connect(reply, &QNetworkReply::finished, this, &DownloadManager::onGetFinished);
			connect(reply, &QNetworkReply::readyRead, this, &DownloadManager::onReadyRead);
			connect(reply, &QNetworkReply::downloadProgress, this, &DownloadManager::onProgress);
		}
	}
	else
	{
		qCritical() << "Wrong method:" << (int)entry->method;
		return false;
	}

	if (!reply)
	{
		qCritical() << "Download failed:" << entry->url;
		return false;
	}

	entry->reply = reply;

	emit downloadStarted(*entry);

	connect(reply, static_cast<void (QNetworkReply::*)(QNetworkReply::NetworkError)>(&QNetworkReply::errorOccurred), this, &DownloadManager::onReplyError);

	m_timerConnection->start();

	return true;
}

void DownloadManager::setUserAgent(const QString &userAgent)
{
	m_userAgent = userAgent;
}

void DownloadManager::setProxy(const QString &p)
{
	if (p.isEmpty())
	{
		m_proxy.setType(QNetworkProxy::NoProxy);
	}
	else
	{
		int pos = p.indexOf(':');

		int port = 80;
		QString host = p;

		if (pos > -1)
		{
			host = p.left(pos);
			port = p.mid(pos + 1).toInt();
		}

		m_proxy.setType(QNetworkProxy::HttpProxy);
		m_proxy.setHostName(host);
		m_proxy.setPort(port);
	}

	m_manager->setProxy(m_proxy);
}

void DownloadManager::addCookie(const QNetworkCookie& cookie)
{
	m_manager->cookieJar()->insertCookie(cookie);
}

void DownloadManager::setCookies(const QList<QNetworkCookie>& cookies, const QString &url)
{
	m_manager->cookieJar()->setCookiesFromUrl(cookies, url);
}

QList<QNetworkCookie> DownloadManager::getCookies(const QString& url)
{
	return m_manager->cookieJar()->cookiesForUrl(url);
}

bool DownloadManager::loadCookiesFromDirectory(const QString& directory, const QString& domain)
{
	QDir dir(directory);

	if (!dir.exists()) return false;

	QFileInfoList files = dir.entryInfoList(QStringList("*.txt"), QDir::Files | QDir::Readable | QDir::NoDotAndDotDot, QDir::Time);

	if (files.isEmpty()) return false;

	QRegularExpression reg("^[0-9a-f]{8}-[0-9a-f]{4}-[0-9a-f]{4}-[0-9a-f]{4}-[0-9a-f]{12}\\.txt$");

	for (const QFileInfo& file : files)
	{
		// Example: 3b52e028-c80e-4b5d-a22d-62564d31d6c1.txt
		if (!reg.match(file.fileName()).hasMatch()) continue;

		if (loadCookiesFromFile(file.absoluteFilePath(), domain)) return true;
	}

	return false;
}

bool DownloadManager::loadCookiesFromFile(const QString& filename, const QString& domain)
{
	QFile file(filename);

	if (!file.open(QFile::ReadOnly)) return false;

	QTextStream in(&file);

	while (!in.atEnd())
	{
		// read each line and trim them
		QString line = in.readLine().trimmed();

		// skip it if empty
		if (line.isEmpty()) continue;

		// ignore comments
		if (line[0] == '#') continue;

		// split variables from <tab>
		QStringList fields = line.split('\t');

		// we expect 7 parameters
		if (fields.size() != 7)
		{
			qDebug() << "not 7 parameters but" << fields.size();
			continue;
		}

		QNetworkCookie cookie;

		int i = 0;

		QString domain = fields[i++];
		bool access = fields[i++] == "TRUE";
		QString path = fields[i++];
		bool secure = fields[i++] == "TRUE";
		qint64 expiration = fields[i++].toLongLong();
		QString name = fields[i++];
		QString value = fields[i++];

		cookie.setDomain(domain);
		cookie.setPath(path);
		cookie.setSecure(secure);

		if (expiration > 0) cookie.setExpirationDate(QDateTime::fromSecsSinceEpoch(expiration));

		cookie.setName(name.toUtf8());
		cookie.setValue(value.toUtf8());

		// add cookie
		if (domain.isEmpty() || cookie.domain().indexOf(domain) > -1) addCookie(cookie);
	}

	file.close();

	return true;
}

void DownloadManager::setStopOnError(bool stop)
{
	m_stopOnError = stop;
}

void DownloadManager::setStopOnExpired(bool stop)
{
	m_stopOnExpired = stop;
}

void DownloadManager::canceled()
{
}

void DownloadManager::onReadyRead()
{
	QNetworkReply* reply = qobject_cast<QNetworkReply*>(sender());

	if (!reply) return;

	DownloadEntry* entry = findEntryByNetworkReply(reply);

	if (!entry) return;

	if (entry->file)
	{
		entry->file->write(reply->readAll());
	}

	// abort after writing data to disk
	if (m_mustStop)
	{
		reply->abort();
	}
}

void DownloadManager::onProgress(qint64 done, qint64 total)
{
	if (m_timerConnection->isActive())
	{
		m_timerConnection->stop();

		m_timerDownload->start();
	}

	QNetworkReply *reply = qobject_cast<QNetworkReply *>(sender());
	Q_ASSERT(reply != nullptr);

	DownloadEntry* entry = findEntryByNetworkReply(reply);

	if (entry)
	{
		qint64 current = entry->fileoffset + done;
		qint64 size = entry->fileoffset + total;

		qint64 percent = current * 100 / size;

		static int s_percent = 0;

		if (percent != s_percent)
		{
			int seconds = entry->downloadStart.secsTo(QDateTime::currentDateTime());

			int speed = seconds > 0 ? done / seconds / 1024 : 0;

			emit downloadProgress(current, size, speed);
		}

		s_percent = percent;
	}

	if (m_mustStop)
	{
		reply->abort();
	}
}

void DownloadManager::processRedirection(DownloadEntry* entry, const QString& redirection)
{
	QString newUrl;

	if (!redirection.isEmpty() && redirection != entry->url)
	{
		newUrl = redirection;
	}

	// relative URL
	if (newUrl.left(1) == "/")
	{
		QUrl url2(entry->url);
		newUrl = QString("%1://%2%3").arg(url2.scheme()).arg(url2.host()).arg(newUrl);
	}

	if (!newUrl.isEmpty())
	{
		qDebug() << "redirected from" << entry->url << "to" << newUrl;

		// use same parameters
		entry->referer = entry->url;
		entry->url = newUrl;

		// redirection on another server, recheck resume
		entry->supportsAcceptRanges = false;
		entry->supportsContentRange = false;
	}
	else
	{
		removeFromQueue(entry);
	}

	downloadNextFile();
}

void DownloadManager::processError(DownloadEntry* entry, const QString& error)
{
	emit downloadFailed(error, *entry);

	removeFromQueue(entry);

	// clear all files in queue
	if (m_stopOnError || m_stopOnExpired)
	{
		stop();
	}
	else
	{
		downloadNextFile();
	}
}

void DownloadManager::processContentDisposition(DownloadEntry *entry, const QString &contentDisposition)
{
	if (contentDisposition.isEmpty()) return;

	QString asciiFilename;

	// both ASCII and UTF-8 filenames
	QRegularExpression reg("^attachment; filename=\"([a-zA-Z0-9._-]+)\"; filename\\*=utf-8''([a-zA-Z0-9._-]+)$");

	QRegularExpressionMatch match = reg.match(contentDisposition);

	if (match.hasMatch())
	{
		asciiFilename = match.captured(1);

		QString utf8Filename = match.captured(2);

		if (asciiFilename != utf8Filename)
		{
			emit downloadWarning(tr("UTF-8 and ASCII filenames are different (ASCII = '%1', UTF-8 = '%2')").arg(asciiFilename).arg(utf8Filename), *entry);
		}
	}
	else
	{
		// ASCII filename
		reg.setPattern("^attachment; filename=\"([a-zA-Z0-9._-]+)\"$");

		match = reg.match(contentDisposition);

		if (match.hasMatch())
		{
			asciiFilename = match.captured(1);
		}
	}

	if (entry->filename != asciiFilename)
	{
		emit downloadWarning(tr("Attachment filenames is different (original = '%1', attachment = '%2')").arg(entry->filename).arg(asciiFilename), *entry);

		// always use filename from content-disposition
		entry->filename = asciiFilename;
	}
}

void DownloadManager::processAcceptRanges(DownloadEntry* entry, const QString& acceptRanges)
{
	if (!entry->supportsAcceptRanges && acceptRanges == "bytes")
	{
		qDebug() << "Server supports resume for" << entry->url;

		// server supports resume, part 1
		entry->supportsAcceptRanges = true;
	}
	else
	{
		// server doesn't support resume or
		// we requested range, but server always returns 200
		// download from the beginning
		qDebug() << "Server doesn't support resume, download" << entry->url << "from the beginning";

		entry->method = DownloadEntry::Method::Get;
	}

	// reprocess it
	downloadEntry(entry);
}

void DownloadManager::processContentRange(DownloadEntry *entry, const QString &contentRange)
{
	// server supports resume
	QRegularExpression reg("^bytes ([0-9]+)-([0-9]+)/([0-9]+)$");

	entry->fileoffset = 0;

	if (entry->supportsAcceptRanges)
	{
		QRegularExpressionMatch match = reg.match(contentRange);

		if (match.hasMatch())
		{
			entry->supportsContentRange = true;
			entry->fileoffset = match.captured(1).toLongLong();

			// when resuming, Content-Length is the size of missing parts to download
			entry->filesize = match.captured(3).toLongLong();

			qDebug() << "Server supports resume for" << entry->url << ":" << entry->fileoffset << "/" << entry->filesize;
		}
		else
		{
			emit downloadWarning(tr("Unable to parse %1").arg(contentRange), *entry);
		}
	}

	entry->method = DownloadEntry::Method::Get;

	// reprocess it in GET
	downloadEntry(entry);
}

void DownloadManager::onGetFinished()
{
	QNetworkReply* reply = qobject_cast<QNetworkReply*>(sender());
	Q_ASSERT(reply != nullptr);

	if (m_timerDownload->isActive()) m_timerDownload->stop();

	int statusCode = reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt();
	QString contentType = reply->header(QNetworkRequest::ContentTypeHeader).toString();
	QString contentDisposition = reply->header(QNetworkRequest::ContentDispositionHeader).toString();
	QString contentEncoding = QString::fromLatin1(reply->rawHeader("Content-Encoding"));
	QDateTime lastModified = reply->header(QNetworkRequest::LastModifiedHeader).toDateTime();
	QString redirection = reply->attribute(QNetworkRequest::RedirectionTargetAttribute).toUrl().toString();
	QNetworkReply::NetworkError error = reply->error();
	QString url = reply->url().toString();

	DownloadEntry* entry = findEntryByNetworkReply(reply);
	Q_ASSERT(entry != nullptr);

	QByteArray data;

	// don't need to call readAll() if all chunks already written to a file
	if (!entry->file)
	{
		data = reply->readAll();

		// if data are still compressed (deflate ?), uncompress them
		if (contentEncoding == "gzip" && !data.isEmpty() && data.at(0) == 0x1f)
		{
			// uncompress data
			data = Kervala::gUncompress(data);
		}
	}

	entry->reply->deleteLater();
	entry->reply = nullptr;

	if (error != QNetworkReply::NoError)
	{
		if ((error == QNetworkReply::OperationCanceledError) || (error == QNetworkReply::UnknownNetworkError && !m_stopOnExpired))
		{
			// connection expired or aborted
			downloadNextFile();
		}
		else
		{
			// CAPTCHA
			if (error == 201 && statusCode == 403)
			{
				removeFromQueue(entry);

				emit authorizationFailed(url, data);
			}
			// email verification
			else if (error == 204 && statusCode == 401)
			{
				removeFromQueue(entry);

				emit authorizationFailed(url, data);
			}
			else
			{
				processError(entry, reply->errorString());
			}
		}
	}
	else
	{
		switch (statusCode)
		{
		case 200:
		case 206:
		{
			processContentDisposition(entry, contentDisposition);

			if (entry->file)
			{
				entry->closeFile();

				qint64 filesize = QFileInfo(entry->fullPath).size();

				if (filesize != entry->filesize)
				{
					processError(entry, tr("File %1 has a wrong size (%2 received / %3 expected)").arg(entry->fullPath).arg(filesize).arg(entry->filesize));

					return;
				}

				setFileModificationDate(entry->fullPath, lastModified);
			}
			else
			{
				if (!saveFile(entry, data)) return;
			}

			emit downloadSucceeded(data, *entry);

			// process next AJAX
			if (!data.isEmpty() && entry->count && entry->offset < entry->count)
			{
				// use same parameters
				DownloadEntry* next = new DownloadEntry(*entry);

				// increase offset
				++next->offset;

				m_entries << next;
			}

			removeFromQueue(entry);

			downloadNextFile();

			break;
		}

		case 301:
		case 302:
		case 303:
		case 305:
		case 307:
		case 308:
		{
			processRedirection(entry, redirection);

			break;
		}

		case 407:
			// ask authorization
			break;

		default:
			processError(entry, tr("Unexpected status code: %1").arg(statusCode));
		}
	}
}

void DownloadManager::onHeadFinished()
{
	QNetworkReply* reply = qobject_cast<QNetworkReply*>(sender());
	Q_ASSERT(reply != nullptr);

	if (m_timerDownload->isActive()) m_timerDownload->stop();

	int statusCode = reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt();
	QString contentType = reply->header(QNetworkRequest::ContentTypeHeader).toString();
	QString contentDisposition = reply->header(QNetworkRequest::ContentDispositionHeader).toString();
	QString contentEncoding = QString::fromLatin1(reply->rawHeader("Content-Encoding"));
	QDateTime lastModified = reply->header(QNetworkRequest::LastModifiedHeader).toDateTime().toUTC();
	QString redirection = reply->attribute(QNetworkRequest::RedirectionTargetAttribute).toUrl().toString();
	QString location = reply->header(QNetworkRequest::LocationHeader).toString();
	QNetworkReply::NetworkError error = reply->error();
	QString errorString = reply->errorString();
	QString url = reply->url().toString();

	qint64 size = reply->header(QNetworkRequest::ContentLengthHeader).toLongLong();

	QString acceptRanges = QString::fromLatin1(reply->rawHeader("Accept-Ranges"));
	QString contentRange = QString::fromLatin1(reply->rawHeader("Content-Range"));

	dumpHeaders(reply);
	dumpCookies(url);

	DownloadEntry* entry = findEntryByNetworkReply(reply);
	Q_ASSERT(entry != nullptr);

	entry->reply->deleteLater();
	entry->reply = nullptr;

	if (error != QNetworkReply::NoError)
	{
		if (error == QNetworkReply::UnknownNetworkError && !m_stopOnExpired)
		{
			// connection expired, retry
			downloadNextFile();
		}
		else
		{
			processError(entry, errorString);
		}
	}
	else
	{
		entry->time = lastModified;
		entry->filesize = size;

		processContentDisposition(entry, contentDisposition);

		switch (statusCode)
		{
		case 200:
		{
			// already downloaded by parts
			if (!entry->fullPath.isEmpty() && QFile::exists(entry->fullPath))
			{
				if (QFileInfo(entry->fullPath).size() == entry->filesize)
				{
					setFileModificationDate(entry->fullPath, lastModified);

					emit downloadSaved(*entry);

					removeFromQueue(entry);

					downloadNextFile();

					return;
				}
			}

			processAcceptRanges(entry, acceptRanges);

			break;
		}

		case 206:
		{
			processContentRange(entry, contentRange);

			break;
		}

		case 301:
		case 302:
		case 303:
		case 305:
		case 307:
		case 308:
		{
			processRedirection(entry, redirection);

			break;
		}

		case 407:
			// ask authorization
			break;

		default:
			processError(entry, tr("Unexpected status code: %1").arg(statusCode));
		}
	}
}

void DownloadManager::onPostFinished()
{
	QNetworkReply* reply = qobject_cast<QNetworkReply*>(sender());
	Q_ASSERT(reply != nullptr);

	if (m_timerDownload->isActive()) m_timerDownload->stop();

	int statusCode = reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt();
	//QString contentType = reply->header(QNetworkRequest::ContentTypeHeader).toString();
	QString contentEncoding = QString::fromLatin1(reply->rawHeader("Content-Encoding"));
	QDateTime lastModified = reply->header(QNetworkRequest::LastModifiedHeader).toDateTime();
	QString redirection = reply->attribute(QNetworkRequest::RedirectionTargetAttribute).toUrl().toString();
	QNetworkReply::NetworkError error = reply->error();
	QString errorString = reply->errorString();
	QString url = reply->url().toString();

	QByteArray data = reply->readAll();

	// if data are still compressed (deflate ?), uncompress them
	if (contentEncoding == "gzip" && !data.isEmpty() && data.at(0) == 0x1f)
	{
		// uncompress data
		data = Kervala::gUncompress(data);
	}

	dumpHeaders(reply);
	dumpCookies(url);

	DownloadEntry* entry = findEntryByNetworkReply(reply);
	Q_ASSERT(entry != nullptr);

	entry->reply->deleteLater();
	entry->reply = nullptr;

	if (error != QNetworkReply::NoError)
	{
		if (error == QNetworkReply::UnknownNetworkError && !m_stopOnExpired)
		{
			// connection expired, retry
			downloadNextFile();
		}
		else
		{
			// CAPTCHA
			if (error == 201 && statusCode == 403)
			{
				removeFromQueue(entry);

				emit authorizationFailed(url, data);
			}
			// email verification
			else if (error == 204 && statusCode == 401)
			{
				removeFromQueue(entry);

				emit authorizationFailed(url, data);
			}
			else
			{
				processError(entry, errorString);
			}
		}
	}
	else
	{
		switch (statusCode)
		{
		case 200:
		{
			emit downloadSucceeded(data, *entry);

			// process next AJAX
			if (!data.isEmpty() && entry->count && entry->offset < entry->count)
			{
				// use same parameters
				DownloadEntry* next = new DownloadEntry(*entry);

				// increase offset
				++next->offset;

				m_entries << next;
			}

			removeFromQueue(entry);

			downloadNextFile();
		}
		break;

		case 301:
		case 302:
		case 303:
		case 305:
		case 307:
		case 308:
		{
			// never redirect after posting data
			emit downloadRedirected(redirection, *entry);

			removeFromQueue(entry);

			downloadNextFile();

			break;
		}

		case 407:
			// ask authorization
			break;

		default:
			processError(entry, tr("Unexpected status code: %1").arg(statusCode));
		}
	}
}

void DownloadManager::dumpHeaders(QNetworkReply *reply)
{
	QNetworkRequest request = reply->request();

	qDebug() << "Request headers:";

	QList<QByteArray> reqHeaders = request.rawHeaderList();

	foreach(QByteArray reqName, reqHeaders)
	{
		QByteArray reqValue = request.rawHeader(reqName);
		qDebug() << reqName << ": " << reqValue;
	}

	qDebug() << "Response headers:";

	QList<QByteArray> headerList = reply->rawHeaderList();

	foreach(QByteArray head, headerList)
	{
		qDebug() << head << ":" << reply->rawHeader(head);
	}
}

void DownloadManager::dumpCookies(const QString &url)
{
	// somehow give reply a value
	QNetworkCookieJar* cookiesJar = m_manager->cookieJar();

	if (cookiesJar)
	{
		qDebug() << "Cookies headers:";

		QList<QNetworkCookie> cookies = cookiesJar->cookiesForUrl(url);

		foreach(const QNetworkCookie & cookie, cookies)
		{
			qDebug() << cookie.name() << ":" << cookie.value();
		}
	}
}
