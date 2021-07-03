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
    void onDownload();
	void onFinished(QNetworkReply *reply);
	void onDownloadProgress(qint64 done, qint64 total);
	void onExportCSV();
	void onImportCSV();
	void onClear();

protected:
	void showEvent(QShowEvent *e);

	void downloadNextBatch();

	bool downloadFile();
	bool downloadUrl(const QString &url);
	void downloadNextFile();

	QString getLastDirectoryFromUrl(const QString &url);
	QString getBeforeLastDirectoryFromUrl(const QString &url);
	QString directoryFromUrl(const QString &url);
	QString fileNameFromUrl(const QString &url);

	bool loadSettings();
	bool saveSettings();

	void printLog(const QString &style, const QString &str);
	void printInfo(const QString &str);
	void printWarning(const QString &str);
	void printError(const QString &str);
	void updateProgress();

	QString redirectUrl(const QString& newUrl, const QString& oldUrl) const;

	bool loadCSV(const QString& file);
	bool saveCSV(const QString& file) const;

	void saveCurrent();
	void restoreCurrent();

	QProgressBar *m_progressCurrent;
	QProgressBar *m_progressTotal;
	QLabel *m_fileLabel;
	QNetworkAccessManager *m_manager;
	int m_currentFile;
	int m_maskCount;
	QString m_urlFormat;
	QString m_refererFormat;
	QSettings m_settings;

	QWinTaskbarButton *m_button;

	bool m_downloading;

	Batches m_batches;

	Batch m_current;
};

#endif
