// vim: set tabstop=4 shiftwidth=4 noexpandtab:
/*
Gwenview: an image viewer
Copyright 2008 Aurélien Gâteau <aurelien.gateau@free.fr>

This program is free software; you can redistribute it and/or
modify it under the terms of the GNU General Public License
as published by the Free Software Foundation; either version 2
of the License, or (at your option) any later version.

This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with this program; if not, write to the Free Software
Foundation, Inc., 51 Franklin Street, Fifth Floor, Cambridge, MA 02110-1301, USA.

*/
// Self
#include "fakemetadatabackend.h"

// Qt
#include <QStringList>

// KDE
#include <kurl.h>

// Local

namespace Gwenview {


void FakeMetaDataBackEnd::storeMetaData(const KUrl&, const MetaData&) {
}


void FakeMetaDataBackEnd::retrieveMetaData(const KUrl& url) {
	QString urlString = url.url();
	MetaData metaData;
	metaData.mRating = int(urlString.length()) % 6;
	metaData.mDescription = url.fileName();
	QStringList lst = url.path().split("/");
	Q_FOREACH(const QString& token, lst) {
		if (!token.isEmpty()) {
			metaData.mTags << '#' + token.toLower();
		}
	}
	emit metaDataRetrieved(url, metaData);
}


QString FakeMetaDataBackEnd::labelForTag(const MetaDataTag& tag) const {
	return tag[1].toUpper() + tag.mid(2);
}


MetaDataTag FakeMetaDataBackEnd::tagForLabel(const QString& label) const {
	return '#' + label.toLower();
}


} // namespace
