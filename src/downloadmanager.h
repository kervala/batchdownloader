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

#ifndef DOWNLOADMANAGER_H
#define DOWNLOADMANAGER_H

class QNetworkAccessManager;
class QTimer;
class QAuthenticator;

#ifndef COMMON_EXPORT
#define COMMON_EXPORT
#endif

struct DownloadEntry
{
	enum Method
	{
		None,
		Get, // for small files
		Head, // for big files
		Post // for forms
	};

	DownloadEntry();
	DownloadEntry(const DownloadEntry &entry);
	~DownloadEntry();

	bool operator == (const DownloadEntry &entry) const;
	DownloadEntry& operator = (const DownloadEntry &entry);

	void reset();
	bool checkDownloadedFile() const;
	bool supportsResume() const;

	bool openFile();
	void closeFile();

	QNetworkReply *reply;
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

	qint64 fileoffset;
	qint64 filesize;

	bool supportsAcceptRanges;
	bool supportsContentRange;

	QString fullPath;
	QSharedPointer<QFile> file; // for head
};

class COMMON_EXPORT DownloadManager : public QObject
{
	Q_OBJECT

public:
	DownloadManager(QObject* parent);
	virtual ~DownloadManager();

	int count() const;
	bool isEmpty() const;

	bool saveFile(const QByteArray &data, const DownloadEntry &entry);

	bool download(const DownloadEntry &entry);
	void downloadNextFile();

	void setUserAgent(const QString &userAgent);
	void setProxy(const QString &proxy);

	void addCookie(const QNetworkCookie& cookie);
	void setCookies(const QList<QNetworkCookie> &cookies, const QString& url);
	QList<QNetworkCookie> getCookies(const QString& url);

	// load cookies from a Netscape/wget compatible TXT file
	bool loadCookiesFromDirectory(const QString& directory, const QString& domain = "");
	bool loadCookiesFromFile(const QString& filename, const QString &domain = "");

	void setStopOnError(bool stop = true);
	void setStopOnExpired(bool stop = true);

signals:
	void downloadQueued(const QString &file);
	void downloadStarted(const DownloadEntry& entry);
	void downloadStop(const DownloadEntry& entry);
	void downloadProgress(int current, int total);
	void downloadSucceeded(const QByteArray &data, const DownloadEntry &entry);
	void downloadRedirected(const QString &url, const DownloadEntry& entry);
	void downloadFailed(const QString &error, const DownloadEntry &entry);
	void downloadSaved(const DownloadEntry &entry);
	void downloadFinished();
	
	void authorizationFailed(const QString& url, const QByteArray &data);

public slots:
	void reset();
	void start();
	void stop();
	void canceled();
	void onReadyRead();
	void onProgress(qint64 done, qint64 total);
	void onHeadFinished();
	void onGetFinished();
	void onPostFinished();
	void addToQueue(const DownloadEntry &entry);
	void removeFromQueue(const QString &file);
	void removeFromQueue(QNetworkReply *reply);
	void removeFromQueue(DownloadEntry *entry);
	void onAuthentication(const QNetworkProxy &proxy, QAuthenticator *auth);
	void onReplyError(QNetworkReply::NetworkError error);
	void onTimeout();
	void onMetaDataChanged();

private:
	QString redirectUrl(const QString &newUrl, const QString &oldUrl) const;
	bool downloadEntry(DownloadEntry *entry);

	DownloadEntry* findEntry(const DownloadEntry &entry) const;
	DownloadEntry* findEntryByNetworkReply(QNetworkReply *reply) const;

	void dumpHeaders(QNetworkReply* reply);
	void dumpCookies(const QString &url);

	QNetworkAccessManager *m_manager;
	bool m_mustStop;
	bool m_stopOnError;
	bool m_stopOnExpired;
	QList<DownloadEntry*> m_entries;
	QString m_userAgent;
	QTimer *m_timerConnection;
	QTimer *m_timerDownload;
	QNetworkProxy m_proxy;
};

#endif
