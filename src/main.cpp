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
#include "mainwindow.h"

#ifdef HAVE_CONFIG_H
	#include "config.h"
#endif

#ifdef QT_STATICPLUGIN

#include <QtPlugin>

#if defined(Q_OS_WIN32)
	Q_IMPORT_PLUGIN(QWindowsIntegrationPlugin)

#if (QT_VERSION >= QT_VERSION_CHECK(5, 10, 0))
	Q_IMPORT_PLUGIN(QWindowsVistaStylePlugin)
#endif

#elif defined(Q_OS_MAC)
	Q_IMPORT_PLUGIN(QCocoaIntegrationPlugin)
#else
	Q_IMPORT_PLUGIN(QXcbIntegrationPlugin)
#endif

	Q_IMPORT_PLUGIN(QSvgPlugin)
	Q_IMPORT_PLUGIN(QSvgIconPlugin)
#endif

#ifdef DEBUG_NEW
	#define new DEBUG_NEW
#endif

int main(int argc, char *argv[])
{
#if defined(_MSC_VER) && defined(_DEBUG)
	_CrtSetDbgFlag (_CRTDBG_ALLOC_MEM_DF | _CRTDBG_LEAK_CHECK_DF);
#endif

	QApplication app(argc, argv);

	QApplication::setApplicationName(PRODUCT);
	QApplication::setOrganizationName(AUTHOR);
	QApplication::setApplicationVersion(VERSION);
	QApplication::setWindowIcon(QIcon(":/icons/icon.svg"));

	QLocale locale = QLocale::system();

	QString folder;
	QDir dir(QCoreApplication::applicationDirPath());
	
#if defined(Q_OS_WIN32)

#ifdef _DEBUG
	dir.cdUp();
	dir.cdUp();
#endif

	folder = dir.absolutePath();

#else
	dir.cdUp();

#ifdef Q_OS_MAC
	folder = dir.absolutePath() + "/Resources";
#elif defined(SHARE_PREFIX)
	folder = SHARE_PREFIX;
#else
	folder = QString("%1/share/%2").arg(dir.absolutePath()).arg(TARGET);
#endif

#endif

	folder += "/translations";

	// load application translations
	QTranslator localTranslator;
	if (localTranslator.load(locale, TARGET, "_", folder))
	{
		QCoreApplication::installTranslator(&localTranslator);
	}

	// load Qt default translations
	QTranslator qtTranslator;
	if (qtTranslator.load(locale, "qt", "_", folder))
	{
		QCoreApplication::installTranslator(&qtTranslator);
	}

	MainWindow mainWindow;
	mainWindow.setWindowTitle(app.applicationName());
	mainWindow.show();

	// only memory leaks are from plugins
	return QApplication::exec();
}
