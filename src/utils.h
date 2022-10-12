/*
 *  kTimer is a timers manager
 *  Copyright (C) 2021  Cedric OCHS
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

#ifndef UTILS_H
#define UTILS_H

#include "window.h"

typedef QVector<Window> Windows;

class QAbstractItemModel;

void mouseLeftClickUp(const QPoint& pos);
void mouseLeftClickDown(const QPoint& pos);

int QKeySequenceToVK(const QKeySequence& seq);
bool isKeyPressed(int key);

void createWindowsList(Windows &windows);
void createWindowsList(QAbstractItemModel *model);
bool RestoreMinimizedWindow(WId id);
void MinimizeWindow(WId id);
void PutForegroundWindow(WId id);
bool IsUsingComposition();
bool IsOS64bits();
bool isWindowMinimized(WId id);
bool isSameWindowAtPos(Window window, const QPoint& pos);

QPixmap grabWindow(WId window);
Window getWindowWithTitle(const QString& title);

QString GetUserAgent();
QString GetSupportedImageFormatsFilter();

QString encodeEntities(const QString& src, const QString& force = QString());
QString decodeEntities(const QString& src);

QString convertDateToISO(const QString &date);
QString convertIDOToDate(const QString &date);

QString base36enc(qint64 value);
QColor average(const QColor &color1, const QColor &color2, qreal coef);

#endif
