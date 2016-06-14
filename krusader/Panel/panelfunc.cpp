/***************************************************************************
                            panelfunc.cpp
                         -------------------
copyright            : (C) 2000 by Shie Erlich & Rafi Yanai
e-mail               : krusader@users.sourceforge.net
web site             : http://krusader.sourceforge.net
---------------------------------------------------------------------------
Description
***************************************************************************

A

 db   dD d8888b. db    db .d8888.  .d8b.  d8888b. d88888b d8888b.
 88 ,8P' 88  `8D 88    88 88'  YP d8' `8b 88  `8D 88'     88  `8D
 88,8P   88oobY' 88    88 `8bo.   88ooo88 88   88 88ooooo 88oobY'
 88`8b   88`8b   88    88   `Y8b. 88~~~88 88   88 88~~~~~ 88`8b
 88 `88. 88 `88. 88b  d88 db   8D 88   88 88  .8D 88.     88 `88.
 YP   YD 88   YD ~Y8888P' `8888Y' YP   YP Y8888D' Y88888P 88   YD

                                                 S o u r c e    F i l e

***************************************************************************
*                                                                         *
*   This program is free software; you can redistribute it and/or modify  *
*   it under the terms of the GNU General Public License as published by  *
*   the Free Software Foundation; either version 2 of the License, or     *
*   (at your option) any later version.                                   *
*                                                                         *
***************************************************************************/

#include "panelfunc.h"

#include <QtCore/QEventLoop>
#include <QtCore/QList>
#include <QtCore/QMimeData>
#include <QtCore/QDir>
#include <QtCore/QTemporaryFile>
#include <QtCore/QUrl>
#include <QtGui/QClipboard>
#include <QtGui/QDrag>
#include <QtWidgets/QApplication>
#include <QtWidgets/QInputDialog>

#include <KArchive/KTar>
#include <KConfigCore/KDesktopFile>
#include <KCoreAddons/KJobTrackerInterface>
#include <KCoreAddons/KProcess>
#include <KCoreAddons/KShell>
#include <KCoreAddons/KUrlMimeData>
#include <KI18n/KLocalizedString>
#include <KIO/DesktopExecParser>
#include <KIO/JobUiDelegate>
#include <KIOCore/KProtocolInfo>
#include <KIOWidgets/KOpenWithDialog>
#include <KIOWidgets/KPropertiesDialog>
#include <KIOWidgets/KRun>
#include <KIOWidgets/KUrlCompletion>
#include <KService/KMimeTypeTrader>
#include <KWidgetsAddons/KCursor>
#include <KWidgetsAddons/KMessageBox>
#include <KWidgetsAddons/KToggleAction>

#include "dirhistoryqueue.h"
#include "krcalcspacedialog.h"
#include "listpanel.h"
#include "krerrordisplay.h"
#include "listpanelactions.h"
#include "quickfilter.h"
#include "../krglobal.h"
#include "../krslots.h"
#include "../kractions.h"
#include "../defaults.h"
#include "../abstractpanelmanager.h"
#include "../krservices.h"
#include "../VFS/vfile.h"
#include "../VFS/vfs.h"
#include "../VFS/virt_vfs.h"
#include "../VFS/krarchandler.h"
#include "../VFS/krpermhandler.h"
#include "../VFS/krvfshandler.h"
#include "../VFS/preservingcopyjob.h"
#include "../VFS/virtualcopyjob.h"
#include "../VFS/packjob.h"
#include "../Dialogs/packgui.h"
#include "../Dialogs/krdialogs.h"
#include "../Dialogs/krpleasewait.h"
#include "../Dialogs/krspwidgets.h"
#include "../Dialogs/checksumdlg.h"
#include "../KViewer/krviewer.h"
#include "../GUI/syncbrowsebutton.h"
#include "../Queue/queue_mgr.h"

QPointer<ListPanelFunc> ListPanelFunc::copyToClipboardOrigin;

ListPanelFunc::ListPanelFunc(ListPanel *parent) : QObject(parent),
        panel(parent), vfsP(0), urlManuallyEntered(false)
{
    history = new DirHistoryQueue(panel);
    delayTimer.setSingleShot(true);
    connect(&delayTimer, SIGNAL(timeout()), this, SLOT(doRefresh()));
}

ListPanelFunc::~ListPanelFunc()
{
    if (vfsP) {
        if (vfsP->vfs_canDelete())
            delete vfsP;
        else {
            connect(vfsP, SIGNAL(deleteAllowed()), vfsP, SLOT(deleteLater()));
            vfsP->vfs_requestDelete();
        }
    }
    delete history;
}

void ListPanelFunc::urlEntered(const QUrl &url)
{
    //TODO
    panel->urlNavigator->setUrlEditable(false);
    openUrl(url, QString(), true);
}

bool ListPanelFunc::isSyncing(const QUrl &url)
{
    if(otherFunc()->otherFunc() == this &&
       panel->otherPanel()->gui->syncBrowseButton->state() == SYNCBROWSE_CD &&
       !otherFunc()->syncURL.isEmpty() &&
       otherFunc()->syncURL == url)
        return true;

    return false;
}

void ListPanelFunc::openFileNameInternal(const QString &name, bool theFileCanBeExecutedOrOpenedWithOtherSoftware)
{
    if (name == "..") {
        dirUp();
        return ;
    }

    vfile *vf = files() ->vfs_search(name);
    if (vf == 0)
        return ;

    QUrl url = files()->vfs_getFile(name);

    if (vf->vfile_isDir()) {
        panel->view->setNameToMakeCurrent(QString());
        openUrl(url);
        return;
    }

    QString mime = vf->vfile_getMime();

    QUrl arcPath = browsableArchivePath(name);
    if (!arcPath.isEmpty()) {
        bool theArchiveMustBeBrowsedAsADirectory = (KConfigGroup(krConfig, "Archives").readEntry("ArchivesAsDirectories", _ArchivesAsDirectories) &&
                                                    KRarcHandler::arcSupported(mime)) || !theFileCanBeExecutedOrOpenedWithOtherSoftware;
        if (theArchiveMustBeBrowsedAsADirectory) {
            openUrl(arcPath);
            return;
        }
    }

    if (theFileCanBeExecutedOrOpenedWithOtherSoftware) {
        if (KRun::isExecutableFile(url, mime)) {
            runCommand(KShell::quoteArg(url.path()));
            return;
        }

        KService::Ptr service = KMimeTypeTrader::self()->preferredService(mime);
        if(service) {
            runService(*service, QList<QUrl>() << url);
            return;
        }

        displayOpenWithDialog(QList<QUrl>() << url);
    }
}

#if 0
//FIXME: see if this is still needed
void ListPanelFunc::popErronousUrl()
{
    QUrl current = urlStack.last();
    while (urlStack.count() != 0) {
        QUrl url = urlStack.takeLast();
        if (!current.equals(url)) {
            immediateOpenUrl(url, true);
            return;
        }
    }
    immediateOpenUrl(QUrl::fromLocalFile(ROOT_DIR), true);
}
#endif
QUrl ListPanelFunc::cleanPath(const QUrl &urlIn)
{
    QUrl url = urlIn;
    url.setPath(QDir::cleanPath(url.path()));
    if (!url.isValid() || url.isRelative()) {
        if (url.url() == "~")
            url = QUrl::fromLocalFile(QDir::homePath());
        else if (!url.url().startsWith('/')) {
            // possible relative URL - translate to full URL
            url = files()->vfs_getOrigin();
            url.setPath(url.path() + '/' + urlIn.path());
        }
    }
    url.setPath(QDir::cleanPath(url.path()));
    return url;
}

void ListPanelFunc::openUrl(const QUrl &url, const QString& nameToMakeCurrent,
                            bool manuallyEntered)
{
    if (panel->syncBrowseButton->state() == SYNCBROWSE_CD) {
        //do sync-browse stuff....
        if(syncURL.isEmpty())
            syncURL = panel->otherPanel()->virtualPath();

        QString relative = QDir(panel->virtualPath().path() + '/').relativeFilePath(url.path());
        syncURL.setPath(QDir::cleanPath(syncURL.path() + '/' + relative));
        panel->otherPanel()->gui->setLocked(false);
        otherFunc()->openUrlInternal(syncURL, nameToMakeCurrent, false, false, false);
    }
    openUrlInternal(url, nameToMakeCurrent, false, false, manuallyEntered);
}

void ListPanelFunc::immediateOpenUrl(const QUrl &url, bool disableLock)
{
    openUrlInternal(url, QString(), true, disableLock, false);
}

void ListPanelFunc::openUrlInternal(const QUrl &url, const QString& nameToMakeCurrent,
                                    bool immediately, bool disableLock, bool manuallyEntered)
{
    QUrl cleanUrl = cleanPath(url);

    if (!disableLock && panel->isLocked() &&
            !files()->vfs_getOrigin().matches(cleanUrl, QUrl::StripTrailingSlash)) {
        panel->_manager->newTab(url);
        urlManuallyEntered = false;
        return;
    }

    urlManuallyEntered = manuallyEntered;

    history->add(cleanUrl, nameToMakeCurrent);

    if(immediately)
        doRefresh();
    else
        refresh();
}

void ListPanelFunc::refresh()
{
    panel->inlineRefreshCancel();
    delayTimer.start(0); // to avoid qApp->processEvents() deadlock situaltion
}

void ListPanelFunc::doRefresh()
{
    delayTimer.stop();

    QUrl url = history->currentUrl();

    if(!url.isValid()) {
        //FIXME go back in history here ?
        panel->slotStartUpdate();  // refresh the panel
        urlManuallyEntered = false;
        return ;
    }

    panel->inlineRefreshCancel();

    // if we are not refreshing to current URL
    bool isEqualUrl = files()->vfs_getOrigin().matches(url, QUrl::StripTrailingSlash);

    if (!isEqualUrl) {
        panel->setCursor(Qt::WaitCursor);
        panel->view->op()->stopQuickFilter(false);
        panel->view->clearSavedSelection();
    }

    if(panel->vfsError)
        panel->vfsError->hide();

    bool refreshFailed = false;
    while (true) {
        QString tmp;

        if (refreshFailed) {
            tmp = history->currentUrl().fileName();
            history->goForward();
            QUrl t = history->currentUrl();
            QString protocol = t.scheme();
            if (protocol != QStringLiteral("file") && KProtocolInfo::protocolClass(protocol).toLower() == QStringLiteral(":local")) {
                t.setScheme("file");
                history->setCurrentUrl(t);
                panel->vfsError->hide();
            } else {
                history->goBack();
            }
        }

        QUrl u = history->currentUrl();

        isEqualUrl = files()->vfs_getOrigin().matches(u, QUrl::StripTrailingSlash);

        vfs* v = KrVfsHandler::getVfs(u, panel, files());
        v->setParentWindow(krMainWindow);
        v->setMountMan(&krMtMan);
        if (v != vfsP) {
            panel->view->setFiles(0);

            // disconnect older signals
            disconnect(vfsP, 0, panel, 0);

            if (vfsP->vfs_canDelete())
                delete vfsP;
            else {
                connect(vfsP, SIGNAL(deleteAllowed()), vfsP, SLOT(deleteLater()));
                vfsP->vfs_requestDelete();
            }
            vfsP = v; // v != 0 so this is safe
        } else {
            if (vfsP->vfs_isBusy()) {
                delayTimer.start(100);    /* if vfs is busy try refreshing later */
                return;
            }
        }
        // (re)connect vfs signals
        disconnect(files(), 0, panel, 0);
        connect(files(), SIGNAL(startUpdate()), panel, SLOT(slotStartUpdate()));
        connect(files(), SIGNAL(incrementalRefreshFinished(const QUrl&)),
                panel, SLOT(slotGetStats(const QUrl&)));
        connect(files(), SIGNAL(startJob(KIO::Job*)),
                panel, SLOT(slotJobStarted(KIO::Job*)));
        connect(files(), SIGNAL(error(QString)),
                panel, SLOT(slotVfsError(QString)));

        panel->view->setFiles(files());

        if(isSyncing(url))
            vfsP->vfs_setQuiet(true);

        if(!history->currentItem().isEmpty() && isEqualUrl) {
            // if the url we're refreshing into is the current one, then the
            // partial refresh will not generate the needed signals to actually allow the
            // view to use nameToMakeCurrent. do it here instead (patch by Thomas Jarosch)
            panel->view->setCurrentItem(history->currentItem());
            panel->view->makeItemVisible(panel->view->getCurrentKrViewItem());
        }
        if (!tmp.isEmpty())
            panel->view->setNameToMakeCurrent(tmp);
        else
            panel->view->setNameToMakeCurrent(history->currentItem());


        int savedHistoryState = history->state();

        if (vfsP->vfs_refresh(u)) {
            // update the history and address bar, as the actual url might differ from the one requested
            history->setCurrentUrl(vfsP->vfs_getOrigin());
            //OK panel->origin->setUrl(vfsP->vfs_getOrigin());
            panel->urlNavigator->setLocationUrl(vfsP->vfs_getOrigin());
            break; // we have a valid refreshed URL now
        }

        refreshFailed = true;

        panel->view->setNameToMakeCurrent(QString());

        if(history->state() != savedHistoryState) // don't go back if the history was touched
            break;
        // prevent repeated error messages
        if (vfsP->vfs_isDeleting())
            break;
        if(!history->goBack()) {
            // put the root dir to the beginning of history, if it's not there yet
            if (!u.matches(QUrl::fromLocalFile(ROOT_DIR), QUrl::StripTrailingSlash))
                history->pushBackRoot();
            else
                break;
        }
        vfsP->vfs_setQuiet(true);
    }
    vfsP->vfs_setQuiet(false);
    panel->view->setNameToMakeCurrent(QString());
    //OK panel->origin->setStartDir(vfsP->vfs_getOrigin());

    panel->setCursor(Qt::ArrowCursor);

    // on local file system change the working directory
    if (files() ->vfs_getType() == vfs::VFS_NORMAL)
        QDir::setCurrent(KrServices::getPath(files()->vfs_getOrigin()));

    // see if the open url operation failed, and if so,
    // put the attempted url in the navigator bar and let the user change it
    if (refreshFailed) {
        if(isSyncing(url))
            panel->otherPanel()->gui->syncBrowseButton->setChecked(false);
        else if(urlManuallyEntered) {
            //OK panel->origin->setUrl(url);
            panel->urlNavigator->setLocationUrl(url);
            if(panel == ACTIVE_PANEL)
                //OK panel->origin->edit();
                panel->editLocation();
        }
    }

    if(otherFunc()->otherFunc() == this)  // not true if our tab is not active
        otherFunc()->syncURL = QUrl();

    urlManuallyEntered = false;

    refreshActions();
}

void ListPanelFunc::redirectLink()
{
    if (files() ->vfs_getType() != vfs::VFS_NORMAL) {
        KMessageBox::sorry(krMainWindow, i18n("You can edit links only on local file systems"));
        return ;
    }

    vfile *vf = files() ->vfs_search(panel->getCurrentName());
    if (!vf)
        return ;

    QString file = files() ->vfs_getFile(vf->vfile_getName()).path();
    QString currentLink = vf->vfile_getSymDest();
    if (currentLink.isEmpty()) {
        KMessageBox::sorry(krMainWindow, i18n("The current file is not a link, so it cannot be redirected."));
        return ;
    }

    // ask the user for a new destination
    bool ok = false;
    QString newLink =
        QInputDialog::getText(krMainWindow, i18n("Link Redirection"), i18n("Please enter the new link destination:"), QLineEdit::Normal, currentLink, &ok);

    // if the user canceled - quit
    if (!ok || newLink == currentLink)
        return ;
    // delete the current link
    if (unlink(file.toLocal8Bit()) == -1) {
        KMessageBox::sorry(krMainWindow, i18n("Cannot remove old link: %1", file));
        return ;
    }
    // try to create a new symlink
    if (symlink(newLink.toLocal8Bit(), file.toLocal8Bit()) == -1) {
        KMessageBox:: /* --=={ Patch by Heiner <h.eichmann@gmx.de> }==-- */sorry(krMainWindow, i18n("Failed to create a new link: %1", file));
        return ;
    }
}

void ListPanelFunc::krlink(bool sym)
{
    if (files() ->vfs_getType() != vfs::VFS_NORMAL) {
        KMessageBox::sorry(krMainWindow, i18n("You can create links only on local file systems"));
        return ;
    }

    QString name = panel->getCurrentName();

    // ask the new link name..
    bool ok = false;
    QString linkName =
        QInputDialog::getText(krMainWindow, i18n("New Link"), i18n("Create a new link to: %1", name), QLineEdit::Normal, name, &ok);

    // if the user canceled - quit
    if (!ok || linkName == name)
        return ;

    // if the name is already taken - quit
    if (files() ->vfs_search(linkName) != 0) {
        KMessageBox::sorry(krMainWindow, i18n("A folder or a file with this name already exists."));
        return ;
    }

    if (linkName.left(1) != "/")
        linkName = files() ->vfs_workingDir() + '/' + linkName;

    if (linkName.contains("/"))
        name = files() ->vfs_getFile(name).path();

    if (sym) {
        if (symlink(name.toLocal8Bit(), linkName.toLocal8Bit()) == -1)
            KMessageBox::sorry(krMainWindow, i18n("Failed to create a new symlink '%1' to: '%2'",
                               linkName, name));
    } else {
        if (link(name.toLocal8Bit(), linkName.toLocal8Bit()) == -1)
            KMessageBox::sorry(krMainWindow, i18n("Failed to create a new link '%1' to '%2'",
                               linkName, name));
    }
}

void ListPanelFunc::view()
{
    QString fileName = panel->getCurrentName();
    if (fileName.isNull())
        return ;

    // if we're trying to view a directory, just exit
    vfile * vf = files() ->vfs_search(fileName);
    if (!vf || vf->vfile_isDir())
        return ;
    if (!vf->vfile_isReadable()) {
        KMessageBox::sorry(0, i18n("No permissions to view this file."));
        return ;
    }
    // call KViewer.
    KrViewer::view(files() ->vfs_getFile(fileName));
    // nothing more to it!
}

void ListPanelFunc::viewDlg()
{
    // ask the user for a url to view
    QUrl dest = KChooseDir::getFile(i18n("Enter a URL to view:"), panel->virtualPath(), panel->virtualPath());
    if (dest.isEmpty())
        return ;   // the user canceled

    KrViewer::view(dest);   // view the file
}

void ListPanelFunc::terminal()
{
    SLOTS->runTerminal(panel->realPath(), QStringList());
}

void ListPanelFunc::edit()
{
    QString name = panel->getCurrentName();
    if (name.isNull())
        return ;

    if (files() ->vfs_search(name) ->vfile_isDir()) {
        KMessageBox::sorry(krMainWindow, i18n("You cannot edit a folder"));
        return ;
    }

    if (!files() ->vfs_search(name) ->vfile_isReadable()) {
        KMessageBox::sorry(0, i18n("No permissions to edit this file."));
        return ;
    }

    KrViewer::edit(files() ->vfs_getFile(name));
}

void ListPanelFunc::editNew()
{
    if(!fileToCreate.isEmpty())
        return;

    // ask the user for the filename to edit
    fileToCreate = KChooseDir::getFile(i18n("Enter the filename to edit:"), panel->virtualPath(), panel->virtualPath());
    if(fileToCreate.isEmpty())
        return ;   // the user canceled

    QTemporaryFile *tempFile = new QTemporaryFile;
    tempFile->open();

    KIO::CopyJob *job = KIO::copy(QUrl::fromLocalFile(tempFile->fileName()), fileToCreate);
    job->setUiDelegate(0);
    job->setDefaultPermissions(true);
    connect(job, SIGNAL(result(KJob*)), SLOT(slotFileCreated(KJob*)));
    connect(job, SIGNAL(result(KJob*)), tempFile, SLOT(deleteLater()));
}

void ListPanelFunc::slotFileCreated(KJob *job)
{
    if(!job->error() || job->error() == KIO::ERR_FILE_ALREADY_EXIST) {
        KrViewer::edit(fileToCreate);

        if(KIO::upUrl(fileToCreate).matches(panel->virtualPath(), QUrl::StripTrailingSlash))
            refresh();
        else if(KIO::upUrl(fileToCreate).matches(panel->otherPanel()->virtualPath(), QUrl::StripTrailingSlash))
            otherFunc()->refresh();
    }
    else
        KMessageBox::sorry(krMainWindow, job->errorString());

    fileToCreate = QUrl();
}

void ListPanelFunc::moveFiles(bool enqueue)
{
    PreserveMode pmode = PM_DEFAULT;
    bool queue = enqueue;

    QStringList fileNames;
    panel->getSelectedNames(&fileNames);
    if (fileNames.isEmpty())
        return ;  // safety

    QUrl dest = panel->otherPanel()->virtualPath();
    QUrl virtualBaseURL;

    KConfigGroup group(krConfig, "Advanced");
    if (group.readEntry("Confirm Move", _ConfirmMove)) {
        bool preserveAttrs = group.readEntry("PreserveAttributes", _PreserveAttributes);
        QString s;

        if (fileNames.count() == 1)
            s = i18n("Move %1 to:", fileNames.first());
        else
            s = i18np("Move %1 file to:", "Move %1 files to:", fileNames.count());

        // ask the user for the copy dest
        virtualBaseURL = getVirtualBaseURL();
        dest = KChooseDir::getDir(s, dest, panel->virtualPath(), queue, preserveAttrs, virtualBaseURL);
        if (dest.isEmpty())
            return ;   // the user canceled
        if (preserveAttrs)
            pmode = PM_PRESERVE_ATTR;
        else
            pmode = PM_NONE;
    }

    if (fileNames.isEmpty())
        return ; // nothing to copy

    QList<QUrl> fileUrls = files() ->vfs_getFiles(fileNames);

    // after the delete return the cursor to the first unmarked
    // file above the current item;
    panel->prepareToDelete();

    if (queue) {
        KIOJobWrapper *job = 0;
        if (!virtualBaseURL.isEmpty()) {
            job = KIOJobWrapper::virtualMove(&fileNames, files(), dest, virtualBaseURL, pmode, true);
            job->connectTo(SIGNAL(result(KJob*)), this, SLOT(refresh()));
            if (dest.matches(panel->otherPanel()->virtualPath(), QUrl::StripTrailingSlash))
                job->connectTo(SIGNAL(result(KJob*)), panel->otherPanel()->func, SLOT(refresh()));
        } else {
            // you can rename only *one* file not a batch,
            // so a batch dest must alwayes be a directory
            if (fileNames.count() > 1) {
                dest = vfs::ensureTrailingSlash(dest);
            }
            job = KIOJobWrapper::move(pmode, fileUrls, dest, true);
            job->setAutoErrorHandlingEnabled(true);
            // refresh our panel when done
            job->connectTo(SIGNAL(result(KJob*)), this, SLOT(refresh()));
            if (dest.matches(panel->virtualPath(), QUrl::StripTrailingSlash) ||
                    KIO::upUrl(dest).matches(panel->virtualPath(), QUrl::StripTrailingSlash))
                // refresh our panel when done
                job->connectTo(SIGNAL(result(KJob*)), this, SLOT(refresh()));
        }
        QueueManager::currentQueue()->enqueue(job);
    } else if (!virtualBaseURL.isEmpty()) {
        // keep the directory structure for virtual paths
        VirtualCopyJob *vjob = new VirtualCopyJob(&fileNames, files(), dest, virtualBaseURL, pmode, KIO::CopyJob::Move, true);
        connect(vjob, SIGNAL(result(KJob*)), this, SLOT(refresh()));
        if (dest.matches(panel->otherPanel()->virtualPath(), QUrl::StripTrailingSlash))
            connect(vjob, SIGNAL(result(KJob*)), panel->otherPanel()->func, SLOT(refresh()));
    }
    // if we are not moving to the other panel :
    else if (!dest.matches(panel->otherPanel()->virtualPath(), QUrl::StripTrailingSlash)) {
        // you can rename only *one* file not a batch,
        // so a batch dest must alwayes be a directory
        if (fileNames.count() > 1) {
            dest = vfs::ensureTrailingSlash(dest);
        }
        KIO::Job* job = PreservingCopyJob::createCopyJob(pmode, fileUrls, dest, KIO::CopyJob::Move, false, true);
        job->ui()->setAutoErrorHandlingEnabled(true);
        // refresh our panel when done
        connect(job, SIGNAL(result(KJob*)), this, SLOT(refresh()));
        // and if needed the other panel as well
        if (dest.matches(panel->otherPanel()->virtualPath(), QUrl::StripTrailingSlash))
            connect(job, SIGNAL(result(KJob*)), panel->otherPanel()->func, SLOT(refresh()));

    } else { // let the other panel do the dirty job
        //check if copy is supported
        if (!otherFunc() ->files() ->vfs_isWritable()) {
            KMessageBox::sorry(krMainWindow, i18n("You cannot move files to this file system"));
            return ;
        }
        // finally..
        otherFunc() ->files() ->vfs_addFiles(fileUrls, KIO::CopyJob::Move, files(), "", pmode);
    }
    if(KConfigGroup(krConfig, "Look&Feel").readEntry("UnselectBeforeOperation", _UnselectBeforeOperation)) {
        panel->view->saveSelection();
        panel->view->unselectAll();
    }
}

// called from SLOTS to begin the renaming process
void ListPanelFunc::rename()
{
    panel->view->renameCurrentItem();
}

// called by signal itemRenamed() from the view to complete the renaming process
void ListPanelFunc::rename(const QString &oldname, const QString &newname)
{
    if (oldname == newname)
        return ; // do nothing
    panel->view->setNameToMakeCurrentIfAdded(newname);
    // as always - the vfs do the job
    files() ->vfs_rename(oldname, newname);
}

void ListPanelFunc::mkdir()
{
    // ask the new dir name..
    // suggested name is the complete name for the directories
    // while filenames are suggested without their extension
    QString suggestedName = panel->getCurrentName();
    if (!suggestedName.isEmpty() && !files()->vfs_search(suggestedName)->vfile_isDir())
        suggestedName = QFileInfo(suggestedName).completeBaseName();

    QString dirName = QInputDialog::getText(krMainWindow, i18n("New folder"), i18n("Folder's name:"), QLineEdit::Normal, suggestedName);

    // if the user canceled - quit
    if (dirName.isEmpty())
        return ;

    QStringList dirTree = dirName.split('/');

    for (QStringList::Iterator it = dirTree.begin(); it != dirTree.end(); ++it) {
        if (*it == ".")
            continue;
        if (*it == "..") {
            immediateOpenUrl(QUrl::fromUserInput(*it, QString(), QUrl::AssumeLocalFile));
            continue;
        }
        // check if the name is already taken
        if (files() ->vfs_search(*it)) {
            // if it is the last dir to be created - quit
            if (*it == dirTree.last()) {
                KMessageBox::sorry(krMainWindow, i18n("A folder or a file with this name already exists."));
                return ;
            }
            // else go into this dir
            else {
                immediateOpenUrl(QUrl::fromUserInput(*it, QString(), QUrl::AssumeLocalFile));
                continue;
            }
        }

        panel->view->setNameToMakeCurrent(*it);
        // as always - the vfs do the job
        files() ->vfs_mkdir(*it);
        if (dirTree.count() > 1)
            immediateOpenUrl(QUrl::fromUserInput(*it, QString(), QUrl::AssumeLocalFile));
    } // for
}

QUrl ListPanelFunc::getVirtualBaseURL()
{
    if (files()->vfs_getType() != vfs::VFS_VIRT || otherFunc()->files()->vfs_getType() == vfs::VFS_VIRT)
        return QUrl();

    QStringList fileNames;
    panel->getSelectedNames(&fileNames);

    QList<QUrl> fileUrls = files() ->vfs_getFiles(fileNames);
    if (fileUrls.count() == 0)
        return QUrl();

    QUrl base = KIO::upUrl(fileUrls.first());

    if (base.scheme() == QStringLiteral("virt"))  // is it a virtual subfolder?
        return QUrl();          // --> cannot keep the directory structure

    for (int i = 1; i < fileUrls.count(); i++) {
        if (base.isParentOf(fileUrls.at(i)))
            continue;
        if (base.scheme() != fileUrls.at(i).scheme())
            return QUrl();

        do {
            QUrl oldBase = base;
            base = KIO::upUrl(base);
            if (oldBase.matches(base, QUrl::StripTrailingSlash))
                return QUrl();
            if (base.isParentOf(fileUrls.at(i)))
                break;
        } while (true);
    }
    return base;
}

void ListPanelFunc::copyFiles(bool enqueue)
{
    PreserveMode pmode = PM_DEFAULT;
    bool queue = enqueue;

    QStringList fileNames;
    panel->getSelectedNames(&fileNames);
    if (fileNames.isEmpty())
        return ;  // safety

    QUrl dest = panel->otherPanel()->virtualPath();
    QUrl virtualBaseURL;

    // confirm copy
    KConfigGroup group(krConfig, "Advanced");
    if (group.readEntry("Confirm Copy", _ConfirmCopy)) {
        bool preserveAttrs = group.readEntry("PreserveAttributes", _PreserveAttributes);
        QString s;

        if (fileNames.count() == 1)
            s = i18n("Copy %1 to:", fileNames.first());
        else
            s = i18np("Copy %1 file to:", "Copy %1 files to:", fileNames.count());

        // ask the user for the copy dest
        virtualBaseURL = getVirtualBaseURL();
        dest = KChooseDir::getDir(s, dest, panel->virtualPath(), queue, preserveAttrs, virtualBaseURL);
        if (dest.isEmpty())
            return ;   // the user canceled
        if (preserveAttrs)
            pmode = PM_PRESERVE_ATTR;
        else
            pmode = PM_NONE;
    }

    QList<QUrl> fileUrls = files() ->vfs_getFiles(fileNames);

    if (queue) {
        KIOJobWrapper *job = 0;
        if (!virtualBaseURL.isEmpty()) {
            job = KIOJobWrapper::virtualCopy(&fileNames, files(), dest, virtualBaseURL, pmode, true);
            job->connectTo(SIGNAL(result(KJob*)), this, SLOT(refresh()));
            if (dest.matches(panel->otherPanel()->virtualPath(), QUrl::StripTrailingSlash))
                job->connectTo(SIGNAL(result(KJob*)), panel->otherPanel()->func, SLOT(refresh()));
        } else {
            // you can rename only *one* file not a batch,
            // so a batch dest must alwayes be a directory
            if (fileNames.count() > 1) {
                dest = vfs::ensureTrailingSlash(dest);
            }
            job = KIOJobWrapper::copy(pmode, fileUrls, dest, true);
            job->setAutoErrorHandlingEnabled(true);
            if (dest.matches(panel->virtualPath(), QUrl::StripTrailingSlash) ||
                    KIO::upUrl(dest).matches(panel->virtualPath(), QUrl::StripTrailingSlash))
                // refresh our panel when done
                job->connectTo(SIGNAL(result(KJob*)), this, SLOT(refresh()));
        }
        QueueManager::currentQueue()->enqueue(job);
    } else if (!virtualBaseURL.isEmpty()) {
        // keep the directory structure for virtual paths
        VirtualCopyJob *vjob = new VirtualCopyJob(&fileNames, files(), dest, virtualBaseURL, pmode, KIO::CopyJob::Copy, true);
        connect(vjob, SIGNAL(result(KJob*)), this, SLOT(refresh()));
        if (dest.matches(panel->otherPanel()->virtualPath(), QUrl::StripTrailingSlash))
            connect(vjob, SIGNAL(result(KJob*)), panel->otherPanel()->func, SLOT(refresh()));
    }
    // if we are  not copying to the other panel :
    else if (!dest.matches(panel->otherPanel()->virtualPath(), QUrl::StripTrailingSlash)) {
        // you can rename only *one* file not a batch,
        // so a batch dest must alwayes be a directory
        if (fileNames.count() > 1) {
            dest = vfs::ensureTrailingSlash(dest);
        }
        KIO::Job* job = PreservingCopyJob::createCopyJob(pmode, fileUrls, dest, KIO::CopyJob::Copy, false, true);
        job->ui()->setAutoErrorHandlingEnabled(true);
        if (dest.matches(panel->virtualPath(), QUrl::StripTrailingSlash) ||
                KIO::upUrl(dest).matches(panel->virtualPath(), QUrl::StripTrailingSlash))
            // refresh our panel when done
            connect(job, SIGNAL(result(KJob*)), this, SLOT(refresh()));
        // let the other panel do the dirty job
    } else {
        //check if copy is supported
        if (!otherFunc() ->files() ->vfs_isWritable()) {
            KMessageBox::sorry(krMainWindow, i18n("You cannot copy files to this file system"));
            return ;
        }
        // finally..
        otherFunc() ->files() ->vfs_addFiles(fileUrls, KIO::CopyJob::Copy, 0, "", pmode);
    }
    if(KConfigGroup(krConfig, "Look&Feel").readEntry("UnselectBeforeOperation", _UnselectBeforeOperation)) {
        panel->view->saveSelection();
        panel->view->unselectAll();
    }
}

void ListPanelFunc::deleteFiles(bool reallyDelete)
{
    // check that the you have write perm
    if (!files() ->vfs_isWritable()) {
        KMessageBox::sorry(krMainWindow, i18n("You do not have write permission to this folder"));
        return ;
    }

    // first get the selected file names list
    QStringList fileNames;
    panel->getSelectedNames(&fileNames);
    if (fileNames.isEmpty())
        return ;

    KConfigGroup gg(krConfig, "General");
    bool trash = gg.readEntry("Move To Trash", _MoveToTrash);
    // now ask the user if he want to delete:
    KConfigGroup group(krConfig, "Advanced");
    if (group.readEntry("Confirm Delete", _ConfirmDelete)) {
        QString s, b;

        if (!reallyDelete && trash && files() ->vfs_getType() == vfs::VFS_NORMAL) {
            s = i18np("Do you really want to move this item to the trash?", "Do you really want to move these %1 items to the trash?", fileNames.count());
            b = i18n("&Trash");
        } else if (files() ->vfs_getType() == vfs::VFS_VIRT && files()->vfs_getOrigin().matches(QUrl("virt:/"), QUrl::StripTrailingSlash)) {
            s = i18np("Do you really want to delete this virtual item (physical files stay untouched)?", "Do you really want to delete these %1 virtual items (physical files stay untouched)?", fileNames.count());
            b = i18n("&Delete");
        } else if (files() ->vfs_getType() == vfs::VFS_VIRT) {
            s = i18np("<qt>Do you really want to delete this item <b>physically</b> (not just removing it from the virtual items)?</qt>", "<qt>Do you really want to delete these %1 items <b>physically</b> (not just removing them from the virtual items)?</qt>", fileNames.count());
            b = i18n("&Delete");
        } else {
            s = i18np("Do you really want to delete this item?", "Do you really want to delete these %1 items?", fileNames.count());
            b = i18n("&Delete");
        }

        // show message
        // note: i'm using continue and not yes/no because the yes/no has cancel as default button
        if (KMessageBox::warningContinueCancelList(krMainWindow, s, fileNames,
                i18n("Warning"), KGuiItem(b)) != KMessageBox::Continue)
            return ;
    }
    //we want to warn the user about non empty dir
    // and files he don't have permission to delete
    bool emptyDirVerify = group.readEntry("Confirm Unempty Dir", _ConfirmUnemptyDir);
    emptyDirVerify = ((emptyDirVerify) && (files() ->vfs_getType() == vfs::VFS_NORMAL));

    QDir dir;
    for (QStringList::Iterator name = fileNames.begin(); name != fileNames.end();) {
        vfile * vf = files() ->vfs_search(*name);

        // verify non-empty dirs delete... (only for normal vfs)
        if (vf && emptyDirVerify && vf->vfile_isDir() && !vf->vfile_isSymLink()) {
            dir.setPath(panel->virtualPath().path() + '/' + (*name));
            if (dir.entryList(QDir::TypeMask | QDir::System | QDir::Hidden).count() > 2) {
                switch (KMessageBox::warningYesNoCancel(krMainWindow,
                                                        i18n("<qt><p>Folder <b>%1</b> is not empty.</p><p>Skip this one or delete all?</p></qt>", *name),
                                                        QString(), KGuiItem(i18n("&Skip")), KGuiItem(i18n("&Delete All")))) {
                case KMessageBox::No :
                    emptyDirVerify = false;
                    break;
                case KMessageBox::Yes :
                    name = fileNames.erase(name);
                    continue;
                default :
                    return ;
                }
            }
        }
        ++name;
    }

    if (fileNames.count() == 0)
        return ;  // nothing to delete

    // after the delete return the cursor to the first unmarked
    // file above the current item;
    panel->prepareToDelete();

    // let the vfs do the job...
    files() ->vfs_delFiles(fileNames, reallyDelete);
}

void ListPanelFunc::goInside(const QString& name)
{
    openFileNameInternal(name, false);
}

void ListPanelFunc::runCommand(QString cmd)
{
    krOut<<cmd<<endl;
    QString workdir = panel->virtualPath().isLocalFile() ?
            panel->virtualPath().path() : QDir::homePath();
    if(!KRun::runCommand(cmd, krMainWindow, workdir))
        KMessageBox::error(0, i18n("Could not start %1", cmd));
}

void ListPanelFunc::runService(const KService &service, QList<QUrl> urls)
{
    krOut<<service.name()<<endl;
    KIO::DesktopExecParser parser(service, urls);
    QStringList args = parser.resultingArguments();
    if (!args.isEmpty())
        runCommand(KShell::joinArgs(args));
    else
        KMessageBox::error(0, i18n("%1 cannot open %2", service.name(), KrServices::toStringList(urls).join(", ")));
}

void ListPanelFunc::displayOpenWithDialog(QList<QUrl> urls)
{
    KOpenWithDialog dialog(urls, panel);
    dialog.hideRunInTerminal();
    if (dialog.exec()) {
        KService::Ptr service = dialog.service();
        if(!service)
            service = KService::Ptr(new KService(dialog.text(), dialog.text(), QString()));
        runService(*service, urls);
    }
}

QUrl ListPanelFunc::browsableArchivePath(const QString &filename)
{
    vfile *vf = files()->vfs_search(filename);
    QUrl url = files()->vfs_getFile(filename);
    QString mime = vf->vfile_getMime();

    if(url.isLocalFile()) {
        QString protocol = KrServices::registeredProtocol(mime);
        if(!protocol.isEmpty()) {
            url.setScheme(protocol);
            return url;
        }
    }
    return QUrl();
}

// this is done when you double click on a file
void ListPanelFunc::execute(const QString& name)
{
    openFileNameInternal(name, true);
}

void ListPanelFunc::pack()
{
    QStringList fileNames;
    panel->getSelectedNames(&fileNames);
    if (fileNames.isEmpty())
        return ;  // safety

    if (fileNames.count() == 0)
        return ; // nothing to pack

    // choose the default name
    QString defaultName = panel->virtualPath().fileName();
    if (defaultName.isEmpty())
        defaultName = "pack";
    if (fileNames.count() == 1)
        defaultName = fileNames.first();
    // ask the user for archive name and packer
    new PackGUI(defaultName, panel->otherPanel()->virtualPath().toDisplayString(QUrl::PreferLocalFile | QUrl::StripTrailingSlash),
                fileNames.count(), fileNames.first());
    if (PackGUI::type.isEmpty()) {
        return ; // the user canceled
    }

    // check for partial URLs
    if (!PackGUI::destination.contains(":/") && !PackGUI::destination.startsWith('/')) {
        PackGUI::destination = panel->virtualPath().toDisplayString() + '/' + PackGUI::destination;
    }

    QString destDir = PackGUI::destination;
    if (!destDir.endsWith('/'))
        destDir += '/';

    bool packToOtherPanel = (destDir == vfs::ensureTrailingSlash(panel->otherPanel()->virtualPath()).toDisplayString(QUrl::PreferLocalFile));

    QUrl destURL = QUrl::fromUserInput(destDir + PackGUI::filename + '.' + PackGUI::type, QString(), QUrl::AssumeLocalFile);
    if (destURL.isLocalFile() && QFile::exists(destURL.path())) {
        QString msg = i18n("<qt><p>The archive <b>%1.%2</b> already exists. Do you want to overwrite it?</p><p>All data in the previous archive will be lost.</p></qt>", PackGUI::filename, PackGUI::type);
        if (PackGUI::type == "zip") {
            msg = i18n("<qt><p>The archive <b>%1.%2</b> already exists. Do you want to overwrite it?</p><p>Zip will replace identically named entries in the zip archive or add entries for new names.</p></qt>", PackGUI::filename, PackGUI::type);
        }
        if (KMessageBox::warningContinueCancel(krMainWindow, msg, QString(), KGuiItem(i18n("&Overwrite")))
                == KMessageBox::Cancel)
            return ; // stop operation
    } else if (destURL.scheme() == QStringLiteral("virt")) {
        KMessageBox::error(krMainWindow, i18n("Cannot pack files onto a virtual destination."));
        return;
    }

    if (PackGUI::queue) {
        KIOJobWrapper *job = KIOJobWrapper::pack(files()->vfs_getOrigin(), destURL, fileNames,
                             PackGUI::type, PackGUI::extraProps, true);
        job->setAutoErrorHandlingEnabled(true);

        if (packToOtherPanel)
            job->connectTo(SIGNAL(result(KJob*)), panel->otherPanel()->func, SLOT(refresh()));

        QueueManager::currentQueue()->enqueue(job);
    } else {
        PackJob * job = PackJob::createPacker(files()->vfs_getOrigin(), destURL, fileNames, PackGUI::type, PackGUI::extraProps);
        job->setUiDelegate(new KIO::JobUiDelegate());
        KIO::getJobTracker()->registerJob(job);
        job->ui()->setAutoErrorHandlingEnabled(true);

        if (packToOtherPanel)
            connect(job, SIGNAL(result(KJob*)), panel->otherPanel()->func, SLOT(refresh()));
    }
}

void ListPanelFunc::testArchive()
{
    QStringList fileNames;
    panel->getSelectedNames(&fileNames);
    if (fileNames.isEmpty())
        return ;  // safety

    TestArchiveJob * job = TestArchiveJob::testArchives(files()->vfs_getOrigin(), fileNames);
    job->setUiDelegate(new KIO::JobUiDelegate());
    KIO::getJobTracker()->registerJob(job);
    job->ui()->setAutoErrorHandlingEnabled(true);
}

void ListPanelFunc::unpack()
{

    QStringList fileNames;
    panel->getSelectedNames(&fileNames);
    if (fileNames.isEmpty())
        return ;  // safety

    QString s;
    if (fileNames.count() == 1)
        s = i18n("Unpack %1 to:", fileNames[0]);
    else
        s = i18np("Unpack %1 file to:", "Unpack %1 files to:", fileNames.count());

    // ask the user for the copy dest
    bool queue = false;
    QUrl dest = KChooseDir::getDir(s, panel->otherPanel()->virtualPath(), panel->virtualPath(), queue);
    if (dest.isEmpty()) return ;   // the user canceled

    bool packToOtherPanel = (dest.matches(panel->otherPanel()->virtualPath(), QUrl::StripTrailingSlash));

    if (queue) {
        KIOJobWrapper *job = KIOJobWrapper::unpack(files()->vfs_getOrigin(), dest, fileNames, true);
        job->setAutoErrorHandlingEnabled(true);

        if (packToOtherPanel)
            job->connectTo(SIGNAL(result(KJob*)), panel->otherPanel()->func, SLOT(refresh()));

        QueueManager::currentQueue()->enqueue(job);
    } else {
        UnpackJob * job = UnpackJob::createUnpacker(files()->vfs_getOrigin(), dest, fileNames);
        job->setUiDelegate(new KIO::JobUiDelegate());
        KIO::getJobTracker()->registerJob(job);
        job->ui()->setAutoErrorHandlingEnabled(true);

        if (packToOtherPanel)
            connect(job, SIGNAL(result(KJob*)), panel->otherPanel()->func, SLOT(refresh()));
    }
}

// a small ugly function, used to prevent duplication of EVERY line of
// code (maybe except 3) from createChecksum and matchChecksum
static void checksum_wrapper(ListPanel *panel, QStringList& args, bool &folders)
{
    KrViewItemList items;
    panel->view->getSelectedKrViewItems(&items);
    if (items.isEmpty()) return ;   // nothing to do
    // determine if we need recursive mode (md5deep)
    folders = false;
    for (KrViewItemList::Iterator it = items.begin(); it != items.end(); ++it) {
        if (panel->func->getVFile(*it)->vfile_isDir()) {
            folders = true;
            args << (*it)->name();
        } else args << (*it)->name();
    }
}

void ListPanelFunc::createChecksum()
{
    QStringList args;
    bool folders;
    checksum_wrapper(panel, args, folders);
    CreateChecksumDlg dlg(args, folders, panel->realPath());
}

void ListPanelFunc::matchChecksum()
{
    QStringList args;
    bool folders;
    checksum_wrapper(panel, args, folders);
    QList<vfile*> checksumFiles = files()->vfs_search(
                                      KRQuery(MatchChecksumDlg::checksumTypesFilter)
                                  );
    MatchChecksumDlg dlg(args, folders, panel->realPath(),
                         (checksumFiles.size() == 1 ? checksumFiles[0]->vfile_getUrl().toDisplayString(QUrl::PreferLocalFile) : QString()));
}

void ListPanelFunc::calcSpace()
{
    QStringList items;
    panel->view->getSelectedItems(&items);
    if (items.isEmpty()) {
        panel->view->selectAllIncludingDirs();
        panel->view->getSelectedItems(&items);
        if (items.isEmpty())
            return ; // nothing to do
    }

    QPointer<KrCalcSpaceDialog> calc = new KrCalcSpaceDialog(krMainWindow, panel, items, false);
    calc->exec();
    panel->slotUpdateTotals();

    delete calc;
}

bool ListPanelFunc::calcSpace(const QStringList & items, KIO::filesize_t & totalSize, unsigned long & totalFiles, unsigned long & totalDirs)
{
    QPointer<KrCalcSpaceDialog> calc = new KrCalcSpaceDialog(krMainWindow, panel, items, true);
    calc->exec();
    calc->getStats(totalSize, totalFiles, totalDirs);
    bool calcWasCanceled = calc->wasCanceled();
    delete calc;

    return !calcWasCanceled;
}

void ListPanelFunc::calcSpace(KrViewItem *item)
{
    //
    // NOTE: this is buggy incase somewhere down in the folder we're calculating,
    // there's a folder we can't enter (permissions). in that case, the returned
    // size will not be correct.
    //
    KIO::filesize_t totalSize = 0;
    unsigned long totalFiles = 0, totalDirs = 0;
    QStringList items;
    items.push_back(item->name());
    if (calcSpace(items, totalSize, totalFiles, totalDirs)) {
        // did we succeed to calcSpace? we'll fail if we don't have permissions
        if (totalSize != 0) {   // just mark it, and bail out
            item->setSize(totalSize);
            item->redraw();
        }
    }
}

void ListPanelFunc::FTPDisconnect()
{
    // you can disconnect only if connected !
    if (files() ->vfs_getType() == vfs::VFS_FTP) {
        panel->_actions->actFTPDisconnect->setEnabled(false);
        panel->view->setNameToMakeCurrent(QString());
        openUrl(QUrl::fromLocalFile(panel->realPath()));   // open the last local URL
    }
}

void ListPanelFunc::newFTPconnection()
{
    QUrl url = KRSpWidgets::newFTP();
    // if the user canceled - quit
    if (url.isEmpty())
        return ;

    panel->_actions->actFTPDisconnect->setEnabled(true);
    openUrl(url);
}

void ListPanelFunc::properties()
{
    QStringList names;
    panel->getSelectedNames(&names);
    if (names.isEmpty())
        return ;  // no names...
    KFileItemList fi;

    for (int i = 0 ; i < names.count() ; ++i) {
        vfile* vf = files() ->vfs_search(names[ i ]);
        if (!vf)
            continue;
        QUrl url = files()->vfs_getFile(names[i]);
        fi.push_back(KFileItem(vf->vfile_getEntry(), url));
    }

    if (fi.isEmpty())
        return ;

    // Show the properties dialog
    KPropertiesDialog *dlg = new KPropertiesDialog(fi, krMainWindow);
    connect(dlg, SIGNAL(applied()), SLOT(refresh()));
    dlg->show();
}

void ListPanelFunc::refreshActions()
{
    panel->updateButtons();

    if(ACTIVE_PANEL != panel)
        return;

    vfs::VFS_TYPE vfsType = files() ->vfs_getType();

    QString protocol = files()->vfs_getOrigin().scheme();
    krRemoteEncoding->setEnabled(protocol == "ftp" || protocol == "sftp" || protocol == "fish" || protocol == "krarc");
    //krMultiRename->setEnabled( vfsType == vfs::VFS_NORMAL );  // batch rename
    //krProperties ->setEnabled( vfsType == vfs::VFS_NORMAL || vfsType == vfs::VFS_FTP ); // file properties

    /*
      krUnpack->setEnabled(true);                            // unpack archive
      krTest->setEnabled(true);                              // test archive
      krSelect->setEnabled(true);                            // select a group by filter
      krSelectAll->setEnabled(true);                         // select all files
      krUnselect->setEnabled(true);                          // unselect by filter
      krUnselectAll->setEnabled( true);                      // remove all selections
      krInvert->setEnabled(true);                            // invert the selection
      krFTPConnect->setEnabled(true);                        // connect to an ftp
      krFTPNew->setEnabled(true);                            // create a new connection
      krAllFiles->setEnabled(true);                          // show all files in list
      krCustomFiles->setEnabled(true);                       // show a custom set of files
      krRoot->setEnabled(true);                              // go all the way up
          krExecFiles->setEnabled(true);                         // show only executables
    */

    panel->_actions->setViewActions[panel->panelType]->setChecked(true);
    panel->_actions->actFTPDisconnect->setEnabled(vfsType == vfs::VFS_FTP);       // disconnect an FTP session
    panel->_actions->actCreateChecksum->setEnabled(vfsType == vfs::VFS_NORMAL);
    panel->_actions->actDirUp->setEnabled(!files()->isRoot());
    panel->_actions->actRoot->setEnabled(!panel->virtualPath().matches(QUrl::fromLocalFile(ROOT_DIR),
                                                                       QUrl::StripTrailingSlash));
    panel->_actions->actHome->setEnabled(!atHome());
    panel->_actions->actHistoryBackward->setEnabled(history->canGoBack());
    panel->_actions->actHistoryForward->setEnabled(history->canGoForward());
    panel->view->op()->emitRefreshActions();
}

vfs* ListPanelFunc::files()
{
    if (!vfsP)
        vfsP = KrVfsHandler::getVfs(QUrl::fromLocalFile("/"), panel, 0);
    return vfsP;
}

void ListPanelFunc::clipboardChanged(QClipboard::Mode mode)
{
    if (mode == QClipboard::Clipboard && this == copyToClipboardOrigin) {
        disconnect(QApplication::clipboard(), 0, this, 0);
        copyToClipboardOrigin = 0;
    }
}

void ListPanelFunc::copyToClipboard(bool move)
{
    QStringList fileNames;

    panel->getSelectedNames(&fileNames);
    if (fileNames.isEmpty())
        return ;  // safety

    QList<QUrl> fileUrls = files() ->vfs_getFiles(fileNames);
    QMimeData *mimeData = new QMimeData;
    mimeData->setData("application/x-kde-cutselection", move ? "1" : "0");
    mimeData->setUrls(fileUrls);

    if (copyToClipboardOrigin)
        disconnect(QApplication::clipboard(), 0, copyToClipboardOrigin, 0);
    copyToClipboardOrigin = this;

    QApplication::clipboard()->setMimeData(mimeData, QClipboard::Clipboard);

    connect(QApplication::clipboard(), SIGNAL(changed(QClipboard::Mode)), this, SLOT(clipboardChanged(QClipboard::Mode)));
}

void ListPanelFunc::pasteFromClipboard()
{
    QClipboard * cb = QApplication::clipboard();

    ListPanelFunc *origin = 0;

    if (copyToClipboardOrigin) {
        disconnect(QApplication::clipboard(), 0, copyToClipboardOrigin, 0);
        origin = copyToClipboardOrigin;
        copyToClipboardOrigin = 0;
    }

    bool move = false;
    const QMimeData *data = cb->mimeData();
    if (data->hasFormat("application/x-kde-cutselection")) {
        QByteArray a = data->data("application/x-kde-cutselection");
        if (!a.isEmpty())
            move = (a.at(0) == '1'); // true if 1
    }

    QList<QUrl> urls = data->urls();
    if (urls.isEmpty())
        return ;

    if(origin && KConfigGroup(krConfig, "Look&Feel").readEntry("UnselectBeforeOperation", _UnselectBeforeOperation)) {
        origin->panel->view->saveSelection();
        for(KrViewItem *item = origin->panel->view->getFirst(); item != 0; item = origin->panel->view->getNext(item)) {
            if (urls.contains(item->getVfile()->vfile_getUrl()))
                item->setSelected(false);
        }
    }

    files()->vfs_addFiles(urls, move ? KIO::CopyJob::Move : KIO::CopyJob::Copy, otherFunc()->files(),
                          "", PM_DEFAULT);
}

ListPanelFunc* ListPanelFunc::otherFunc()
{
    return panel->otherPanel()->func;
}

void ListPanelFunc::historyGotoPos(int pos)
{
    if(history->gotoPos(pos))
        refresh();
}

void ListPanelFunc::historyBackward()
{
    if(history->goBack())
        refresh();
}

void ListPanelFunc::historyForward()
{
    if(history->goForward())
        refresh();
}

void ListPanelFunc::dirUp()
{
    openUrl(KIO::upUrl(files() ->vfs_getOrigin()), files() ->vfs_getOrigin().fileName());
}

void ListPanelFunc::home()
{
    openUrl(QUrl::fromLocalFile(QDir::homePath()));
}

void ListPanelFunc::root()
{
    openUrl(QUrl::fromLocalFile(ROOT_DIR));
}

void ListPanelFunc::cdToOtherPanel()
{
    openUrl(panel->otherPanel()->virtualPath());
}

void ListPanelFunc::syncOtherPanel()
{
    otherFunc()->openUrl(panel->virtualPath());
}

bool ListPanelFunc::atHome()
{
    return QUrl::fromLocalFile(QDir::homePath()).matches(panel->virtualPath(), QUrl::StripTrailingSlash);
}

