/***************************************************************************
                          krpopupmenu.cpp  -  description
                             -------------------
    begin                : Tue Aug 26 2003
    copyright            : (C) 2003 by Shie Erlich & Rafi Yanai
    email                : 
 ***************************************************************************/

/***************************************************************************
 *                                                                         *
 *   This program is free software; you can redistribute it and/or modify  *
 *   it under the terms of the GNU General Public License as published by  *
 *   the Free Software Foundation; either version 2 of the License, or     *
 *   (at your option) any later version.                                   *
 *                                                                         *
 ***************************************************************************/

#include <klocale.h>
#include <k3process.h>
#include <krun.h>
#include <kiconloader.h>
#include <kmessagebox.h>
#include <kmimetypetrader.h>
#include "../krservices.h"
#include "../defaults.h"
#include "../MountMan/kmountman.h"
#include "../krslots.h"
#include "krpopupmenu.h"
#include "krview.h"
#include "krviewitem.h"
#include "panelfunc.h"
#include "../krusaderview.h"
#include "../panelmanager.h"
//Added by qt3to4:
#include <QPixmap>
#include <ktoolinvocation.h>

void KrPopupMenu::run(const QPoint &pos, ListPanel *panel) {
	KrPopupMenu menu(panel);
	QAction * res = menu.exec(pos);
	int result = -1;
	if( res && res->data().canConvert<int>() )
		result = res->data().toInt();
	menu.performAction(result);
}

KrPopupMenu::KrPopupMenu(ListPanel *thePanel, QWidget *parent) : KMenu(parent), panel(thePanel), empty(false), 
	multipleSelections(false),actions(0) {
#ifdef __LIBKONQ__
	konqMenu = 0;
#endif
	
   panel->view->getSelectedKrViewItems( &items );
   if ( items.empty() ) {
   	addCreateNewMenu();
   	addSeparator();
   	addEmptyMenuEntries();
      return;
	} else if ( items.size() > 1 ) multipleSelections = true;

   item = items.first();
   vfile *vf = panel->func->getVFile(item);
   
   // ------------ the OPEN option - open prefered service
   QAction * openAct = addAction( i18n("Open/Run") );
   openAct->setData( QVariant( OPEN_ID ) );
   if ( !multipleSelections ) { // meaningful only if one file is selected
      openAct->setIcon( item->icon() );
      openAct->setText( vf->vfile_isExecutable() && !vf->vfile_isDir() ? i18n("Run") : i18n("Open") );
      // open in a new tab (if folder)
      if ( vf->vfile_isDir() ) {
         QAction * openTab = addAction( i18n( "Open in New Tab" ) );
         openTab->setData( QVariant( OPEN_TAB_ID ) );
         openTab->setIcon( krLoader->loadIcon( "tab_new", K3Icon::Panel ) );
         openTab->setText( i18n( "Open in New Tab" ) );
      }
      addSeparator();
   }

   // ------------- Preview - normal vfs only ?
   if ( panel->func->files()->vfs_getType() == vfs::NORMAL ) {
      // create the preview popup
      QStringList names;
      panel->getSelectedNames( &names );
      preview.setUrls( panel->func->files() ->vfs_getFiles( &names ) );
      QAction *pAct = addMenu( &preview );
      pAct->setData( QVariant( PREVIEW_ID ) );
      pAct->setText( i18n("Preview") );
   }

   // -------------- Open with: try to find-out which apps can open the file
   // this too, is meaningful only if one file is selected or if all the files
   // have the same mimetype !
   QString mime = panel->func->getVFile(item)->vfile_getMime();
   // check if all the list have the same mimetype
   for ( unsigned int i = 1; i < items.size(); ++i ) {
      if ( panel->func->getVFile(( *items.at( i ) )) ->vfile_getMime() != mime ) {
         mime = QString();
         break;
      }
   }
   if ( !mime.isEmpty() ) {
      offers = KMimeTypeTrader::self()->query( mime );
      for ( unsigned int i = 0; i < offers.count(); ++i ) {
         KSharedPtr<KService> service = offers[ i ];
         if ( service->isValid() && service->type() == "Application" ) {
            openWith.addAction( krLoader->loadIcon( service->icon(), K3Icon::Small ), service->name() )->setData( QVariant( SERVICE_LIST_ID + i ) );
         }
      }
      openWith.addSeparator();
      if ( vf->vfile_isDir() )
         openWith.addAction( krLoader->loadIcon( "konsole", K3Icon::Small ), i18n( "Terminal" ))->setData( QVariant( OPEN_TERM_ID ) );
      openWith.addAction( i18n( "Other..." ) )->setData( QVariant( CHOOSE_ID ) );
      QAction *owAct = addMenu( &openWith );
      owAct->setData( QVariant( OPEN_WITH_ID ) );
      owAct->setText( i18n( "Open With" ) );
      addSeparator();
   }
	
	// --------------- user actions
   QAction *uAct = addMenu( new UserActionPopupMenu( panel->func->files()->vfs_getFile( item->name() ).url() ) );
   uAct->setText( i18n("User Actions") );
   for ( KrViewItemList::Iterator it = items.begin(); it != items.end(); ++it ) {
		vfile *file = panel->func->files()->vfs_search(((*it)->name()));
		KUrl url = file->vfile_getUrl();
		_items.append( new KFileItem( url,  file->vfile_getMime(), file->vfile_getMode() ) );
   }
   
#ifdef __LIBKONQ__
	// -------------- konqueror menu
   actions = new KActionCollection(this);
	konqMenu = new KonqPopupMenu( KonqBookmarkManager::self(), _items, panel->func->files()->vfs_getOrigin(), *actions, 0, this, 
                           KonqPopupMenu::NoFlags, KParts::BrowserExtension::DefaultPopupItems );
   QAction * konqAct = addMenu( konqMenu );
   konqAct->setData( QVariant( KONQ_MENU_ID ) );
   konqAct->setText( i18n( "Konqueror Menu" ) );
#endif
   
	// ------------- 'create new' submenu
   addCreateNewMenu();
	addSeparator();
   
   // ---------- COPY
   addAction( i18n( "Copy..." ) )->setData( QVariant( COPY_ID ) );
   if ( panel->func->files() ->vfs_isWritable() ) {
      // ------- MOVE
      addAction( i18n( "Move..." ) )->setData( QVariant( MOVE_ID ) );
      // ------- RENAME - only one file
      if ( !multipleSelections )
         addAction( i18n( "Rename" ) )->setData( QVariant( RENAME_ID ) );
  
      // -------- MOVE TO TRASH
      KConfigGroup saver = krConfig->group("General");
      bool trash = krConfig->readBoolEntry( "Move To Trash", _MoveToTrash );
      if( trash )
        addAction( i18n( "Move to Trash" ) )->setData( QVariant( TRASH_ID ) );
      // -------- DELETE
      addAction( i18n( "Delete" ) )->setData( QVariant( DELETE_ID ) );
      // -------- SHRED - only one file
/*      if ( panel->func->files() ->vfs_getType() == vfs::NORMAL &&
            !vf->vfile_isDir() && !multipleSelections )
         addAction( i18n( "Shred" ) )->setData( QVariant( SHRED_ID ) );*/
   }
   
   // ---------- link handling
   // create new shortcut or redirect links - only on local directories:
   if ( panel->func->files() ->vfs_getType() == vfs::NORMAL ) {
      addSeparator();
      linkPopup.addAction( i18n( "New Symlink..." ) )->setData( QVariant( NEW_SYMLINK_ID ) );
      linkPopup.addAction( i18n( "New Hardlink..." ) )->setData( QVariant( NEW_LINK_ID ) );
      if ( panel->func->getVFile(item)->vfile_isSymLink() )
         linkPopup.addAction( i18n( "Redirect Link..." ) )->setData( QVariant( REDIRECT_LINK_ID) );
      QAction *linkAct = addMenu( &linkPopup );
      linkAct->setData( QVariant( LINK_HANDLING_ID ) );
      linkAct->setText( i18n( "Link Handling" ) );
   }
   addSeparator();

   // ---------- calculate space
   if ( panel->func->files() ->vfs_getType() == vfs::NORMAL && ( vf->vfile_isDir() || multipleSelections ) )
      addAction( krCalculate  );
	
	// ---------- mount/umount/eject
   if ( panel->func->files() ->vfs_getType() == vfs::NORMAL && vf->vfile_isDir() && !multipleSelections ) {
      if ( krMtMan.getStatus( panel->func->files() ->vfs_getFile( item->name() ).path( KUrl::RemoveTrailingSlash ) ) == KMountMan::MOUNTED )
         addAction( i18n( "Unmount" ) )->setData( QVariant( UNMOUNT_ID ) );
      else if ( krMtMan.getStatus( panel->func->files() ->vfs_getFile( item->name() ).path( KUrl::RemoveTrailingSlash ) ) == KMountMan::NOT_MOUNTED )
         addAction( i18n( "Mount" ) )->setData( QVariant( MOUNT_ID ) );
      if ( krMtMan.ejectable( panel->func->files() ->vfs_getFile( item->name() ).path( KUrl::RemoveTrailingSlash ) ) )
         addAction( i18n( "Eject" ) )->setData( QVariant( EJECT_ID ) );
   }
   
   // --------- send by mail
   if ( Krusader::supportedTools().contains( "MAIL" ) && !vf->vfile_isDir() ) {
      addAction( i18n( "Send by Email" ) )->setData( QVariant( SEND_BY_EMAIL_ID ) );
   }
   
   // --------- synchronize
   if ( panel->view->numSelected() ) {
      addAction( i18n( "Synchronize Selected Files..." ) )->setData( QVariant( SYNC_SELECTED_ID ) );
   }
   
   // --------- copy/paste
   addSeparator();
   addAction( i18n( "Copy to Clipboard" ) )->setData( QVariant( COPY_CLIP_ID ) );
   if ( panel->func->files() ->vfs_isWritable() )
   {
     addAction( i18n( "Cut to Clipboard" ) )->setData( QVariant( MOVE_CLIP_ID ) );
     addAction( i18n( "Paste from Clipboard" ) )->setData( QVariant( PASTE_CLIP_ID ) );
   }
   addSeparator();
   
   // --------- properties
   addAction( krProperties );
}

KrPopupMenu::~KrPopupMenu() {
	for( int i=0; i != _items.count(); i++ )
		delete _items[ i ];
	_items.clear();
	if (actions) delete actions;
#ifdef __LIBKONQ__
	if (konqMenu) delete konqMenu;
#endif	
}

void KrPopupMenu::addEmptyMenuEntries() {
	addAction( i18n( "Paste from Clipboard" ) )->setData( QVariant( PASTE_CLIP_ID ) );
}

void KrPopupMenu::addCreateNewMenu() {
	createNewPopup.addAction( krLoader->loadIcon( "folder", K3Icon::Small ), i18n("Folder...") )->setData( QVariant( MKDIR_ID) );
	createNewPopup.addAction( krLoader->loadIcon( "txt", K3Icon::Small ), i18n("Text File...") )->setData( QVariant( NEW_TEXT_FILE_ID) );
	
	QAction *newAct = addMenu( &createNewPopup );
	newAct->setData( QVariant( CREATE_NEW_ID) );
	newAct->setText( i18n( "Create New" ) );
	
}

void KrPopupMenu::performAction(int id) {
   KUrl u;
   KUrl::List lst;

   switch ( id ) {
         case - 1 : // the user clicked outside of the menu
         return ;
         case OPEN_TAB_ID :
         	// assuming only 1 file is selected (otherwise we won't get here)
         	( ACTIVE_PANEL == LEFT_PANEL ? LEFT_MNG : RIGHT_MNG )->
         		slotNewTab( panel->func->files()->vfs_getFile( item->name() ).url() );
         	break;
         case OPEN_ID :
         	for ( KrViewItemList::Iterator it = items.begin(); it != items.end(); ++it ) {
            	u = panel->func->files()->vfs_getFile( ( *it ) ->name() );
            	KRun::runUrl( u, panel->func->getVFile(item)->vfile_getMime(), parentWidget() );
         	}
         	break;
         case COPY_ID :
         	panel->func->copyFiles();
         	break;
         case MOVE_ID :
         	panel->func->moveFiles();
         	break;
         case RENAME_ID :
         	SLOTS->rename();
         	break;
         case TRASH_ID :
         	panel->func->deleteFiles( false );
         	break;
         case DELETE_ID :
         	panel->func->deleteFiles( true );
         	break;
         case EJECT_ID :
         	KMountMan::eject( panel->func->files() ->vfs_getFile( item->name() ).path( KUrl::RemoveTrailingSlash ) );
         	break;
/*         case SHRED_ID :
            if ( KMessageBox::warningContinueCancel( krApp,
                 i18n("<qt>Do you really want to shred <b>%1</b>? Once shred, the file is gone forever!</qt>").arg(item->name()),
                 QString(), KStandardGuiItem::cont(), KStandardGuiItem::cancel(), "Shred" ) == KMessageBox::Continue )
               KShred::shred( panel->func->files() ->vfs_getFile( item->name() ).path( KUrl::RemoveTrailingSlash ) );
         	break;*/
         case OPEN_KONQ_ID :
         	KToolInvocation::startServiceByDesktopName( "konqueror", panel->func->files() ->vfs_getFile( item->name() ).url() );
         	break;
         case CHOOSE_ID : // open-with dialog
         	u = panel->func->files() ->vfs_getFile( item->name() );
         	lst.append( u );
         	KRun::displayOpenWithDialog( lst, parentWidget() );
         	break;
         case MOUNT_ID :
         	krMtMan.mount( panel->func->files() ->vfs_getFile( item->name() ).path( KUrl::RemoveTrailingSlash ) );
         	break;
         case NEW_LINK_ID :
         	panel->func->krlink( false );
         	break;
         case NEW_SYMLINK_ID :
         	panel->func->krlink( true );
         	break;
         case REDIRECT_LINK_ID :
         	panel->func->redirectLink();
         	break;
         case UNMOUNT_ID :
         	krMtMan.unmount( panel->func->files() ->vfs_getFile( item->name() ).path( KUrl::RemoveTrailingSlash ) );
         	break;
         case COPY_CLIP_ID :
         	panel->func->copyToClipboard();
         	break;
         case MOVE_CLIP_ID :
         	panel->func->copyToClipboard( true );
         	break;
         case PASTE_CLIP_ID :
				panel->func->pasteFromClipboard();
         	break;
         case SEND_BY_EMAIL_ID :
         	SLOTS->sendFileByEmail( panel->func->files() ->vfs_getFile( item->name() ).url() );
         	break;
			case MKDIR_ID :
				SLOTS->mkdir();
				break;
			case NEW_TEXT_FILE_ID:
				SLOTS->editDlg();
				break;
         case SYNC_SELECTED_ID :
         	{
				QStringList selectedNames;
            for ( KrViewItemList::Iterator it = items.begin(); it != items.end(); ++it )
               selectedNames.append( ( *it ) ->name() );
            if( panel->otherPanel->view->numSelected() ) {
               KrViewItemList otherItems;
               panel->otherPanel->view->getSelectedKrViewItems( &otherItems );
               
               for ( KrViewItemList::Iterator it2 = otherItems.begin(); it2 != otherItems.end(); ++it2 ) {
                  QString name = ( *it2 ) ->name();
                  if( !selectedNames.contains( name ) )
                    selectedNames.append( name );
               }
            }
            SLOTS->slotSynchronizeDirs( selectedNames );
         	}
         	break;
         case OPEN_TERM_ID :
         	QString save = getcwd( 0, 0 );
         	chdir( panel->func->files() ->vfs_getFile( item->name() ).path( KUrl::RemoveTrailingSlash ).local8Bit() );
				K3Process proc;
				{
				KConfigGroup saver = krConfig->group("General");
         	QString term = krConfig->readEntry( "Terminal", _Terminal );
         	proc << KrServices::separateArgs( term );
         	if ( !panel->func->getVFile(item)->vfile_isDir() ) proc << "-e" << item->name();
         	if ( term.contains( "konsole" ) ) {   /* KDE 3.2 bug (konsole is killed by pressing Ctrl+C) */
         	                                      /* Please remove the patch if the bug is corrected */
					proc << "&";
            	proc.setUseShell( true );
         	}
         	if ( !proc.start( K3Process::DontCare ) )
               KMessageBox::sorry( krApp, i18n( "Can't open \"%1\"" ).arg(term) );
				} // group-saver is blown out of scope here
         	chdir( save.local8Bit() );
         	break;
	}
   
   // check if something from the open-with-offered-services was selected
   if ( id >= SERVICE_LIST_ID ) {
      QStringList names;
      panel->getSelectedNames( &names );
      KRun::run( *( offers[ id - SERVICE_LIST_ID ] ),
                 *( panel->func->files() ->vfs_getFiles( &names ) ), parentWidget() );
   }
}

#include "krpopupmenu.moc"
