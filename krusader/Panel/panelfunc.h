/*****************************************************************************
 * Copyright (C) 2000 Shie Erlich <krusader@users.sourceforge.net>           *
 * Copyright (C) 2000 Rafi Yanai <krusader@users.sourceforge.net>            *
 * Copyright (C) 2004-2018 Krusader Krew [https://krusader.org]              *
 *                                                                           *
 * This file is part of Krusader [https://krusader.org].                     *
 *                                                                           *
 * Krusader is free software: you can redistribute it and/or modify          *
 * it under the terms of the GNU General Public License as published by      *
 * the Free Software Foundation, either version 2 of the License, or         *
 * (at your option) any later version.                                       *
 *                                                                           *
 * Krusader is distributed in the hope that it will be useful,               *
 * but WITHOUT ANY WARRANTY; without even the implied warranty of            *
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the             *
 * GNU General Public License for more details.                              *
 *                                                                           *
 * You should have received a copy of the GNU General Public License         *
 * along with Krusader.  If not, see [http://www.gnu.org/licenses/].         *
 *****************************************************************************/


#ifndef PANELFUNC_H
#define PANELFUNC_H

// QtCore
#include <QObject>
#include <QTimer>
#include <QUrl>
// QtGui
#include <QClipboard>

#include <KCoreAddons/KJob>
#include <KIO/Global>
#include <KService/KService>

class DirHistoryQueue;
class FileItem;
class FileSystem;
class KrViewItem;
class ListPanel;
class SizeCalculator;

class ListPanelFunc : public QObject
{
    friend class ListPanel;
    Q_OBJECT
public slots:
    void execute(const QString&);
    void goInside(const QString&);
    void openUrl(const QUrl &path, const QString& nameToMakeCurrent = QString(),
                 bool manuallyEntered = false);
    void rename(const QString &oldname, const QString &newname);

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
    void moveFilesDelayed() { moveFiles(true); }
    void copyFilesDelayed() { copyFiles(true); }
    void moveFiles(bool enqueue = false) { copyFiles(enqueue, true); }
    void copyFiles(bool enqueue = false, bool move = false);

    /*!
     * asks the user the new directory name
     */
    void mkdir();
    /** Delete or move selected files to trash - depending on user setting. */
    void defaultDeleteFiles() { defaultOrAlternativeDeleteFiles(false); }
    /** Delete or move selected files to trash - inverting the user setting. */
    void alternativeDeleteFiles() { defaultOrAlternativeDeleteFiles(true); }
    /** Delete virtual files or directories in virtual filesystem. */
    void removeVirtualFiles();
    void rename();
    void krlink(bool sym = true);
    void createChecksum();
    void matchChecksum();
    void pack();
    void unpack();
    void testArchive();
    /** Calculate the occupied space of the currently selected items and show a dialog. */
    void calcSpace();
    /** Calculate the occupied space of the current item without dialog. */
    void quickCalcSpace();
    void properties();
    void cut() {
        copyToClipboard(true);
    }
    void copyToClipboard(bool move = false);
    void pasteFromClipboard();
    void syncOtherPanel();
    /** Disable refresh if panel is not visible. */
    void setPaused(bool paused);

public:
    explicit ListPanelFunc(ListPanel *parent);
    ~ListPanelFunc();

    FileSystem* files();  // return a pointer to the filesystem
    QUrl virtualDirectory(); // return the current URL (simulated when panel is paused)

    FileItem* getFileItem(KrViewItem *item);
    FileItem* getFileItem(const QString& name);

    void refreshActions();
    void redirectLink();
    void runService(const KService &service, QList<QUrl> urls);
    void displayOpenWithDialog(QList<QUrl> urls);
    QUrl browsableArchivePath(const QString &);
    void deleteFiles(bool moveToTrash);

    ListPanelFunc* otherFunc();
    bool atHome();

    /** Ask user for confirmation before deleting files. Returns only confirmed files.*/
    static QList<QUrl> confirmDeletion(const QList<QUrl> &urls, bool moveToTrash,
                                       bool virtualFS, bool showPath);

protected slots:
    // Load the current url from history and refresh filesystem and panel to it
    void doRefresh();
    void slotFileCreated(KJob *job); // a file has been created by editNewFile()
    void historyGotoPos(int pos);
    void clipboardChanged(QClipboard::Mode mode);
    // Update the directory size in view
    void slotSizeCalculated(const QUrl &url, KIO::filesize_t size);

protected:
    QUrl cleanPath(const QUrl &url);
    bool isSyncing(const QUrl &url);
    // when externallyExecutable == true, the file can be executed or opened with other software
    void openFileNameInternal(const QString &name, bool externallyExecutable);
    void immediateOpenUrl(const QUrl &url);
    void openUrlInternal(const QUrl &url, const QString& makeCurrent,
                         bool immediately, bool manuallyEntered);
    void runCommand(QString cmd);

    ListPanel*               panel;     // our ListPanel
    DirHistoryQueue*         history;
    FileSystem*              fileSystemP;      // pointer to fileSystem.
    QTimer                   delayTimer;
    QUrl                     syncURL;
    QUrl                     fileToCreate; // file that's to be created by editNewFile()
    bool                     urlManuallyEntered;

    static QPointer<ListPanelFunc> copyToClipboardOrigin;

private:
    void defaultOrAlternativeDeleteFiles(bool invert);
    bool getSelectedFiles(QStringList& args);
    SizeCalculator *createAndConnectSizeCalculator(const QList<QUrl> &urls);
    bool _isPaused; // do not refresh while panel is not visible
    bool _refreshAfterPaused; // refresh after not paused anymore
    QPointer<SizeCalculator> _quickSizeCalculator;
};

#endif
