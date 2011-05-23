//=========================================================
//  OOMidi
//  OpenOctave Midi and Audio Editor
//  $Id: $
//
//  (C) Copyright 2011 Andrew Williams and Christopher Cherrett
//=========================================================

#include "track.h"
#include "ccinfo.h"

CCInfo::CCInfo(Track* t, int port, int chan, int control, int cc, int rec, int toggle)
{
	m_track = t;
	m_port = port;
	m_channel = chan;
	m_control = control;
	m_ccnum = cc;
	m_reconly = rec;
	m_faketoggle = toggle;
}

CCInfo::CCInfo()
{
	m_track = 0;
	m_port = 0;
	m_channel = 0;
	m_control = 0;
	m_ccnum = 0;
	m_reconly = 0;
	m_faketoggle = 0;
}

CCInfo::CCInfo(const CCInfo& i)
 : QObject(0)
,m_track(i.m_track)
,m_port(i.m_port)
,m_channel(i.m_channel)
,m_control(i.m_control)
,m_ccnum(i.m_ccnum)
,m_reconly(i.m_reconly)
,m_faketoggle(i.m_faketoggle)
{
}

CCInfo::~CCInfo()
{
}