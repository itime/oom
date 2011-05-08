//=========================================================
//  OOMidi
//  OpenOctave Midi and Audio Editor
//  $Id: track.cpp,v 1.34.2.11 2009/11/30 05:05:49 terminator356 Exp $
//
//  (C) Copyright 2000-2004 Werner Schweer (ws@seh.de)
//=========================================================

#include <QStringList>
#include "track.h"
#include "event.h"
#include "midictrl.h"
#include "mididev.h"
#include "midiport.h"
#include "song.h"
#include "xml.h"
#include "plugin.h"
#include "drummap.h"
#include "audio.h"
#include "globaldefs.h"
#include "route.h"

unsigned int Track::_soloRefCnt = 0;
Track* Track::_tmpSoloChainTrack = 0;
bool Track::_tmpSoloChainDoIns = false;
bool Track::_tmpSoloChainNoDec = false;

const char* Track::_cname[] = {
	"Midi", "Drum", "Wave", "AudioOut", "AudioIn", "AudioBuss",
	"AudioAux", "AudioSynth"
};

//---------------------------------------------------------
//   addPortCtrlEvents
//---------------------------------------------------------

void addPortCtrlEvents(MidiTrack* t)
{
	const PartList* pl = t->cparts();
	for (ciPart ip = pl->begin(); ip != pl->end(); ++ip)
	{
		Part* part = ip->second;
		const EventList* el = part->cevents();
		unsigned len = part->lenTick();
		for (ciEvent ie = el->begin(); ie != el->end(); ++ie)
		{
			const Event& ev = ie->second;
			// Added by T356. Do not add events which are past the end of the part.
			if (ev.tick() >= len)
				break;

			if (ev.type() == Controller)
			{
				int tick = ev.tick() + part->tick();
				int cntrl = ev.dataA();
				int val = ev.dataB();
				int ch = t->outChannel();

				MidiPort* mp = &midiPorts[t->outPort()];
				// Is it a drum controller event, according to the track port's instrument?
				if (t->type() == Track::DRUM)
				{
					MidiController* mc = mp->drumController(cntrl);
					if (mc)
					{
						int note = cntrl & 0x7f;
						cntrl &= ~0xff;
						ch = drumMap[note].channel;
						mp = &midiPorts[drumMap[note].port];
						cntrl |= drumMap[note].anote;
					}
				}

				mp->setControllerVal(ch, tick, cntrl, val, part);
			}
		}
	}
}

//---------------------------------------------------------
//   removePortCtrlEvents
//---------------------------------------------------------

void removePortCtrlEvents(MidiTrack* t)
{
	const PartList* pl = t->cparts();
	for (ciPart ip = pl->begin(); ip != pl->end(); ++ip)
	{
		Part* part = ip->second;
		const EventList* el = part->cevents();
		//unsigned len = part->lenTick();
		for (ciEvent ie = el->begin(); ie != el->end(); ++ie)
		{
			const Event& ev = ie->second;
			// Added by T356. Do not remove events which are past the end of the part.
			// No, actually, do remove ALL of them belonging to the part.
			// Just in case there are stray values left after the part end.
			//if(ev.tick() >= len)
			//  break;

			if (ev.type() == Controller)
			{
				int tick = ev.tick() + part->tick();
				int cntrl = ev.dataA();
				int ch = t->outChannel();

				MidiPort* mp = &midiPorts[t->outPort()];
				// Is it a drum controller event, according to the track port's instrument?
				if (t->type() == Track::DRUM)
				{
					MidiController* mc = mp->drumController(cntrl);
					if (mc)
					{
						int note = cntrl & 0x7f;
						cntrl &= ~0xff;
						ch = drumMap[note].channel;
						mp = &midiPorts[drumMap[note].port];
						cntrl |= drumMap[note].anote;
					}
				}

				mp->deleteController(ch, tick, cntrl, part);
			}
		}
	}
}

//---------------------------------------------------------
//   y
//---------------------------------------------------------

int Track::y() const
{
	TrackList* tl = song->visibletracks();
	int yy = 0;
	for (ciTrack it = tl->begin(); it != tl->end(); ++it)
	{
		if (this == *it)
			return yy;
		yy += (*it)->height();
	}
	printf("Track::y(%s): track not in tracklist\n", name().toLatin1().constData());
	return -1;
}

//---------------------------------------------------------
//   Track::init
//---------------------------------------------------------

void Track::init()
{
	_activity = 0;
	_lastActivity = 0;
	_recordFlag = false;
	_mute = false;
	_solo = false;
	_internalSolo = 0;
	_off = false;
	_channels = 0; // 1 - mono, 2 - stereo
	_reminder1 = false;
	_reminder2 = false;
	_reminder3 = false;

	_volumeEnCtrl = true;
	_volumeEn2Ctrl = true;
	_panEnCtrl = true;
	_panEn2Ctrl = true;
	m_midiassign.port = -1;
	m_midiassign.channel = -1;
	m_midiassign.enabled = false;

	_selected = false;
	_height = 40;
	_locked = false;
	for (int i = 0; i < MAX_CHANNELS; ++i)
	{
		//_meter[i] = 0;
		//_peak[i]  = 0;
		_meter[i] = 0.0;
		_peak[i] = 0.0;
	}
	m_midiassign.enabled = false;
	m_midiassign.port = 0;
	m_midiassign.channel = 0;
	m_midiassign.track = this;
	m_midiassign.midimap.clear();
	m_midiassign.midimap[CTRL_VOLUME] = -1;
	m_midiassign.midimap[CTRL_PANPOT] = -1;
	m_midiassign.midimap[CTRL_REVERB_SEND] = -1;
	m_midiassign.midimap[CTRL_CHORUS_SEND] = -1;
	m_midiassign.midimap[CTRL_VARIATION_SEND] = -1;
	m_midiassign.midimap[CTRL_RECORD] = -1;
	m_midiassign.midimap[CTRL_MUTE] = -1;
	m_midiassign.midimap[CTRL_SOLO] = -1;
}

Track::Track(Track::TrackType t)
{
	init();
	_type = t;
}

//Track::Track(const Track& t)

Track::Track(const Track& t, bool cloneParts)
{
	_activity = t._activity;
	_lastActivity = t._lastActivity;
	_recordFlag = t._recordFlag;
	_mute = t._mute;
	_solo = t._solo;
	_internalSolo = t._internalSolo;
	_off = t._off;
	_channels = t._channels;

	_volumeEnCtrl = t._volumeEnCtrl;
	_volumeEn2Ctrl = t._volumeEn2Ctrl;
	_panEnCtrl = t._panEnCtrl;
	_panEn2Ctrl = t._panEn2Ctrl;

	_selected = t.selected();
	_y = t._y;
	_height = t._height;
	_comment = t.comment();
	_name = t.name();
	_type = t.type();
	_locked = t.locked();
	m_midiassign = t.m_midiassign;

	if (cloneParts)
	{
		const PartList* pl = t.cparts();
		for (ciPart ip = pl->begin(); ip != pl->end(); ++ip)
		{
			Part* newPart = ip->second->clone();
			newPart->setTrack(this);
			_parts.add(newPart);
		}
	}
	else
	{
		_parts = *(t.cparts());
		// NOTE: We can't do this because of the way clipboard, cloneList, and undoOp::ModifyTrack, work.
		// A couple of schemes were conceived to deal with cloneList being invalid, but the best way is
		//  to not alter the part list here. It's a big headache because: Either the parts in the cloneList
		//  need to be reliably looked up replaced with the new ones, or the clipboard and cloneList must be cleared.
		// Fortunately the ONLY part of oom using this function is track rename (in TrackList and TrackInfo).
		// So we can get away with leaving this out:
		//for (iPart ip = _parts.begin(); ip != _parts.end(); ++ip)
		//      ip->second->setTrack(this);
	}

	for (int i = 0; i < MAX_CHANNELS; ++i)
	{
		//_meter[i] = 0;
		//_peak[i]  = 0;
		_meter[i] = 0.0;
		_peak[i] = 0.0;
	}
}

//---------------------------------------------------------
//   operator =
//   Added by Tim. Parts' track members MUST point to this track, 
//    not some other track, so simple assignment operator won't do!
//---------------------------------------------------------

Track& Track::operator=(const Track& t)
{
	_activity = t._activity;
	_lastActivity = t._lastActivity;
	_recordFlag = t._recordFlag;
	_mute = t._mute;
	_solo = t._solo;
	_internalSolo = t._internalSolo;
	_off = t._off;
	_channels = t._channels;

	_volumeEnCtrl = t._volumeEnCtrl;
	_volumeEn2Ctrl = t._volumeEn2Ctrl;
	_panEnCtrl = t._panEnCtrl;
	_panEn2Ctrl = t._panEn2Ctrl;

	_selected = t.selected();
	_y = t._y;
	_height = t._height;
	_comment = t.comment();
	_name = t.name();
	_type = t.type();
	_locked = t.locked();

	_parts = *(t.cparts());
	// NOTE: Can't do this. See comments in copy constructor.
	//for (iPart ip = _parts.begin(); ip != _parts.end(); ++ip)
	//      ip->second->setTrack(this);

	for (int i = 0; i < MAX_CHANNELS; ++i)
	{
		_meter[i] = t._meter[i];
		_peak[i] = t._peak[i];
	}
	return *this;
}

//---------------------------------------------------------
//   setDefaultName
//    generate unique name for track
//---------------------------------------------------------

void Track::setDefaultName()
{
	QString base;
	switch (_type)
	{
		case MIDI:
			base = QString("Midi");
			break;
		case DRUM:
			base = QString("Drum");
			break;
		case WAVE:
			base = QString("Audio");
			break;
		case AUDIO_OUTPUT:
			base = QString("Out");
			break;
		case AUDIO_BUSS:
			base = QString("Buss");
			break;
		case AUDIO_AUX:
			base = QString("Aux");
			break;
		case AUDIO_INPUT:
			base = QString("Input");
			break;
		case AUDIO_SOFTSYNTH:
			base = QString("Synth");
			break;
	};
	base += " ";
	for (int i = 1; true; ++i)
	{
		QString n;
		n.setNum(i);
		QString s = base + n;
		Track* track = song->findTrack(s);
		if (track == 0)
		{
			setName(s);
			break;
		}
	}
}

void Track::deselectParts()
{
	for (iPart ip = parts()->begin(); ip != parts()->end(); ++ip)
	{
		Part* p = ip->second;
		p->setSelected(false);
	}
}

//---------------------------------------------------------
//   clearRecAutomation
//---------------------------------------------------------

void Track::clearRecAutomation(bool clearList)
{
	_volumeEnCtrl = true;
	_volumeEn2Ctrl = true;
	_panEnCtrl = true;
	_panEn2Ctrl = true;

	if (isMidiTrack())
		return;

	AudioTrack *t = (AudioTrack*)this;
	Pipeline *pl = t->efxPipe();
	PluginI *p;
	for (iPluginI i = pl->begin(); i != pl->end(); ++i)
	{
		p = *i;
		if (!p)
			continue;
		p->enableAllControllers(true);
	}

	if (clearList)
		t->recEvents()->clear();
}

//---------------------------------------------------------
//   dump
//---------------------------------------------------------

void Track::dump() const
{
	printf("Track <%s>: typ %d, parts %zd sel %d\n",
			_name.toLatin1().constData(), _type, _parts.size(), _selected);
}

//---------------------------------------------------------
//   MidiTrack
//---------------------------------------------------------

MidiTrack::MidiTrack()
: Track(MIDI)
{
	init();
	_events = new EventList;
	_mpevents = new MPEventList;
}

//MidiTrack::MidiTrack(const MidiTrack& mt)
//   : Track(mt)

MidiTrack::MidiTrack(const MidiTrack& mt, bool cloneParts)
: Track(mt, cloneParts)
{
	_outPort = mt.outPort();
	_outChannel = mt.outChannel();
	///_inPortMask    = mt.inPortMask();
	///_inChannelMask = mt.inChannelMask();
	_events = new EventList;
	_mpevents = new MPEventList;
	transposition = mt.transposition;
	transpose = mt.transpose;
	velocity = mt.velocity;
	delay = mt.delay;
	len = mt.len;
	compression = mt.compression;
	_recEcho = mt.recEcho();
}

MidiTrack::~MidiTrack()
{
	delete _events;
	delete _mpevents;
}

//---------------------------------------------------------
//   init
//---------------------------------------------------------

void MidiTrack::init()
{
	_outPort = 0;
	_outChannel = 0;

	transposition = 0;
	velocity = 0;
	delay = 0;
	len = 100; // percent
	compression = 100; // percent
	_recEcho = true;
	transpose = false;
}

int MidiTrack::getTransposition()
{
	if(transpose)
	{
		return transposition;
	}
	return 0;
}

//---------------------------------------------------------
//   setOutChanAndUpdate
//---------------------------------------------------------

void MidiTrack::setOutChanAndUpdate(int i)
{
	if (_outChannel == i)
		return;

	removePortCtrlEvents(this);
	_outChannel = i;
	addPortCtrlEvents(this);
}

//---------------------------------------------------------
//   setOutPortAndUpdate
//---------------------------------------------------------

void MidiTrack::setOutPortAndUpdate(int i)
{
	if (_outPort == i)
		return;

	removePortCtrlEvents(this);
	_outPort = i;
	addPortCtrlEvents(this);
}

//---------------------------------------------------------
//   setInPortAndChannelMask
//   For old song files with port mask (max 32 ports) and channel mask (16 channels), 
//    before midi routing was added (the iR button). p3.3.48
//---------------------------------------------------------

void MidiTrack::setInPortAndChannelMask(unsigned int portmask, int chanmask)
{
	bool changed = false;

	for (int port = 0; port < 32; ++port) // 32 is the old maximum number of ports.
	{
		// p3.3.50 If the port was not used in the song file to begin with, just ignore it.
		// This saves from having all of the first 32 ports' channels connected.
		if (!midiPorts[port].foundInSongFile())
			continue;

		Route aRoute(port, chanmask); // p3.3.50
		Route bRoute(this, chanmask);

		if (portmask & (1 << port)) // p3.3.50
		{
			audio->msgAddRoute(aRoute, bRoute);
			changed = true;
		}
		else
		{
			audio->msgRemoveRoute(aRoute, bRoute);
			changed = true;
		}
	}

	if (changed)
	{
		audio->msgUpdateSoloStates();
		song->update(SC_ROUTE);
	}
}

//---------------------------------------------------------
//   addPart
//---------------------------------------------------------

iPart Track::addPart(Part* p)
{
	p->setTrack(this);
	return _parts.add(p);
}

//---------------------------------------------------------
//   findPart
//---------------------------------------------------------

Part* Track::findPart(unsigned tick)
{
	for (iPart i = _parts.begin(); i != _parts.end(); ++i)
	{
		Part* part = i->second;
		if (tick >= part->tick() && tick < (part->tick() + part->lenTick()))
			return part;
	}
	return 0;
}

//---------------------------------------------------------
//   newPart
//---------------------------------------------------------

Part* MidiTrack::newPart(Part*p, bool clone)
{
	MidiPart* part = clone ? new MidiPart(this, p->events()) : new MidiPart(this);
	if (p)
	{
		part->setName(p->name());
		part->setColorIndex(p->colorIndex());

		*(PosLen*) part = *(PosLen*) p;
		part->setMute(p->mute());
	}

	if (clone)
		//p->chainClone(part);
		chainClone(p, part);

	return part;
}

//---------------------------------------------------------
//   automationType
//---------------------------------------------------------

AutomationType MidiTrack::automationType() const
{
	MidiPort* port = &midiPorts[outPort()];
	return port->automationType(outChannel());
}

//---------------------------------------------------------
//   setAutomationType
//---------------------------------------------------------

void MidiTrack::setAutomationType(AutomationType t)
{
	MidiPort* port = &midiPorts[outPort()];
	port->setAutomationType(outChannel(), t);
}

bool MidiTrack::setRecordFlag1(bool f, bool monitor)
{
    _recordFlag = f;
	if(!monitor)
	{
		//Call the monitor here if it was not called from the monitor
		//midimonitor->msgSendMidiOutputEvent((Track*)this, CTRL_SOLO, f ? 127 : 0);
	}
    return true;
}

void MidiTrack::setRecordFlag2(bool, bool)
{
}

//---------------------------------------------------------
//   Track::writeProperties
//---------------------------------------------------------

void Track::writeProperties(int level, Xml& xml) const/*{{{*/
{
	xml.strTag(level, "name", _name);
	if (!_comment.isEmpty())
		xml.strTag(level, "comment", _comment);
	xml.intTag(level, "record", _recordFlag);
	xml.intTag(level, "mute", mute());
	xml.intTag(level, "solo", solo());
	xml.intTag(level, "off", off());
	xml.intTag(level, "channels", _channels);
	xml.intTag(level, "height", _height);
	xml.intTag(level, "locked", _locked);
	xml.intTag(level, "reminder1", _reminder1);
	xml.intTag(level, "reminder2", _reminder2);
	xml.intTag(level, "reminder3", _reminder3);
	if (_selected)
		xml.intTag(level, "selected", _selected);
	xml.nput(level, "<MidiAssign port=\"%d\"", m_midiassign.port);/*{{{*/
	xml.nput(" channel=\"%d\"", m_midiassign.channel);
	xml.nput(" enabled=\"%d\"", (int)m_midiassign.enabled);
	QString assign;
	QHashIterator<int, int> iter(m_midiassign.midimap);
	while(iter.hasNext())
	{
		iter.next();
		assign.append(QString::number(iter.key())).append(":").append(QString::number(iter.value())).append(" ");
		//xml.nput(" %d=\"%d\"", iter.key(), iter.value());
	}
	xml.nput(" midimap=\"%s\"", assign.toUtf8().constData());
	xml.put(" />");/*}}}*/
}/*}}}*/

//---------------------------------------------------------
//   Track::readProperties
//---------------------------------------------------------

bool Track::readProperties(Xml& xml, const QString& tag)/*{{{*/
{
	if (tag == "name")
		_name = xml.parse1();
	else if (tag == "comment")
		_comment = xml.parse1();
	else if (tag == "record")
	{
		bool recordFlag = xml.parseInt();
		setRecordFlag1(recordFlag);
		setRecordFlag2(recordFlag);
	}
	else if (tag == "mute")
		_mute = xml.parseInt();
	else if (tag == "solo")
		_solo = xml.parseInt();
	else if (tag == "off")
		_off = xml.parseInt();
	else if (tag == "height")
		_height = xml.parseInt();
	else if (tag == "channels")
	{
		_channels = xml.parseInt();
		if (_channels > MAX_CHANNELS)
			_channels = MAX_CHANNELS;
	}
	else if (tag == "locked")
		_locked = xml.parseInt();
	else if (tag == "selected")
		_selected = xml.parseInt();
	else if (tag == "reminder1")
		_reminder1 = (bool)xml.parseInt();
	else if (tag == "reminder2")
		_reminder2 = (bool)xml.parseInt();
	else if (tag == "reminder3")
		_reminder3 = (bool)xml.parseInt();
	else if(tag == "MidiAssign")
		m_midiassign.read(xml, (Track*)this);
	else
		return true;
	return false;
}/*}}}*/

//---------------------------------------------------------
//   writeRouting
//---------------------------------------------------------

void Track::writeRouting(int level, Xml& xml) const/*{{{*/
{
	QString s;

	if (type() == Track::AUDIO_INPUT)
	{
		const RouteList* rl = &_inRoutes;
		for (ciRoute r = rl->begin(); r != rl->end(); ++r)
		{
			if (!r->name().isEmpty())
			{
				s = QT_TRANSLATE_NOOP("@default", "Route");
				if (r->channel != -1)
					s += QString(QT_TRANSLATE_NOOP("@default", " channel=\"%1\"")).arg(r->channel);

				xml.tag(level++, s.toAscii().constData());

				s = QT_TRANSLATE_NOOP("@default", "source");
				if (r->type != Route::TRACK_ROUTE)
					s += QString(QT_TRANSLATE_NOOP("@default", " type=\"%1\"")).arg(r->type);
				s += QString(QT_TRANSLATE_NOOP("@default", " name=\"%1\"/")).arg(Xml::xmlString(r->name()));
				xml.tag(level, s.toAscii().constData());

				xml.tag(level, "dest name=\"%s\"/", Xml::xmlString(name()).toLatin1().constData());

				xml.etag(level--, "Route");
			}
		}
	}

	const RouteList* rl = &_outRoutes;
	for (ciRoute r = rl->begin(); r != rl->end(); ++r)
	{
		if (r->midiPort != -1 || !r->name().isEmpty()) // p3.3.49
		{
			s = QT_TRANSLATE_NOOP("@default", "Route");
			if (r->type == Route::MIDI_PORT_ROUTE) // p3.3.50
			{
				if (r->channel != -1 && r->channel != 0)
					s += QString(QT_TRANSLATE_NOOP("@default", " channelMask=\"%1\"")).arg(r->channel); // Use new channel mask.
			}
			else
			{
				if (r->channel != -1)
					s += QString(QT_TRANSLATE_NOOP("@default", " channel=\"%1\"")).arg(r->channel);
			}
			if (r->channels != -1)
				s += QString(QT_TRANSLATE_NOOP("@default", " channels=\"%1\"")).arg(r->channels);
			if (r->remoteChannel != -1)
				s += QString(QT_TRANSLATE_NOOP("@default", " remch=\"%1\"")).arg(r->remoteChannel);

			xml.tag(level++, s.toAscii().constData());

			xml.tag(level, "source name=\"%s\"/", Xml::xmlString(name()).toLatin1().constData());

			s = QT_TRANSLATE_NOOP("@default", "dest");

			if (r->type != Route::TRACK_ROUTE && r->type != Route::MIDI_PORT_ROUTE)
				s += QString(QT_TRANSLATE_NOOP("@default", " type=\"%1\"")).arg(r->type);

			if (r->type == Route::MIDI_PORT_ROUTE) // p3.3.49
				s += QString(QT_TRANSLATE_NOOP("@default", " mport=\"%1\"/")).arg(r->midiPort);
			else
				s += QString(QT_TRANSLATE_NOOP("@default", " name=\"%1\"/")).arg(Xml::xmlString(r->name()));

			xml.tag(level, s.toAscii().constData());

			xml.etag(level--, "Route");
		}
	}
}/*}}}*/

//---------------------------------------------------------
//   MidiTrack::write
//---------------------------------------------------------

void MidiTrack::write(int level, Xml& xml) const/*{{{*/
{
	const char* tag;

	if (type() == DRUM)
		tag = "drumtrack";
	else
		tag = "miditrack";
	xml.tag(level++, tag);
	Track::writeProperties(level, xml);

	xml.intTag(level, "device", outPort());
	xml.intTag(level, "channel", outChannel());
	xml.intTag(level, "locked", _locked);
	xml.intTag(level, "echo", _recEcho);

	xml.intTag(level, "transposition", transposition);
	xml.intTag(level, "transpose", transpose);
	xml.intTag(level, "velocity", velocity);
	xml.intTag(level, "delay", delay);
	xml.intTag(level, "len", len);
	xml.intTag(level, "compression", compression);
	xml.intTag(level, "automation", int(automationType()));

	const PartList* pl = cparts();
	for (ciPart p = pl->begin(); p != pl->end(); ++p)
		p->second->write(level, xml);
	xml.etag(level, tag);
}/*}}}*/

//---------------------------------------------------------
//   MidiTrack::read
//---------------------------------------------------------

void MidiTrack::read(Xml& xml)/*{{{*/
{
	unsigned int portmask = 0;
	int chanmask = 0;

	for (;;)
	{
		Xml::Token token = xml.parse();
		const QString& tag = xml.s1();
		switch (token)
		{
			case Xml::Error:
			case Xml::End:
				return;
			case Xml::TagStart:
				if (tag == "transposition")
					transposition = xml.parseInt();
				else if (tag == "transpose")
					transpose = (bool)xml.parseInt();
				else if (tag == "velocity")
					velocity = xml.parseInt();
				else if (tag == "delay")
					delay = xml.parseInt();
				else if (tag == "len")
					len = xml.parseInt();
				else if (tag == "compression")
					compression = xml.parseInt();
				else if (tag == "part")
				{
					Part* p = 0;
					p = readXmlPart(xml, this);
					if (p)
						parts()->add(p);
				}
				else if (tag == "device")
					setOutPort(xml.parseInt());
				else if (tag == "channel")
					setOutChannel(xml.parseInt());
				else if (tag == "inportMap")
					portmask = xml.parseUInt(); // p3.3.48: Support old files.
				else if (tag == "inchannelMap")
					chanmask = xml.parseInt(); // p3.3.48: Support old files.
				else if (tag == "locked")
					_locked = xml.parseInt();
				else if (tag == "echo")
					_recEcho = xml.parseInt();
				else if (tag == "automation")
					setAutomationType(AutomationType(xml.parseInt()));
				else if (Track::readProperties(xml, tag))
				{
					// version 1.0 compatibility:
					if (tag == "track" && xml.majorVersion() == 1 && xml.minorVersion() == 0)
						break;
					xml.unknown("MidiTrack");
				}
				break;
			case Xml::Attribut:
				break;
			case Xml::TagEnd:
				if (tag == "miditrack" || tag == "drumtrack")
				{
					setInPortAndChannelMask(portmask, chanmask); // p3.3.48: Support old files.
					return;
				}
			default:
				break;
		}
	}
}/*}}}*/

void MidiAssignData::read(Xml& xml, Track* t)
{
	enabled = false;
	port = 0;
	channel = 0;
	track = t;
	midimap.clear();
	midimap[CTRL_VOLUME] = -1;
	midimap[CTRL_PANPOT] = -1;
	midimap[CTRL_REVERB_SEND] = -1;
	midimap[CTRL_CHORUS_SEND] = -1;
	midimap[CTRL_VARIATION_SEND] = -1;
	midimap[CTRL_RECORD] = -1;
	midimap[CTRL_MUTE] = -1;
	midimap[CTRL_SOLO] = -1;
	for (;;)/*{{{*/
	{
		Xml::Token token = xml.parse();
		const QString& tag = xml.s1();
		switch (token)
		{
			case Xml::Error:
			case Xml::End:
				return;
			case Xml::Attribut:
			{
				QString s = xml.s2();
				if (tag == "port")
					port = xml.s2().toInt();
				else if (tag == "channel")
					channel = xml.s2().toInt();
				else if(tag == "enabled")
					enabled = (bool)xml.s2().toInt();
				else if (tag == "midimap")
				{
					QStringList vals = xml.s2().split(" ", QString::SkipEmptyParts);
					foreach(QString ccpair, vals)
					{
						QStringList cclist = ccpair.split(":", QString::SkipEmptyParts);
						if(cclist.size() == 2)
						{
							midimap[cclist[0].toInt()] = cclist[1].toInt();
						}
					}
				}
			}
				break;
			case Xml::TagStart:
				xml.unknown("MidiAssign");
				break;
			case Xml::TagEnd:
				if (tag == "MidiAssign")
				{
					return;
				}
			default:
				break;
		}
	}/*}}}*/
}

