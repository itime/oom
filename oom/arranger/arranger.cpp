//=========================================================
//  OOMidi
//  OpenOctave Midi and Audio Editor
//    $Id: arranger.cpp,v 1.33.2.21 2009/11/17 22:08:22 terminator356 Exp $
//  (C) Copyright 1999-2004 Werner Schweer (ws@seh.de)
//=========================================================

#include "config.h"

#include <stdio.h>
#include <values.h>

#include <QComboBox>
#include <QGridLayout>
#include <QKeyEvent>
#include <QLabel>
#include <QList>
#include <QMainWindow>
#include <QScrollArea>
#include <QScrollBar>
#include <QToolBar>
#include <QToolButton>
#include <QVBoxLayout>
#include <QWheelEvent>
#include <QPainter>
#include <QDockWidget>
#include <QTabWidget>
#include <QStackedWidget>
#include <QVBoxLayout>
#include <QLayoutItem>

#include "arranger.h"
#include "song.h"
#include "app.h"
#include "mtscale.h"
#include "scrollscale.h"
#include "pcanvas.h"
#include "poslabel.h"
#include "xml.h"
#include "splitter.h"
#include "lcombo.h"
#include "mtrackinfo.h"
#include "midiport.h"
#include "mididev.h"
#include "utils.h"
#include "globals.h"
#include "tlist.h"
#include "icons.h"
#include "header.h"
#include "utils.h"
#include "audio.h"
#include "event.h"
#include "midiseq.h"
#include "midictrl.h"
#include "mpevent.h"
#include "gconfig.h"
#include "mixer/astrip.h"
#include "mixer/mstrip.h"
#include "mixer/strip.h"
#include "spinbox.h"
#include "tvieweditor.h"
#include "traverso_shared/TConfig.h"
#include "tviewdock.h"
#include "rmap.h"

//---------------------------------------------------------
//   Arranger::setHeaderToolTips
//---------------------------------------------------------

void Arranger::setHeaderToolTips()
{
	header->setToolTip(COL_RECORD, tr("Enable Recording"));
	header->setToolTip(COL_MUTE, tr("Mute/Off Indicator"));
	header->setToolTip(COL_SOLO, tr("Solo Indicator"));
	header->setToolTip(COL_CLASS, tr("Track Type"));
	header->setToolTip(COL_NAME, tr("Track Name"));
	header->setToolTip(COL_OCHANNEL, tr("Midi output channel number or audio channels"));
	header->setToolTip(COL_OPORT, tr("Midi output port or synth midi port"));
	header->setToolTip(COL_TIMELOCK, tr("Time Lock"));
	header->setToolTip(COL_AUTOMATION, tr("Automation parameter selection"));
}



//---------------------------------------------------------
//   Arranger::setHeaderWhatsThis
//---------------------------------------------------------

void Arranger::setHeaderWhatsThis()
{
	header->setWhatsThis(COL_RECORD, tr("Enable recording. Click to toggle."));
	header->setWhatsThis(COL_MUTE, tr("Mute indicator. Click to toggle.\nRight-click to toggle track on/off.\nMute is designed for rapid, repeated action.\nOn/Off is not!"));
	header->setWhatsThis(COL_SOLO, tr("Solo indicator. Click to toggle.\nConnected tracks are also 'phantom' soloed,\n indicated by a dark square."));
	header->setWhatsThis(COL_CLASS, tr("Track type. Right-click to change\n midi and drum track types."));
	header->setWhatsThis(COL_NAME, tr("Track name. Double-click to edit.\nRight-click for more options."));
	header->setWhatsThis(COL_OCHANNEL, tr("Midi/drum track: Output channel number.\nAudio track: Channels.\nMid/right-click to change."));
	header->setWhatsThis(COL_OPORT, tr("Midi/drum track: Output port.\nSynth track: Assigned midi port.\nLeft-click to change.\nRight-click to show GUI."));
	header->setWhatsThis(COL_TIMELOCK, tr("Time lock"));
}

//---------------------------------------------------------
//   Arranger
//    is the central widget in app
//---------------------------------------------------------

Arranger::Arranger(QMainWindow* parent, const char* name)
: QWidget(parent)
{
	setObjectName(name);
	_raster = 0; // measure
	_lastStrip = 0;
	selected = 0;
	// Since program covers 3 controls at once, it is in 'midi controller' units rather than 'gui control' units.
	//program  = -1;
	///program  = CTRL_VAL_UNKNOWN;
	///pan      = -65;
	///volume   = -1;
	setMinimumSize(600, 50);
	showTrackinfoFlag = true;

	cursVal = MAXINT;

	//setFocusPolicy(Qt::StrongFocus);

	//---------------------------------------------------
	//  ToolBar
	//    create toolbar in toplevel widget
	//---------------------------------------------------

	parent->addToolBarBreak();
	QToolBar* toolbar = parent->addToolBar(tr("Arranger"));
	toolbar->setObjectName("tbArranger");
	toolbar->setMovable(false);
	toolbar->setFloatable(false);
	QToolBar* toolbar2 = new QToolBar(tr("Snap"));
	parent->addToolBar(Qt::BottomToolBarArea, toolbar2);
	toolbar2->setObjectName("tbSnap");
	toolbar2->setMovable(false);
	toolbar2->setFloatable(false);


	_rtabs = new QTabWidget(oom->resourceDock());
	_rtabs->setObjectName("tabControlCenter");
	_rtabs->setTabPosition(QTabWidget::West);
	_rtabs->setTabShape(QTabWidget::Triangular);
	_rtabs->setMinimumSize(QSize(200, 150));
	oom->resourceDock()->setWidget(_rtabs);
	connect(oom->resourceDock(), SIGNAL(dockLocationChanged(Qt::DockWidgetArea)), SLOT(resourceDockAreaChanged(Qt::DockWidgetArea)));


	cursorPos = new PosLabel(0);
	cursorPos->setEnabled(false);
	cursorPos->setFixedHeight(22);
	cursorPos->setObjectName("arrangerCursor");
	toolbar2->addWidget(cursorPos);

	QToolButton* preloadCtrl  = new QToolButton();
	preloadCtrl->setText(QString("PC"));
	toolbar2->addWidget(preloadCtrl);
	connect(preloadCtrl, SIGNAL(clicked()), SLOT(preloadControllers()));
	const char* rastval[] = {
		QT_TRANSLATE_NOOP("@default", "Off"), QT_TRANSLATE_NOOP("@default", "Bar"), "1/2", "1/4", "1/8", "1/16"
	};

    raster = new QComboBox();
	for (int i = 0; i < 6; i++)
		raster->insertItem(i, tr(rastval[i]));
	raster->setCurrentIndex(1);
	// Set the audio record part snapping. Set to 0 (bar), the same as this combo box intial raster.
	song->setArrangerRaster(0);
	toolbar2->addWidget(raster);
	connect(raster, SIGNAL(activated(int)), SLOT(_setRaster(int)));
	///raster->setFocusPolicy(Qt::NoFocus);
	raster->setFocusPolicy(Qt::TabFocus);

	// Song len
	QLabel* label = new QLabel(tr("Len"));
	label->setAlignment(Qt::AlignRight | Qt::AlignVCenter);
	label->setIndent(3);
	toolbar->addWidget(label);

	// song length is limited to 10000 bars; the real song len is limited
	// by overflows in tick computations
	//
	lenEntry = new SpinBox(1, 10000, 1);
	lenEntry->setValue(song->len());
	lenEntry->setToolTip(tr("song length - bars"));
	lenEntry->setWhatsThis(tr("song length - bars"));
	toolbar->addWidget(lenEntry);
	connect(lenEntry, SIGNAL(valueChanged(int)), SLOT(songlenChanged(int)));

	typeBox = new LabelCombo(tr("Type"), 0);
	typeBox->insertItem(0, tr("NO"));
	typeBox->insertItem(1, tr("GM"));
	typeBox->insertItem(2, tr("GS"));
	typeBox->insertItem(3, tr("XG"));
	typeBox->setCurrentIndex(0);
	typeBox->setToolTip(tr("midi song type"));
	typeBox->setWhatsThis(tr("midi song type"));
	///typeBox->setFocusPolicy(Qt::NoFocus);
	typeBox->setFocusPolicy(Qt::TabFocus);
	toolbar->addWidget(typeBox);
	connect(typeBox, SIGNAL(activated(int)), SLOT(modeChange(int)));

	label = new QLabel(tr("Pitch"));
	label->setAlignment(Qt::AlignRight | Qt::AlignVCenter);
	label->setIndent(3);
	toolbar->addWidget(label);

	globalPitchSpinBox = new SpinBox(-127, 127, 1);
	globalPitchSpinBox->setValue(song->globalPitchShift());
	globalPitchSpinBox->setToolTip(tr("midi pitch"));
	globalPitchSpinBox->setWhatsThis(tr("global midi pitch shift"));
	toolbar->addWidget(globalPitchSpinBox);
	connect(globalPitchSpinBox, SIGNAL(valueChanged(int)), SLOT(globalPitchChanged(int)));

	label = new QLabel(tr("Tempo"));
	label->setAlignment(Qt::AlignRight | Qt::AlignVCenter);
	label->setIndent(3);
	toolbar->addWidget(label);

	globalTempoSpinBox = new SpinBox(50, 200, 1, toolbar);
	globalTempoSpinBox->setSuffix(QString("%"));
	globalTempoSpinBox->setValue(tempomap.globalTempo());
	globalTempoSpinBox->setToolTip(tr("midi tempo"));
	globalTempoSpinBox->setWhatsThis(tr("midi tempo"));
	toolbar->addWidget(globalTempoSpinBox);
	connect(globalTempoSpinBox, SIGNAL(valueChanged(int)), SLOT(globalTempoChanged(int)));

	QToolButton* tempo50 = new QToolButton();
	tempo50->setText(QString("50%"));
	toolbar->addWidget(tempo50);
	connect(tempo50, SIGNAL(clicked()), SLOT(setTempo50()));

	QToolButton* tempo100 = new QToolButton();
	tempo100->setText(tr("N"));
	toolbar->addWidget(tempo100);
	connect(tempo100, SIGNAL(clicked()), SLOT(setTempo100()));

	QToolButton* tempo200 = new QToolButton();
	tempo200->setText(QString("200%"));
	toolbar->addWidget(tempo200);
	connect(tempo200, SIGNAL(clicked()), SLOT(setTempo200()));
	toolbar->hide();

	QVBoxLayout* box = new QVBoxLayout(this);
	box->setContentsMargins(0, 0, 0, 0);
	box->setSpacing(0);
	box->addWidget(hLine(this), Qt::AlignTop);

	//---------------------------------------------------
	//  Tracklist
	//---------------------------------------------------

	int xscale = -100;
	int yscale = 1;

	split = new Splitter(Qt::Horizontal, this, "arsplit");
	split->setSizePolicy(QSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding));
	box->addWidget(split, 1000);
	//split->setHandleWidth(10);

	QWidget* tracklist = new QWidget();
	QWidget* wtlist = new QWidget(split);
	QVBoxLayout *tg = new QVBoxLayout(wtlist);
        tg->setSpacing(0);

	split->setStretchFactor(split->indexOf(wtlist), 0);
	//split->setStretchFactor(split->indexOf(tracklist), 1);
	//tracklist->setSizePolicy(QSizePolicy(QSizePolicy::Fixed, QSizePolicy::Expanding, 0, 100));
	QSizePolicy tpolicy = QSizePolicy(QSizePolicy::Fixed, QSizePolicy::Expanding);
	tpolicy.setHorizontalStretch(0);
	tpolicy.setVerticalStretch(100);
	wtlist->setSizePolicy(tpolicy);
	//tracklist->setSizePolicy(tpolicy);

	QWidget* editor = new QWidget(split);
	split->setStretchFactor(split->indexOf(editor), 1);
	//editor->setSizePolicy(QSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding,
	QSizePolicy epolicy = QSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);
	epolicy.setHorizontalStretch(255);
	epolicy.setVerticalStretch(100);
	editor->setSizePolicy(epolicy);

	//---------------------------------------------------
	//    Track Info
	//---------------------------------------------------

	infoScroll = new QScrollArea;
	infoScroll->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
	infoScroll->setVerticalScrollBarPolicy(Qt::ScrollBarAsNeeded);
	//infoScroll->setMaximumWidth(300);
	infoScroll->setMinimumWidth(100);
	//infoScroll->setSizePolicy(QSizePolicy(QSizePolicy::Preferred, QSizePolicy::Expanding));
	mixerScroll = new QScrollArea;
	mixerScroll->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
	mixerScroll->setVerticalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
	//mixerScroll->setMaximumWidth(300);
	mixerScroll->setMinimumWidth(100);
	//mixerScroll->setSizePolicy(QSizePolicy(QSizePolicy::Preferred, QSizePolicy::Expanding));
	
	header = new Header(tracklist, "header");
	header->setObjectName("trackHeaders");

	header->setFixedHeight(30);

	QFontMetrics fm1(header->font());
	int fw = 8;

	header->setColumnLabel(tr("R"), COL_RECORD, fm1.width('R') + fw);
	header->setColumnLabel(tr("M"), COL_MUTE, fm1.width('M') + fw);
	header->setColumnLabel(tr("S"), COL_SOLO, fm1.width('S') + fw);
	//header->setColumnLabel(tr("C"), COL_CLASS, fm1.width('C') + fw);
	header->setColumnLabel(tr("C"), COL_CLASS, 0);
	header->setColumnLabel(tr("Track"), COL_NAME, 375);
	header->setColumnLabel(tr("Port"), COL_OPORT, 22);
	header->setColumnLabel(tr(""), COL_OCHANNEL, 0);
	header->setColumnLabel(tr("T"), COL_TIMELOCK, 0/*fm1.width('T') + fw*/);
	header->setColumnLabel(tr("Automation"), COL_AUTOMATION, 22);
	header->setResizeMode(COL_RECORD, QHeaderView::Fixed);
	header->setResizeMode(COL_MUTE, QHeaderView::Fixed);
	header->setResizeMode(COL_SOLO, QHeaderView::Fixed);
	header->setResizeMode(COL_CLASS, QHeaderView::Fixed);
	header->setResizeMode(COL_NAME, QHeaderView::Interactive);
	header->setResizeMode(COL_OPORT, QHeaderView::Interactive);
	header->setResizeMode(COL_OCHANNEL, QHeaderView::Fixed);
	header->setResizeMode(COL_TIMELOCK, QHeaderView::Fixed);
	header->setResizeMode(COL_AUTOMATION, QHeaderView::Fixed);

	setHeaderToolTips();
	setHeaderWhatsThis();
	header->setMovable(true);
	list = new TList(header, wtlist, "tracklist");

	// Do this now that the list is available.
	genTrackInfo(tracklist);

	if(_tvdock)
		connect(list, SIGNAL(trackInserted(int)), _tvdock, SLOT(selectStaticView(int)));
	connect(list, SIGNAL(selectionChanged(Track*)), SLOT(trackSelectionChanged()));
	connect(list, SIGNAL(selectionChanged(Track*)), midiTrackInfo, SLOT(setTrack(Track*)));
	connect(header, SIGNAL(sectionResized(int, int, int)), list, SLOT(redraw()));
	connect(header, SIGNAL(sectionMoved(int, int, int)), list, SLOT(redraw()));
	connect(header, SIGNAL(sectionMoved(int, int, int)), this, SLOT(headerMoved()));

        tg->addItem(new QSpacerItem(0, 24));
	tg->addWidget(list);
	list->setMinimumSize(QSize(100, 50));
	list->setMaximumSize(QSize(540, 10000));


	//---------------------------------------------------
	//    Editor
	//---------------------------------------------------

	int offset = AL::sigmap.ticksMeasure(0);
	hscroll = new ScrollScale(-1000, -10, xscale, song->len(), Qt::Horizontal, editor, -offset);
	hscroll->setFocusPolicy(Qt::NoFocus);

	// Changed p3.3.43 Too small steps for me...
	//vscroll = new QScrollBar(1, 20*20, 1, 5, 0, Vertical, editor);
	//vscroll = new QScrollBar(1, 20*20, 5, 25, 0, Qt::Vertical, editor);
	vscroll = new QScrollBar(editor);
	vscroll->setMinimum(0);
	vscroll->setMaximum(20 * 20);
	vscroll->setSingleStep(5);
	vscroll->setPageStep(25);
	vscroll->setValue(0);
	vscroll->setOrientation(Qt::Vertical);

	list->setScroll(vscroll);

	QGridLayout* egrid = new QGridLayout(editor);
	egrid->setColumnStretch(0, 50);
	egrid->setRowStretch(2, 50);
	egrid->setContentsMargins(0, 0, 0, 0);
	egrid->setSpacing(0);

	time = new MTScale(&_raster, editor, xscale);
	time->setOrigin(-offset, 0);
	canvas = new PartCanvas(&_raster, editor, xscale, yscale);
	canvas->setBg(config.partCanvasBg);
	canvas->setCanvasTools(arrangerTools);
	canvas->setOrigin(-offset, 0);
	canvas->setFocus();
	//parent->setFocusProxy(canvas);   // Tim.

	connect(canvas, SIGNAL(setUsedTool(int)), this, SIGNAL(setUsedTool(int)));
	connect(canvas, SIGNAL(trackChanged(Track*)), list, SLOT(selectTrack(Track*)));
	connect(canvas, SIGNAL(renameTrack(Track*)), list, SLOT(renameTrack(Track*)));
	connect(list, SIGNAL(keyPressExt(QKeyEvent*)), canvas, SLOT(redirKeypress(QKeyEvent*)));
	connect(canvas, SIGNAL(selectTrackAbove()), list, SLOT(selectTrackAbove()));
	connect(canvas, SIGNAL(selectTrackBelow()), list, SLOT(selectTrackBelow()));

	connect(this, SIGNAL(redirectWheelEvent(QWheelEvent*)), canvas, SLOT(redirectedWheelEvent(QWheelEvent*)));
	connect(list, SIGNAL(redirectWheelEvent(QWheelEvent*)), canvas, SLOT(redirectedWheelEvent(QWheelEvent*)));

	//egrid->addMultiCellWidget(time,           0, 0, 0, 1);
	//egrid->addMultiCellWidget(hLine(editor),  1, 1, 0, 1);
	egrid->addWidget(time, 0, 0, 1, 2);
	egrid->addWidget(hLine(editor), 1, 0, 1, 2);

	egrid->addWidget(canvas, 2, 0);
	egrid->addWidget(vscroll, 2, 1);
	egrid->addWidget(hscroll, 3, 0, Qt::AlignBottom);

	connect(vscroll, SIGNAL(valueChanged(int)), canvas, SLOT(setYPos(int)));
	connect(hscroll, SIGNAL(scrollChanged(int)), canvas, SLOT(setXPos(int)));
	connect(hscroll, SIGNAL(scaleChanged(float)), canvas, SLOT(setXMag(float)));
	connect(vscroll, SIGNAL(valueChanged(int)), list, SLOT(setYPos(int)));
	connect(hscroll, SIGNAL(scrollChanged(int)), time, SLOT(setXPos(int))); //
	connect(hscroll, SIGNAL(scaleChanged(float)), time, SLOT(setXMag(float)));
	connect(canvas, SIGNAL(timeChanged(unsigned)), SLOT(setTime(unsigned)));
	connect(canvas, SIGNAL(verticalScroll(unsigned)), SLOT(verticalScrollSetYpos(unsigned)));
	connect(canvas, SIGNAL(horizontalScroll(unsigned)), hscroll, SLOT(setPos(unsigned)));
	connect(canvas, SIGNAL(horizontalScrollNoLimit(unsigned)), hscroll, SLOT(setPosNoLimit(unsigned)));
	connect(time, SIGNAL(timeChanged(unsigned)), SLOT(setTime(unsigned)));

	connect(canvas, SIGNAL(tracklistChanged()), list, SLOT(tracklistChanged()));
	connect(canvas, SIGNAL(dclickPart(Track*)), SIGNAL(editPart(Track*)));
	connect(canvas, SIGNAL(startEditor(PartList*, int)), SIGNAL(startEditor(PartList*, int)));

	connect(song, SIGNAL(songChanged(int)), SLOT(songChanged(int)));
	//connect(song,   SIGNAL(mTypeChanged(MType)), SLOT(setMode((int)MType)));    // p4.0.7 Tim.
	connect(canvas, SIGNAL(followEvent(int)), hscroll, SLOT(setOffset(int)));
	connect(canvas, SIGNAL(selectionChanged()), SIGNAL(selectionChanged()));
	connect(canvas, SIGNAL(dropSongFile(const QString&)), SIGNAL(dropSongFile(const QString&)));
	connect(canvas, SIGNAL(dropMidiFile(const QString&)), SIGNAL(dropMidiFile(const QString&)));

	connect(canvas, SIGNAL(toolChanged(int)), SIGNAL(toolChanged(int)));
	connect(split, SIGNAL(splitterMoved(int, int)),  SLOT(splitterMoved(int, int)));
	//      connect(song, SIGNAL(posChanged(int, unsigned, bool)), SLOT(seek()));

	// Removed p3.3.43
	// Song::addMarker() already emits a 'markerChanged'.
	//connect(time, SIGNAL(addMarker(int)), SIGNAL(addMarker(int)));

	configChanged(); // set configuration values
	if (canvas->part())
		midiTrackInfo->setTrack(canvas->part()->track()); // Tim.
	showTrackInfo(showTrackinfoFlag);

	// Take care of some tabbies!
	setTabOrder(tempo200, list);
	setTabOrder(list, canvas);

	QList<int> vl;
	QString str = tconfig().get_property("arsplit", "sizes", "200 50").toString();
	QStringList sl = str.split(QString(" "), QString::SkipEmptyParts);
	for (QStringList::Iterator it = sl.begin(); it != sl.end(); ++it)
	{
		int val = (*it).toInt();
		vl.append(val);
	}
	split->setSizes(vl);
}

Arranger::~Arranger()
{
	//tconfig().set_property(split->objectName(), "listwidth", split->sizes().at(0));
	//tconfig().set_property(split->objectName(), "canvaswidth", split->sizes().at(1));
}

//---------------------------------------------------------
//   updateHScrollRange
//---------------------------------------------------------

//void Arranger::updateHScrollRange()
//{
//      int s = 0, e = song->len();
// Show one more measure.
//      e += AL::sigmap.ticksMeasure(e);  
// Show another quarter measure due to imprecise drawing at canvas end point.
//      e += AL::sigmap.ticksMeasure(e) / 4;
// Compensate for the fixed vscroll width. 
//      e += canvas->rmapxDev(-vscroll->width()); 
//      int s1, e1;
//      hscroll->range(&s1, &e1);
//      if(s != s1 || e != e1) 
//        hscroll->setRange(s, e);
//}

//---------------------------------------------------------
//   headerMoved
//---------------------------------------------------------

void Arranger::headerMoved()
{
	//header->setResizeMode(COL_NAME, QHeaderView::Stretch);
}

//---------------------------------------------------------
//   setTime
//---------------------------------------------------------

void Arranger::setTime(unsigned tick)
{
	if (tick == MAXINT)
		return;
	else
	{
		cursVal = tick;
		cursorPos->setEnabled(true);
		cursorPos->setValue(tick);
		time->setPos(3, tick, false);
	}
}

//---------------------------------------------------------
//   toolChange
//---------------------------------------------------------

void Arranger::setTool(int t)
{
	canvas->setTool(t);
}

//---------------------------------------------------------
//   dclickPart
//---------------------------------------------------------

void Arranger::dclickPart(Track* t)
{
	emit editPart(t);
}

//---------------------------------------------------------
//   configChanged
//---------------------------------------------------------

void Arranger::configChanged()
{
	//printf("Arranger::configChanged\n");

	if (config.canvasBgPixmap.isEmpty())
	{
		canvas->setBg(config.partCanvasBg);
		canvas->setBg(QPixmap());
		//printf("Arranger::configChanged - no bitmap!\n");
	}
	else
	{

		//printf("Arranger::configChanged - bitmap %s!\n", config.canvasBgPixmap.ascii());
		canvas->setBg(QPixmap(config.canvasBgPixmap));
	}
	///midiTrackInfo->setFont(config.fonts[2]);
	//updateTrackInfo(type);
}

//---------------------------------------------------------
//   songlenChanged
//---------------------------------------------------------

void Arranger::songlenChanged(int n)
{
	int newLen = AL::sigmap.bar2tick(n, 0, 0);
	song->setLen(newLen);
}
//---------------------------------------------------------
//   songChanged
//---------------------------------------------------------

void Arranger::songChanged(int type)
{
	// Is it simply a midi controller value adjustment? Forget it.
	if (type != SC_MIDI_CONTROLLER)
	{
		unsigned endTick = song->len();
		int offset = AL::sigmap.ticksMeasure(endTick);
		hscroll->setRange(-offset, endTick + offset); //DEBUG
		canvas->setOrigin(-offset, 0);
		time->setOrigin(-offset, 0);

		int bar, beat;
		unsigned tick;
		AL::sigmap.tickValues(endTick, &bar, &beat, &tick);
		if (tick || beat)
			++bar;
		lenEntry->blockSignals(true);
		lenEntry->setValue(bar);
		lenEntry->blockSignals(false);

		if (type & SC_SONG_TYPE) // p4.0.7 Tim.
			setMode(song->mtype());

		trackSelectionChanged();
		canvas->partsChanged();
		typeBox->setCurrentIndex(int(song->mtype()));
		if (type & SC_SIG)
			time->redraw();
		if (type & SC_TEMPO)
			setGlobalTempo(tempomap.globalTempo());

		if (type & (SC_TRACK_REMOVED | SC_VIEW_CHANGED))
		{
			canvas->trackViewChanged();
			/*AudioStrip* w = (AudioStrip*) _lastStrip;//(trackInfo->getWidget(2));
			//AudioStrip* w = (AudioStrip*)(trackInfo->widget(2));
			if (w)
			{
				Track* t = w->getTrack();
				if (t)
				{
					TrackList* tl;
					if(!song->viewselected)
						tl = song->tracks();
					else
						tl = song->visibletracks();
					iTrack it = tl->find(t);
					if (it == tl->end())
					{
						delete w;
						//trackInfo->addWidget(0, 2);
						//trackInfo->insertWidget(2, 0);
						selected = 0;
					}
				}
			}*/
		}
		if(type & SC_VIEW_CHANGED)
		{//Scroll to top
			//canvas->setYPos(0);
			//vscroll->setValue(0);
		}
	}

	updateTrackInfo(type);
}

void Arranger::splitterMoved(int pos, int)
{
	if(pos > list->maximumSize().width())
	{
		QList<int> def;
		def.append(list->maximumSize().width());
		def.append(50);
		split->setSizes(def);
	}
}

//---------------------------------------------------------
//   trackSelectionChanged
//---------------------------------------------------------

void Arranger::trackSelectionChanged()
{
	TrackList* tracks = song->visibletracks();
	Track* track = 0;
	for (iTrack t = tracks->begin(); t != tracks->end(); ++t)
	{
		if ((*t)->selected())
		{
			track = *t;
			break;
		}
	}
	if (track == selected)
		return;
	selected = track;
	updateTrackInfo(-1);

        // Check if the selected track is inside the view, if not
        // scroll the track to the center of the view
        int vScrollValue = vscroll->value();
        int trackYPos = canvas->track2Y(selected);
        if (trackYPos > (vScrollValue + canvas->height()) ||
            trackYPos < vScrollValue)
        {
                vscroll->setValue(trackYPos - (canvas->height() / 2));
        }
}

//---------------------------------------------------------
//   modeChange
//---------------------------------------------------------

void Arranger::modeChange(int mode)
{
	song->setMType(MType(mode));
	updateTrackInfo(-1);
}

//---------------------------------------------------------
//   setMode
//---------------------------------------------------------

void Arranger::setMode(int mode)
{
	typeBox->blockSignals(true); //
	// This will only set if different.
	typeBox->setCurrentIndex(mode);
	typeBox->blockSignals(false); //
}

void Arranger::showTrackViews()
{
	TrackViewEditor* ted = new TrackViewEditor(this);
	ted->show();
}

void Arranger::resourceDockAreaChanged(Qt::DockWidgetArea area)
{
	switch(area)
	{
		case Qt::LeftDockWidgetArea:
			_rtabs->setTabPosition(QTabWidget::West);
		break;
		case Qt::RightDockWidgetArea:
			_rtabs->setTabPosition(QTabWidget::East);
		break;
		default:
		break;
	}
}

//---------------------------------------------------------
//   writeStatus
//---------------------------------------------------------

void Arranger::writeStatus(int level, Xml& xml)
{
	xml.tag(level++, "arranger");
	//xml.intTag(level, "info", ib->isChecked());
	split->writeStatus(level, xml);
	list->writeStatus(level, xml, "list");

	xml.intTag(level, "xpos", hscroll->pos());
	xml.intTag(level, "xmag", hscroll->mag());
	xml.intTag(level, "ypos", vscroll->value());
	xml.etag(level, "arranger");
}

//---------------------------------------------------------
//   readStatus
//---------------------------------------------------------

void Arranger::readStatus(Xml& xml)
{
	//printf("Arranger::readStatus() entering\n");
	for (;;)
	{
		Xml::Token token(xml.parse());
		const QString & tag(xml.s1());
		switch (token)
		{
			case Xml::Error:
			case Xml::End:
				return;
			case Xml::TagStart:
				if (tag == "info")
					showTrackinfoFlag = xml.parseInt();
				else if(tag == "split") //backwards compat
					xml.skip(tag);
					//split->readStatus(xml);
				else if (tag == split->objectName())
					xml.skip(tag);
					//split->readStatus(xml);
				else if (tag == "list")
					xml.skip(tag);
					//list->readStatus(xml, "list");
				else if (tag == "xmag")
					hscroll->setMag(xml.parseInt());
				else if (tag == "xpos")
				{
					int hpos = xml.parseInt();
					hscroll->setPos(hpos);
				}
				else if (tag == "ypos")
					vscroll->setValue(xml.parseInt());
				else
					xml.unknown("Arranger");
				break;
			case Xml::TagEnd:
				if (tag == "arranger")
				{
					//ib->setChecked(showTrackinfoFlag);
					//printf("Arranger::readStatus() leaving end tag\n");
					return;
				}
			default:
				break;
		}
	}
	//printf("Arranger::readStatus() leaving\n");
}

//---------------------------------------------------------
//   setRaster
//---------------------------------------------------------

void Arranger::_setRaster(int index)
{
	static int rasterTable[] = {
		1, 0, 768, 384, 192, 96
	};
	_raster = rasterTable[index];
	// Set the audio record part snapping.
	song->setArrangerRaster(_raster);
	canvas->redraw();
}

//---------------------------------------------------------
//   reset
//---------------------------------------------------------

void Arranger::reset()
{
	canvas->setXPos(0);
	canvas->setYPos(0);
	hscroll->setPos(0);
	vscroll->setValue(0);
	time->setXPos(0);
	time->setYPos(0);
}

//---------------------------------------------------------
//   cmd
//---------------------------------------------------------

void Arranger::cmd(int cmd)
{
	int ncmd;
	switch (cmd)
	{
		case CMD_CUT_PART:
			ncmd = PartCanvas::CMD_CUT_PART;
			break;
		case CMD_COPY_PART:
			ncmd = PartCanvas::CMD_COPY_PART;
			break;
		case CMD_PASTE_PART:
			ncmd = PartCanvas::CMD_PASTE_PART;
			break;
		case CMD_PASTE_CLONE_PART:
			ncmd = PartCanvas::CMD_PASTE_CLONE_PART;
			break;
		case CMD_PASTE_PART_TO_TRACK:
			ncmd = PartCanvas::CMD_PASTE_PART_TO_TRACK;
			break;
		case CMD_PASTE_CLONE_PART_TO_TRACK:
			ncmd = PartCanvas::CMD_PASTE_CLONE_PART_TO_TRACK;
			break;
		case CMD_INSERT_PART:
			ncmd = PartCanvas::CMD_INSERT_PART;
			break;
		case CMD_INSERT_EMPTYMEAS:
			ncmd = PartCanvas::CMD_INSERT_EMPTYMEAS;
			break;
		default:
			return;
	}
	canvas->cmd(ncmd);
}

//---------------------------------------------------------
//   globalPitchChanged
//---------------------------------------------------------

void Arranger::globalPitchChanged(int val)
{
	song->setGlobalPitchShift(val);
}

//---------------------------------------------------------
//   globalTempoChanged
//---------------------------------------------------------

void Arranger::globalTempoChanged(int val)
{
	audio->msgSetGlobalTempo(val);
	song->tempoChanged();
}

//---------------------------------------------------------
//   setTempo50
//---------------------------------------------------------

void Arranger::setTempo50()
{
	setGlobalTempo(50);
}

//---------------------------------------------------------
//   setTempo100
//---------------------------------------------------------

void Arranger::setTempo100()
{
	setGlobalTempo(100);
}

//---------------------------------------------------------
//   setTempo200
//---------------------------------------------------------

void Arranger::setTempo200()
{
	setGlobalTempo(200);
}

//---------------------------------------------------------
//   setGlobalTempo
//---------------------------------------------------------

void Arranger::setGlobalTempo(int val)
{
	if (val != globalTempoSpinBox->value())
		globalTempoSpinBox->setValue(val);
}

//---------------------------------------------------------
//   verticalScrollSetYpos
//---------------------------------------------------------

void Arranger::verticalScrollSetYpos(unsigned ypos)
{
	vscroll->setValue(ypos);
}

//---------------------------------------------------------
//   trackInfoScroll
//---------------------------------------------------------

void Arranger::trackInfoScroll(int)
{
	//if (trackInfo->visibleWidget())
	//	trackInfo->visibleWidget()->move(0, -y);
}

//---------------------------------------------------------
//   clear
//---------------------------------------------------------

void Arranger::clear()
{
	selected = 0;
	midiTrackInfo->setTrack(0);
        foreach(Strip* strip, m_strips)
        {
                delete strip;
        }
        m_strips.clear();
        _lastStrip = 0;
        if (canvas)
        {
                canvas->setCurrentPart(0);
        }
}

void Arranger::wheelEvent(QWheelEvent* ev)
{
	emit redirectWheelEvent(ev);
}

void Arranger::controllerChanged(Track *t)
{
	canvas->controllerChanged(t);
}

//---------------------------------------------------------
//   showTrackInfo
//---------------------------------------------------------

void Arranger::showTrackInfo(bool)
{
	//showTrackinfoFlag = flag;
	//trackInfo->setVisible(flag);
	//infoScroll->setVisible(flag);
	updateTrackInfo(-1);
}

//---------------------------------------------------------
//   genTrackInfo
//---------------------------------------------------------

void Arranger::genTrackInfo(QWidget*)
{
	midiTrackInfo = new MidiTrackInfo(this);
	_tvdock = new TrackViewDock(this);
	//infoScroll->setWidget(midiTrackInfo);
	infoScroll->setWidgetResizable(true);
	_rmdock = new RouteMapDock(this);
	_rtabs->addTab(_tvdock, tr("   Views   "));
	_rtabs->addTab(mixerScroll, tr("   Mixer   "));
	_rtabs->addTab(midiTrackInfo, tr("   Patch Sequencer   "));
	//_rtabs->addTab(infoScroll, tr("   Patch Sequencer   "));
	_rtabs->addTab(_rmdock, tr("  Route Connector  "));

	central = new QWidget(this);
	central->setObjectName("dockMixerCenter");
	mlayout = new QVBoxLayout();
	central->setLayout(mlayout);
	mlayout->setSpacing(0);
	mlayout->setContentsMargins(0, 0, 0, 0);
	mlayout->setSpacing(0);
	mlayout->setAlignment(Qt::AlignHCenter);
	mixerScroll->setWidget(central);
	mixerScroll->setWidgetResizable(true);
}

//---------------------------------------------------------
//   updateTrackInfo
//---------------------------------------------------------

void Arranger::updateTrackInfo(int flags)
{
	if (!showTrackinfoFlag)
	{
		switchInfo(-1);
		return;
	}
	if (selected == 0)
	{
		return;
		//switchInfo(0);
	}
	if (selected->isMidiTrack())
        {
		if ((flags & SC_SELECTION) || (flags & SC_TRACK_REMOVED))
			switchInfo(2);
                // If a new part was selected, and only if it's different.
		if ((flags & SC_SELECTION) && midiTrackInfo->track() != selected)
			// Set a new track and do a complete update.
			midiTrackInfo->setTrack(selected);
		else
			// Otherwise just regular update with specific flags.
			midiTrackInfo->updateTrackInfo(flags);
        }
	else
	{
		if ((flags & SC_SELECTION) || (flags & SC_TRACK_REMOVED))
			switchInfo(2);
	}
}

//---------------------------------------------------------
//   switchInfo
//---------------------------------------------------------

void Arranger::switchInfo(int n)/*{{{*/
{
	bool chview = false;
	if(selected && n == 2)
	{
		Strip* w = 0;
		
		QLayoutItem* item = mlayout->takeAt(0);
		if(item) {
			Strip* strip = (Strip*)item->widget();
			if(strip && (strip->getTrack()->isMidiTrack() && !selected->isMidiTrack() && _rtabs->currentIndex() == 2))
				chview = true;
			m_strips.removeAll(strip);
			delete item;
		}
		if(_lastStrip)
		{
		        m_strips.removeAll(_lastStrip);
			delete _lastStrip;
			_lastStrip = 0;
		}
		if(selected->isMidiTrack())
		{
		 	_rtabs->setTabEnabled(1, true);
		 	_rtabs->setTabEnabled(2, true);
		 	//_rtabs->setCurrentIndex(2);
		 	w = new MidiStrip(central, (MidiTrack*) selected);
		}
		else
		{
		 	_rtabs->setTabEnabled(2, false);
		 	_rtabs->setTabEnabled(1, true);
		 	if(chview)
				_rtabs->setCurrentIndex(1);
		 	w = new AudioStrip(central, (AudioTrack*) selected);
		}
		switch (selected->type())//{{{
		{
		 	case Track::AUDIO_OUTPUT:
		 	    w->setObjectName("MixerAudioOutStrip");
		 	    break;
		 	case Track::AUDIO_BUSS:
		 	    w->setObjectName("MixerAudioBussStrip");
		 	    break;
		 	case Track::AUDIO_AUX:
		 	    w->setObjectName("MixerAuxStrip");
		 	    break;
		 	case Track::WAVE:
		 	    w->setObjectName("MixerWaveStrip");
		 	    break;
		 	case Track::AUDIO_INPUT:
		 	    w->setObjectName("MixerAudioInStrip");
		 	    break;
		 	case Track::AUDIO_SOFTSYNTH:
		 	    w->setObjectName("MixerSynthStrip");
		 	    break;
		 	case Track::MIDI:
			{
		 	    w->setObjectName("MidiTrackStrip");
		 		break;
			}
		 	case Track::DRUM:
		 	{
		 	    w->setObjectName("MidiDrumTrackStrip");
		 		break;
		 	}
		 	break;
		}//}}}
		if (w)
		{
			connect(song, SIGNAL(songChanged(int)), w, SLOT(songChanged(int)));
			if(!selected->isMidiTrack())
				connect(oom, SIGNAL(configChanged()), w, SLOT(configChanged()));
			mlayout->addWidget(w);
			m_strips.append(w);
			w->show();
			_lastStrip = w;
		}
	}
	else
	{
		printf("Arranger::switchInfo(int %d)\n", n);
		 _rtabs->setTabEnabled(2, false);
		 _rtabs->setTabEnabled(1, false);
		 _rtabs->setTabEnabled(0, true);
		 _rtabs->setCurrentIndex(0);
	}
}/*}}}*/

void Arranger::preloadControllers()
{
	QApplication::setOverrideCursor(QCursor(Qt::WaitCursor));
	audio->preloadControllers();
	QApplication::restoreOverrideCursor();
}

