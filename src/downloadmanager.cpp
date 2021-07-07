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
#include "qzipreader.h"

#ifdef DEBUG_NEW
#define new DEBUG_NEW
#endif

DownloadEntry::DownloadEntry():reply(NULL), method(Get), offset(0), count(0), type(0), fileoffset(0), filesize(0), supportsAcceptRanges(false), supportsContentRange(false), file(nullptr)
{
}

DownloadEntry::DownloadEntry(const DownloadEntry& entry) : reply(nullptr), url(entry.url), filename(entry.filename),
referer(entry.referer), method(entry.method), headers(entry.headers), parameters(entry.parameters),
offset(entry.offset), offsetParameter(entry.offsetParameter), count(entry.count), countParameter(entry.countParameter),
type(entry.type), error(entry.error), data(entry.data), time(entry.time), fileoffset(entry.fileoffset), filesize(entry.filesize),
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

	if (file)
	{
		file->close();
		file->deleteLater();
	}
}

bool DownloadEntry::operator == (const DownloadEntry &entry) const
{
	return url == entry.url && method == entry.method && parameters == entry.parameters && offset == entry.offset && count == entry.count;
}

DownloadEntry& DownloadEntry::operator = (const DownloadEntry &entry)
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
	method = None;
	headers.clear();
	parameters.clear();
	offset = 0;
	offsetParameter.clear();
	count = 0;
	countParameter.clear();
	type = 0;
	error.clear();
	time = QDateTime();

	fileoffset = 0;
	filesize = 0;

	supportsAcceptRanges = false;
	supportsContentRange = false;;

	fullPath.clear();

	file = nullptr;
}

bool DownloadEntry::checkDownloadedFile() const
{
	QFileInfo file(fullPath);

	return file.size() == filesize && file.lastModified().toUTC() == time;
}

bool DownloadEntry::openFile()
{
	if (fullPath.isEmpty()) return false;

	closeFile();

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
	stop();
}

int DownloadManager::count() const
{
	return (int)m_entries.size();
}

bool DownloadManager::isEmpty() const
{
	return m_entries.empty();
}

bool DownloadManager::saveFile(const QByteArray &data, const DownloadEntry &entry)
{
	// if no filename has been specified or invalid, don't save it
	if (entry.fullPath.isEmpty() || entry.fullPath.startsWith("?")) return false;

	// don't save empty files
	if (data.size() < 1) return false;

	if (!QFile::exists(entry.fullPath))
	{
		QString directory = QFileInfo(entry.fullPath).absolutePath();

		// create directory if not exists
		if (!QFile::exists(directory)) QDir().mkpath(directory);

		if (getFreeDiskSpace(directory) < data.size())
		{
			emit downloadFailed(tr("Not enough disk space to save %1").arg(directory), entry);

			stop();

			return false;
		}

		if (!::saveFile(entry.fullPath, data, entry.time))
		{
			qCritical() << "Unable to save the file" << entry.fullPath;
			return false;
		}
	}

	emit downloadSaved(entry);

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

void DownloadManager::removeFromQueue(QNetworkReply *reply)
{
	DownloadEntry *entry = NULL;
	int index = -1;

	for (int i = 0, len = m_entries.size(); i < len; ++i)
	{
		entry = m_entries[i];

		if (entry && entry->reply == reply)
		{
			index = i;
			break;
		}
	}

	if (index != -1)
	{
		m_entries.removeAt(index);

		if (entry) delete entry;
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
	foreach(DownloadEntry *entry, m_entries)
	{
		if (entry) downloadEntry(entry);

	downloadNextFile();

	if (m_entries.empty()) emit downloadFinished();
}

void DownloadManager::stop()
{
	reset();

	m_mustStop = true;
}

void DownloadManager::downloadNextFile()
{
	if (!isEmpty())
	{
		DownloadEntry *entry = m_entries.front();

		if (entry) downloadEntry(entry);
	}
	else
	{
		emit downloadFinished();
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

	QUrl url(entry->url);

	if (!url.isValid() || url.scheme().left(4) != "http" || m_mustStop)
	{
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

	if (entry->method == DownloadEntry::Post)
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
	else if (entry->method == DownloadEntry::Get || entry->method == DownloadEntry::Head)
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

		if (entry->method == DownloadEntry::Head)
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

			reply = m_manager->get(request);

			connect(reply, &QNetworkReply::finished, this, &DownloadManager::onGetFinished);
			connect(reply, &QNetworkReply::readyRead, this, &DownloadManager::onReadyRead);
			connect(reply, &QNetworkReply::downloadProgress, this, &DownloadManager::onProgress);
		}
	}
	else
	{
		qCritical() << "Wrong method:" << entry->method;
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
	Q_ASSERT(entry != nullptr);

	int seconds = entry->downloadStart.secsTo(QDateTime::currentDateTime());

	int speed = seconds > 0 ? done / seconds / 1024:0;

	emit downloadProgress(entry->fileoffset + done, entry->fileoffset + total, speed);

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

/*
	int pos = contentDisposition.indexOf("filename=");

	if (pos > -1)
		fileName = contentDisposition.mid(pos + 9);
*/
	QRegularExpression reg("^attachment; filename=\"([a-zA-Z0-9._-]+)\"; filename\\*=utf-8''([a-zA-Z0-9._-]+)$");

	QRegularExpressionMatch match = reg.match(contentDisposition);

	if (match.hasMatch())
	{
		QString asciiFilename = match.captured(1);
		QString utf8Filename = match.captured(2);

		if (asciiFilename != utf8Filename)
		{
			qDebug() << "UTF-8 and ASCII filenames are different";
		}

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

	if (!entry)
	{
		// already removed from queue
		// TODO: what to do?
		return;
	}

	QByteArray data;

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

	if (error != QNetworkReply::NoError)
	{
		if (error == QNetworkReply::UnknownNetworkError && !m_stopOnExpired)
		{
			// connection expired or aborted
			downloadNextFile();
		}
		else
		{
			// CAPTCHA
			if (error == 201 && statusCode == 403)
			{
				removeFromQueue(reply);

				emit authorizationFailed(url, data);
			}
			// email verification
			else if (error == 204 && statusCode == 401)
			{
				removeFromQueue(reply);

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

				if (QFileInfo(entry->fullPath).size() == entry->filesize)
				{
					setFileModificationDate(entry->fullPath, lastModified);

					downloadNextFile();
				}
			}
			else
			{
				QFile file(entry->fullPath);

				if (file.open(QIODevice::WriteOnly))
				{
					file.write(data);
					file.close();

					setFileModificationDate(entry->fullPath, lastModified);

					downloadNextFile();
				}
				else
				{
					//printError(tr("Unable to save the file %1").arg(fileName));
				}
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

			removeFromQueue(reply);

			downloadNextFile();

			break;
		}
		break;

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

	if (isEmpty())
	{
		stop();
		emit downloadFinished();
	}
}

void DownloadManager::onHeadFinished()
{
	QNetworkReply* reply = qobject_cast<QNetworkReply*>(sender());

	if (m_timerDownload->isActive()) m_timerDownload->stop();

	int statusCode = reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt();
	QString contentType = reply->header(QNetworkRequest::ContentTypeHeader).toString();
	QString contentDisposition = reply->header(QNetworkRequest::ContentDispositionHeader).toString();
	QString contentEncoding = QString::fromLatin1(reply->rawHeader("Content-Encoding"));
	QDateTime lastModified = reply->header(QNetworkRequest::LastModifiedHeader).toDateTime().toUTC();
	QString redirection = reply->attribute(QNetworkRequest::RedirectionTargetAttribute).toUrl().toString();
	QString location = reply->header(QNetworkRequest::LocationHeader).toString();
	QNetworkReply::NetworkError error = reply->error();
	QString url = reply->url().toString();

	qint64 size = reply->header(QNetworkRequest::ContentLengthHeader).toLongLong();

	QString acceptRanges = QString::fromLatin1(reply->rawHeader("Accept-Ranges"));
	QString contentRange = QString::fromLatin1(reply->rawHeader("Content-Range"));

	dumpHeaders(reply);
	dumpCookies(url);

	reply->deleteLater();

	DownloadEntry* entry = findEntryByNetworkReply(reply);

	if (!entry)
	{
		// already removed from queue
		// TODO: what to do?
		return;
	}

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
			if (QFile::exists(entry->fullPath))
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

	if (isEmpty())
	{
		stop();
	}
}

void DownloadManager::onPostFinished()
{
	QNetworkReply* reply = qobject_cast<QNetworkReply*>(sender());

	if (m_timerDownload->isActive()) m_timerDownload->stop();

	int statusCode = reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt();
	QString contentType = reply->header(QNetworkRequest::ContentTypeHeader).toString();
	QString contentDisposition = reply->header(QNetworkRequest::ContentDispositionHeader).toString();
	QString contentEncoding = QString::fromLatin1(reply->rawHeader("Content-Encoding"));
	QDateTime lastModified = reply->header(QNetworkRequest::LastModifiedHeader).toDateTime();
	QString redirection = reply->attribute(QNetworkRequest::RedirectionTargetAttribute).toUrl().toString();
	QNetworkReply::NetworkError error = reply->error();
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
				removeFromQueue(reply);

				emit authorizationFailed(url, data);
			}
			// email verification
			else if (error == 204 && statusCode == 401)
			{
				removeFromQueue(reply);

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
		DownloadEntry* entry = findEntryByNetworkReply(reply);

		if (entry)
		{
			switch (statusCode)
			{
			case 200:
			{
				if (!contentDisposition.isEmpty())
				{
					QRegularExpression reg("^attachment; filename=\"([a-zA-Z0-9._-]+)\"; filename\\*=utf-8''([a-zA-Z0-9._-]+)$");

					QRegularExpressionMatch match = reg.match(contentDisposition);

					if (match.hasMatch())
					{
						QString asciiFilename = match.captured(1);
						QString utf8Filename = match.captured(2);

						if (asciiFilename != utf8Filename)
						{
							qDebug() << "UTF-8 and ASCII filenames are different";
						}

						// always use filename from content-disposition
						entry->filename = asciiFilename;
					}
				}

				emit downloadSucceeded(data, *entry);

				// process next AJAX
				if (!data.isEmpty() && entry->count && entry->offset < entry->count)
				{
					// use same parameters
					DownloadEntry* next = new DownloadEntry(*entry);

			removeFromQueue(entry);

			downloadNextFile();

					m_entries << next;
				}

		case 407:
			// ask authorization
			break;

		default:
			processError(entry, tr("Unexpected status code: %1").arg(statusCode));
		}
	}

	if (isEmpty())
	{
		stop();
		emit downloadFinished();
	}
}

QString DownloadManager::redirectUrl(const QString &newUrl, const QString &oldUrl) const
{
	QString redirectUrl;
	/*
	 * Check if the URL is empty and
	 * that we aren't being fooled into a infinite redirect loop.
	 * We could also keep track of how many redirects we have been to
	 * and set a limit to it, but we'll leave that to you.
	 */
	if (!newUrl.isEmpty() && newUrl != oldUrl) redirectUrl = newUrl;

	return redirectUrl;
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
