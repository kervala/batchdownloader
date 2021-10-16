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

#ifndef MAINWINDOW_H
#define MAINWINDOW_H

class QProgressBar;
class QWinTaskbarButton;
class DownloadManager;
class Updater;

struct DownloadEntry;

namespace Ui
{
	class MainWindow;
}

struct Batch
{
	Batch():first(-1), last(-1), step(-1)
	{
	}

	QString url;
	QString referer;
	int first;
	int last;
	int step;
	QString directory;
};

typedef QVector<Batch> Batches;

class MainWindow : public QMainWindow
{
	Q_OBJECT

public:
	MainWindow();
	virtual ~MainWindow();

public slots:
	void onDetectFromURL();
	void onBrowse();
	void onDownloadClicked();
	void onExportCSV();
	void onImportCSV();
	void onClear();

	// help menu
	void onCheckUpdates();
	void onAbout();
	void onAboutQt();

	// signals from OAuth2
	void onNewVersion(const QString& url, const QString& date, uint size, const QString& version);
	void onNoNewVersion();
	void onProgress(qint64 readBytes, qint64 totalBytes);

	void onQueueStarted(int total);
	void onQueueProgress(int current, int total);
	void onQueueFinished(bool aborted);

	void onDownloadStarted(const DownloadEntry& entry);
	void onDownloadProgress(qint64 done, qint64 total, int speed);
	void onDownloadSucceeded(const QByteArray& data, const DownloadEntry& entry);
	void onDownloadSaved(const DownloadEntry& entry);

	void onDownloadInfo(const QString& info, const DownloadEntry& entry);
	void onDownloadWarning(const QString& warning, const DownloadEntry& entry);
	void onDownloadError(const QString& error, const DownloadEntry& entry);

protected:
	void showEvent(QShowEvent *e);

	void downloadNextBatch();

	QString getLastDirectoryFromUrl(const QString &url);
	QString getBeforeLastDirectoryFromUrl(const QString &url);
	QString directoryFromUrl(const QString &url);
	QString fileNameFromUrl(const QString &url, int currentFile);

	bool loadSettings();
	bool saveSettings();

	void printLog(const QString &style, const QString &str);
	void printSuccess(const QString& str);
	void printInfo(const QString &str);
	void printWarning(const QString &str);
	void printError(const QString &str);

	bool loadCSV(const QString& file);
	bool saveCSV(const QString& file) const;

	void saveCurrent();
	void restoreCurrent();

	Ui::MainWindow* m_ui;
	DownloadManager* m_manager;

	Updater* m_updater;

	QLabel* m_fileLabel;
	QLabel* m_speedLabel;
	QProgressBar *m_progressCurrent;
	QProgressBar *m_progressTotal;

	int m_maskCount;
	QString m_urlFormat;
	QString m_refererFormat;

	QSettings m_settings;

	QWinTaskbarButton *m_button;

	Batches m_batches;

	Batch m_current;
};

#endif
