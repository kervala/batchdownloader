/*
 *  KORPS is a new role-playing system with its tools
 *  Copyright (C) 2007-2009  Cedric OCHS
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
	// first, read the current time of the file
	struct stat buf;
	int result = stat(fn.c_str(), &buf);
	if (result != 0)
		return false;

	// prepare the new time to apply
	utimbuf tb;
	tb.actime = buf.st_atime;
	tb.modtime = modTime;
	// set eh new time
	int res = utime(fn.c_str(), &tb);
	if (res == -1)
	{
		qDebug() << QString("Can't set modification date on file '%1'").arg(filename);
	}

	return res != -1;
#endif
}

MainWindow::MainWindow():QMainWindow()
{
	setupUi(this);

	manager = new QNetworkAccessManager(this);

	fileLabel = new QLabel(this);
	fileLabel->setMinimumSize(QSize(500, 12));
	statusbar->addWidget(fileLabel);

	sizeLabel = new QLabel(this);
	sizeLabel->setMaximumSize(QSize(16777215, 12));
	statusbar->addPermanentWidget(sizeLabel);

	progress = new QProgressBar(this);
	progress->setMaximumSize(QSize(16777215, 12));
	progress->setMaximum(100);
	statusbar->addPermanentWidget(progress);

	loadSettings();

	connect(downloadButton, SIGNAL(clicked()), this, SLOT(download()));
	connect(browseButton, SIGNAL(clicked()), this, SLOT(browse()));
	connect(manager, SIGNAL(finished(QNetworkReply*)), SLOT(finish(QNetworkReply*)));
}

MainWindow::~MainWindow()
{
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

	m_settings.setValue("SkipExistingFiles", skipCheckBox->isChecked());
	m_settings.setValue("StopOnError", stopCheckBox->isChecked());

	return true;
}

QString MainWindow::fileNameFromUrl(const QString &url)
{
	int posParameter = -1;
	QString param = filenameParameterEdit->text();
	QString fileName = QFileInfo(url).fileName();

	if (!param.isEmpty())
	{
		QRegExp paramReg(param + "=([^&]+)");

		if (paramReg.indexIn(url) > -1)
			fileName = paramReg.cap(1);
	}

	QFileInfo fileInfo(QDir(folderEdit->text()), fileName);
	fileInfo.makeAbsolute();
	QString out = fileInfo.filePath();
	qDebug() << out;
	return out;
}

void MainWindow::browse()
{
	QString folder = QFileDialog::getExistingDirectory(this, tr("Destination folder"),
		"", QFileDialog::ShowDirsOnly | QFileDialog::DontResolveSymlinks);

	if (!folder.isEmpty())
		folderEdit->setText(folder);
}

void MainWindow::download()
{
	saveSettings();

	urlFormat = urlEdit->text();
	refererFormat = refererEdit->text();
	maskCount = 0;

	QRegExp maskReg("(#+)");
	QString mask;

	if (maskReg.indexIn(urlFormat) < 0)
		return;

	mask = maskReg.cap(1);
	maskCount = mask.length();
	urlFormat.replace(mask, "%1");

	currentFile = 0;

	progress->setMinimum(firstSpinBox->value());
	progress->setMaximum(lastSpinBox->value());

	downloadNextFile();
}

bool MainWindow::downloadNextFile()
{
	do
	{
		if (currentFile == 0)
			currentFile = firstSpinBox->value();
		else
			currentFile += stepSpinBox->value();

		if (currentFile > lastSpinBox->value())
			return false;
	}
	while(!downloadFile());

	return true;
}

bool MainWindow::downloadFile()
{
	QString str = urlFormat.arg(currentFile, maskCount, 10, QChar('0'));

	fileLabel->setText(str);
	progress->setValue(currentFile);

	QString fileName = fileNameFromUrl(str);

	// if file already exists
	if (QFile::exists(fileName))
		return false;

	bool dontDownload = false;

	if (dontDownload)
	{
		qDebug() << fileName << "doesn't exist";

		downloadNextFile();
	}
	else
	{
		QUrl url(str);

		// if bad url
		if (!url.isValid() || url.scheme().isEmpty())
			return false;

		QString referer = refererFormat;

		if (refererFormat.indexOf("%1") > -1)
			referer = referer.arg(currentFile, maskCount, 10, QChar('0'));

		QNetworkRequest request;
		request.setUrl(url);
		request.setRawHeader("Referer", referer.toUtf8());
		request.setRawHeader("User-Agent", userAgentEdit->text().toUtf8());

		QNetworkReply *reply = manager->get(request);

		if (!reply)
		{
			qCritical() << "Download failed:" << str;
			return false;
		}

		connect(reply, SIGNAL(downloadProgress(qint64, qint64)), this, SLOT(downloadProgress(qint64, qint64)));
	}

	return true;
}

void MainWindow::downloadProgress(qint64 done, qint64 total)
{
	sizeLabel->setText(QString::number(done));
}

void MainWindow::finish(QNetworkReply *reply)
{
	int statusCode = reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt();
	QDateTime lastModified = reply->header(QNetworkRequest::LastModifiedHeader).toDateTime();

	switch (statusCode)
	{
		case 200:
		{
			QString fileName = QFileInfo(reply->url().path()).fileName();
	
			QString content = reply->rawHeader("Content-Disposition");

			if (!content.isEmpty())
			{
				int pos = content.indexOf("filename=");

				if (pos > -1)
					fileName = content.mid(pos+9);
			}

			QFileInfo fileInfo(QDir(folderEdit->text()), fileName);
			fileInfo.makeAbsolute();
			fileName = fileInfo.filePath();

			if (!m_settings.value("SkipExistingFiles").toBool() || !QFile::exists(fileName))
			{
				QFile file(fileName);

				qDebug() << "create" << fileName;

				if (file.open(QIODevice::WriteOnly))
				{
					file.write(reply->readAll());
					file.close();

					setFileModificationDate(fileName, lastModified);

					downloadNextFile();
				}
				else
				{
					qCritical() << "Unable to save the file" << fileName;
				}
			}
			else
			{
				qCritical() << "File" << fileName << "already exists";
			}
		}
		break;

		default:
		qDebug() << "Error:" << statusCode << reply->errorString();

		if (!m_settings.value("StopOnError").toBool())
		{
			downloadNextFile();
		}
	}

	reply->deleteLater();
}

/*
static int stringToInt(QString str)
{
	int length = str.length();
	int digit, i, value = 0;
	char c;

	for(i=0; i<length; i++)
	{
		c = str[unsigned(i)];

		if (isdigit(c))
		{
			value *= 10;
			digit = int(c) - 48;
			value += digit; 
		}
		else return -1;
	}

	return value;
}

static QString padding(unsigned valeur, unsigned taille)
{
    unsigned reste = 0;
    QString res;
    
    while (valeur != 0)
    {
        reste = valeur % 10;
        valeur /= 10;
        
        res = QChar(48 + reste) + res;
    }
    
    while (res.length() < taille)
    {
        res = "0" + res;
    }
    
    return res;
}

static int analyserFormat(QString format)
{
	bool	trouve = false;
	int		i, nb = 0;

	for(i=0; i < int(format.length()); i++)
	{
		if (format[unsigned(1)] == '/' && nb) return 0;
		else if (format[unsigned(i)] == '#')
		{
			if (trouve) return 0;

			nb++;
		}
		else if (nb) trouve = true;
    }
    
    return nb ? 1:0;
}

static int analyserValeurs(QString format, int debut, int fin, int pas, QString fichier)
{
	QString path;
	wxFileName fic(fichier);
	wxFileName::SplitPath(fichier, &path, NULL, NULL);

	if (!analyserFormat(format)) return 1;
	if (debut < 0) return 2;
	if (fin <= debut) return 3;
	if (pas < 1 || pas > 100) return 4;
	if (!fic.IsOk()) return 5;
	if (fic.FileExists()) return 6;
	if (!wxFileName::DirExists(path)) return 7;

	return 0;
}

void MainWindow::generate()
{
	QString format = srcURL->GetValue();
	QString file = dstFile->GetValue();
	QString error;

	int step = srcStep->value();
	int first = srcFirst->value();
	int last = srcLast->value();

	bool skip = dstSkip->checked();

	switch (analyserValeurs(format, debut, fin, pas, fichier))
	{
		case 1:	erreur = tr("Le format de l'URL est invalide.\nIl doit comporter au moins un #."); break;
		case 2:	erreur = tr("D�but est invalide.\nIl doit �tre positif et entier."); break;
		case 3:	erreur = tr("Fin est invalide.\nIl doit �tre positif, entier et sup�rieur � D�but."); break;
		case 4:	erreur = tr("Pas est invalide.\nIl doit �tre compris entre 1 et 100."); break;
		case 5:	erreur = tr("Le chemin du fichier n'est pas valide."); break;
		case 6:	erreur = tr("Le fichier existe d�j�."); break;
		case 7:	erreur = tr("Le r�pertoire n'existe pas."); break;

		case 0:

		wxFile *out = new wxFile(fichier, wxFile::write);

		if (out)
		{
			out->Write(genererListe(format, debut, fin, pas, fichier, skip));
			out->Close();
		}
		else erreur = tr("Erreur lors de la cr�ation du fichier.");

		break;
	}

	if (erreur != _T("")) wxMessageBox(erreur, tr("Erreur !"), wxOK | wxICON_WARNING);
	else wxMessageBox(tr("Le fichier a correctement �t� cr��."), tr("OK !"), wxOK | wxICON_INFORMATION);
}
*/
