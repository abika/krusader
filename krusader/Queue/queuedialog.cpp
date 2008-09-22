/***************************************************************************
                               queuedialog.cpp
                             -------------------
    copyright            : (C) 2008+ by Csaba Karai
    email                : krusader@users.sourceforge.net
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

#include "queuedialog.h"
#include "queuewidget.h"
#include "../krusader.h"
#include <qlayout.h>
#include <qframe.h>
#include <klocale.h>
#include <qevent.h>
#include <qpainter.h>
#include <qrect.h>
#include <qstyleoption.h>
#include <qlabel.h>
#include <kglobalsettings.h>
#include <qfont.h>
#include <qtoolbutton.h>
#include <qimage.h>
#include <kiconeffect.h>

class KrImageButton : public QToolButton
{
public:
  KrImageButton( QWidget * parent ) : QToolButton ( parent ), _onIcon( false )
  {
  }

  virtual QSize sizeHint() const
  {
    int size = QFontMetrics( font() ).height();
    return QSize( size, size );
  }

  virtual void paintEvent( QPaintEvent* )
  {
    int size = QFontMetrics( font() ).height();

    QPixmap pm = icon().pixmap( size, size );

    if( _onIcon )
    {
      QImage img = pm.toImage();
      KIconEffect::colorize( img, Qt::white, 0.4f );
      pm = QPixmap::fromImage( img );
    }

    QPainter paint( this );
    paint.drawPixmap( QPoint(), pm );
  }

  virtual void enterEvent( QEvent * )
  {
    _onIcon = true;
     repaint();
  }

  virtual void leaveEvent( QEvent * )
  {
    _onIcon = false;
     repaint();
  }

private:
  bool _onIcon;
};

QueueDialog * QueueDialog::_queueDialog = 0;

QueueDialog::QueueDialog() : QDialog( 0, Qt::FramelessWindowHint )
{
  setWindowModality( Qt::NonModal );
  setWindowTitle( i18n("Krusader::Queue Manager") );
  setSizeGripEnabled( true );

  QGridLayout *grid_main = new QGridLayout;
  grid_main->setContentsMargins( 0, 0, 0, 12 );
  grid_main->setSpacing( 0 );

  QFrame *titleWg = new QFrame( this );
  titleWg->setFrameShape( QFrame::Box );
  titleWg->setFrameShadow( QFrame::Raised );
  titleWg->setLineWidth( 1 );  // a nice 3D touch :-)
  titleWg->setAutoFillBackground( true );
  titleWg->setContentsMargins( 2, 2, 2, 2 );

  QPalette palette = titleWg->palette();
  palette.setColor( QPalette::WindowText, KGlobalSettings::activeTextColor() );
  palette.setColor( QPalette::Window, KGlobalSettings::activeTitleColor() );
  titleWg->setPalette( palette );

  QHBoxLayout * hbox = new QHBoxLayout( titleWg );
  hbox->setSpacing( 0 );

  QLabel *title = new QLabel( i18n("Queue Manager"), titleWg );
  QSizePolicy titlepolicy( QSizePolicy::Expanding, QSizePolicy::Fixed);
  title->setSizePolicy( titlepolicy );
  title->setAlignment( Qt::AlignHCenter );
  QFont fnt = title->font();
  fnt.setBold( true );
  title->setFont( fnt );
  hbox->addWidget( title );

  KrImageButton * closeBtn = new KrImageButton( titleWg );
  closeBtn->setIcon( KIcon( "window-close" ) );
  connect( closeBtn, SIGNAL( clicked() ), this, SLOT( reject() ) );
  hbox->addWidget( closeBtn );

  grid_main->addWidget( titleWg, 0, 0 );

  QueueWidget *wdg = new QueueWidget( this );
  grid_main->addWidget( wdg, 1, 0 );

  setLayout( grid_main );

  KConfigGroup group( krConfig, "QueueManager");
  _sizeX = group.readEntry( "Window Width",  -1 );
  _sizeY = group.readEntry( "Window Height",  -1 );
  _x = group.readEntry( "Window X",  -1 );
  _y = group.readEntry( "Window Y",  -1 );

  if( _sizeX != -1 && _sizeY != -1 )
    resize( _sizeX, _sizeY );
  else
    resize( 300, 400 );

  if( group.readEntry( "Window Maximized",  false ) )
      showMaximized();
  else {
      if( _x != -1 && _y != -1 )
        move( _x, _y );
      else
        move( 20, 20 );

      show();
  }

  _queueDialog = this;
}

QueueDialog::~QueueDialog()
{
  _queueDialog = 0;
}

void QueueDialog::showDialog()
{
  if( _queueDialog == 0 )
    _queueDialog = new QueueDialog();
  else {
    _queueDialog->raise();
    _queueDialog->activateWindow();
  }
}

void QueueDialog::paintEvent ( QPaintEvent * event )
{
  QDialog::paintEvent( event );
  QPainter p( this );

  int lineWidth = 2;
  int midLineWidth = 0;

  QRect rect = contentsRect();
  rect.adjust( -2, -2, 2, 2 );

  QStyleOptionFrame opt;
  qDrawShadeRect(&p, rect, opt.palette, true, lineWidth, midLineWidth);
}

void QueueDialog::mousePressEvent(QMouseEvent *me)
{
  _clickPos = me->globalPos();
  _startPos = pos();
}

void QueueDialog::mouseMoveEvent(QMouseEvent *me)
{
  move(_startPos + me->globalPos() - _clickPos);
}