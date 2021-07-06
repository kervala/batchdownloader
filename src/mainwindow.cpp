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

#include "functions.h"

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

	connect(detectFromURLButton, &QPushButton::clicked, this, &MainWindow::onDetectFromURL);
	connect(downloadButton, &QPushButton::clicked, this, &MainWindow::onDownload);
	connect(browseButton, &QPushButton::clicked, this, &MainWindow::onBrowse);
	connect(m_manager, &QNetworkAccessManager::finished, this, &MainWindow::onFinished);
	connect(urlsImportPushButton, &QPushButton::clicked, this, &MainWindow::onImportCSV);
	connect(urlsExportPushButton, &QPushButton::clicked, this, &MainWindow::onExportCSV);
	connect(urlsClearPushButton, &QPushButton::clicked, this, &MainWindow::onClear);

	QTextDocument *doc = new QTextDocument(this);
	doc->setDefaultStyleSheet(".error { color: #f00; }\n.warning { color: #f80; }\n.info { }\n");
	logsTextEdit->setDocument(doc);

	urlsListView->setModel(new QStringListModel(this));
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

void MainWindow::onImportCSV()
{
	QString filename = QFileDialog::getOpenFileName(this, tr("CSV file to import"),
		"", tr("CSV files (*.csv)"));

	if (filename.isEmpty())
		return;

	if (!loadCSV(filename))
	{
		QMessageBox::critical(this, tr("Error"), tr("Unable to load or parse CSV file."));
	}
}

void MainWindow::onExportCSV()
{
	QString filename = QFileDialog::getSaveFileName(this, tr("CSV file to export"),
		"", tr("CSV Files (*.csv)"));

	if (filename.isEmpty())
		return;

	if (!saveCSV(filename))
	{
		QMessageBox::critical(this, tr("Error"), tr("Unable to save CSV file."));
	}
}
void MainWindow::onClear()
{
	urlsListView->model()->removeRows(0, m_batches.size());

	m_batches.clear();
}

bool MainWindow::loadCSV(const QString& filename)
{
	QFile file(filename);

	if (!file.open(QFile::ReadOnly)) return false;

	// parse first line with headers
	QByteArray line = file.readLine().trimmed();

	QByteArrayList headers = line.split(',');

	int urlIndex = -1;
	int refererIndex = -1;
	int directoryIndex = -1;
	int firstIndex = -1;
	int lastIndex = -1;
	int stepIndex = -1;

	for (int i = 0, ilen = headers.size(); i < ilen; ++i)
	{
		auto header = headers[i];

		if (header == "url")
		{
			urlIndex = i;
		}
		else if (header == "referer")
		{
			refererIndex = i;
		}
		else if (header == "directory")
		{
			directoryIndex = i;
		}
		else if (header == "first")
		{
			firstIndex = i;
		}
		else if (header == "last")
		{
			lastIndex = i;
		}
		else if (header == "step")
		{
			stepIndex = i;
		}
		else
		{
			qDebug() << "Unknown field" << header;

			return false;
		}
	}

	if (urlIndex == -1)
	{
		qDebug() << "URL index is required";

		return false;
	}

	m_batches.clear();

	QStringList urls;

	int rows = 0;

	while (!file.atEnd())
	{
		++rows;

		// parse line with data
		line = file.readLine().trimmed();

		QByteArrayList data;

		bool quoteOpen = false;
		QByteArray value;
		
		for (int i = 0, ilen = line.length(); i < ilen; ++i)
		{
			char c = line[i];

			if (c == '"')
			{
				if (!quoteOpen)
				{
					quoteOpen = true;
				}
				else
				{
					quoteOpen = false;
				}
			}
			else if (c == ',' && !quoteOpen)
			{
				data << value;

				value.clear();
			}
			else
			{
				value += c;
			}
		}

		if (!value.isEmpty())
		{
			data << value;
		}

		if (data.size() != headers.size())
		{
			qDebug() << "Wrong fields number";

			return false;
		}

		Batch batch;

		batch.url = data[urlIndex];

		urls << batch.url;

		if (refererIndex > -1)
		{
			batch.referer = data[refererIndex];
		}

		if (directoryIndex > -1)
		{
			batch.directory = data[directoryIndex];

			// remove quotes
			if (batch.directory.length() > 2 && batch.directory[0] == '"' && batch.directory[batch.directory.length() - 1] == '"')
			{
				batch.directory = batch.directory.mid(1, batch.directory.length() - 2);
			}
		}

		if (firstIndex > -1)
		{
			batch.first = data[firstIndex].toInt();
		}
		else
		{
			batch.first = 1;
		}

		if (lastIndex > -1)
		{
			batch.last = data[lastIndex].toInt();
		}
		else
		{
			batch.last = 1;
		}

		if (stepIndex > -1)
		{
			batch.step = data[stepIndex].toInt();
		}
		else
		{
			batch.step = 1;
		}

		m_batches << batch;
	}

	qobject_cast<QStringListModel*>(urlsListView->model())->setStringList(urls);

	return true;
}

bool MainWindow::saveCSV(const QString& filename) const
{
	QFile file(filename);

	if (!file.open(QFile::WriteOnly)) return false;

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

void MainWindow::onBrowse()
{
	QString folder = QFileDialog::getExistingDirectory(this, tr("Destination folder"), "", QFileDialog::ShowDirsOnly | QFileDialog::DontResolveSymlinks);

	if (!folder.isEmpty())
		folderEdit->setText(folder);
}

void MainWindow::onDownload()
{
	if (m_downloading)
	{
		m_downloading = false;

		downloadButton->setText(tr("Download"));

		restoreCurrent();

		return;
	}

	saveSettings();

	// save settings for later
	if (!m_batches.isEmpty())
	{
		saveCurrent();
	}

	downloadNextBatch();
}

void MainWindow::saveCurrent()
{
	m_current.directory = folderEdit->text();
	m_current.url = urlEdit->text();
	m_current.referer = refererEdit->text();
	m_current.first = firstSpinBox->value();
	m_current.last = lastSpinBox->value();
	m_current.step = stepSpinBox->value();
}

void MainWindow::restoreCurrent()
{
	// restore current batch
	const Batch& batch = m_current;

	folderEdit->setText(batch.directory);
	urlEdit->setText(batch.url);
	refererEdit->setText(batch.referer);
	firstSpinBox->setValue(batch.first);
	lastSpinBox->setValue(batch.last);
	stepSpinBox->setValue(batch.step);
}

void MainWindow::downloadNextBatch()
{
	if (!m_batches.isEmpty())
	{
		const Batch &batch = m_batches[0];

		folderEdit->setText(batch.directory);
		urlEdit->setText(batch.url);
		refererEdit->setText(batch.referer);
		firstSpinBox->setValue(batch.first);
		lastSpinBox->setValue(batch.last);
		stepSpinBox->setValue(batch.step);
	}

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

	if (m_current.url.isEmpty()) return;

	if (m_batches.isEmpty())
	{
		m_downloading = false;

		downloadButton->setText(tr("Download"));

		restoreCurrent();

		return;
	}

	m_batches.removeFirst();

	urlsListView->model()->removeRow(0);

	downloadNextBatch();
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

	return downloadUrl(str);
}

bool MainWindow::downloadUrl(const QString& str)
{
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

	QNetworkReply* reply = m_manager->get(request);

	if (!reply)
	{
		printError(tr("Unable to download %1").arg(str));
		return false;
	}

	connect(reply, &QNetworkReply::downloadProgress, this, &MainWindow::onDownloadProgress);

	return true;
}

void MainWindow::onDownloadProgress(qint64 done, qint64 total)
{
	m_progressCurrent->setValue(total > 0 ? done * 100 / total:0);
}

QString MainWindow::redirectUrl(const QString& newUrl, const QString& oldUrl) const
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

void MainWindow::onFinished(QNetworkReply *reply)
{
	int statusCode = reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt();
	QDateTime lastModified = reply->header(QNetworkRequest::LastModifiedHeader).toDateTime();

	QString contentType = reply->header(QNetworkRequest::ContentTypeHeader).toString();
	QString contentDisposition = reply->header(QNetworkRequest::ContentDispositionHeader).toString();
	QString contentEncoding = QString::fromLatin1(reply->rawHeader("Content-Encoding"));
	QNetworkReply::NetworkError error = reply->error();
	QString url = reply->url().toString();
	QString redirection = reply->attribute(QNetworkRequest::RedirectionTargetAttribute).toUrl().toString();

	QByteArray data = reply->readAll();

	reply->deleteLater();
/*
	nlwarning("Download finished with HTTP status code %d when downloading %s", status, Q2C(url));

	closeFile();

	if (QFileInfo(m_fullPath).size() == m_size)
	{
		bool ok = NLMISC::CFile::setFileModificationDate(m_fullPath.toUtf8().constData(), m_lastModified.toTime_t());

		if (m_listener) m_listener->operationSuccess(m_size);

		emit downloadDone();
	}
	else if (status >= 200 && status < 300)
	{
		if (m_listener) m_listener->operationContinue();
	}
	else
	{
		if (m_listener) m_listener->operationFail(tr("HTTP error: %1").arg(status));
	}
*/
	// if data are still compressed (deflate ?), uncompress them
	if (contentEncoding == "gzip" && !data.isEmpty() && data.at(0) == 0x1f)
	{
		// uncompress data
		//data = Kervala::gUncompress(data);
		qDebug() << "compressed";
	}

	switch (statusCode)
	{
		case 200:
		{
			QString dir = directoryFromUrl(url);

			// create all intermediate directories
			QDir().mkpath(dir);

			QString fileName;

			if (!contentDisposition.isEmpty())
			{
				int pos = contentDisposition.indexOf("filename=");

				if (pos > -1)
					fileName = contentDisposition.mid(pos+9);
			}

			if (fileName.isEmpty()) fileName = dir + "/" + fileNameFromUrl(url);

			if (!m_settings.value("SkipExistingFiles").toBool() || !QFile::exists(fileName))
			{
				QFile file(fileName);

				if (file.open(QIODevice::WriteOnly))
				{
					file.write(data);
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

		case 301:
		case 302:
		case 303:
		case 305:
		case 307:
		case 308:
		{
			QString newUrl = redirectUrl(redirection, url);

			// relative URL
			if (newUrl.left(1) == "/")
			{
				QUrl url2(url);
				newUrl = QString("%1://%2%3").arg(url2.scheme()).arg(url2.host()).arg(newUrl);
			}

			if (!newUrl.isEmpty())
			{
				qDebug() << "redirected from" << url << "to" << newUrl;

				downloadUrl(newUrl);
			}

			break;
		}

		default:
		printError(tr("Error HTTP %1: %2").arg(statusCode).arg(reply->errorString()));

		if (m_settings.value("StopOnError").toBool())
		{
			m_downloading = false;
		}

		downloadNextFile();
	}
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
/*
bool CDownloader::prepareFile(const QString& url, const QString& fullPath)
{
	if (url.isEmpty()) return false;

	m_downloadAfterHead = false;

	if (m_listener) m_listener->operationPrepare();

	m_fullPath = fullPath;
	m_url = url;

	getFileHead();

	return true;
}

bool CDownloader::getFile()
{
	if (m_fullPath.isEmpty() || m_url.isEmpty())
	{
		nlwarning("You forget to call prepareFile before");

		return false;
	}

	m_downloadAfterHead = true;

	getFileHead();

	return true;
}

bool CDownloader::openFile()
{
	closeFile();

	m_file = new QFile(m_fullPath);

	if (m_file->open(QFile::Append)) return true;

	closeFile();

	return false;
}

void CDownloader::closeFile()
{
	if (m_file)
	{
		m_file->close();

		delete m_file;
		m_file = NULL;
	}
}

void CDownloader::getFileHead()
{
	if (m_supportsAcceptRanges)
	{
		QFileInfo fileInfo(m_fullPath);

		if (fileInfo.exists())
		{
			m_offset = fileInfo.size();
		}
		else
		{
			m_offset = 0;
		}

		// continue if offset less than size
		if (m_offset >= m_size)
		{
			if (checkDownloadedFile())
			{
				// file is already downloaded
				if (m_listener) m_listener->operationSuccess(m_size);

				emit downloadDone();
			}
			else
			{
				// or has wrong size
				if (m_listener) m_listener->operationFail(tr("File is larger (%1B) than expected (%2B)").arg(m_offset).arg(m_size));
			}

			return;
		}
	}

	QNetworkRequest request(m_url);
	request.setHeader(QNetworkRequest::UserAgentHeader, "Mozilla/5.0 (Macintosh; Intel Mac OS X 10.8; rv:24.0) Gecko/20100101 Firefox/24.0");

	if (m_supportsAcceptRanges)
	{
		request.setRawHeader("Range", QString("bytes=%1-").arg(m_offset).toLatin1());
	}

	QNetworkReply* reply = m_manager->head(request);

	connect(reply, SIGNAL(finished()), SLOT(onHeadFinished()));
	connect(reply, SIGNAL(error(QNetworkReply::NetworkError)), SLOT(onError(QNetworkReply::NetworkError)));

	startTimer();
}

void CDownloader::downloadFile()
{
	bool ignoreFreeDiskSpaceChecks = CConfigFile::getInstance()->ignoreFreeDiskSpaceChecks();
	qint64 freeSpace = NLMISC::CSystemInfo::availableHDSpace(m_fullPath.toUtf8().constData());

	if (!ignoreFreeDiskSpaceChecks && freeSpace == 0)
	{
		if (m_listener)
		{
			QString error = qFromUtf8(NLMISC::formatErrorMessage(NLMISC::getLastError()));
			m_listener->operationFail(tr("Error '%1' occurred when trying to check free disk space on %2.").arg(error).arg(m_fullPath));
		}
		return;
	}

	if (!ignoreFreeDiskSpaceChecks && freeSpace < m_size - m_offset)
	{
		// we have not enough free disk space to continue download
		if (m_listener) m_listener->operationFail(tr("You only have %1 bytes left on the device, but %2 bytes are needed.").arg(freeSpace).arg(m_size - m_offset));
		return;
	}

	if (!openFile())
	{
		if (m_listener) m_listener->operationFail(tr("Unable to write file"));
		return;
	}

	QNetworkRequest request(m_url);
	request.setHeader(QNetworkRequest::UserAgentHeader, "Opera/9.80 (Windows NT 6.2; Win64; x64) Presto/2.12.388 Version/12.17");

	if (supportsResume())
	{
		request.setRawHeader("Range", QString("bytes=%1-%2").arg(m_offset).arg(m_size - 1).toLatin1());
	}

	QNetworkReply* reply = m_manager->get(request);

	connect(reply, SIGNAL(finished()), SLOT(onDownloadFinished()));
	connect(reply, SIGNAL(error(QNetworkReply::NetworkError)), SLOT(onError(QNetworkReply::NetworkError)));
	connect(reply, SIGNAL(downloadProgress(qint64, qint64)), SLOT(onDownloadProgress(qint64, qint64)));
	connect(reply, SIGNAL(readyRead()), SLOT(onDownloadRead()));

	if (m_listener) m_listener->operationStart();

	startTimer();
}

bool CDownloader::checkDownloadedFile()
{
	QFileInfo file(m_fullPath);

	return file.size() == m_size && file.lastModified().toUTC() == m_lastModified;
}

void CDownloader::onHeadFinished()
{
	stopTimer();

	QNetworkReply* reply = qobject_cast<QNetworkReply*>(sender());

	int status = reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt();
	QString url = reply->url().toString();

	QString redirection = reply->header(QNetworkRequest::LocationHeader).toString();

	m_size = reply->header(QNetworkRequest::ContentLengthHeader).toInt();
	m_lastModified = reply->header(QNetworkRequest::LastModifiedHeader).toDateTime().toUTC();

	QString acceptRanges = QString::fromLatin1(reply->rawHeader("Accept-Ranges"));
	QString contentRange = QString::fromLatin1(reply->rawHeader("Content-Range"));

	reply->deleteLater();

	nlinfo("HTTP status code %d on HEAD for %s", status, Q2C(url));

	if (!redirection.isEmpty())
	{
		nlinfo("Redirected to %s", Q2C(redirection));
	}

	// redirection
	if (status >= 300 && status < 400)
	{
		if (redirection.isEmpty())
		{
			nlwarning("No redirection defined");

			if (m_listener) m_listener->operationFail(tr("Redirection URL is not defined"));
			return;
		}

		// redirection on another server, recheck resume
		m_supportsAcceptRanges = false;
		m_supportsContentRange = false;

		m_referer = m_url;

		// update real URL
		m_url = redirection;

		getFileHead();

		return;
	}

	// we requested without range
	else if (status == 200)
	{
		// update size
		if (m_listener) m_listener->operationInit(0, m_size);

		if (!m_supportsAcceptRanges && acceptRanges == "bytes")
		{
			nlinfo("Server supports resume for %s", Q2C(url));

			// server supports resume, part 1
			m_supportsAcceptRanges = true;

			// request range
			getFileHead();
			return;
		}

		// server doesn't support resume or
		// we requested range, but server always returns 200
		// download from the beginning
		nlwarning("Server doesn't support resume, download %s from the beginning", Q2C(url));
	}

	// we requested with a range
	else if (status == 206)
	{
		// server supports resume
		QRegExp regexp("^bytes ([0-9]+)-([0-9]+)/([0-9]+)$");

		if (m_supportsAcceptRanges && regexp.exactMatch(contentRange))
		{
			m_supportsContentRange = true;
			m_offset = regexp.cap(1).toLongLong();

			// when resuming, Content-Length is the size of missing parts to download
			m_size = regexp.cap(3).toLongLong();

			// update offset and size
			if (m_listener) m_listener->operationInit(m_offset, m_size);

			nlinfo("Server supports resume for %s: offset %" NL_I64 "d, size %" NL_I64 "d", Q2C(url), m_offset, m_size);
		}
		else
		{
			nlwarning("Unable to parse %s", Q2C(contentRange));
		}
	}

	// other status
	else
	{
		if (m_listener) m_listener->operationFail(tr("Incorrect status code: %1").arg(status));
		return;
	}

	if (m_downloadAfterHead)
	{
		if (checkDownloadedFile())
		{
			nlwarning("Same date and size");
		}
		else
		{
			downloadFile();
		}
	}
	else
	{
		emit downloadPrepared();
	}
}

void CDownloader::onError(QNetworkReply::NetworkError error)
{
	QNetworkReply* reply = qobject_cast<QNetworkReply*>(sender());

	nlwarning("Network error %s (%d) when downloading %s", Q2C(reply->errorString()), error, Q2C(m_url));

	if (!m_listener) return;

	if (error == QNetworkReply::OperationCanceledError)
	{
		m_listener->operationStop();
	}
}

void CDownloader::onDownloadProgress(qint64 current, qint64 total)
{
	stopTimer();

	if (!m_listener) return;

	QNetworkReply* reply = qobject_cast<QNetworkReply*>(sender());

	m_listener->operationProgress(m_offset + current, m_url);

	// abort download
	if (m_listener->operationShouldStop() && reply) reply->abort();
}

void CDownloader::onDownloadRead()
{
	QNetworkReply* reply = qobject_cast<QNetworkReply*>(sender());

	if (m_file && reply) m_file->write(reply->readAll());
}
*/