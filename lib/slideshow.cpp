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
#include "slideshow.moc"

// STL
#include <algorithm>

// Qt
#include <QAction>
#include <QTimer>

// KDE
#include <kconfig.h>
#include <kdebug.h>
#include <kicon.h>
#include <klocale.h>

// Local
#include <gwenviewconfig.h>

namespace Gwenview {

#undef ENABLE_LOG
#undef LOG
//#define ENABLE_LOG
#ifdef ENABLE_LOG
#define LOG(x) kDebug() << x
#else
#define LOG(x) ;
#endif

struct SlideShowPrivate {
	QTimer* mTimer;
	bool mStarted;
	QVector<KUrl> mUrls;
	QVector<KUrl> mShuffledUrls;
	QVector<KUrl>::ConstIterator mStartIt;
	KUrl mCurrentUrl;

	QAction* mLoopAction;
	QAction* mRandomAction;

	KUrl findNextUrl() {
		if (GwenviewConfig::random()) {
			return findNextRandomUrl();
		} else {
			return findNextOrderedUrl();
		}
	}


	KUrl findNextOrderedUrl() {
		QVector<KUrl>::ConstIterator it = qFind(mUrls.constBegin(), mUrls.constEnd(), mCurrentUrl);
		if (it == mUrls.constEnd()) {
			kWarning() << "Current url not found in list. This should not happen.\n";
			return KUrl();
		}

		++it;
		if (GwenviewConfig::loop()) {
			// Looping, if we reach the end, start again
			if (it == mUrls.constEnd()) {
				it = mUrls.constBegin();
			}
		} else {
			// Not looping, have we reached the end?
			// FIXME: stopAtEnd
			if (/*(it==mUrls.end() && GwenviewConfig::stopAtEnd()) ||*/ it == mStartIt) {
				it = mUrls.constEnd();
			}
		}

		if (it != mUrls.constEnd()) {
			return *it;
		} else {
			return KUrl();
		}
	}


	void initShuffledUrls() {
		mShuffledUrls = mUrls;
		std::random_shuffle(mShuffledUrls.begin(), mShuffledUrls.end());
	}


	KUrl findNextRandomUrl() {
		if (mShuffledUrls.empty()) {
			if (GwenviewConfig::loop()) {
				initShuffledUrls();
			} else {
				return KUrl();
			}
		}

		KUrl url = mShuffledUrls.last();
		mShuffledUrls.pop_back();

		return url;
	}


	void updateTimerInterval() {
		mTimer->setInterval(int(GwenviewConfig::interval() * 1000));
	}
};




SlideShow::SlideShow(QObject* parent)
: QObject(parent)
, d(new SlideShowPrivate) {
	d->mStarted = false;

	d->mTimer = new QTimer(this);
	connect(d->mTimer, SIGNAL(timeout()),
			this, SLOT(slotTimeout()) );

	d->mLoopAction = new QAction(this);
	d->mLoopAction->setText(i18nc("@item:inmenu toggle loop in slideshow", "Loop"));
	d->mLoopAction->setCheckable(true);
	connect(d->mLoopAction, SIGNAL(triggered()), SLOT(updateConfig()) );

	d->mRandomAction = new QAction(this);
	d->mRandomAction->setText(i18nc("@item:inmenu toggle random order in slideshow", "Random"));
	d->mRandomAction->setCheckable(true);
	connect(d->mRandomAction, SIGNAL(toggled(bool)), SLOT(slotRandomActionToggled(bool)) );
	connect(d->mRandomAction, SIGNAL(triggered()), SLOT(updateConfig()) );

	d->mLoopAction->setChecked(GwenviewConfig::loop());
	d->mRandomAction->setChecked(GwenviewConfig::random());
}


SlideShow::~SlideShow() {
	GwenviewConfig::self()->writeConfig();
	delete d;
}


QAction* SlideShow::loopAction() const {
	return d->mLoopAction;
}


QAction* SlideShow::randomAction() const {
	return d->mRandomAction;
}


void SlideShow::start(const QList<KUrl>& urls) {
	d->mUrls.resize(urls.size());
	qCopy(urls.begin(), urls.end(), d->mUrls.begin());

	d->mStartIt=qFind(d->mUrls.constBegin(), d->mUrls.constEnd(), d->mCurrentUrl);
	if (d->mStartIt==d->mUrls.constEnd()) {
		kWarning() << "Current url not found in list, aborting.\n";
		return;
	}

	if (GwenviewConfig::random()) {
		d->initShuffledUrls();
	}
	
	d->updateTimerInterval();
	d->mTimer->setSingleShot(false);
	d->mTimer->start();
	d->mStarted=true;
	stateChanged(true);
}


void SlideShow::setInterval(int intervalInSeconds) {
	GwenviewConfig::setInterval(double(intervalInSeconds));
	d->updateTimerInterval();
}


void SlideShow::stop() {
	d->mTimer->stop();
	d->mStarted=false;
	stateChanged(false);
}


void SlideShow::slotTimeout() {
	LOG("");
	KUrl url = d->findNextUrl();
	LOG("url:" << url);
	if (!url.isValid()) {
		stop();
		return;
	}
	goToUrl(url);
}


void SlideShow::setCurrentUrl(const KUrl& url) {
	d->mCurrentUrl = url;
}


bool SlideShow::isRunning() const {
	return d->mStarted;
}


void SlideShow::updateConfig() {
	GwenviewConfig::setLoop(d->mLoopAction->isChecked());
	GwenviewConfig::setRandom(d->mRandomAction->isChecked());
}


void SlideShow::slotRandomActionToggled(bool on) {
	if (on && d->mStarted) {
		d->initShuffledUrls();
	}
}


} // namespace
