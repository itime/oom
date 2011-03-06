//=========================================================
//  OOMidi
//  OpenOctave Midi and Audio Editor
//    $Id: piano.h,v 1.2 2004/05/31 11:48:55 lunar_shuttle Exp $
//  (C) Copyright 1999 Werner Schweer (ws@seh.de)
//=========================================================

#ifndef __PIANO_H__
#define __PIANO_H__

#include "view.h"
#include <QList>

class QEvent;
class QMouseEvent;
class QPainter;
class QPixmap;

#define KH  13

//---------------------------------------------------------
//   Piano
//---------------------------------------------------------

class Piano : public View
{
    int curPitch;
    QPixmap* c_keys[10];
    QPixmap* mk1;
    QPixmap* mk2;
    QPixmap* mk3;
    QPixmap* mk4;
    QPixmap* mk1_l;
    QPixmap* mk2_l;
    QPixmap* mk3_l;
    QPixmap* mk4_l;
    QPixmap* mk1_n;
    QPixmap* mk2_n;
    QPixmap* mk3_n;
    QPixmap* mk4_n;
    int keyDown;
    bool shift;
    int button;
	QList<int>* enabled;

    Q_OBJECT
    int y2pitch(int) const;
    int pitch2y(int) const;
    void viewMouseMoveEvent(QMouseEvent* event);
    virtual void leaveEvent(QEvent*e);

    virtual void viewMousePressEvent(QMouseEvent* event);
    virtual void viewMouseReleaseEvent(QMouseEvent*);

protected:
    virtual void draw(QPainter&, const QRect&);

signals:
    void pitchChanged(int);
    void keyPressed(int, int, bool);
    void keyReleased(int, bool);

public slots:
    void setPitch(int);

public:
    Piano(QWidget*, int);
};

#endif

