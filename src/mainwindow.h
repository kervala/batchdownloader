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

#include "ui_mainwindow.h"

class QProgressBar;
class QWinTaskbarButton;
class DownloadManager;

struct DownloadEntry;

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

class MainWindow : public QMainWindow, public Ui::MainWindow
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

	void onQueueStarted(int total);
	void onQueueProgress(int current, int total);
	void onQueueFinished(bool aborted);

	void onDownloadProgress(qint64 done, qint64 total, int speed);
	void onDownloadFailed(const QString& error, const DownloadEntry& entry);
	
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
	void printInfo(const QString &str);
	void printWarning(const QString &str);
	void printError(const QString &str);
	void updateProgress(int currentFile);

	bool loadCSV(const QString& file);
	bool saveCSV(const QString& file) const;

	void saveCurrent();
	void restoreCurrent();

	DownloadManager* m_manager;

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
