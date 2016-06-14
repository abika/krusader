/***************************************************************************
                                panelfunc.h
                             -------------------
    begin                : Thu May 4 2000
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

                                                     H e a d e r    F i l e

 ***************************************************************************
 *                                                                         *
 *   This program is free software; you can redistribute it and/or modify  *
 *   it under the terms of the GNU General Public License as published by  *
 *   the Free Software Foundation; either version 2 of the License, or     *
 *   (at your option) any later version.                                   *
 *                                                                         *
 ***************************************************************************/


#ifndef PANELFUNC_H
#define PANELFUNC_H

#include "krviewitem.h"
#include "../VFS/vfs.h"

#include <QtCore/QObject>
#include <QtCore/QTimer>
#include <QtGui/QClipboard>

#include <KService/KService>

class DirHistoryQueue;

class ListPanelFunc : public QObject
{
    friend class ListPanel;
    Q_OBJECT
public slots:
    void execute(const QString&);
    void goInside(const QString&);
    void urlEntered(const QUrl &url);
    void openUrl(const QUrl &path, const QString& nameToMakeCurrent = QString(),
                 bool manuallyEntered = false);
//     void popErronousUrl();
    void immediateOpenUrl(const QUrl &url, bool disableLock = false);
    void rename(const QString &oldname, const QString &newname);
    void calcSpace(KrViewItem *item);

    // actions
    void historyBackward();
    void historyForward();
    void dirUp();
    void refresh();
    void home();
    void root();
    void cdToOtherPanel();
    void FTPDisconnect();
    void newFTPconnection();
    void terminal();
    void view();
    void viewDlg();
    void edit();
    void editNew(); // create a new textfile and edit it
    void copyFiles(bool enqueue = false);
    void moveFiles(bool enqueue = false);
    void copyFilesByQueue() {
        copyFiles(true);
    }
    void moveFilesByQueue() {
        moveFiles(true);
    }
    /*!
     * asks the user the new directory name
     */
    void mkdir();
    void deleteFiles(bool reallyDelete = false);
    void rename();
    void krlink(bool sym = true);
    void createChecksum();
    void matchChecksum();
    void pack();
    void unpack();
    void testArchive();
    void calcSpace(); // calculate the occupied space and show it in a dialog
    void properties();
    void cut() {
        copyToClipboard(true);
    }
    void copyToClipboard(bool move = false);
    void pasteFromClipboard();
    void syncOtherPanel();

public:
    ListPanelFunc(ListPanel *parent);
    ~ListPanelFunc();

    vfs* files();  // return a pointer to the vfs

    inline vfile* getVFile(KrViewItem *item) {
        return files()->vfs_search(item->name());
    }
    inline vfile* getVFile(const QString& name) {
        return files()->vfs_search(name);
    }

    void refreshActions();
    void redirectLink();
    void runService(const KService &service, QList<QUrl> urls);
    void displayOpenWithDialog(QList<QUrl> urls);
    QUrl browsableArchivePath(const QString &);

    // calculate the occupied space. A dialog appears, if calculation lasts more than 3 seconds
    // and disappears, if the calculation is done. Returns true, if the result is ok and false
    // otherwise (Cancel was pressed).
    bool calcSpace(const QStringList & items, KIO::filesize_t & totalSize, unsigned long & totalFiles, unsigned long & totalDirs);
    ListPanelFunc* otherFunc();
    bool atHome();

protected slots:
    void doRefresh();
    void slotFileCreated(KJob *job); // a file has been created by editNewFile()
    void historyGotoPos(int pos);
    void clipboardChanged(QClipboard::Mode mode);

protected:
    QUrl cleanPath(const QUrl &url);
    bool isSyncing(const QUrl &url);
    void openFileNameInternal(const QString &name, bool theFileCanBeExecutedOrOpenedWithOtherSoftware);
    void openUrlInternal(const QUrl &url, const QString& makeCurrent,
                         bool immediately, bool disableLock, bool manuallyEntered);
    void runCommand(QString cmd);

    ListPanel*           panel;     // our ListPanel
    DirHistoryQueue*     history;
    vfs*                 vfsP;      // pointer to vfs.
    QTimer               delayTimer;
    QUrl                 syncURL;
    QUrl                 fileToCreate; // file that's to be created by editNewFile()
    bool                 urlManuallyEntered;

    static QPointer<ListPanelFunc> copyToClipboardOrigin;

private:
    QUrl getVirtualBaseURL();
};

#endif
