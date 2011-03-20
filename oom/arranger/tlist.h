//=========================================================
//  OOMidi
//  OpenOctave Midi and Audio Editor
//    $Id: tlist.h,v 1.8.2.5 2008/01/19 13:33:46 wschweer Exp $
//  (C) Copyright 1999 Werner Schweer (ws@seh.de)
//=========================================================

#ifndef __TLIST_H__
#define __TLIST_H__

#include "track.h"

#include <QWidget>

class QKeyEvent;
class QLineEdit;
class QMouseEvent;
class QPaintEvent;
class QResizeEvent;
class QScrollBar;
class QWheelEvent;

class ScrollScale;
class Track;
class Xml;
class Header;

enum TrackColumn
{
    COL_CLASS = 0,
    COL_AUTOMATION,
    COL_OPORT,
    COL_RECORD,
    COL_MUTE,
    COL_SOLO,
    COL_OCHANNEL,
    COL_TIMELOCK,
    COL_NAME,
    COL_NONE = -1
};

//---------------------------------------------------------
//   TList
//---------------------------------------------------------

class TList : public QWidget
{
    Q_OBJECT

    int ypos;
    bool editMode;

    QPixmap bgPixmap; // background Pixmap
    bool resizeFlag; // true if resize cursor is shown

    Header* header;
    QScrollBar* _scroll;
    QLineEdit* editor;
    Track* editTrack;
	Track* editAutomation;

    int startY;
    int curY;
    int sTrack;
    int dragHeight;
    int dragYoff;

    enum
    {
        NORMAL, START_DRAG, DRAG, RESIZE
    } mode;

    virtual void paintEvent(QPaintEvent*);
    virtual void mousePressEvent(QMouseEvent* event);
    //virtual void mouseDoubleClickEvent(QMouseEvent*);
    virtual void mouseMoveEvent(QMouseEvent*);
    virtual void mouseReleaseEvent(QMouseEvent*);
    virtual void keyPressEvent(QKeyEvent* e);
    virtual void wheelEvent(QWheelEvent* e);

    void portsPopupMenu(Track*, int, int);
    void oportPropertyPopupMenu(Track*, int x, int y);
    void adjustScrollbar();
    void paint(const QRect& r);
    virtual void resizeEvent(QResizeEvent*);
    void redraw(const QRect& r);
    Track* y2Track(int) const;
    void classesPopupMenu(Track*, int x, int y);
    TrackList getRecEnabledTracks();
    void setHeaderToolTips();

private slots:
    void returnPressed();
    void songChanged(int flags);
    void changeAutomation(QAction*);
	void updateSelection(Track*, bool);

signals:
    ///void selectionChanged();
    void selectionChanged(Track*);
    void keyPressExt(QKeyEvent*);
    void redirectWheelEvent(QWheelEvent*);
	void trackInserted(int);

public slots:
    void tracklistChanged();
    void setYPos(int);
    void redraw();
    void selectTrack(Track*);
    void selectTrackAbove();
    void selectTrackBelow();
	void renameTrack(Track*);
    void moveSelection(int n);
    void moveSelectedTrack(int n);

public:
    TList(Header*, QWidget* parent, const char* name);

    void setScroll(QScrollBar* s)
    {
        _scroll = s;
    }

    Track* track() const
    {
        return editTrack;
    }
    void writeStatus(int level, Xml&, const char* name) const;
    void readStatus(Xml&, const char* name);
};

#endif

