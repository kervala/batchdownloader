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
#include "downloadmanager.h"
#include "downloadentry.h"

#include <QtWidgets/QFileDialog>

#ifdef Q_OS_WIN32
#include <QtWinExtras/QWinTaskbarProgress>
#include <QtWinExtras/QWinTaskbarButton>
#endif

MainWindow::MainWindow():QMainWindow()
{
	setupUi(this);

#ifdef Q_OS_WIN32
	m_button = new QWinTaskbarButton(this);
#endif

	m_manager = new DownloadManager(this);

	connect(m_manager, &DownloadManager::queueStarted, this, &MainWindow::onQueueStarted);
	connect(m_manager, &DownloadManager::queueProgress, this, &MainWindow::onQueueProgress);
	connect(m_manager, &DownloadManager::queueFinished, this, &MainWindow::onQueueFinished);

	connect(m_manager, &DownloadManager::downloadStarted, this, &MainWindow::onDownloadStarted);
	connect(m_manager, &DownloadManager::downloadProgress, this, &MainWindow::onDownloadProgress);
	connect(m_manager, &DownloadManager::downloadSucceeded, this, &MainWindow::onDownloadSucceeded);
	connect(m_manager, &DownloadManager::downloadSaved, this, &MainWindow::onDownloadSaved);

	connect(m_manager, &DownloadManager::downloadInfo, this, &MainWindow::onDownloadInfo);
	connect(m_manager, &DownloadManager::downloadWarning, this, &MainWindow::onDownloadWarning);
	connect(m_manager, &DownloadManager::downloadError, this, &MainWindow::onDownloadError);

	// void downloadRedirected(const QString & url, const QDateTime & lastModified, const DownloadEntry & entry);

	m_fileLabel = new QLabel(this);
	m_fileLabel->setMinimumSize(QSize(500, 12));
	statusbar->addWidget(m_fileLabel);

	m_speedLabel = new QLabel(this);
	m_speedLabel->setMaximumSize(QSize(16777215, 12));
	statusbar->addWidget(m_speedLabel);

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
	connect(downloadButton, &QPushButton::clicked, this, &MainWindow::onDownloadClicked);
	connect(browseButton, &QPushButton::clicked, this, &MainWindow::onBrowse);
	connect(urlsImportPushButton, &QPushButton::clicked, this, &MainWindow::onImportCSV);
	connect(urlsExportPushButton, &QPushButton::clicked, this, &MainWindow::onExportCSV);
	connect(urlsClearPushButton, &QPushButton::clicked, this, &MainWindow::onClear);

	QTextDocument *doc = new QTextDocument(this);
	doc->setDefaultStyleSheet(".error { color: #f00; }\n.warning { color: #f80; }\n.info { }\n.success { #0f0; }\n");
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

		batch.first = firstIndex > -1 ? data[firstIndex].toInt() : 1;
		batch.last = lastIndex > -1 ? data[lastIndex].toInt() : 1;
		batch.step = stepIndex > -1 ? data[stepIndex].toInt() : 1;

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

QString MainWindow::fileNameFromUrl(const QString &url, int currentFile)
{
	int posParameter = -1;
	QString param = filenameParameterEdit->text();
	QString fileName = QFileInfo(url).fileName();
	QString formatFileName = QFileInfo(m_urlFormat).fileName();

	// no mask
	if (m_maskCount > 0)
	{
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

			fileName = fileName.arg(currentFile, m_maskCount, 10, QChar('0'));
		}
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

void MainWindow::onDownloadClicked()
{
	if (!m_manager->isEmpty())
	{
		m_manager->stop();

		return;
	}

	saveSettings();

	// save settings for later
	if (!m_batches.isEmpty())
	{
		saveCurrent();
	}

	// update manager settings before to call it
	m_manager->setStopOnError(m_settings.value("StopOnError").toBool());
	m_manager->setUserAgent(m_settings.value("UserAgent").toString());

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

	// don't reset if URL is empty
	if (batch.url.isEmpty()) return;

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

	QString url = m_urlFormat;

	int first = firstSpinBox->value();
	int last = lastSpinBox->value();
	int step = stepSpinBox->value();

	for (int i = first; i <= last; i += step)
	{
		if (m_maskCount > 0)
		{
			url = m_urlFormat.arg(i, m_maskCount, 10, QChar('0'));
		}

		QString directory = directoryFromUrl(url);

		// create all intermediate directories
		QDir().mkpath(directory);

		QString fileName = fileNameFromUrl(url, i);
		QString fullPath = directory + "/" + fileName;

		// if file already exists
		if (m_settings.value("SkipExistingFiles").toBool() && QFile::exists(fullPath))
		{
			printWarning(tr("File %1 already exists, skip it").arg(fullPath));
			continue;
		}

		QString referer = m_refererFormat;

		if (m_refererFormat.indexOf("%1") > -1)
		{
			referer = referer.arg(i, m_maskCount, 10, QChar('0'));
		}

		DownloadEntry entry;
		entry.url = url;
		entry.referer = referer;
		entry.filename = fileName;
		entry.fullPath = fullPath;
		entry.method = DownloadEntry::Method::Head; // download big files

		m_manager->addToQueue(entry);
	}

	m_manager->start();
}

void MainWindow::onQueueStarted(int total)
{
	downloadButton->setText(tr("Stop"));

	m_progressTotal->setMaximum(total);

#ifdef Q_OS_WIN32
		QWinTaskbarProgress* progress = m_button->progress();

		// beginning
		progress->show();
		progress->setRange(0, total);
#else
		// TODO: for other OSes
#endif
}

void MainWindow::onQueueProgress(int current, int total)
{
	m_progressTotal->setValue(current);

#ifdef Q_OS_WIN32
		QWinTaskbarProgress* progress = m_button->progress();

		// progress
		progress->setValue(current);
#else
	// TODO: for other OSes
#endif
}

void MainWindow::onQueueFinished(bool aborted)
{

#ifdef Q_OS_WIN32
	QWinTaskbarProgress* progress = m_button->progress();

	// end
	progress->hide();
#else
	// TODO: for other OSes
#endif

	if (!aborted)
	{
		// remove current batch if any
		if (!m_batches.isEmpty())
		{
			m_batches.removeFirst();

			urlsListView->model()->removeRow(0);
		}

		if (m_batches.isEmpty())
		{
			restoreCurrent();

			downloadButton->setText(tr("Download"));
		}
		else
		{
			downloadNextBatch();
		}
	}
	else
	{
		// don't delete batches when aborted
		restoreCurrent();

		downloadButton->setText(tr("Download"));
	}
}

void MainWindow::onDownloadStarted(const DownloadEntry& entry)
{
	m_fileLabel->setText(entry.url);

	printInfo(tr("Start downloading: %1").arg(entry.url));
}

void MainWindow::onDownloadProgress(qint64 done, qint64 total, int speed)
{
	m_progressCurrent->setValue(total > 0 ? done * 100 / total:0);

	m_speedLabel->setText(tr("%1 KiB/s").arg(speed));
}

void MainWindow::onDownloadSucceeded(const QByteArray& data, const DownloadEntry& entry)
{
	printSuccess(tr("Download succeeded"));
}

void MainWindow::onDownloadSaved(const DownloadEntry& entry)
{
	printSuccess(tr("File %1 saved").arg(entry.filename));
}

void MainWindow::onDownloadInfo(const QString& info, const DownloadEntry& entry)
{
	printInfo(info);
}

void MainWindow::onDownloadWarning(const QString& warning, const DownloadEntry& entry)
{
	printWarning(warning);
}

void MainWindow::onDownloadError(const QString& error, const DownloadEntry& entry)
{
	printError(error);

	downloadButton->setText(tr("Download"));

	restoreCurrent();
}

void MainWindow::printLog(const QString &style, const QString &str)
{
	logsTextEdit->append(QString("<div class='%1'>%2</div>").arg(style).arg(str));
	logsTextEdit->moveCursor(QTextCursor::End);
	logsTextEdit->ensureCursorVisible();
}

void MainWindow::printSuccess(const QString& str)
{
	printLog("success", str);
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
