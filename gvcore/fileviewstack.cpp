// vim: set tabstop=4 shiftwidth=4 noexpandtab:
/*
Gwenview - A simple image viewer for KDE
Copyright 2000-2004 Aur�ien G�eau

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
Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA  02111-1307, USA.

*/

// Qt
#include <qpopupmenu.h>

// KDE
#include <kaction.h>
#include <kapplication.h>
#include <kdebug.h>
#include <kdirlister.h>
#include <kicontheme.h>
#include <kiconloader.h>
#include <kimageio.h>
#include <kinputdialog.h>
#include <klocale.h>
#include <kpropertiesdialog.h>
#include <kprotocolinfo.h>
#include <kstdaction.h>
#include <kurldrag.h>
#include <klistview.h>
#include <kio/job.h>
#include <kio/file.h>

// Local
#include "archive.h"
#include "cache.h"
#include "cursortracker.h"
#include "externaltoolcontext.h"
#include "externaltoolmanager.h"
#include "filedetailview.h"
#include "fileoperation.h"
#include "filethumbnailview.h"
#include "imageloader.h"
#include "thumbnailsize.h"

#include "fileviewstack.moc"
namespace Gwenview {

//#define ENABLE_LOG
#ifdef ENABLE_LOG
#define LOG(x) kdDebug() << k_funcinfo << x << endl
#else
#define LOG(x) ;
#endif

static const char* CONFIG_START_WITH_THUMBNAILS="start with thumbnails";
static const char* CONFIG_SHOW_DIRS="show dirs";
static const char* CONFIG_SHOW_DOT_FILES="show dot files";
static const char* CONFIG_SHOWN_COLOR="shown color";

static const int SLIDER_RESOLUTION=4;


//-----------------------------------------------------------------------
//
// FileViewStackPrivate
//
//-----------------------------------------------------------------------
class FileViewStackPrivate {
public:
	~FileViewStackPrivate() {
		delete mSliderTracker;
	}
	KSelectAction* mSortAction;
	KToggleAction* mRevertSortAction;
	KAction* mThumbnailDetailsDialogAction;
	TipTracker* mSliderTracker;
};


//-----------------------------------------------------------------------
//
// FileViewStack
//
//-----------------------------------------------------------------------
FileViewStack::FileViewStack(QWidget* parent,KActionCollection* actionCollection)
: QWidgetStack(parent)
, mMode(FILE_LIST)
, mPrefetch( NULL )
, mChangeDirStatus(CHANGE_DIR_STATUS_NONE)
, mBrowsing(false)
, mSelecting(false)
{
	d=new FileViewStackPrivate;

	// Actions
	mSelectFirst=new KAction(i18n("&First"),
		QApplication::reverseLayout() ? "2rightarrow":"2leftarrow", Key_Home,
		this,SLOT(slotSelectFirst()), actionCollection, "first");

	mSelectLast=new KAction(i18n("&Last"),
		QApplication::reverseLayout() ? "2leftarrow":"2rightarrow", Key_End,
		this,SLOT(slotSelectLast()), actionCollection, "last");

	mSelectPrevious=new KAction(i18n("&Previous"),
		QApplication::reverseLayout() ? "1rightarrow":"1leftarrow", Key_BackSpace,
		this,SLOT(slotSelectPrevious()), actionCollection, "previous");

	mSelectNext=new KAction(i18n("&Next"),
		QApplication::reverseLayout() ? "1leftarrow":"1rightarrow", Key_Space,
		this,SLOT(slotSelectNext()), actionCollection, "next");

	mSelectPreviousDir=new KAction(i18n("&Previous Folder"),
		QApplication::reverseLayout() ? "player_fwd":"player_rew", ALT + Key_BackSpace,
		this,SLOT(slotSelectPreviousDir()), actionCollection, "previous_folder");

	mSelectNextDir=new KAction(i18n("&Next Folder"),
		QApplication::reverseLayout() ? "player_rew":"player_fwd", ALT + Key_Space,
		this,SLOT(slotSelectNextDir()), actionCollection, "next_folder");

	mSelectFirstSubDir=new KAction(i18n("&First Sub Folder"), "down", ALT + Key_Down,
		this,SLOT(slotSelectFirstSubDir()), actionCollection, "first_sub_folder");

	mListMode=new KRadioAction(i18n("Details"),"view_detailed",0,this,SLOT(updateViewMode()),actionCollection,"list_mode");
	mListMode->setExclusiveGroup("thumbnails");
	mSideThumbnailMode=new KRadioAction(i18n("Thumbnails with Info on Side"),"view_multicolumn",0,this,SLOT(updateViewMode()),actionCollection,"side_thumbnail_mode");
	mSideThumbnailMode->setExclusiveGroup("thumbnails");
	mBottomThumbnailMode=new KRadioAction(i18n("Thumbnails with Info on Bottom"),"view_icon",0,this,SLOT(updateViewMode()),actionCollection,"bottom_thumbnail_mode");
	mBottomThumbnailMode->setExclusiveGroup("thumbnails");
	
	// Size slider
	mSizeSlider=new QSlider(Horizontal, this);
	mSizeSlider->setMaximumWidth(150);
	mSizeSlider->setRange(
		ThumbnailSize::MIN/SLIDER_RESOLUTION,
		ThumbnailSize::LARGE/SLIDER_RESOLUTION);
	
	connect(mSizeSlider, SIGNAL(valueChanged(int)), this, SLOT(updateThumbnailSize(int)) );
	connect(mListMode, SIGNAL(toggled(bool)), mSizeSlider, SLOT(setDisabled(bool)) );
	new KWidgetAction(mSizeSlider, i18n("Thumbnail Size"), 0, 0, 0, actionCollection, "thumbnails_slider");
	d->mSliderTracker=new TipTracker("", mSizeSlider);
	// /Size slider
	
	mShowDotFiles=new KToggleAction(i18n("Show &Hidden Files"),CTRL + Key_H,this,SLOT(toggleShowDotFiles()),actionCollection,"show_dot_files");

	d->mSortAction=new KSelectAction(i18n("Sort"), 0, this, SLOT(setSorting()), actionCollection, "view_sort");
	QStringList sortItems;
	sortItems << i18n("By Name") << i18n("By Date") << i18n("By Size");
	d->mSortAction->setItems(sortItems);
	d->mSortAction->setCurrentItem(0);

	d->mRevertSortAction=new KToggleAction(i18n("Descending"),0, this, SLOT(setSorting()), actionCollection, "descending");
	QPopupMenu* sortMenu=d->mSortAction->popupMenu();
	Q_ASSERT(sortMenu);
	sortMenu->insertSeparator();
	d->mRevertSortAction->plug(sortMenu);

	// Dir lister
	mDirLister=new DirLister;
	mDirLister->setMainWindow(topLevelWidget());
	connect(mDirLister,SIGNAL(clear()),
		this,SLOT(dirListerClear()) );

	connect(mDirLister,SIGNAL(newItems(const KFileItemList&)),
		this,SLOT(dirListerNewItems(const KFileItemList&)) );

	connect(mDirLister,SIGNAL(deleteItem(KFileItem*)),
		this,SLOT(dirListerDeleteItem(KFileItem*)) );

	connect(mDirLister,SIGNAL(refreshItems(const KFileItemList&)),
		this,SLOT(dirListerRefreshItems(const KFileItemList&)) );

	connect(mDirLister,SIGNAL(started(const KURL&)),
		this,SLOT(dirListerStarted()) );

	connect(mDirLister,SIGNAL(completed()),
		this,SLOT(dirListerCompleted()) );

	connect(mDirLister,SIGNAL(canceled()),
		this,SLOT(dirListerCanceled()) );

	// Propagate canceled signals
	connect(mDirLister,SIGNAL(canceled()),
		this,SIGNAL(canceled()) );

	// File detail widget
	mFileDetailView=new FileDetailView(this,"filedetailview");
	addWidget(mFileDetailView,0);
	mFileDetailView->viewport()->installEventFilter(this);

	connect(mFileDetailView,SIGNAL(executed(QListViewItem*)),
		this,SLOT(slotViewExecuted()) );
	connect(mFileDetailView,SIGNAL(returnPressed(QListViewItem*)),
		this,SLOT(slotViewExecuted()) );
	connect(mFileDetailView,SIGNAL(currentChanged(QListViewItem*)),
		this,SLOT(slotViewClicked()) );
	connect(mFileDetailView,SIGNAL(selectionChanged()),
		this,SLOT(slotViewClicked()) );
	connect(mFileDetailView,SIGNAL(clicked(QListViewItem*)),
		this,SLOT(slotViewClicked()) );
	connect(mFileDetailView,SIGNAL(contextMenu(KListView*, QListViewItem*, const QPoint&)),
		this,SLOT(openContextMenu(KListView*, QListViewItem*, const QPoint&)) );
	connect(mFileDetailView,SIGNAL(dropped(QDropEvent*,KFileItem*)),
		this,SLOT(openDropURLMenu(QDropEvent*, KFileItem*)) );
	connect(mFileDetailView, SIGNAL(sortingChanged(QDir::SortSpec)),
		this, SLOT(updateSortMenu(QDir::SortSpec)) );
	connect(mFileDetailView, SIGNAL(doubleClicked(QListViewItem*)),
		this, SLOT(slotViewDoubleClicked()) );
	connect(mFileDetailView, SIGNAL(selectionChanged()),
		this, SIGNAL(selectionChanged()) );

	// Thumbnail widget
	mFileThumbnailView=new FileThumbnailView(this);
	addWidget(mFileThumbnailView,1);
	mFileThumbnailView->viewport()->installEventFilter(this);

	connect(mFileThumbnailView,SIGNAL(executed(QIconViewItem*)),
		this,SLOT(slotViewExecuted()) );
	connect(mFileThumbnailView,SIGNAL(returnPressed(QIconViewItem*)),
		this,SLOT(slotViewExecuted()) );
	connect(mFileThumbnailView,SIGNAL(currentChanged(QIconViewItem*)),
		this,SLOT(slotViewClicked()) );
	connect(mFileThumbnailView,SIGNAL(selectionChanged()),
		this,SLOT(slotViewClicked()) );
	connect(mFileThumbnailView,SIGNAL(clicked(QIconViewItem*)),
		this,SLOT(slotViewClicked()) );
	connect(mFileThumbnailView,SIGNAL(contextMenuRequested(QIconViewItem*,const QPoint&)),
		this,SLOT(openContextMenu(QIconViewItem*,const QPoint&)) );
	connect(mFileThumbnailView,SIGNAL(dropped(QDropEvent*,KFileItem*)),
		this,SLOT(openDropURLMenu(QDropEvent*, KFileItem*)) );
	connect(mFileThumbnailView, SIGNAL(doubleClicked(QIconViewItem*)),
		this, SLOT(slotViewDoubleClicked()) );
	connect(mFileThumbnailView, SIGNAL(selectionChanged()),
		this, SIGNAL(selectionChanged()) );
	
	// Thumbnail details dialog
	d->mThumbnailDetailsDialogAction=new KAction(i18n("Edit Thumbnail Details..."), "configure", 0, mFileThumbnailView, SLOT(showThumbnailDetailsDialog()), actionCollection, "thumbnail_details_dialog"); 
	connect(mBottomThumbnailMode, SIGNAL(toggled(bool)),
		d->mThumbnailDetailsDialogAction, SLOT(setEnabled(bool)) );
	
}


FileViewStack::~FileViewStack() {
	delete d;
	delete mDirLister;
}


void FileViewStack::setFocus() {
	currentFileView()->widget()->setFocus();
}


/**
 * Do not let double click events propagate if Ctrl or Shift is down, to avoid
 * toggling fullscreen
 */
bool FileViewStack::eventFilter(QObject*, QEvent* event) {
	if (event->type()!=QEvent::MouseButtonDblClick) return false;

	QMouseEvent* mouseEvent=static_cast<QMouseEvent*>(event);
	if (mouseEvent->state() & Qt::ControlButton || mouseEvent->state() & Qt::ShiftButton) {
		return true;
	}
	return false;
}


//-----------------------------------------------------------------------
//
// Public slots
//
//-----------------------------------------------------------------------
void FileViewStack::setDirURL(const KURL& url) {
	LOG(url.prettyURL());
	if ( mDirURL.equals(url,true) ) {
		LOG("Same URL");
		return;
	}
	prefetchDone();
	mDirURL=url;
	if (!KProtocolInfo::supportsListing(mDirURL)) {
		LOG("Protocol does not support listing");
		return;
	}
	
	mDirLister->clearError();
	currentFileView()->setShownFileItem(0L);
	mFileNameToSelect=QString::null;
	mDirLister->openURL(mDirURL);
	emit urlChanged(mDirURL);
	emit directoryChanged(mDirURL);
	updateActions();
}


/**
 * Sets the file to select once the dir lister is done. If it's not running,
 * immediatly selects the file.
 */
void FileViewStack::setFileNameToSelect(const QString& fileName) {
	mFileNameToSelect=fileName;
	if (mDirLister->isFinished()) {
		browseToFileNameToSelect();
	}
}

void FileViewStack::prefetch( KFileItem* item ) {
	prefetchDone();
	if( item == NULL ) return;
	mPrefetch = ImageLoader::loader( item->url(), this, BUSY_PRELOADING );
	connect( mPrefetch, SIGNAL( imageLoaded( bool )), SLOT( prefetchDone()));
}

void FileViewStack::prefetchDone() {
	if( mPrefetch != NULL ) {
		mPrefetch->release( this );
		mPrefetch = NULL;
	}
}

void FileViewStack::slotSelectFirst() {
	browseTo( findFirstImage());
	prefetch( findNextImage());
}

void FileViewStack::slotSelectLast() {
	browseTo(findLastImage());
	prefetch( findPreviousImage());
}

void FileViewStack::slotSelectPrevious() {
	browseTo(findPreviousImage());
	prefetch( findPreviousImage());
}

void FileViewStack::slotSelectNext() {
	browseTo(findNextImage());
	prefetch( findNextImage());
}

void FileViewStack::slotSelectPreviousDir() {
	mChangeDirStatus = CHANGE_DIR_STATUS_PREV;
	mDirLister->clearError();
	mDirLister->openURL(mDirURL.upURL());
}

void FileViewStack::slotSelectNextDir() {
	mChangeDirStatus = CHANGE_DIR_STATUS_NEXT;
	mDirLister->clearError();
	mDirLister->openURL(mDirURL.upURL());
}

void FileViewStack::slotSelectFirstSubDir() {
	KFileItem* item=currentFileView()->firstFileItem();
	while (item && !Archive::fileItemIsDirOrArchive(item)) {
		item=currentFileView()->nextItem(item);
	}
	if (!item) {
		LOG("No item found");
		return;
	}
	LOG("item->url(): " << item->url().prettyURL());
	KURL tmp=item->url();
	if (Archive::fileItemIsArchive(item)) {
		tmp.setProtocol(Archive::protocolForMimeType(item->mimetype()));
	}
	tmp.adjustPath(1);
	setDirURL(tmp);
}


void FileViewStack::browseTo(KFileItem* item) {
	prefetchDone();
	if (mBrowsing) return;
	mBrowsing = true;
	if (item) {
		currentFileView()->setCurrentItem(item);
		currentFileView()->clearSelection();
		currentFileView()->setSelected(item,true);
		currentFileView()->ensureItemVisible(item);
		if (!item->isDir() && !Archive::fileItemIsArchive(item)) {
			emitURLChanged();
		}
	}
	updateActions();
	mBrowsing = false;
}


void FileViewStack::browseToFileNameToSelect() {
	// There's something to select
	if (!mFileNameToSelect.isEmpty()) {
		browseTo(findItemByFileName(mFileNameToSelect));
		mFileNameToSelect=QString::null;
		return;
	}

	// Nothing to select, but an item is already shown
	if (currentFileView()->shownFileItem()) return;

	// Now we have to make some default choice
	slotSelectFirst();

	// If no item is selected, make sure the first one is
	if (currentFileView()->selectedItems()->count()==0) {
		KFileItem* item=currentFileView()->firstFileItem();
		if (item) {
			currentFileView()->setCurrentItem(item);
			currentFileView()->setSelected(item, true);
			currentFileView()->ensureItemVisible(item);
		}
	}
}


void FileViewStack::updateThumbnail(const KURL& url) {
	if (mMode==FILE_LIST) return;

	KFileItem* item=mDirLister->findByURL(url);
	if (!item) return;
	mFileThumbnailView->updateThumbnail(item);
}


//-----------------------------------------------------------------------
//
// Private slots
//
//-----------------------------------------------------------------------
void FileViewStack::slotViewExecuted() {
	KFileItem* item=currentFileView()->currentFileItem();
	if (!item) return;

	bool isDir=item->isDir();
	bool isArchive=Archive::fileItemIsArchive(item);
	if (isDir || isArchive) {
		KURL tmp=url();

		if (isArchive) {
			tmp.setProtocol(Archive::protocolForMimeType(item->mimetype()));
		}
		tmp.adjustPath(1);
		setDirURL(tmp);
	} else {
		emitURLChanged();
	}
}


void FileViewStack::slotViewClicked() {
	updateActions();
	KFileItem* item=currentFileView()->currentFileItem();
	if (!item || Archive::fileItemIsDirOrArchive(item)) return;

	mSelecting = true;
	emitURLChanged();
	mSelecting = false;
}


void FileViewStack::slotViewDoubleClicked() {
	updateActions();
	KFileItem* item=currentFileView()->currentFileItem();
	if (item && !Archive::fileItemIsDirOrArchive(item)) emit imageDoubleClicked();
}


void FileViewStack::updateViewMode() {
	if (mListMode->isChecked()) {
		setMode(FILE_LIST);
		return;
	}
	if (mSideThumbnailMode->isChecked()) {
		mFileThumbnailView->setItemTextPos(QIconView::Right);
	} else {
		mFileThumbnailView->setItemTextPos(QIconView::Bottom);
	}
	
	// Only switch the view if we are going from no thumbs to either side or
	// bottom thumbs, not when switching between side and bottom thumbs
	if (mMode==FILE_LIST) {
		setMode(THUMBNAIL);
	} else {
		KFileItemList items=*mFileThumbnailView->items();
		KFileItem* shownFileItem=mFileThumbnailView->shownFileItem();

		mFileThumbnailView->FileViewBase::clear();
		mFileThumbnailView->addItemList(items);
		mFileThumbnailView->setShownFileItem(shownFileItem);
	}
	
	mFileThumbnailView->startThumbnailUpdate();
}


void FileViewStack::updateThumbnailSize(int size) {
	size*=SLIDER_RESOLUTION;
	d->mSliderTracker->setText(i18n("Thumbnail size: %1x%2").arg(size).arg(size));
	mFileThumbnailView->setThumbnailSize(size);
	Cache::instance()->checkThumbnailSize(size);
}


void FileViewStack::toggleShowDotFiles() {
	mDirLister->setShowingDotFiles(mShowDotFiles->isChecked());
	mDirLister->openURL(mDirURL);
}


void FileViewStack::updateSortMenu(QDir::SortSpec _spec) {
	int	spec=_spec & (QDir::Name | QDir::Time | QDir::Size);
	int item;
	switch (spec) {
	case QDir::Name:
		item=0;
		break;
	case QDir::Time:
		item=1;
		break;
	case QDir::Size:
		item=2;
		break;
	default:
		item=-1;
		break;
	}
	d->mSortAction->setCurrentItem(item);
}


void FileViewStack::setSorting() {
	QDir::SortSpec spec;

	switch (d->mSortAction->currentItem()) {
	case 0: // Name
		spec=QDir::Name;
		break;
	case 1: // Date
		spec=QDir::Time;
		break;
	case 2: // Size
		spec=QDir::Size;
		break;
	default:
		return;
	}
	if (d->mRevertSortAction->isChecked()) {
		spec=QDir::SortSpec(spec | QDir::Reversed);
	}
	currentFileView()->setSorting(QDir::SortSpec(spec | QDir::DirsFirst));
	emit sortingChanged();
}


//-----------------------------------------------------------------------
//
// Context menu
//
//-----------------------------------------------------------------------
void FileViewStack::openContextMenu(const QPoint& pos, bool onItem) {
	int selectionSize;
	ExternalToolContext* externalToolContext;
	
	if (onItem) {
		selectionSize=currentFileView()->selectedItems()->count();
		externalToolContext=
			ExternalToolManager::instance()->createContext(
			this, currentFileView()->selectedItems());
	} else {
		selectionSize=0;
		externalToolContext=
			ExternalToolManager::instance()->createContext(
			this, mDirURL);
	}

	QPopupMenu menu(this);


	menu.insertItem(
		i18n("External Tools"), externalToolContext->popupMenu());

	d->mSortAction->plug(&menu);

	menu.connectItem(
		menu.insertItem( i18n("Parent Folder") ),
		this,SLOT(openParentDir()) );

	menu.insertItem(SmallIcon("folder_new"), i18n("New Folder..."), this, SLOT(makeDir()));

	menu.insertSeparator();

	if (selectionSize==1) {
		menu.connectItem(
			menu.insertItem( i18n("&Rename...") ),
			this,SLOT(renameFile()) );
	}

	if (selectionSize>=1) {
		menu.connectItem(
			menu.insertItem( i18n("&Copy To...") ),
			this,SLOT(copyFiles()) );
		menu.connectItem(
			menu.insertItem( i18n("&Move To...") ),
			this,SLOT(moveFiles()) );
		menu.connectItem(
			menu.insertItem( i18n("&Link to...") ),
			this,SLOT(linkFiles()) );
		menu.connectItem(
			menu.insertItem( i18n("&Delete") ),
			this,SLOT(deleteFiles()) );
		menu.insertSeparator();
	}

	menu.connectItem(
		menu.insertItem( i18n("Properties") ),
		this,SLOT(showFileProperties()) );
	menu.exec(pos);
}


void FileViewStack::openContextMenu(KListView*,QListViewItem* item,const QPoint& pos) {
	openContextMenu(pos, item!=0);
}


void FileViewStack::openContextMenu(QIconViewItem* item,const QPoint& pos) {
	openContextMenu(pos, item!=0);
}


//-----------------------------------------------------------------------
//
// Drop URL menu
//
//-----------------------------------------------------------------------
void FileViewStack::openDropURLMenu(QDropEvent* event, KFileItem* item) {
	KURL dest;

	if (item) {
		dest=item->url();
	} else {
		dest=mDirURL;
	}

	KURL::List urls;
	if (!KURLDrag::decode(event,urls)) return;

	FileOperation::openDropURLMenu(this, urls, dest);
}


//-----------------------------------------------------------------------
//
// File operations
//
//-----------------------------------------------------------------------
KURL::List FileViewStack::selectedURLs() const {
	KURL::List list;

	KFileItemListIterator it( *currentFileView()->selectedItems() );
	for ( ; it.current(); ++it ) {
		list.append(it.current()->url());
	}
	if (list.isEmpty()) {
		const KFileItem* item=currentFileView()->shownFileItem();
		if (item) list.append(item->url());
	}
	return list;
}


KURL::List FileViewStack::selectedImageURLs() const {
	KURL::List list;

	KFileItemListIterator it( *currentFileView()->selectedItems() );
	for ( ; it.current(); ++it ) {
		KFileItem* item=it.current();
		if (!Archive::fileItemIsDirOrArchive(item)) {
			list.append(item->url());
		}
	}
	if (list.isEmpty()) {
		const KFileItem* item=currentFileView()->shownFileItem();
		if (item && !Archive::fileItemIsDirOrArchive(item)) list.append(item->url());
	}
	return list;
}


void FileViewStack::openParentDir() {
	KURL url(mDirURL.upURL());
	emit urlChanged(url);
	emit directoryChanged(url);
}


void FileViewStack::copyFiles() {
	KURL::List list=selectedURLs();
	FileOperation::copyTo(list,this);
}

void FileViewStack::linkFiles() {
	KURL::List list=selectedURLs();
	FileOperation::linkTo(list,this);
}

void FileViewStack::moveFiles() {
	KURL::List list=selectedURLs();
	FileOperation::moveTo(list,this);
}


void FileViewStack::deleteFiles() {
	KURL::List list=selectedURLs();
	FileOperation::del(list,this);
}


void FileViewStack::showFileProperties() {
	const KFileItemList* selectedItems=currentFileView()->selectedItems();
	if (selectedItems->count()>0) {
		(void)new KPropertiesDialog(*selectedItems);
	} else {
		(void)new KPropertiesDialog(mDirURL);
	}
}


void FileViewStack::renameFile() {
	const KFileItemList* selectedItems=currentFileView()->selectedItems();
	const KFileItem* item;
	if (selectedItems->count()>0) {
		item=selectedItems->getFirst();
	} else {
		item=currentFileView()->shownFileItem();
	}
	if (item) FileOperation::rename(item->url(),this );
}


//-----------------------------------------------------------------------
//
// Properties
//
//-----------------------------------------------------------------------
QString FileViewStack::fileName() const {
	KFileItem* item=currentFileView()->currentFileItem();
	if (!item) return "";
	return item->text();
}


FileViewBase* FileViewStack::currentFileView() const {
	if (mMode==FILE_LIST) {
		return mFileDetailView;
	} else {
		return mFileThumbnailView;
	}
}


uint FileViewStack::fileCount() const {
	uint count=currentFileView()->count();

	KFileItem* item=currentFileView()->firstFileItem();
	while (item && Archive::fileItemIsDirOrArchive(item)) {
		item=currentFileView()->nextItem(item);
		count--;
	}
	return count;
}


int FileViewStack::shownFilePosition() const {
	KFileItem* shownItem=currentFileView()->shownFileItem();
	if (!shownItem) return -1;
	KFileItem* item=currentFileView()->firstFileItem();
	int position=0;
	for (;
		item && item!=shownItem;
		item=currentFileView()->nextItem(item) )
	{
		if (!Archive::fileItemIsDirOrArchive(item)) ++position;
	}
	return position;
}


KURL FileViewStack::url() const {
	KFileItem* item=currentFileView()->currentFileItem();
	if (!item) return mDirURL;
	return item->url();
}

KURL FileViewStack::dirURL() const {
	return mDirURL;
}


uint FileViewStack::selectionSize() const {
	const KFileItemList* selectedItems=currentFileView()->selectedItems();
	return selectedItems->count();
}


void FileViewStack::setMode(FileViewStack::Mode mode) {
	const KFileItemList* items;
	FileViewBase* oldView;
	FileViewBase* newView;

	mMode=mode;

	if (mMode==FILE_LIST) {
		mFileThumbnailView->stopThumbnailUpdate();
		oldView=mFileThumbnailView;
		newView=mFileDetailView;
	} else {
		oldView=mFileDetailView;
		newView=mFileThumbnailView;
	}

	bool wasFocused=oldView->widget()->hasFocus();
	// Show the new active view
	raiseWidget(newView->widget());
	if (wasFocused) newView->widget()->setFocus();

	// Fill the new view
	newView->clear();
	newView->addItemList(*oldView->items());

	// Set the new view to the same state as the old
	items=oldView->selectedItems();
	for(KFileItemListIterator it(*items);it.current()!=0L;++it) {
		newView->setSelected(it.current(), true);
	}
	newView->setShownFileItem(oldView->shownFileItem());
	newView->setCurrentItem(oldView->currentFileItem());

	// Remove references to the old view from KFileItems
	items=oldView->items();
	for(KFileItemListIterator it(*items);it.current()!=0L;++it) {
		it.current()->removeExtraData(oldView);
	}

	// Update sorting
	newView->setSorting(oldView->sorting());

	// Clear the old view
	oldView->FileViewBase::clear();
}


bool FileViewStack::showDirs() const {
	return mShowDirs;
}


void FileViewStack::setShowDirs(bool value) {
	mShowDirs=value;
	initDirListerFilter();
}


void FileViewStack::setShownColor(const QColor& value) {
	mShownColor=value;
	mFileDetailView->setShownFileItemColor(mShownColor);
	mFileThumbnailView->setShownFileItemColor(mShownColor);
}


void FileViewStack::setSilentMode( bool silent ) {
	mDirLister->setCheck( !silent );
}


void FileViewStack::retryURL() {
	mDirLister->clearError();
	mDirLister->openURL( url());
}

bool DirLister::validURL( const KURL& url ) const {
	if( !url.isValid()) mError = true;
	if( mCheck ) return KDirLister::validURL( url );
	return url.isValid();
}


void DirLister::handleError( KIO::Job* job ) {
	mError = true;
	if( mCheck ) KDirLister::handleError( job );
}


//-----------------------------------------------------------------------
//
// Dir lister slots
//
//-----------------------------------------------------------------------
void FileViewStack::dirListerDeleteItem(KFileItem* item) {
	KFileItem* newShownItem=0L;
	const KFileItem* shownItem=currentFileView()->shownFileItem();
	if (shownItem==item) {
		newShownItem=findNextImage();
		if (!newShownItem) newShownItem=findPreviousImage();
	}

	currentFileView()->removeItem(item);

	if (shownItem==item) {
		currentFileView()->setShownFileItem(newShownItem);
		if (newShownItem) {
			emit urlChanged(newShownItem->url());
		} else {
			emit urlChanged(KURL());
		}
	}
}


void FileViewStack::dirListerNewItems(const KFileItemList& items) {
	LOG("");
	mThumbnailsNeedUpdate=true;
	currentFileView()->addItemList(items);
}


void FileViewStack::dirListerRefreshItems(const KFileItemList& list) {
	LOG("");
	const KFileItem* item=currentFileView()->shownFileItem();
	KFileItemListIterator it(list);
	for (; *it!=0L; ++it) {
		currentFileView()->updateView(*it);
		if (*it==item) {
			emit shownFileItemRefreshed(item);
		}
	}
}


void FileViewStack::refreshItems(const KURL::List& urls) {
	LOG("");
	KFileItemList list;
	for( KURL::List::ConstIterator it = urls.begin();
	     it != urls.end();
	     ++it ) {
		KURL dir = *it;
		dir.setFileName( QString::null );
		if( dir != mDirURL ) continue;
		// TODO this could be quite slow for many images?
		KFileItem* item = findItemByFileName( (*it).filename());
		if( item ) list.append( item );
	}
	dirListerRefreshItems( list );
}


void FileViewStack::dirListerClear() {
	currentFileView()->clear();
}


void FileViewStack::dirListerStarted() {
	LOG("");
	mThumbnailsNeedUpdate=false;
}


void FileViewStack::dirListerCompleted() {
	LOG("");
	// Delay the code to be executed when the dir lister has completed its job
	// to avoid crash in KDirLister (see bug #57991)
	QTimer::singleShot(0,this,SLOT(delayedDirListerCompleted()));
}


void FileViewStack::delayedDirListerCompleted() {
	// The call to sort() is a work around to a bug which causes
	// FileThumbnailView::firstFileItem() to return a wrong item.  This work
	// around is not in firstFileItem() because it's const and sort() is a non
	// const method
	if (mMode!=FILE_LIST) {
		mFileThumbnailView->sort(mFileThumbnailView->sortDirection());
	}

	if (mChangeDirStatus != CHANGE_DIR_STATUS_NONE) {
		KFileItem *item;
		QString fileName = mDirURL.filename();
		for (item=currentFileView()->firstFileItem(); item; item=currentFileView()->nextItem(item) ) {
		if (item->name() == fileName) {
			if (mChangeDirStatus == CHANGE_DIR_STATUS_NEXT) {
				do {
					item=currentFileView()->nextItem(item);
				} while (item && !Archive::fileItemIsDirOrArchive(item));
			} else {
				do {
					item=currentFileView()->prevItem(item);
				} while (item && !Archive::fileItemIsDirOrArchive(item));
			}
			break;
		};
		}
		mChangeDirStatus = CHANGE_DIR_STATUS_NONE;
		if (!item) {
			mDirLister->openURL(mDirURL);
		} else {
			KURL tmp=item->url();
			LOG("item->url(): " << item->url().prettyURL());
			if (Archive::fileItemIsArchive(item)) {
				tmp.setProtocol(Archive::protocolForMimeType(item->mimetype()));
			}
			tmp.adjustPath(1);
			setDirURL(tmp);
		}
	} else {
		browseToFileNameToSelect();
		emit completed();

		if (mMode!=FILE_LIST && mThumbnailsNeedUpdate) {
			mFileThumbnailView->startThumbnailUpdate();
		}
	}
}


void FileViewStack::dirListerCanceled() {
	if (mMode!=FILE_LIST) {
		mFileThumbnailView->stopThumbnailUpdate();
	}

	browseToFileNameToSelect();
}


//-----------------------------------------------------------------------
//
// Private
//
//-----------------------------------------------------------------------
void FileViewStack::initDirListerFilter() {
	QStringList mimeTypes=KImageIO::mimeTypes(KImageIO::Reading);
	mimeTypes.append("image/x-xcf-gimp");
	mimeTypes.append("image/x-xcursor");
	mimeTypes.append("image/pjpeg"); // KImageIO does not return this one :'(
	if (mShowDirs) {
		mimeTypes.append("inode/directory");
		mimeTypes+=Archive::mimeTypes();
	}
	mDirLister->setShowingDotFiles(mShowDotFiles->isChecked());
	mDirLister->setMimeFilter(mimeTypes);
	mDirLister->emitChanges();
}


void FileViewStack::updateActions() {
	KFileItem* firstImage=findFirstImage();

	// There isn't any image, no need to continue
	if (!firstImage) {
		mSelectFirst->setEnabled(false);
		mSelectPrevious->setEnabled(false);
		mSelectNext->setEnabled(false);
		mSelectLast->setEnabled(false);
		return;
	}

	// We did not select any image, let's activate everything
	KFileItem* currentItem=currentFileView()->currentFileItem();
	if (!currentItem || Archive::fileItemIsDirOrArchive(currentItem)) {
		mSelectFirst->setEnabled(true);
		mSelectPrevious->setEnabled(true);
		mSelectNext->setEnabled(true);
		mSelectLast->setEnabled(true);
		return;
	}

	// There is at least one image, and an image is selected, let's be precise
	bool isFirst=currentItem==firstImage;
	bool isLast=currentItem==findLastImage();

	mSelectFirst->setEnabled(!isFirst);
	mSelectPrevious->setEnabled(!isFirst);
	mSelectNext->setEnabled(!isLast);
	mSelectLast->setEnabled(!isLast);
}


void FileViewStack::emitURLChanged() {
	KFileItem* item=currentFileView()->currentFileItem();
	currentFileView()->setShownFileItem(item);

	// We use a tmp value because the signal parameter is a reference
	KURL tmp=url();
	LOG("urlChanged: " << tmp.prettyURL());
	emit urlChanged(tmp);
}

KFileItem* FileViewStack::findFirstImage() const {
	KFileItem* item=currentFileView()->firstFileItem();
	while (item && Archive::fileItemIsDirOrArchive(item)) {
		item=currentFileView()->nextItem(item);
	}
	if (item) {
		LOG("item->url(): " << item->url().prettyURL());
	} else {
		LOG("No item found");
	}
	return item;
}

KFileItem* FileViewStack::findLastImage() const {
	KFileItem* item=currentFileView()->items()->getLast();
	while (item && Archive::fileItemIsDirOrArchive(item)) {
		item=currentFileView()->prevItem(item);
	}
	return item;
}

KFileItem* FileViewStack::findPreviousImage() const {
	KFileItem* item=currentFileView()->shownFileItem();
	if (!item) return 0L;
	do {
		item=currentFileView()->prevItem(item);
	} while (item && Archive::fileItemIsDirOrArchive(item));
	return item;
}

KFileItem* FileViewStack::findNextImage() const {
	KFileItem* item=currentFileView()->shownFileItem();
	if (!item) return 0L;
	do {
		item=currentFileView()->nextItem(item);
	} while (item && Archive::fileItemIsDirOrArchive(item));
	return item;
}

KFileItem* FileViewStack::findItemByFileName(const QString& fileName) const {
	KFileItem *item;
	if (fileName.isEmpty()) return 0L;
	for (item=currentFileView()->firstFileItem();
		item;
		item=currentFileView()->nextItem(item) ) {
		if (item->name()==fileName) return item;
	}

	return 0L;
}


//-----------------------------------------------------------------------
//
// Configuration
//
//-----------------------------------------------------------------------
void FileViewStack::readConfig(KConfig* config,const QString& group) {
	mFileThumbnailView->readConfig(config,group);
	mSizeSlider->setValue(mFileThumbnailView->thumbnailSize() / SLIDER_RESOLUTION);
	// Make sure the tooltip is updated
	updateThumbnailSize(mSizeSlider->value());

	config->setGroup(group);
	mShowDirs=config->readBoolEntry(CONFIG_SHOW_DIRS,true);
	mShowDotFiles->setChecked(config->readBoolEntry(CONFIG_SHOW_DOT_FILES,false));
	initDirListerFilter();

	bool startWithThumbnails=config->readBoolEntry(CONFIG_START_WITH_THUMBNAILS,true);
	setMode(startWithThumbnails?THUMBNAIL:FILE_LIST);
	mSizeSlider->setEnabled(startWithThumbnails);

	if (startWithThumbnails) {
		if (mFileThumbnailView->itemTextPos()==QIconView::Right) {
			mSideThumbnailMode->setChecked(true);
		} else {
			mBottomThumbnailMode->setChecked(true);
		}
		mFileThumbnailView->startThumbnailUpdate();
	} else {
		mListMode->setChecked(true);
	}
	d->mThumbnailDetailsDialogAction->setEnabled(mBottomThumbnailMode->isChecked());

	QColor defaultColor=colorGroup().highlight().light(150);
	setShownColor(config->readColorEntry(CONFIG_SHOWN_COLOR, &defaultColor));
}

void FileViewStack::kpartConfig() {
	mFileThumbnailView->kpartConfig();
	mShowDirs=true;
	mShowDotFiles->setChecked(false);
	initDirListerFilter();

	setMode(THUMBNAIL);

	mFileThumbnailView->startThumbnailUpdate();

	setShownColor(Qt::red);
}

void FileViewStack::writeConfig(KConfig* config,const QString& group) const {
	mFileThumbnailView->writeConfig(config,group);

	config->setGroup(group);

	config->writeEntry(CONFIG_START_WITH_THUMBNAILS,!mListMode->isChecked());
	config->writeEntry(CONFIG_SHOW_DIRS, mShowDirs);
	config->writeEntry(CONFIG_SHOW_DOT_FILES, mShowDotFiles->isChecked());
	config->writeEntry(CONFIG_SHOWN_COLOR,mShownColor);
}

void FileViewStack::makeDir() {
	KIO::Job* job;

	bool ok;
	QString newDir=KInputDialog::getText(
			i18n("Creating Folder"),
			i18n("Enter the name of the new folder:"),
			QString::null, &ok, this);
	if (!ok) return;

	KURL newURL(url().directory());
	newURL.addPath(newDir);
	job=KIO::mkdir(newURL);

	connect( job, SIGNAL(result(KIO::Job*)), this, SLOT(slotDirMade(KIO::Job*)) );
}

void FileViewStack::slotDirMade(KIO::Job* job) {
	if (job->error()) {
		job->showErrorDialog(this);
		return;
	}
}

} // namespace
