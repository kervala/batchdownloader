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

#include "common.h"
#include "utils.h"

static QMap<QChar, QString> s_specialEntities;
static QString s_userAgent;
static QString s_imagesFilter;

void initSpecialEntities()
{
	if (!s_specialEntities.isEmpty()) return;

	s_specialEntities['<'] = "lt";
	s_specialEntities['>'] = "gt";
	s_specialEntities['&'] = "amp";
}

/***************************************************************************//*!
 * @brief Encode all non ASCII characters into &#...;
 * @param[in] src        Text to analyze
 * @param[in,opt] force  Force the characters "list" to be converted.
 * @return ASCII text compatible.
 *
 * @note Original code: http://www.qtforum.org/article/3891/text-encoding.html
 *
 * @warning Do not forget to use QString::fromUtf8()
 */
QString encodeEntities(const QString& src, const QString& force)
{
	initSpecialEntities();

	QString tmp(src);
	uint len = tmp.length();
	uint i = 0;

	while(i < len)
	{
		if (tmp[i].unicode() > 128 || force.contains(tmp[i]))
		{
			QString ent;

			if (s_specialEntities.contains(tmp[i]))
			{
				// look first for named entities
				ent = s_specialEntities[tmp[i]];
			}
			else
			{
				// use unicode value
				ent = "#" + QString::number(tmp[i].unicode());
			}

			QString rp = "&" + ent + ";";
			tmp.replace(i, 1, rp);
			len += rp.length()-1;
			i += rp.length();
		}
		else
		{
			++i;
		}
	}

	return tmp;
}

/***************************************************************************//*!
 * @brief Allows decode &#...; into UNICODE (utf8) character.
 * @param[in] src    Text to analyze
 * @return UNICODE (utf8) text.
 *
 * @note Do not forget to include QRegExp
 */
QString decodeEntities(const QString& src)
{
	initSpecialEntities();

	QString ret(src);
	QRegularExpression re("&#([0-9]+);");

	QRegularExpressionMatchIterator it = re.globalMatch(src);

	while(it.hasNext())
	{
		QRegularExpressionMatch match = it.next();

		ret = ret.replace(match.captured(0), QChar(match.captured(1).toInt(0, 10)));
	}

	return ret;
}

QString convertDateToISO(const QString &date)
{
	// Oct 30, 2014, 1:50:33 PM
	QString format = "MMM d, yyyy, h:mm:ss AP";
	QDateTime dateTime = QLocale(QLocale::English).toDateTime(date, format);
	QString iso = dateTime.toString(Qt::ISODate);
	return iso.replace("T", " ");
}

QString convertIDOToDate(const QString &date)
{
	QString iso = date;
	iso.replace("T", " ");
	QDateTime valid = QDateTime::fromString(iso, Qt::ISODate);

	return QLocale().toString(valid, QLocale::ShortFormat); // valid.toString(Qt::DefaultLocaleShortDate);
}

QString base36enc(qint64 value)
{
	static const QString base36("0123456789abcdefghijklmnopqrstuvwxyz");

	QString res;

	do
	{
		res.prepend(base36[(int)(value % 36)]);
	}
	while (value /= 36);

	return res;
}

QColor average(const QColor &color1, const QColor &color2, qreal coef)
{
	QColor c1 = color1.toHsv();
	QColor c2 = color2.toHsv();

	qreal h = -1.0;

	if (c1.hsvHueF() == -1.0)
	{
		h = c2.hsvHueF();
	}
	else if (c2.hsvHueF() == -1.0)
	{
		h = c1.hsvHueF();
	}
	else
	{
		h = ((1.0 - coef) * c2.hsvHueF()) + (coef * c1.hsvHueF());
	}

	qreal s = ((1.0 - coef) * c2.hsvSaturationF()) + (coef * c1.hsvSaturationF());
	qreal v = ((1.0 - coef) * c2.valueF()) + (coef * c1.valueF());

	return QColor::fromHsvF(h, s, v).toRgb();
}

QString GetUserAgent()
{
	if (s_userAgent.isEmpty())
	{
		QString system;

		auto current = QOperatingSystemVersion::current();

		if (current.type() == QOperatingSystemVersion::Windows)
		{
			system = QString("%1 NT %2.%3; Win%4; ").arg(current.name()).arg(current.majorVersion()).arg(current.minorVersion()).arg(IsOS64bits() ? 64 : 32);

			// application target processor
#ifdef _WIN64
			system += "x64; ";
#else
			system += "i386;";
#endif
		}
		else if (current.type() == QOperatingSystemVersion::MacOS)
		{
			// TODO
		}
		else if (current.type() == QOperatingSystemVersion::Unknown)
		{
			// TODO
		}
		else
		{
			// TODO
		}

		system += QLocale::system().name().replace('_', '-');

		s_userAgent = QString("%1/%2 (%3)").arg(QApplication::applicationName()).arg(QApplication::applicationVersion()).arg(system);
	}

	return s_userAgent;
}

QString GetSupportedImageFormatsFilter()
{
	if (s_imagesFilter.isEmpty())
	{
		QList<QByteArray> formats = QImageReader::supportedImageFormats();

		foreach(const QByteArray &format, formats)
		{
			if (!s_imagesFilter.isEmpty()) s_imagesFilter += "|";

			s_imagesFilter += format;
		}

		s_imagesFilter = "(" + s_imagesFilter + ")";
	}

	return s_imagesFilter;
}

Window getWindowWithTitle(const QString& title)
{
	if (title.isEmpty()) return Window();

	Windows windows;
	createWindowsList(windows);

	for (int i = 0; i < windows.size(); ++i)
	{
		if (windows[i].title == title)
		{
			return windows[i];
		}
	}

	return Window();
}
