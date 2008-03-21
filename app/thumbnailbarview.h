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
#ifndef THUMBNAILBARVIEW_H
#define THUMBNAILBARVIEW_H

// Qt
#include <QAbstractItemDelegate>

// KDE

// Local
#include <lib/thumbnailview/thumbnailview.h>

namespace Gwenview {


class ThumbnailBarItemDelegatePrivate;

class ThumbnailBarItemDelegate : public QAbstractItemDelegate {
	Q_OBJECT
public:
	ThumbnailBarItemDelegate(ThumbnailView*);
	~ThumbnailBarItemDelegate();

	virtual void paint( QPainter * painter, const QStyleOptionViewItem & option, const QModelIndex & index ) const;
	virtual QSize sizeHint( const QStyleOptionViewItem & /*option*/, const QModelIndex & /*index*/ ) const;

private Q_SLOTS:
	void setThumbnailSize(int);

protected:
	virtual bool eventFilter(QObject*, QEvent*);

private:
	ThumbnailBarItemDelegatePrivate* const d;
	friend class ThumbnailBarItemDelegatePrivate;
};


class ThumbnailBarView : public ThumbnailView {
	Q_OBJECT
public:
	ThumbnailBarView(QWidget *);
	~ThumbnailBarView();

	QSize sizeHint() const;

private Q_SLOTS:
	void showContextMenu();

private:
	QStyle* mStyle;
	QString defaultStyleSheet() const;
};

} // namespace

#endif /* THUMBNAILBARVIEW_H */
