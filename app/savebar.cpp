// vim: set tabstop=4 shiftwidth=4 noexpandtab:
/*
Gwenview: an image viewer
Copyright 2007 Aurélien Gâteau <aurelien.gateau@free.fr>

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
Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301, USA.

*/
// Self
#include "savebar.moc"

// Qt
#include <QHBoxLayout>
#include <QLabel>
#include <QToolButton>
#include <QToolTip>

// KDE
#include <kactioncollection.h>
#include <kdebug.h>
#include <klocale.h>
#include <kurl.h>

// Local
#include "lib/document/documentfactory.h"
#include "lib/gwenviewconfig.h"
#include "lib/memoryutils.h"
#include "lib/paintutils.h"

namespace Gwenview {


struct SaveBarPrivate {
	SaveBar* that;
	KActionCollection* mActionCollection;
	QWidget* mSaveBarWidget;
	QWidget* mTopRowWidget;
	QToolButton* mUndoButton;
	QToolButton* mRedoButton;
	QLabel* mMessageLabel;
	QLabel* mActionsLabel;
	QLabel* mTooManyChangesLabel;
	KUrl mCurrentUrl;
	bool mFullScreenMode;

	void initBackground(QWidget* widget) {
		widget->setAutoFillBackground(true);

		QColor color = QToolTip::palette().base().color();
		QColor borderColor = PaintUtils::adjustedHsv(color, 0, 150, 0);

		QString css =
			".QWidget {"
			"	background-color: %1;"
			"	border-top: 1px solid %2;"
			"	border-bottom: 1px solid %2;"
			"}"
			;
		css = css
			.arg(color.name())
			.arg(borderColor.name());
		widget->setStyleSheet(css);
	}


	void updateUndoButtons() {
		mUndoButton->setDefaultAction(mActionCollection->action("edit_undo"));
		mUndoButton->show();
		mRedoButton->setDefaultAction(mActionCollection->action("edit_redo"));
		mRedoButton->show();
	}


	void updateTooManyChangesLabel(const QList<KUrl>& list) {
		qreal maxPercentageOfMemoryUsage = GwenviewConfig::percentageOfMemoryUsageWarning();
		int maxMemoryUsage = MemoryUtils::getTotalMemory() * maxPercentageOfMemoryUsage;
		int memoryUsage = 0;
		Q_FOREACH(const KUrl& url, list) {
			Document::Ptr doc = DocumentFactory::instance()->load(url);
			memoryUsage += doc->memoryUsage();
		}

		mTooManyChangesLabel->setVisible(memoryUsage > maxMemoryUsage);
	}


	void updateTopRowWidget(const QList<KUrl>& lst) {
		QStringList links;
		QString message;

		if (lst.contains(mCurrentUrl)) {
			message = i18n("Current image modified");

			updateUndoButtons();

			if (lst.size() > 1) {
				QString previous = i18n("Previous modified image");
				QString next = i18n("Next modified image");
				if (mCurrentUrl == lst[0]) {
					links << previous;
				} else {
					links << QString("<a href='previous'>%1</a>").arg(previous);
				}
				if (mCurrentUrl == lst[lst.size() - 1]) {
					links << next;
				} else {
					links << QString("<a href='next'>%1</a>").arg(next);
				}
			}
		} else {
			mUndoButton->hide();
			mRedoButton->hide();

			message = i18np("One image modified", "%1 images modified", lst.size());
			if (lst.size() > 1) {
				links << QString("<a href='first'>%1</a>").arg(i18n("Go to first modified image"));
			} else {
				links << QString("<a href='first'>%1</a>").arg(i18n("Go to it"));
			}
		}

		if (lst.contains(mCurrentUrl)) {
			links << QString("<a href='save'>%1</a>").arg(i18n("Save"));
		}
		if (lst.size() > 1) {
			links << QString("<a href='saveAll'>%1</a>").arg(i18n("Save All"));
		}

		mMessageLabel->setText(message);
		mActionsLabel->setText(links.join(" | "));
	}


	void updateWidgetSizes() {
		int height = mSaveBarWidget->sizeHint().height();
		mSaveBarWidget->setFixedHeight(height);
		that->setFixedHeight(height);
	}
};


SaveBar::SaveBar(QWidget* parent, KActionCollection* actionCollection)
: SlideContainer(parent)
, d(new SaveBarPrivate) {
	d->that = this;
	d->mFullScreenMode = false;
	d->mActionCollection = actionCollection;
	d->mSaveBarWidget = new QWidget();
	d->initBackground(d->mSaveBarWidget);

	d->mMessageLabel = new QLabel;
	d->mMessageLabel->setSizePolicy(QSizePolicy::Minimum, QSizePolicy::Fixed);

	d->mUndoButton = new QToolButton;
	d->mUndoButton->setToolButtonStyle(Qt::ToolButtonTextBesideIcon);
	d->mUndoButton->hide();

	d->mRedoButton = new QToolButton;
	d->mRedoButton->setToolButtonStyle(Qt::ToolButtonTextBesideIcon);
	d->mRedoButton->hide();

	d->mActionsLabel = new QLabel;
	d->mActionsLabel->setAlignment(Qt::AlignRight);
	d->mActionsLabel->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);

	d->mTooManyChangesLabel = new QLabel;
	d->mTooManyChangesLabel->setText(
		i18n("You have modified many images. To avoid memory problems, you should save your changes.")
		);

	d->mTopRowWidget = new QWidget;
	QHBoxLayout* rowLayout = new QHBoxLayout(d->mTopRowWidget);
	rowLayout->addWidget(d->mMessageLabel);
	rowLayout->addWidget(d->mUndoButton);
	rowLayout->addWidget(d->mRedoButton);
	rowLayout->addWidget(d->mActionsLabel);
	rowLayout->setMargin(0);
	// Use mUndoButton sizehint instead of d->mTopRowWidget sizehint because at this time mUndoButton is hidden
	d->mTopRowWidget->setFixedHeight(d->mUndoButton->sizeHint().height());

	QVBoxLayout* layout = new QVBoxLayout(d->mSaveBarWidget);
	layout->addWidget(d->mTopRowWidget);
	layout->addWidget(d->mTooManyChangesLabel);
	layout->setMargin(3);
	layout->setSpacing(3);

	hide();

	setContent(d->mSaveBarWidget);

	d->updateWidgetSizes();

	connect(DocumentFactory::instance(), SIGNAL(modifiedDocumentListChanged()),
		SLOT(updateContent()) );

	connect(d->mActionsLabel, SIGNAL(linkActivated(const QString&)),
		SLOT(triggerAction(const QString&)) );
}


SaveBar::~SaveBar() {
	delete d;
}


void SaveBar::setFullScreenMode(bool value) {
	d->mFullScreenMode = value;
	updateContent();
}


void SaveBar::updateContent() {
	QList<KUrl> lst = DocumentFactory::instance()->modifiedDocumentList();
	if (lst.size() == 0) {
		slideOut();
		return;
	}

	if (d->mFullScreenMode) {
		d->mTopRowWidget->hide();
	} else {
		d->mTopRowWidget->show();
		d->updateTopRowWidget(lst);
	}

	d->updateTooManyChangesLabel(lst);

	d->updateWidgetSizes();
	if (d->mFullScreenMode && !d->mTooManyChangesLabel->isVisibleTo(d->mSaveBarWidget)) {
		slideOut();
	} else {
		slideIn();
	}
}


void SaveBar::triggerAction(const QString& action) {
	QList<KUrl> lst = DocumentFactory::instance()->modifiedDocumentList();
	if (action == "save") {
		requestSave(d->mCurrentUrl);
	} else if (action == "saveAll") {
		requestSaveAll();
	} else if (action == "first") {
		goToUrl(lst[0]);
	} else if (action == "previous") {
		int pos = lst.indexOf(d->mCurrentUrl);
		--pos;
		Q_ASSERT(pos >= 0);
		goToUrl(lst[pos]);
	} else if (action == "next") {
		int pos = lst.indexOf(d->mCurrentUrl);
		++pos;
		Q_ASSERT(pos < lst.size());
		goToUrl(lst[pos]);
	} else {
		kWarning() << "Unknown action: " << action ;
	}
}


void SaveBar::setCurrentUrl(const KUrl& url) {
	d->mCurrentUrl = url;
	updateContent();
}


} // namespace
