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
 *  $Id: mainwindow.cpp 628 2010-02-03 20:23:05Z kervala $
 *
 */

#include "common.h"
#include "mainwindow.h"

#include <QtWidgets/QFileDialog>

#ifdef Q_OS_WIN32
#include <QtWinExtras/QWinTaskbarProgress>
#include <QtWinExtras/QWinTaskbarButton>
#else
#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>
#include <utime.h>
#endif

#ifdef Q_OS_WIN
 /** Return the offset in 10th of micro sec between the windows base time (
 *	01-01-1601 0:0:0 UTC) and the unix base time (01-01-1970 0:0:0 UTC).
 *	This value is used to convert windows system and file time back and
 *	forth to unix time (aka epoch)
 */
quint64 getWindowsToUnixBaseTimeOffset()
{
	static bool init = false;

	static quint64 offset = 0;

	if (!init)
	{
		// compute the offset to convert windows base time into unix time (aka epoch)
		// build a WIN32 system time for jan 1, 1970
		SYSTEMTIME baseTime;
		baseTime.wYear = 1970;
		baseTime.wMonth = 1;
		baseTime.wDayOfWeek = 0;
		baseTime.wDay = 1;
		baseTime.wHour = 0;
		baseTime.wMinute = 0;
		baseTime.wSecond = 0;
		baseTime.wMilliseconds = 0;

		FILETIME baseFileTime = { 0,0 };
		// convert it into a FILETIME value
		SystemTimeToFileTime(&baseTime, &baseFileTime);
		offset = baseFileTime.dwLowDateTime | (quint64(baseFileTime.dwHighDateTime) << 32);

		init = true;
	}

	return offset;
}
#endif

static bool setFileModificationDate(const QString &filename, const QDateTime &modTime)
{
#if defined (Q_OS_WIN)
	// Use the WIN32 API to set the file times in UTC
	wchar_t wFilename[256];
	int res = filename.toWCharArray(wFilename);

	if (res < filename.size()) return 0;

	wFilename[res] = L'\0';

	// create a file handle (this does not open the file)
	HANDLE h = CreateFileW(wFilename, GENERIC_WRITE | GENERIC_READ, FILE_SHARE_WRITE | FILE_SHARE_READ, NULL, OPEN_EXISTING, 0, 0);
	if (h == INVALID_HANDLE_VALUE)
	{
		qDebug() << QString("Can't set modification date on file '%1' (error accessing file)").arg(filename);
		return false;
	}

	FILETIME creationFileTime;
	FILETIME accessFileTime;
	FILETIME modFileTime;

	// read the current file time
	if (GetFileTime(h, &creationFileTime, &accessFileTime, &modFileTime) == 0)
	{
		qDebug() << QString("Can't set modification date on file '%1'").arg(filename);
		CloseHandle(h);
		return false;
	}

	// win32 file times are in 10th of micro sec (100ns resolution), starting at jan 1, 1601
	// hey Mr Gates, why 1601 ?

	// convert the unix time in ms to a windows file time
	quint64 t = modTime.toMSecsSinceEpoch();
	// convert to 10th of microsec
	t *= 1000;	// microsec
	t *= 10;	// 10th of micro sec (rez of windows file time is 100ns <=> 1/10 us

				// apply the windows to unix base time offset
	t += getWindowsToUnixBaseTimeOffset();

	// update the windows modTime structure
	modFileTime.dwLowDateTime = quint32(t & 0xffffffff);
	modFileTime.dwHighDateTime = quint32(t >> 32);

	// update the file time on disk
	BOOL rez = SetFileTime(h, &creationFileTime, &accessFileTime, &modFileTime);
	if (rez == 0)
	{
		qDebug() << QString("Can't set modification date on file '%1'").arg(filename);

		CloseHandle(h);
		return false;
	}

	// close the handle
	CloseHandle(h);

	return true;

#else
	const char *fn = filename.toUtf8().constData();

	// first, read the current time of the file
	struct stat buf;
	int result = stat(fn, &buf);
	if (result != 0)
		return false;

	// prepare the new time to apply
	utimbuf tb;
	tb.actime = buf.st_atime;
	tb.modtime = modTime.toMSecsSinceEpoch() / 1000;

	// set the new time
	int res = utime(fn, &tb);
	if (res == -1)
	{
		qDebug() << QString("Can't set modification date on file '%1'").arg(filename);
	}

	return res != -1;
#endif
}

MainWindow::MainWindow():QMainWindow(), m_downloading(false)
{
	setupUi(this);

#ifdef Q_OS_WIN32
	m_button = new QWinTaskbarButton(this);
#endif

	m_manager = new QNetworkAccessManager(this);

	m_fileLabel = new QLabel(this);
	m_fileLabel->setMinimumSize(QSize(500, 12));
	statusbar->addWidget(m_fileLabel);

	m_progressCurrent = new QProgressBar(this);
	m_progressCurrent->setMaximumSize(QSize(16777215, 12));
	m_progressCurrent->setMaximum(100);
	statusbar->addPermanentWidget(m_progressCurrent);

	m_progressTotal = new QProgressBar(this);
	m_progressTotal->setMaximumSize(QSize(16777215, 12));
	m_progressTotal->setMaximum(100);
	statusbar->addPermanentWidget(m_progressTotal);

	loadSettings();

	connect(detectFromURLButton, SIGNAL(clicked()), this, SLOT(onDetectFromURL()));
	connect(downloadButton, SIGNAL(clicked()), this, SLOT(download()));
	connect(browseButton, SIGNAL(clicked()), this, SLOT(browse()));
	connect(m_manager, SIGNAL(finished(QNetworkReply*)), SLOT(finish(QNetworkReply*)));

	QTextDocument *doc = new QTextDocument(this);
	doc->setDefaultStyleSheet(".error { color: #f00; }\n.warning { color: #f80; }\n.info { }\n");
	logsTextEdit->setDocument(doc);
}

MainWindow::~MainWindow()
{
}

void MainWindow::showEvent(QShowEvent *e)
{
#ifdef Q_OS_WIN32
	m_button->setWindow(windowHandle());
#endif

	e->accept();
}

bool MainWindow::loadSettings()
{
	urlEdit->setText(m_settings.value("SourceURL").toString());
	filenameParameterEdit->setText(m_settings.value("FilenameParameter").toString());
	refererEdit->setText(m_settings.value("RefererURL").toString());
	userAgentEdit->setText(m_settings.value("UserAgent").toString());
	folderEdit->setText(m_settings.value("DestinationFolder").toString());

	firstSpinBox->setValue(m_settings.value("First").toInt());
	lastSpinBox->setValue(m_settings.value("Last").toInt());
	stepSpinBox->setValue(m_settings.value("Step").toInt());

	useLastDirectoryCheckBox->setChecked(m_settings.value("UseLastDirectoryFromURL").toBool());
	useBeforeLastDirectoryCheckBox->setChecked(m_settings.value("UseBeforeLastDirectoryFromURL").toBool());
	replaceUnderscoresBySpacesCheckBox->setChecked(m_settings.value("ReplaceUnderscoresBySpaces").toBool());
	skipCheckBox->setChecked(m_settings.value("SkipExistingFiles").toBool());
	stopCheckBox->setChecked(m_settings.value("StopOnError").toBool());

	return true;
}

bool MainWindow::saveSettings()
{
	m_settings.setValue("SourceURL", urlEdit->text());
	m_settings.setValue("FilenameParameter", filenameParameterEdit->text());
	m_settings.setValue("RefererURL", refererEdit->text());
	m_settings.setValue("UserAgent", userAgentEdit->text());
	m_settings.setValue("DestinationFolder", folderEdit->text());

	m_settings.setValue("First", firstSpinBox->value());
	m_settings.setValue("Last", lastSpinBox->value());
	m_settings.setValue("Step", stepSpinBox->value());

	m_settings.setValue("UseLastDirectoryFromURL", useLastDirectoryCheckBox->isChecked());
	m_settings.setValue("UseBeforeLastDirectoryFromURL", useBeforeLastDirectoryCheckBox->isChecked());
	m_settings.setValue("ReplaceUnderscoresBySpaces", replaceUnderscoresBySpacesCheckBox->isChecked());
	m_settings.setValue("SkipExistingFiles", skipCheckBox->isChecked());
	m_settings.setValue("StopOnError", stopCheckBox->isChecked());

	return true;
}

QString MainWindow::getLastDirectoryFromUrl(const QString &url)
{
	int posEnd = url.lastIndexOf('/');

	if (posEnd > -1)
	{
		int posStart = url.lastIndexOf('/', posEnd - 1);

		if (posStart > -1)
		{
			return url.mid(posStart + 1, posEnd - posStart - 1);
		}
	}

	printWarning(tr("Unable to find a directory in URL %1").arg(url));

	return "";
}

QString MainWindow::getBeforeLastDirectoryFromUrl(const QString &url)
{
	int posEnd = url.lastIndexOf('/');
	
	posEnd = url.lastIndexOf('/', posEnd - 1);

	if (posEnd > -1)
	{
		int posStart = url.lastIndexOf('/', posEnd - 1);

		if (posStart > -1)
		{
			return url.mid(posStart + 1, posEnd - posStart - 1);
		}
	}

	printWarning(tr("Unable to find a directory in URL %1").arg(url));

	return "";
}

QString MainWindow::directoryFromUrl(const QString &url)
{
	QString dir = folderEdit->text();
	QString lastDir;

	if (useLastDirectoryCheckBox->isChecked())
	{
		lastDir = getLastDirectoryFromUrl(url);
	}
	else if (useBeforeLastDirectoryCheckBox->isChecked())
	{
		lastDir = getBeforeLastDirectoryFromUrl(url);
	}

	if (!lastDir.isEmpty())
	{
		if (replaceUnderscoresBySpacesCheckBox->isChecked())
		{
			lastDir.replace("_", " ");
		}

		dir += "/" + lastDir;
	}

	return dir;
}

QString MainWindow::fileNameFromUrl(const QString &url)
{
	int posParameter = -1;
	QString param = filenameParameterEdit->text();
	QString fileName = QFileInfo(url).fileName();
	QString formatFileName = QFileInfo(m_urlFormat).fileName();

	// check if mask is in filename or directory
	bool staticFilename = fileName == formatFileName;

	if (staticFilename)
	{
		int extPos = fileName.lastIndexOf('.');

		QString ext;
		QString base;

		if (extPos > -1)
		{
			ext = fileName.mid(extPos + 1);
			base = fileName.mid(0, extPos);

			fileName = base + "%1." + ext;
		}
		else
		{
			fileName = "%1.jpg";
		}

		fileName = fileName.arg(m_currentFile, m_maskCount, 10, QChar('0'));
	}

	if (!param.isEmpty())
	{
		QRegExp paramReg(param + "=([^&]+)");

		if (paramReg.indexIn(url) > -1)
			fileName = paramReg.cap(1);
	}

	return fileName;
}

struct SNumber
{
	SNumber()
	{
		number = -1;
		pos = -1;
		length = -1;
	}

	int number;
	int pos;
	int length;
};

void MainWindow::onDetectFromURL()
{
	QString url = urlEdit->text();

	// already detected
	if (url.indexOf('#') > -1) return;

	QRegularExpression reg("([0-9]+)");

	QRegularExpressionMatchIterator i = reg.globalMatch(url);

	QVector<SNumber> numbers;

	while (i.hasNext())
	{
		QRegularExpressionMatch match = i.next();

		if (match.hasMatch())
		{
			SNumber number;

			number.number = match.captured().toInt();
			number.pos = match.capturedStart();
			number.length = match.capturedLength();

			// remove too big or small numbers
			if (number.number < 1000 && number.number > 1) numbers.push_back(number);
		}
	}

	if (!numbers.isEmpty())
	{
		// look for greater length
		int maxIndex = -1;

		for (int i = 0; i < numbers.size(); ++i)
		{
			if (maxIndex < 0 || numbers[i].length > numbers[maxIndex].length)
			{
				maxIndex = i;
			}
		}

		SNumber lastNumber = numbers[maxIndex];

		printInfo(tr("Detected %1 files in URL %2").arg(lastNumber.number).arg(url));

		lastSpinBox->setValue(lastNumber.number);

		url.replace(lastNumber.pos, lastNumber.length, QString('#').repeated(lastNumber.length));

		urlEdit->setText(url);
	}
	else
	{
		printWarning(tr("Unable to detect a number in URL %1").arg(url));
	}
}

void MainWindow::browse()
{
	QString folder = QFileDialog::getExistingDirectory(this, tr("Destination folder"), "", QFileDialog::ShowDirsOnly | QFileDialog::DontResolveSymlinks);

	if (!folder.isEmpty())
		folderEdit->setText(folder);
}

void MainWindow::download()
{
	if (m_downloading)
	{
		m_downloading = false;

		downloadButton->setText(tr("Download"));

		return;
	}

	saveSettings();

	m_urlFormat = urlEdit->text();
	m_refererFormat = refererEdit->text();
	m_maskCount = 0;

	QRegExp maskReg("(#+)");
	QString mask;

	if (maskReg.indexIn(m_urlFormat) >= 0)
	{
		mask = maskReg.cap(1);
		m_maskCount = mask.length();
		m_urlFormat.replace(mask, "%1");
	}

	// fix incorrect values
	if (firstSpinBox->value() < 0) firstSpinBox->setValue(0);
	if (lastSpinBox->value() < 0) lastSpinBox->setValue(0);
	if (stepSpinBox->value() < 1) stepSpinBox->setValue(1);

	// invalid file
	m_currentFile = -1;

	// initialize progress range
	if (m_maskCount == 0)
	{
		m_progressTotal->hide();
	}
	else
	{
		m_progressTotal->show();
		m_progressTotal->setMinimum(firstSpinBox->value());
		m_progressTotal->setMaximum(lastSpinBox->value());
	}

	// start download
	m_downloading = true;

	downloadButton->setText(tr("Stop"));

	downloadNextFile();
}

void MainWindow::downloadNextFile()
{
	if (m_downloading)
	{
		// only one file to download (no mask)
		if (m_maskCount == 0)
		{
			if (m_currentFile < 0)
			{
				// no need to use step
				++m_currentFile;

				// error while downloading
				if (!downloadFile()) return;
			}
		}
		else
		{
			updateProgress();

			while(m_currentFile < lastSpinBox->value())
			{
				if (m_currentFile < 0)
				{
					m_currentFile = firstSpinBox->value();
				}
				else
				{
					m_currentFile += stepSpinBox->value();
				}

				// if succeeded to start download, exit from this method
				if (downloadFile()) return;
			}
		}
	}

	m_downloading = false;

	downloadButton->setText(tr("Download"));
}

void MainWindow::updateProgress()
{
	m_progressTotal->setValue(m_currentFile);

#ifdef Q_OS_WIN32
	QWinTaskbarProgress *progress = m_button->progress();

	if (m_currentFile == lastSpinBox->value())
	{
		// end
		progress->hide();
	}
	else if (m_currentFile == firstSpinBox->value())
	{
		// beginning
		progress->show();
		progress->setRange(0, lastSpinBox->value());
	}
	else
	{
		// progress
		progress->show();
		progress->setValue(m_currentFile);
	}
#else
	// TODO: for other OSes
#endif
}

bool MainWindow::downloadFile()
{
	QString str;

	if (m_maskCount)
	{
		str = m_urlFormat.arg(m_currentFile, m_maskCount, 10, QChar('0'));
	}
	else
	{
		str = m_urlFormat;
	}

	m_fileLabel->setText(str);

	QString fileName = directoryFromUrl(str) + "/" + fileNameFromUrl(str);

	// if file already exists
	if (m_settings.value("SkipExistingFiles").toBool() && QFile::exists(fileName))
	{
		printWarning(tr("File %1 already exists, skip it").arg(fileName));
		return false;
	}

	QUrl url(str);

	// if bad url
	if (!url.isValid() || url.scheme().isEmpty())
	{
		printWarning(tr("URL %1 is invalid").arg(str));
		return false;
	}

	QString referer = m_refererFormat;

	if (m_refererFormat.indexOf("%1") > -1)
		referer = referer.arg(m_currentFile, m_maskCount, 10, QChar('0'));

	QNetworkRequest request;
	request.setUrl(url);
	request.setRawHeader("Referer", referer.toUtf8());
	request.setRawHeader("User-Agent", userAgentEdit->text().toUtf8());

	QNetworkReply *reply = m_manager->get(request);

	if (!reply)
	{
		printError(tr("Unable to download %1").arg(str));
		return false;
	}

	connect(reply, SIGNAL(downloadProgress(qint64, qint64)), this, SLOT(downloadProgress(qint64, qint64)));

	return true;
}

void MainWindow::downloadProgress(qint64 done, qint64 total)
{
	m_progressCurrent->setValue(done * 100 / total);
}

void MainWindow::finish(QNetworkReply *reply)
{
	int statusCode = reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt();
	QDateTime lastModified = reply->header(QNetworkRequest::LastModifiedHeader).toDateTime();

	switch (statusCode)
	{
		case 200:
		{
			QString dir = directoryFromUrl(reply->url().toString());

			// create all intermediate directories
			QDir().mkpath(dir);

			QString content = reply->rawHeader("Content-Disposition");

			QString fileName;

			if (!content.isEmpty())
			{
				int pos = content.indexOf("filename=");

				if (pos > -1)
					fileName = content.mid(pos+9);
			}

			if (fileName.isEmpty()) fileName = dir + "/" + fileNameFromUrl(reply->url().toString());

			if (!m_settings.value("SkipExistingFiles").toBool() || !QFile::exists(fileName))
			{
				QFile file(fileName);

				if (file.open(QIODevice::WriteOnly))
				{
					file.write(reply->readAll());
					file.close();

					setFileModificationDate(fileName, lastModified);

					downloadNextFile();
				}
				else
				{
					printError(tr("Unable to save the file %1").arg(fileName));
				}
			}
			else
			{
				printWarning(tr("File %1 already exists").arg(fileName));
			}
		}
		break;

		default:
		printError(tr("Error HTTP %1: %2").arg(statusCode).arg(reply->errorString()));

		if (m_settings.value("StopOnError").toBool())
		{
			m_downloading = false;
		}

		downloadNextFile();
	}

	reply->deleteLater();
}

void MainWindow::printLog(const QString &style, const QString &str)
{
	logsTextEdit->append(QString("<div class='%1'>%2</div>").arg(style).arg(str));
	logsTextEdit->moveCursor(QTextCursor::End);
	logsTextEdit->ensureCursorVisible();
}

void MainWindow::printInfo(const QString &str)
{
	printLog("info", str);
}

void MainWindow::printWarning(const QString &str)
{
	printLog("warning", str);
}

void MainWindow::printError(const QString &str)
{
	printLog("error", str);
}
