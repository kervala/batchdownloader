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

#include <QtWidgets/QProgressBar>

class MainWindow : public QMainWindow, public Ui::MainWindow
{
	Q_OBJECT

public:
	MainWindow();
	virtual ~MainWindow();

public slots:
    void browse();
    void download();
	void finish(QNetworkReply *reply);
	void downloadProgress(qint64 done, qint64 total);

protected:
	bool downloadFile();
	bool downloadNextFile();

	QString getLastDirectoryFromUrl(const QString &url);
	QString directoryFromUrl(const QString &url);
	QString fileNameFromUrl(const QString &url);

	bool loadSettings();
	bool saveSettings();

	QProgressBar *progress;
	QLabel *fileLabel;
	QLabel *sizeLabel;
	QNetworkAccessManager *manager;
	int currentFile;
	int maskCount;
	QString urlFormat;
	QString refererFormat;
	QSettings m_settings;
};

#endif
