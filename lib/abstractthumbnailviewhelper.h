/*
Gwenview: an image viewer
Copyright 2007 Aurélien Gâteau

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
Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301, USA.

*/
#ifndef ABSTRACTTHUMBNAILVIEWHELPER_H
#define ABSTRACTTHUMBNAILVIEWHELPER_H

#include "gwenviewlib_export.h"

// Qt
#include <QList>
#include <QObject>

class KFileItem;
class KFileItemList;
class QPixmap;

namespace Gwenview {

/**
 * This class is used by the ThumbnailView to request various things.
 */
class GWENVIEWLIB_EXPORT AbstractThumbnailViewHelper : public QObject {
	Q_OBJECT
public:
	AbstractThumbnailViewHelper(QObject* parent);
	virtual ~AbstractThumbnailViewHelper();

	virtual void generateThumbnailsForItems(const KFileItemList& list) = 0;

	virtual void abortThumbnailGenerationForItems(const KFileItemList& list) = 0;

	virtual void showContextMenu(QWidget* parent) = 0;

Q_SIGNALS:
	void thumbnailLoaded(const KFileItem&, const QPixmap&);
};

} // namespace

#endif /* ABSTRACTTHUMBNAILVIEWHELPER_H */
