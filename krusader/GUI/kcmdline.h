/***************************************************************************
                               kcmdline.h
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


#ifndef KCMDLINE_H
#define KCMDLINE_H

// QT includes
#include <qwidget.h>
#include <QtWidgets/QLabel>
#include <QtWidgets/QLayout>
#include <QtWidgets/QToolButton>
#include <QKeyEvent>

// KDE includes
#include <klineedit.h>
#include <kshellcompletion.h>
#include <khistorycombobox.h>

#include "../UserAction/kractionbase.h"

class KCMDModeButton;

class CmdLineCombo : public KHistoryComboBox
{
    Q_OBJECT
public:
    CmdLineCombo(QWidget *parent);

    virtual bool eventFilter(QObject *watched, QEvent *e);

    QString path() {
        return _path;
    }
    void setPath(QString path);

signals:
    void returnToPanel();

protected slots:
    void doLayout();

protected:
    virtual void resizeEvent(QResizeEvent *e);
    virtual void keyPressEvent(QKeyEvent *e);

    void updateLineEditGeometry();

    QLabel *_pathLabel;
    QString _path;
    bool _handlingLineEditResize;
};


class KCMDLine : public QWidget, KrActionBase
{
    Q_OBJECT
public:
    KCMDLine(QWidget *parent = 0);
    ~KCMDLine();
    void setCurrent(const QString &path);
    //virtual methods from KrActionBase
    void setText(QString text);
    QString command() const;
    ExecType execType() const;
    QString startpath() const;
    QString user() const;
    QString text() const;
    bool acceptURLs() const;
    bool confirmExecution() const;
    bool doSubstitution() const;

signals:
    void signalRun();

public slots:
    void slotReturnFocus(); // returns keyboard focus to panel
    void slotRun();
    void addPlaceholder();
    void addText(QString text) {
        cmdLine->lineEdit()->setText(cmdLine->lineEdit()->text() + text);
    }
    void popup() {
        cmdLine->showPopup();
    }

protected:
    virtual void focusInEvent(QFocusEvent*) {
        cmdLine->setFocus();
    }

    void calcLabelSize();

private:
    CmdLineCombo *cmdLine;
    KCMDModeButton *terminal;
    QToolButton *buttonAddPlaceholder;
    KShellCompletion completion;
};

#endif
