// vim: set tabstop=4 shiftwidth=4 noexpandtab:
// kate: indent-mode csands; indent-width 4; replace-tabs-save off; replace-tabs off; replace-trailing-space-save off; space-indent off; tabs-indents on; tab-width 4;
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
#ifndef MAINWINDOW_H
#define MAINWINDOW_H

// STL
#include <memory>

// Qt
#include <qptrlist.h>

// KDE
#include <kmainwindow.h>
#include <kurl.h>

// Local
#include "imageutils/orientation.h"

#include "config.h"
#ifdef GV_HAVE_KIPI
#include <libkipi/pluginloader.h>
#endif

class QLabel;
class QTimer;
class QWidgetStack;

class KAction;
class KDockArea;
class KDockWidget;
class KFileItem;
class KHistoryCombo;
class KListView;
class KToggleAction;
class KToolBar;
class KToolBarPopupAction;
class KURLCompletion;

namespace Gwenview {
class CaptionFormatter;
class DirView;
class Document;
class FileViewStack;
class History;
class MetaEdit;
class ImageView;
class SlideShow;


class MainWindow : public KMainWindow {
Q_OBJECT
public:
	MainWindow();

	FileViewStack* fileViewStack() const { return mFileViewStack; }
	ImageView* imageView() const { return mImageView; }
	SlideShow* slideShow() const { return mSlideShow; }
	bool showBusyPtrInFullScreen() const { return mShowBusyPtrInFullScreen; }
	bool showAutoDeleteThumbnailCache() const { return mAutoDeleteThumbnailCache; }
	Document* document() const { return mDocument; }
#ifdef GV_HAVE_KIPI
	KIPI::PluginLoader* pluginLoader() const { return mPluginLoader; }
#endif

	void setShowMenuBarInFullScreen(bool);
	void setShowToolBarInFullScreen(bool);
	void setShowStatusBarInFullScreen(bool);
	void setShowBusyPtrInFullScreen(bool);
	void setAutoDeleteThumbnailCache(bool);

protected:
	bool queryClose();
	virtual void saveProperties( KConfig* );
	virtual void readProperties( KConfig* );

private:
	QWidgetStack* mCentralStack;
	QWidget* mViewModeWidget;
	KDockArea* mDockArea;
	KDockWidget* mFolderDock;
	KDockWidget* mBookmarkDock;
	KDockWidget* mFileDock;
	KDockWidget* mPixmapDock;
	KDockWidget* mMetaDock;
	QLabel* mSBDetailLabel;
	QLabel* mSBHintLabel;
	QTimer* mHintTimer;

	FileViewStack* mFileViewStack;
	DirView* mDirView;
	KListView* mBookmarkView;
	ImageView* mImageView;
	MetaEdit *mMetaEdit;

	Document* mDocument;
	History* mHistory;
	SlideShow* mSlideShow;

	KAction* mRenameFile;
	KAction* mCopyFiles;
	KAction* mMoveFiles;
	KAction* mLinkFiles;
	KAction* mDeleteFiles;
	KAction* mShowConfigDialog;
	KAction* mShowKeyDialog;
	KToggleAction* mToggleFullScreen;
	KAction* mReload;
	KToolBarPopupAction* mGoUp;
	KAction* mShowFileProperties;
	KAction* mToggleSlideShow;
	KAction* mRotateLeft;
	KAction* mRotateRight;
	KAction* mMirror;
	KAction* mFlip;
	KAction* mSaveFile;
	KAction* mSaveFileAs;
	KAction* mFilePrint;
	KAction* mResetDockWidgets;
	bool	 mLoadingCursor;
	bool	 mAutoDeleteThumbnailCache;
	KToggleAction* mToggleBrowse;

	KHistoryCombo* mURLEdit;
	KURLCompletion* mURLEditCompletion;
	QPtrList<KAction> mWindowListActions;

	bool mShowBusyPtrInFullScreen;

#ifdef GV_HAVE_KIPI
	KIPI::PluginLoader* mPluginLoader;
#endif

	KToolBar* mFileViewToolBar;
	KToolBar* mBookmarkToolBar;
	std::auto_ptr<CaptionFormatter> mCaptionFormatter;

	void hideToolBars();
	void showToolBars();
	void createWidgets();
	void createActions();
	void createLocationToolBar();
	void createObjectInteractions();
	void updateLocationURL();
	void createConnections();

	void readConfig(KConfig*,const QString&);
	void writeConfig(KConfig*,const QString&) const;

private slots:
	void openURL(const KURL&);
	void goUp();
	void goUpTo(int);

	void goHome();
	void renameFile();
	void copyFiles();
	void moveFiles();
	void linkFiles();
	void deleteFiles();
	void showFileProperties();
	void showFileDialog();
	void printFile();  /** print the actual file */
	void clearLocationLabel();
	void activateLocationLabel();

	void toggleFullScreen();
	void showConfigDialog();
	void showExternalToolDialog();
	void showKeyDialog();
	void showToolBarDialog();
	void applyMainWindowSettings();
	void slotImageLoading();
	void slotImageLoaded();
	void toggleSlideShow();
	void slotSlideShowChanged(bool);
	void slotDirRenamed(const KURL& oldURL, const KURL& newURL);
	void slotDirURLChanged(const KURL&);
	void modifyImage(ImageUtils::Orientation);
	void rotateLeft();
	void rotateRight();
	void mirror();
	void flip();
	void resetDockWidgets();

	void slotToggleCentralStack();
	/**
	 * Update status bar and caption
	 */
	void updateStatusInfo();

	/**
	 * Enable or disable image actions
	 */
	void updateImageActions();

	void slotShownFileItemRefreshed(const KFileItem* item);

	/**
	 * Allow quitting full screen mode by pressing Escape key.
	 */
	void escapePressed();

	/**
	 * Address bar related
	 */
	void slotGo();

	void updateWindowActions();

	void loadPlugins();

	// Helper function for updateWindowActions()
	void createHideShowAction(KDockWidget* dock);

	void slotReplug();

	void showHint(const QString&);
};


} // namespace
#endif

