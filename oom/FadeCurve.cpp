//===========================================================
//  OOMidi
//  OpenOctave Midi and Audio Editor
//  (C) Copyright 2011 Andrew Williams & Christopher Cherrett
//===========================================================

#include "FadeCurve.h"
#include "part.h"

FadeCurve::FadeCurve(CurveType type, CurveMode mode, WavePart* p, QObject* parent)
: QObject(parent)
{
	m_type = type;
	m_mode = mode;
	m_part = p;
	m_active = false;
	m_width = 0;
}

FadeCurve::~FadeCurve()
{
}

bool FadeCurve::active()
{
	return m_active;
}

void FadeCurve::setActive(bool act)
{
	if(m_active != act)
	{
		m_active = act;
		emit stateChanged();
	}
}

void FadeCurve::setWidth(long width)
{
	if(width > m_part->lenFrame())
	{
		m_width = m_part->lenFrame();
		return;
	}
	else if(width < 0)
	{
		m_width = 0;
		return;
	}
	m_width = width;
}

long FadeCurve::width()
{
	return m_width;
}
