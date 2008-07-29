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
Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301, USA.

*/
#include "documentpanel.moc"

// Qt
#include <QLabel>
#include <QShortcut>
#include <QSplitter>
#include <QStylePainter>
#include <QToolButton>
#include <QVBoxLayout>

// KDE
#include <kactioncollection.h>
#include <kdebug.h>
#include <klocale.h>
#include <kmimetype.h>
#include <kparts/componentfactory.h>
#include <kparts/statusbarextension.h>
#include <kstatusbar.h>
#include <ktoggleaction.h>

// Local
#include "thumbnailbarview.h"
#include <lib/imageview.h>
#include <lib/imageviewpart.h>
#include <lib/mimetypeutils.h>
#include <lib/paintutils.h>
#include <lib/gwenviewconfig.h>
#include <lib/statusbartoolbutton.h>


namespace Gwenview {

#undef ENABLE_LOG
#undef LOG
//#define ENABLE_LOG
#ifdef ENABLE_LOG
#define LOG(x) kDebug() << x
#else
#define LOG(x) ;
#endif


static QString rgba(const QColor &color) {
	return QString::fromAscii("rgba(%1, %2, %3, %4)")
		.arg(color.red())
		.arg(color.green())
		.arg(color.blue())
		.arg(color.alpha());
}


static QString gradient(const QColor &color, int value) {
	QString grad =
		"qlineargradient(x1:0, y1:0, x2:0, y2:1,"
		"stop:0 %1, stop: 1 %2)";
	return grad.arg(
		rgba(PaintUtils::adjustedHsv(color, 0, 0, qMin(255 - color.value(), value/2))),
		rgba(PaintUtils::adjustedHsv(color, 0, 0, -qMin(color.value(), value/2)))
		);
}


class SplitterHandle : public QSplitterHandle {
public:
	SplitterHandle(Qt::Orientation orientation, QSplitter* parent)
	: QSplitterHandle(orientation, parent) {}

protected:
	virtual void paintEvent(QPaintEvent*) {
		QStylePainter painter(this);

		QStyleOption opt;
		opt.initFrom(this);

		// Draw a thin styled line below splitter handle
		QStyleOption lineOpt = opt;
		const int lineSize = style()->pixelMetric(QStyle::PM_DefaultFrameWidth, 0, this);
		const int margin = 4 * lineSize;
		lineOpt.rect = QRect(-margin, height() - lineSize, width() + 2*margin, height());
		lineOpt.state |= QStyle::State_Sunken;
		painter.drawPrimitive(QStyle::PE_Frame, lineOpt);

		// Draw the normal splitter handle
		opt.rect.adjust(0, 0, 0, -lineSize);
		painter.drawControl(QStyle::CE_Splitter, opt);
	}
};


/**
 * Home made splitter to be able to define a custom handle:
 * We want to show a thin line between the splitter and the thumbnail bar but
 * we don't do it with css because "border-top:" forces a border around the
 * whole widget (Qt 4.4.0)
 */
class Splitter : public QSplitter {
public:
	Splitter(Qt::Orientation orientation, QWidget* parent)
	: QSplitter(orientation, parent) {
		const int lineSize = style()->pixelMetric(QStyle::PM_DefaultFrameWidth, 0, this);
		setHandleWidth(handleWidth() + lineSize);
	}

protected:
	virtual QSplitterHandle* createHandle() {
		return new SplitterHandle(orientation(), this);
	}
};


/*
 * Layout of mThumbnailSplitter is:
 *
 * +-mThumbnailSplitter--------------------------------+
 * |+-mPartContainer----------------------------------+|
 * ||..part widget....................................||
 * ||.                                               .||
 * ||.                                               .||
 * ||.                                               .||
 * ||.                                               .||
 * ||.                                               .||
 * ||.................................................||
 * ||+-mStatusBarContainer---------------------------+||
 * |||+---------------------------++-mStatusBar-----+|||
 * ||||[mToggleThumbnailBarButton]||                ||||
 * |||+---------------------------++----------------+|||
 * ||+-----------------------------------------------+||
 * |+-------------------------------------------------+|
 * |===================================================|
 * |+-mThumbnailBar-----------------------------------+|
 * ||                                                 ||
 * ||                                                 ||
 * |+-------------------------------------------------+|
 * +---------------------------------------------------+
 */
struct DocumentPanelPrivate {
	DocumentPanel* mView;
	QLabel* mNoDocumentLabel;
	QSplitter *mThumbnailSplitter;
	QWidget* mPartContainer;
	QVBoxLayout* mPartContainerLayout;
	QToolButton* mToggleThumbnailBarButton;
	QWidget* mStatusBarContainer;
	KStatusBar* mStatusBar;
	ThumbnailBarView* mThumbnailBar;
	KToggleAction* mToggleThumbnailBarAction;
	bool mFullScreenMode;
	QPalette mNormalPalette;
	QPalette mFullScreenPalette;
	bool mThumbnailBarVisibleBeforeFullScreen;

	KParts::ReadOnlyPart* mPart;
	QString mPartLibrary;

	void setupNoDocumentLabel() {
		mNoDocumentLabel = new QLabel(mView);
		mNoDocumentLabel->setText(i18n("No document selected"));
		mNoDocumentLabel->setAlignment(Qt::AlignCenter);
		mNoDocumentLabel->setAutoFillBackground(true);
		mNoDocumentLabel->setBackgroundRole(QPalette::Base);
		mNoDocumentLabel->setForegroundRole(QPalette::Text);
	}

	void setupStatusBar() {
		mStatusBarContainer = new QWidget;
		mStatusBar = new KStatusBar;
		KStatusBar* toggleThumbnailBarButtonStatusBar = new KStatusBar;
		mToggleThumbnailBarButton = new StatusBarToolButton;
		toggleThumbnailBarButtonStatusBar->addPermanentWidget(mToggleThumbnailBarButton);

		QHBoxLayout* layout = new QHBoxLayout(mStatusBarContainer);
		layout->setMargin(0);
		layout->setSpacing(0);
		layout->addWidget(toggleThumbnailBarButtonStatusBar);
		layout->addWidget(mStatusBar, 1);
	}

	void setupThumbnailBar() {
		mThumbnailBar = new ThumbnailBarView(mPartContainer);
		ThumbnailBarItemDelegate* delegate = new ThumbnailBarItemDelegate(mThumbnailBar);
		mThumbnailBar->setItemDelegate(delegate);
		mThumbnailBar->setVisible(GwenviewConfig::thumbnailBarIsVisible());

		QColor bgColor = mThumbnailBar->palette().color(QPalette::Normal, QPalette::Window);
		QColor bgSelColor = mThumbnailBar->palette().color(QPalette::Normal, QPalette::Highlight);

		// Avoid dark and bright colors
		bgColor.setHsv(bgColor.hue(), bgColor.saturation(), (127 + 3 * bgColor.value()) / 4);

		QColor leftBorderColor = PaintUtils::adjustedHsv(bgColor, 0, 0, qMin(20, 255 - bgColor.value()));
		QColor rightBorderColor = PaintUtils::adjustedHsv(bgColor, 0, 0, -qMin(40, bgColor.value()));
		QColor borderSelColor = PaintUtils::adjustedHsv(bgSelColor, 0, 0, -qMin(60, bgSelColor.value()));

		QString viewCss =
			"#thumbnailBarView {"
			"	background-color: rgba(0, 0, 0, 10%);"
			"}";

		QString itemCss =
			"QListView::item {"
			"	background-color: %1;"
			"	border-left: 1px solid %2;"
			"	border-right: 1px solid %3;"
			"}";
		itemCss = itemCss.arg(
			gradient(bgColor, 46),
			gradient(leftBorderColor, 36),
			gradient(rightBorderColor, 26));

		QString itemSelCss =
			"QListView::item:selected {"
			"	background-color: %1;"
			"	border-left: 1px solid %2;"
			"	border-right: 1px solid %2;"
			"}";
		itemSelCss = itemSelCss.arg(
			gradient(bgSelColor, 56),
			rgba(borderSelColor));

		mThumbnailBar->setStyleSheet(viewCss + itemCss + itemSelCss);
	}

	void setupPartContainer() {
		mPartContainer = new QWidget;
		mPartContainerLayout = new QVBoxLayout(mPartContainer);
		mPartContainerLayout->addWidget(mStatusBarContainer);
		mPartContainerLayout->setMargin(0);
		mPartContainerLayout->setSpacing(0);
	}

	void setupSplitter() {
		mThumbnailSplitter = new Splitter(Qt::Vertical, mView);
		mThumbnailSplitter->addWidget(mPartContainer);
		mThumbnailSplitter->addWidget(mThumbnailBar);
		mThumbnailSplitter->setSizes(GwenviewConfig::thumbnailSplitterSizes());
	}

	void setPartWidget(QWidget* partWidget) {
		if (partWidget) {
			// Insert the widget above the status bar
			mPartContainerLayout->insertWidget(0 /* position */, partWidget, 1 /* stretch */);
			mView->setCurrentWidget(mThumbnailSplitter);
		} else {
			mView->setCurrentWidget(mNoDocumentLabel);
		}
	}

	void applyPalette() {
		QPalette palette = mFullScreenMode ? mFullScreenPalette : mNormalPalette;
		mView->setPalette(palette);

		if (!mPart) {
			return;
		}

		QPalette partPalette = mPart->widget()->palette();
		partPalette.setBrush(mPart->widget()->backgroundRole(), palette.base());
		partPalette.setBrush(mPart->widget()->foregroundRole(), palette.text());
		mPart->widget()->setPalette(partPalette);
	}

	void saveSplitterConfig() {
		if (mThumbnailBar->isVisible()) {
			GwenviewConfig::setThumbnailSplitterSizes(mThumbnailSplitter->sizes());
		}
	}
};


DocumentPanel::DocumentPanel(QWidget* parent, KActionCollection* actionCollection)
: QStackedWidget(parent)
, d(new DocumentPanelPrivate)
{
	d->mView = this;
	d->mPart = 0;
	d->mFullScreenMode = false;
	d->mThumbnailBarVisibleBeforeFullScreen = false;
	d->mFullScreenPalette = QPalette(palette());
	d->mFullScreenPalette.setColor(QPalette::Base, Qt::black);
	d->mFullScreenPalette.setColor(QPalette::Text, Qt::white);

	QShortcut* enterFullScreenShortcut = new QShortcut(d->mView);
	enterFullScreenShortcut->setKey(Qt::Key_Return);
	connect(enterFullScreenShortcut, SIGNAL(activated()), SIGNAL(enterFullScreenRequested()) );

	d->setupNoDocumentLabel();

	d->setupStatusBar();

	d->setupPartContainer();

	d->setupThumbnailBar();

	d->setupSplitter();

	addWidget(d->mNoDocumentLabel);
	addWidget(d->mThumbnailSplitter);

	d->mToggleThumbnailBarAction = actionCollection->add<KToggleAction>("toggle_thumbnailbar");
	d->mToggleThumbnailBarAction->setText(i18n("Thumbnail Bar"));
	d->mToggleThumbnailBarAction->setIcon(KIcon("folder-image"));
	d->mToggleThumbnailBarAction->setShortcut(Qt::CTRL | Qt::Key_B);
	d->mToggleThumbnailBarAction->setChecked(GwenviewConfig::thumbnailBarIsVisible());
	connect(d->mToggleThumbnailBarAction, SIGNAL(triggered(bool)),
		this, SLOT(setThumbnailBarVisibility(bool)) );
	d->mToggleThumbnailBarButton->setDefaultAction(d->mToggleThumbnailBarAction);
}


DocumentPanel::~DocumentPanel() {
	delete d;
}


void DocumentPanel::saveConfig() {
	d->saveSplitterConfig();
	GwenviewConfig::setThumbnailBarIsVisible(d->mToggleThumbnailBarAction->isChecked());
}


void DocumentPanel::setThumbnailBarVisibility(bool visible) {
	d->saveSplitterConfig();
	d->mThumbnailBar->setVisible(visible);
}


KStatusBar* DocumentPanel::statusBar() const {
	return d->mStatusBar;
}


void DocumentPanel::setStatusBarHeight(int height) {
	d->mStatusBarContainer->setFixedHeight(height);
}


void DocumentPanel::setFullScreenMode(bool fullScreenMode) {
	d->mFullScreenMode = fullScreenMode;
	d->mStatusBarContainer->setVisible(!fullScreenMode);
	d->applyPalette();
	if (fullScreenMode) {
		d->mThumbnailBarVisibleBeforeFullScreen = d->mToggleThumbnailBarAction->isChecked();
		if (d->mThumbnailBarVisibleBeforeFullScreen) {
			d->mToggleThumbnailBarAction->trigger();
		}
	} else if (d->mThumbnailBarVisibleBeforeFullScreen) {
		d->mToggleThumbnailBarAction->trigger();
	}
	d->mToggleThumbnailBarAction->setEnabled(!fullScreenMode);
}


ThumbnailBarView* DocumentPanel::thumbnailBar() const {
	return d->mThumbnailBar;
}


QSize DocumentPanel::sizeHint() const {
	return QSize(400, 300);
}


KUrl DocumentPanel::url() const {
	if (!d->mPart) {
		return KUrl();
	}

	return d->mPart->url();
}


void DocumentPanel::reset() {
	if (!d->mPart) {
		return;
	}
	d->setPartWidget(0);
	partChanged(0);
	delete d->mPart;
	d->mPartLibrary.clear();
	d->mPart=0;
}


void DocumentPanel::createPartForUrl(const KUrl& url) {
	QString mimeType = MimeTypeUtils::urlMimeType(url);
	LOG("mimeType:" << mimeType);
	if (!url.isLocalFile() && mimeType == "text/html") {
		// Try harder, some webservers do not really know the mimetype of the
		// content they serve (KDE Bugzilla for example)
		mimeType = MimeTypeUtils::urlMimeTypeByContent(url);
		LOG("mimeType after downloading content:" << mimeType);
	}

	QString library;
	QVariantList partArgs;
	if (MimeTypeUtils::rasterImageMimeTypes().contains(mimeType)) {
		LOG("Enforcing use of Gwenview part");
		library = "gvpart";
		partArgs << QVariant("gwenviewHost");
	} else {
		LOG("Query system for available parts for" << mimeType);
		// Get a list of possible parts
		const KService::List offers = KMimeTypeTrader::self()->query( mimeType, QLatin1String("KParts/ReadOnlyPart"));
		if (offers.isEmpty()) {
			kWarning() << "Couldn't find a KPart for " << mimeType ;
			reset();
			return;
		}

		// Check if we are already using it
		KService::Ptr service = offers.first();
		library=service->library();
	}
	Q_ASSERT(!library.isNull());
	if (library == d->mPartLibrary) {
		LOG("Reusing current part");
		return;
	}

	// Load new part
	LOG("Loading part from library" << library);
	KPluginFactory* factory = KPluginLoader(library).factory();
	if (!factory) {
		kWarning() << "Failed to load library" << library;
		return;
	}
	LOG("Loading part from library" << library);
	KParts::ReadOnlyPart* part = factory->create<KParts::ReadOnlyPart>(d->mPartContainer, partArgs);
	if (!part) {
		kWarning() << "Failed to instantiate KPart from library" << library ;
		return;
	}

	ImageViewPart* ivPart = dynamic_cast<ImageViewPart*>(part);
	if (ivPart) {
		connect(ivPart, SIGNAL(resizeRequested(const QSize&)),
			d->mView, SIGNAL(resizeRequested(const QSize&)) );
		connect(ivPart, SIGNAL(previousImageRequested()),
			d->mView, SIGNAL(previousImageRequested()) );
		connect(ivPart, SIGNAL(nextImageRequested()),
			d->mView, SIGNAL(nextImageRequested()) );
	}

	// Handle statusbar extension otherwise a statusbar will get created in
	// the main window.
	KParts::StatusBarExtension* extension = KParts::StatusBarExtension::childObject(part);
	if (extension) {
		extension->setStatusBar(statusBar());
	}

	d->setPartWidget(part->widget());
	partChanged(part);

	connect(part, SIGNAL(completed()), SIGNAL(completed()) );

	// Delete the old part, don't do it before mMainWindow->createGUI(),
	// otherwise some UI elements from the old part won't be properly
	// removed. 
	delete d->mPart;
	d->mPart = part;

	d->applyPalette();

	d->mPartLibrary = library;
}


bool DocumentPanel::openUrl(const KUrl& url) {
	createPartForUrl(url);
	if (!d->mPart) {
		return false;
	}
	d->mPart->openUrl(url);
	return true;
}


bool DocumentPanel::currentDocumentIsRasterImage() const {
	// If the document view is visible, we assume we have a raster
	// image if and only if we are using the ImageViewPart. This avoids
	// having to determine the mimetype a second time.
	return dynamic_cast<ImageViewPart*>(d->mPart) != 0;
}


bool DocumentPanel::isEmpty() const {
	return !d->mPart;
}


ImageViewPart* DocumentPanel::imageViewPart() const {
	return dynamic_cast<ImageViewPart*>(d->mPart);
}


void DocumentPanel::setNormalPalette(const QPalette& palette) {
	d->mNormalPalette = palette;
	d->applyPalette();
}


} // namespace