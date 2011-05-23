//=========================================================
//  OOMidi
//  OpenOctave Midi and Audio Editor
//    $Id: utils.cpp,v 1.1.1.1.2.3 2009/11/14 03:37:48 terminator356 Exp $
//  (C) Copyright 1999 Werner Schweer (ws@seh.de)
//=========================================================

#include <fastlog.h>
#include <stdio.h>
#include <stdlib.h>
#include <math.h>
#include <sys/time.h>

#include <QFrame>
#include <QDateTime>

#include "utils.h"

//---------------------------------------------------------
//   curTime
//---------------------------------------------------------

double curTime()
{
	struct timeval t;
	gettimeofday(&t, 0);
	return (double) ((double) t.tv_sec + (t.tv_usec / 1000000.0));
}

//---------------------------------------------------------
//   dump
//    simple debug output
//---------------------------------------------------------

void dump(const unsigned char* p, int n)
{
	printf("dump %d\n", n);
	for (int i = 0; i < n; ++i)
	{
		printf("%02x ", *p++);
		if ((i > 0) && (i % 16 == 0) && (i + 1 < n))
			printf("\n");
	}
	printf("\n");
}

//---------------------------------------------------------
//   num2cols
//---------------------------------------------------------

int num2cols(int min, int max)
{
	int amin = abs(min);
	int amax = abs(max);
	int l = amin > amax ? amin : amax;
	return int(log10(l)) + 1;
}

//---------------------------------------------------------
//   hLine
//---------------------------------------------------------

QFrame* hLine(QWidget* w)
{
	QFrame* delim = new QFrame(w);
	delim->setFrameStyle(QFrame::HLine | QFrame::Sunken);
	return delim;
}

//---------------------------------------------------------
//   vLine
//---------------------------------------------------------

QFrame* vLine(QWidget* w)
{
	QFrame* delim = new QFrame(w);
	delim->setFrameStyle(QFrame::VLine | QFrame::Sunken);
	return delim;
}

//---------------------------------------------------------
//   bitmap2String
//    5c -> 1-4 1-6
//
//    01011100
//
//---------------------------------------------------------

QString bitmap2String(int bm)
{
	QString s;
	//printf("bitmap2string: bm %04x", bm);
	if (bm == 0xffff)
		s = "all";
	else if (bm == 0)
		s = "none";
	else
	{
		bool range = false;
		int first = 0;
		bool needSpace = false;
		bm &= 0xffff;
		for (int i = 0; i < 17; ++i)
		{
			//for (int i = 0; i < 16; ++i) {
			if ((1 << i) & bm)
			{
				if (!range)
				{
					range = true;
					first = i;
				}
			}
			else
			{
				if (range)
				{
					if (needSpace)
						s += " ";
					QString ns;
					if (first == i - 1)
						ns.sprintf("%d", first + 1);
					else
						ns.sprintf("%d-%d", first + 1, i);
					s += ns;
					needSpace = true;
				}
				range = false;
			}
		}
	}
	//printf(" -> <%s>\n", s.toLatin1());
	return s;
}

//---------------------------------------------------------
//   u32bitmap2String
//---------------------------------------------------------
// Added by Tim. p3.3.8

QString u32bitmap2String(unsigned int bm)
{
	QString s;
	//printf("bitmap2string: bm %04x", bm);
	//if (bm == 0xffff)
	if (bm == 0xffffffff)
		s = "all";
	else if (bm == 0)
		s = "none";
	else
	{
		bool range = false;
		int first = 0;
		//unsigned int first = 0;
		bool needSpace = false;
		//bm &= 0xffff;
		//for (int i = 0; i < 17; ++i) {
		for (int i = 0; i < 33; ++i)
		{
			if ((i < 32) && ((1U << i) & bm))
			{
				if (!range)
				{
					range = true;
					first = i;
				}
			}
			else
			{
				if (range)
				{
					if (needSpace)
						s += " ";
					QString ns;
					if (first == i - 1)
						ns.sprintf("%d", first + 1);
						//ns.sprintf("%u", first+1);
					else
						ns.sprintf("%d-%d", first + 1, i);
					//ns.sprintf("%u-%u", first+1, i);
					s += ns;
					needSpace = true;
				}
				range = false;
			}
		}
	}
	//printf(" -> <%s>\n", s.toLatin1());
	return s;
}

//---------------------------------------------------------
//   string2bitmap
//---------------------------------------------------------

int string2bitmap(const QString& str)
{
	int val = 0;
	QString ss = str.simplified();
	QByteArray ba = ss.toLatin1();
	const char* s = ba.constData();
	//printf("string2bitmap <%s>\n", s);

	if (s == 0)
		return 0;
	if (strcmp(s, "all") == 0)
		return 0xffff;
	if (strcmp(s, "none") == 0)
		return 0;
	// printf("str2bitmap: <%s> ", str.toLatin1);
	int tval = 0;
	bool range = false;
	int sval = 0;
	while (*s == ' ')
		++s;
	while (*s)
	{
		if (*s >= '0' && *s <= '9')
		{
			tval *= 10;
			tval += *s - '0';
		}
		else if (*s == ' ' || *s == ',')
		{
			if (range)
			{
				for (int i = sval - 1; i < tval; ++i)
					val |= (1 << i);
				range = false;
			}
			else
			{
				val |= (1 << (tval - 1));
			}
			tval = 0;
		}
		else if (*s == '-')
		{
			range = true;
			sval = tval;
			tval = 0;
		}
		++s;
	}
	if (range && tval)
	{
		for (int i = sval - 1; i < tval; ++i)
			val |= (1 << i);
	}
	else if (tval)
	{
		val |= (1 << (tval - 1));
	}
	return val & 0xffff;
}

//---------------------------------------------------------
//   string2u32bitmap
//---------------------------------------------------------
// Added by Tim. p3.3.8

unsigned int string2u32bitmap(const QString& str)
{
	//int val = 0;
	unsigned int val = 0;
	QString ss = str.simplified();
	QByteArray ba = ss.toLatin1();
	const char* s = ba.constData();
	//printf("string2bitmap <%s>\n", s);

	if (s == 0)
		return 0;
	if (strcmp(s, "all") == 0)
		//return 0xffff;
		return 0xffffffff;
	if (strcmp(s, "none") == 0)
		return 0;
	// printf("str2bitmap: <%s> ", str.toLatin1);
	int tval = 0;
	//unsigned int tval   = 0;
	bool range = false;
	int sval = 0;
	//unsigned int sval   = 0;
	while (*s == ' ')
		++s;
	while (*s)
	{
		if (*s >= '0' && *s <= '9')
		{
			tval *= 10;
			tval += *s - '0';
		}
		else if (*s == ' ' || *s == ',')
		{
			if (range)
			{
				for (int i = sval - 1; i < tval; ++i)
					//for (unsigned int i = sval-1; i < tval; ++i)
					val |= (1U << i);
				range = false;
			}
			else
			{
				val |= (1U << (tval - 1));
			}
			tval = 0;
		}
		else if (*s == '-')
		{
			range = true;
			sval = tval;
			tval = 0;
		}
		++s;
	}
	if (range && tval)
	{
		for (int i = sval - 1; i < tval; ++i)
			//for (unsigned int i = sval-1; i < tval; ++i)
			val |= (1U << i);
	}
	else if (tval)
	{
		val |= (1U << (tval - 1));
	}
	//return val & 0xffff;
	return val;
}

//---------------------------------------------------------
//   autoAdjustFontSize
//   w: Widget to auto adjust font size
//   s: String to fit
//   ignoreWidth: Set if dealing with a vertically constrained widget - one which is free to resize horizontally.
//   ignoreHeight: Set if dealing with a horizontally constrained widget - one which is free to resize vertically. 
//---------------------------------------------------------

bool autoAdjustFontSize(QFrame* w, const QString& s, bool ignoreWidth, bool ignoreHeight, int max, int min)
{
	// In case the max or min was obtained from QFont::pointSize() which returns -1
	//  if the font is a pixel font, or if min is greater than max...
	if (!w || (min < 0) || (max < 0) || (min > max))
		return false;

	// Limit the minimum and maximum sizes to something at least readable.
	if (max < 4)
		max = 4;
	if (min < 4)
		min = 4;

	QRect cr = w->contentsRect();
	QRect r;
	QFont fnt = w->font();
	// An extra amount just to be sure - I found it was still breaking up two words which would fit on one line.
	int extra = 4;
	// Allow at least one loop. min can be equal to max.
	for (int i = max; i >= min; --i)
	{
		fnt.setPointSize(i);
		QFontMetrics fm(fnt);
		r = fm.boundingRect(s);
		// Would the text fit within the widget?
		if ((ignoreWidth || (r.width() <= (cr.width() - extra))) && (ignoreHeight || (r.height() <= cr.height())))
			break;
	}
	// Here we will always have a font ranging from min to max point size.
	w->setFont(fnt);

	// Force minimum height. Use the expected height for the highest given point size.
	// This way the mixer strips aren't all different label heights, but can be larger if necessary.
	// Only if ignoreHeight is set (therefore the height is adjustable).
	if (ignoreHeight)
	{
		fnt.setPointSize(max);
		QFontMetrics fm(fnt);
		// Set the label's minimum height equal to the height of the font.
		w->setMinimumHeight(fm.height() + 2 * w->frameWidth());
	}

	return true;
}


double dbToVal(double inDb)
{
    return (20.0*log10(inDb)+60) / 70.0;
}

double valToDb(double inV)
{
    return exp10((inV*70.0-60)/20.0);
}

int dbToMidi(double val)
{
	int v = (int)(val + 60)/0.55;
	//printf("dbToMidi v: %g \n", v, v);
	if(v > 127)
		v = 127;
	if(v < 0)
		v = 0;
	return v; //(val + 60)*0.5546875;
}

double midiToDb(int val)
{
	return (double)(val / 1.8028169)-60;
}


double trackVolToDb(double vol)
{
	return fast_log10(vol) * 20.0;
}

double dbToTrackVol(double val)
{
	double rv = pow(10.0, val / 20.0);
	return rv;
}

int trackPanToMidi(double val)
{
	double pan = (val+1)*64;
	int p = (int)pan;
	if(p > 127)
		p = 127;
	if(p < 0)
		p = 0;
	return p;
}

double midiToTrackPan(int val)
{
	double v = (double)(double(val)/64)-1;
	if(v >= 0.98)
		v = 1;
	if(v < -1)
		v = -1;
	return v;
}
									
qint64 genId( )
{
	uint r = qrand();
	QDateTime time = QDateTime::currentDateTime();
	uint timeValue = time.toTime_t();
	qint64 id = timeValue;
	id *= 1000000000;
	id += r;

	return id;
}
							