//=========================================================
//  OOMidi
//  OpenOctave Midi and Audio Editor
//  $Id: audio.cpp,v 1.59.2.30 2009/12/20 05:00:35 terminator356 Exp $
//
//  (C) Copyright 2001-2004 Werner Schweer (ws@seh.de)
//=========================================================

#include <cmath>
#include <errno.h>

#include <QSocketNotifier>

#include "app.h"
#include "song.h"
#include "node.h"
#include "audiodev.h"
//#include "driver/audiodev.h"   // p4.0.2
#include "mididev.h"
#include "alsamidi.h"
//#include "driver/alsamidi.h"   // p4.0.2
#include "audioprefetch.h"
#include "plugin.h"
#include "audio.h"
#include "wave.h"
#include "midictrl.h"
#include "midiseq.h"
#include "sync.h"
#include "midi.h"
#include "event.h"
#include "gconfig.h"
#include "pos.h"
#include "ticksynth.h"

extern double curTime();
Audio* audio;
AudioDevice* audioDevice; // current audio device in use

// p3.3.25
extern unsigned int volatile midiExtSyncTicks;


//static const unsigned char mmcDeferredPlayMsg[] = { 0x7f, 0x7f, 0x06, 0x03 };
//static const unsigned char mmcStopMsg[] =         { 0x7f, 0x7f, 0x06, 0x01 };

const char* seqMsgList[] = {
	"SEQM_ADD_TRACK", "SEQM_REMOVE_TRACK", "SEQM_CHANGE_TRACK", "SEQM_MOVE_TRACK",
	"SEQM_ADD_PART", "SEQM_REMOVE_PART", "SEQM_REMOVE_PART_LIST", "SEQM_CHANGE_PART",
	"SEQM_ADD_EVENT", "SEQM_REMOVE_EVENT", "SEQM_CHANGE_EVENT",
	"SEQM_ADD_TEMPO", "SEQM_SET_TEMPO", "SEQM_REMOVE_TEMPO", "SEQM_REMOVE_TEMPO_RANGE", 
	"SEQM_ADD_SIG", "SEQM_REMOVE_SIG",
	"SEQM_SET_GLOBAL_TEMPO",
	"SEQM_UNDO", "SEQM_REDO",
	"SEQM_RESET_DEVICES", "SEQM_INIT_DEVICES", "SEQM_PANIC",
	"SEQM_MIDI_LOCAL_OFF",
	"SEQM_SET_MIDI_DEVICE",
	"SEQM_PLAY_MIDI_EVENT",
	"SEQM_SET_HW_CTRL_STATE",
	"SEQM_SET_HW_CTRL_STATES",
	"SEQM_SET_TRACK_OUT_PORT",
	"SEQM_SET_TRACK_OUT_CHAN",
	"SEQM_REMAP_PORT_DRUM_CTL_EVS",
	"SEQM_CHANGE_ALL_PORT_DRUM_CTL_EVS",
	"SEQM_SCAN_ALSA_MIDI_PORTS",
	"SEQM_SET_AUX",
	"SEQM_UPDATE_SOLO_STATES",
	"MIDI_SHOW_INSTR_GUI",
	"AUDIO_RECORD",
	"AUDIO_ROUTEADD", "AUDIO_ROUTEREMOVE", "AUDIO_REMOVEROUTES",
	"AUDIO_VOL", "AUDIO_PAN",
	"AUDIO_ADDPLUGIN",
	"AUDIO_SET_SEG_SIZE",
	"AUDIO_SET_PREFADER", "AUDIO_SET_CHANNELS",
	"AUDIO_SET_PLUGIN_CTRL_VAL",
	"AUDIO_SWAP_CONTROLLER_IDX",
	"AUDIO_CLEAR_CONTROLLER_EVENTS",
	"AUDIO_SEEK_PREV_AC_EVENT",
	"AUDIO_SEEK_NEXT_AC_EVENT",
	"AUDIO_ERASE_AC_EVENT",
	"AUDIO_ERASE_RANGE_AC_EVENTS",
	"AUDIO_ADD_AC_EVENT",
	"AUDIO_SET_SOLO", "AUDIO_SET_SEND_METRONOME",
	"MS_PROCESS", "MS_STOP", "MS_SET_RTC", "MS_UPDATE_POLL_FD",
	"SEQM_IDLE", "SEQM_SEEK", "SEQM_PRELOAD_PROGRAM", "SEQM_REMOVE_TRACK_GROUP"
};

const char* audioStates[] = {
	"STOP", "START_PLAY", "PLAY", "LOOP1", "LOOP2", "SYNC", "PRECOUNT"
};


//---------------------------------------------------------
//   Audio
//---------------------------------------------------------

Audio::Audio()
{
	_running = false;
	recording = false;
	idle = false;
	_freewheel = false;
	_bounce = false;
	//loopPassed    = false;
	_loopFrame = 0;
	_loopCount = 0;

	_pos.setType(Pos::FRAMES);
	_pos.setFrame(0);
	curTickPos = 0;

	midiClick = 0;
	clickno = 0;
	clicksMeasure = 0;
	ticksBeat = 0;

	syncTime = 0.0;
	syncFrame = 0;
	frameOffset = 0;

	state = STOP;
	msg = 0;

	// Changed by Tim. p3.3.8
	//startRecordPos.setType(Pos::TICKS);
	//endRecordPos.setType(Pos::TICKS);
	startRecordPos.setType(Pos::FRAMES);
	endRecordPos.setType(Pos::FRAMES);

	_audioMonitor = 0;
	_audioMaster = 0;

	//---------------------------------------------------
	//  establish pipes/sockets
	//---------------------------------------------------

	int filedes[2]; // 0 - reading   1 - writing
	if (pipe(filedes) == -1)
	{
		perror("creating pipe0");
		exit(-1);
	}
	fromThreadFdw = filedes[1];
	fromThreadFdr = filedes[0];
	int rv = fcntl(fromThreadFdw, F_SETFL, O_NONBLOCK);
	if (rv == -1)
		perror("set pipe O_NONBLOCK");

	if (pipe(filedes) == -1)
	{
		perror("creating pipe1");
		exit(-1);
	}
	sigFd = filedes[1];
	QSocketNotifier* ss = new QSocketNotifier(filedes[0], QSocketNotifier::Read);
	song->connect(ss, SIGNAL(activated(int)), song, SLOT(seqSignal(int)));
}

//---------------------------------------------------------
//   start
//    start audio processing
//---------------------------------------------------------

extern bool initJackAudio();

bool Audio::start()
{
	//process(segmentSize);   // warm up caches
	state = STOP;
	_loopCount = 0;
	oom->setHeartBeat();
	if (!audioDevice)
	{
		if (!initJackAudio())
		{
			InputList* itl = song->inputs();
			for (iAudioInput i = itl->begin(); i != itl->end(); ++i)
			{
				//printf("reconnecting input %s\n", (*i)->name().ascii());
				for (int x = 0; x < (*i)->channels(); x++)
					(*i)->setJackPort(x, 0);
				(*i)->setName((*i)->name()); // restore jack connection
			}

			OutputList* otl = song->outputs();
			for (iAudioOutput i = otl->begin(); i != otl->end(); ++i)
			{
				//printf("reconnecting output %s\n", (*i)->name().ascii());
				for (int x = 0; x < (*i)->channels(); x++)
					(*i)->setJackPort(x, 0);
				//printf("name=%s\n",(*i)->name().toLatin1());
				(*i)->setName((*i)->name()); // restore jack connection
			}
		}
		else
		{
			printf("Failed to init audio!\n");
			return false;
		}
	}

	if(audioDevice)
	{
		audioDevice->start(realTimePriority);

		_running = true;

		// shall we really stop JACK transport and locate to
		// saved position?
		audioDevice->stopTransport();
		audioDevice->seekTransport(song->cPos());
		return true;
	}
	return false;
}

//---------------------------------------------------------
//   stop
//    stop audio processing
//---------------------------------------------------------

void Audio::stop(bool)
{
	if (audioDevice)
		audioDevice->stop();
	_running = false;
}

//---------------------------------------------------------
//   sync
//    return true if sync is completed
//---------------------------------------------------------

bool Audio::sync(int jackState, unsigned frame)
{

	// Changed by Tim. p3.3.24
	/*
		  bool done = true;
		  if (state == LOOP1)
				state = LOOP2;
		  else {
				if (_pos.frame() != frame) {
					  Pos p(frame, false);
					  seek(p);
					  }
				state = State(jackState);
				if (!_freewheel)
					  //done = audioPrefetch->seekDone;
					  done = audioPrefetch->seekDone();
				}
      
		  return done;
	 */
	bool done = true;
	if (state == LOOP1)
		state = LOOP2;
	else
	{
		State s = State(jackState);
		//
		//  STOP -> START_PLAY      start rolling
		//  STOP -> STOP            seek in stop state
		//  PLAY -> START_PLAY  seek in play state

		if (state != START_PLAY)
		{
			//Pos p(frame, AL::FRAMES);
			//    seek(p);
			Pos p(frame, false);
			seek(p);
			if (!_freewheel)
				done = audioPrefetch->seekDone();
			if (s == START_PLAY)
				state = START_PLAY;
		}
		else
		{
			//if (frame != _seqTime.pos.frame()) {
			if (frame != _pos.frame())
			{
				// seek during seek
				//seek(Pos(frame, AL::FRAMES));
				seek(Pos(frame, false));
			}
			done = audioPrefetch->seekDone();
		}
	}
	return done;

}

//---------------------------------------------------------
//   setFreewheel
//---------------------------------------------------------

void Audio::setFreewheel(bool val)
{
	// printf("JACK: freewheel callback %d\n", val);
	_freewheel = val;
}

//---------------------------------------------------------
//   shutdown
//---------------------------------------------------------

void Audio::shutdown()
{
	_running = false;
	printf("Audio::shutdown()\n");
	write(sigFd, "S", 1);
}

//---------------------------------------------------------
//   process
//    process one audio buffer at position "_pos "
//    of size "frames"
//---------------------------------------------------------

void Audio::process(unsigned frames)
{
	if (!checkAudioDevice()) return;
	if (msg)
	{
		processMsg(msg);
		int sn = msg->serialNo;
		msg = 0; // dont process again
		int rv = write(fromThreadFdw, &sn, sizeof (int));
		if (rv != sizeof (int))
		{
			fprintf(stderr, "audio: write(%d) pipe failed: %s\n",
					fromThreadFdw, strerror(errno));
		}
	}

    OutputList* ol = song->outputs();
	if (idle)
	{
		// deliver no audio
		for (iAudioOutput i = ol->begin(); i != ol->end(); ++i)
			(*i)->silence(frames);
		return;
	}

	int jackState = audioDevice->getState();

	//if(debugMsg)
	//  printf("Audio::process Current state:%s jackState:%s\n", audioStates[state], audioStates[jackState]);

	if (state == START_PLAY && jackState == PLAY)
	{
		_loopCount = 0;
		startRolling();
		if (_bounce)
			write(sigFd, "f", 1);
	}
	else if (state == LOOP2 && jackState == PLAY)
	{
		++_loopCount; // Number of times we have looped so far
		Pos newPos(_loopFrame, false);
		seek(newPos);
		startRolling();
	}
	else if (isPlaying() && jackState == STOP)
	{
		// p3.3.43 Make sure to stop bounce and freewheel mode, for example if user presses stop
		//  in QJackCtl before right-hand marker is reached (which is handled below).
		//printf("Audio::process isPlaying() && jackState == STOP\n");
		if (freewheel())
		{
		//printf("  stopping bounce...\n");
		  _bounce = false;
		  write(sigFd, "F", 1);
		}

		stopRolling();
	}
	else if (state == START_PLAY && jackState == STOP)
	{
		state = STOP;
		if (_bounce)
		{
			audioDevice->startTransport();
		}
		else
			write(sigFd, "3", 1); // abort rolling
	}
	else if (state == STOP && jackState == PLAY)
	{
		_loopCount = 0;
		startRolling();
	}
	else if (state == LOOP1 && jackState == PLAY)
		; // treat as play
	else if (state == LOOP2 && jackState == START_PLAY)
	{
		; // sync cycle
	}
	else if (state != jackState)
		printf("JACK: state transition %s -> %s ?\n",
			audioStates[state], audioStates[jackState]);

	// printf("p %s %s %d\n", audioStates[jackState], audioStates[state], _pos.frame());

	//
	// clear aux send buffers
	//
	AuxList* al = song->auxs();
	for (unsigned i = 0; i < al->size(); ++i)
	{
		AudioAux* a = (AudioAux*) ((*al)[i]);
		float** dst = a->sendBuffer();
		for (int ch = 0; ch < a->channels(); ++ch)
			memset(dst[ch], 0, sizeof (float) * segmentSize);
	}

	for (iAudioOutput i = ol->begin(); i != ol->end(); ++i)
		(*i)->processInit(frames);
	int samplePos = _pos.frame();
	int offset = 0; // buffer offset in audio buffers

	if (isPlaying())
	{
		if (!freewheel())
			audioPrefetch->msgTick();

		if (_bounce && _pos >= song->rPos())
		{
			_bounce = false;
			write(sigFd, "F", 1);
			return;
		}

		//
		//  check for end of song
		//
		if ((curTickPos >= song->len())
				&& !(song->record()
				|| _bounce
				|| song->loop()))
		{
			//if(debugMsg)
			//  printf("Audio::process curTickPos >= song->len\n");

			audioDevice->stopTransport();
			return;
		}

		//
		//  check for loop end
		//
		if (state == PLAY && song->loop() && !_bounce && !extSyncFlag.value())
		{
			const Pos& loop = song->rPos();
			unsigned n = loop.frame() - samplePos - (3 * frames);
			if (n < frames)
			{
				// loop end in current cycle
				unsigned lpos = song->lPos().frame();
				// adjust loop start so we get exact loop len
				if (n > lpos)
					n = 0;
				state = LOOP1;
				_loopFrame = lpos - n;

				// clear sustain
				for (int i = 0; i < MIDI_PORTS; ++i)
				{
					MidiPort* mp = &midiPorts[i];
					for (int ch = 0; ch < MIDI_CHANNELS; ++ch)
					{
						if (mp->hwCtrlState(ch, CTRL_SUSTAIN) == 127)
						{
							if (mp->device() != NULL)
							{
								//printf("send clear sustain!!!!!!!! port %d ch %d\n", i,ch);
								MidiPlayEvent ev(0, i, ch, ME_CONTROLLER, CTRL_SUSTAIN, 0);
								// may cause problems, called from audio thread
								mp->device()->putEvent(ev);
							}
						}
					}
				}

				//audioDevice->seekTransport(_loopFrame);
				Pos lp(_loopFrame, false);
				audioDevice->seekTransport(lp);


				// printf("  process: seek to %d, end %d\n", _loopFrame, loop.frame());
			}
		}


		// p3.3.25
		if (extSyncFlag.value())
		{
			nextTickPos = curTickPos + midiExtSyncTicks;
			// Probably not good - interfere with midi thread.
			midiExtSyncTicks = 0;
		}
		else
		{

			Pos ppp(_pos);
			ppp += frames;
			nextTickPos = ppp.tick();
		}
	}
	//
	// resync with audio interface
	//
	syncFrame = audioDevice->framePos();
	syncTime = curTime();
	frameOffset = syncFrame - samplePos;

	//printf("Audio::process calling process1:\n");

	process1(samplePos, offset, frames);
	for (iAudioOutput i = ol->begin(); i != ol->end(); ++i)
		(*i)->processWrite();
	if (isPlaying())
	{
		_pos += frames;
		curTickPos = nextTickPos;
	}
}

//---------------------------------------------------------
//   process1
//---------------------------------------------------------

void Audio::process1(unsigned samplePos, unsigned offset, unsigned frames)
{
	if (midiSeqRunning)
	{
		processMidi();
	}
	//midiSeq->msgProcess();

	//
	// process not connected tracks
	// to animate meter display
	//
	TrackList* tl = song->tracks();
	AudioTrack* track;
	int channels;
	for (ciTrack it = tl->begin(); it != tl->end(); ++it)
	{
		Track* t = *it;
		if (!t || t->isMidiTrack())
			continue;
		track = (AudioTrack*) t;

		// For audio track types, synths etc. which need some kind of non-audio
		//  (but possibly audio-affecting) processing always, even if their output path
		//  is ultimately unconnected.
		// Example: A fluidsynth instance whose output path ultimately led to nowhere
		//  would not allow us to load a font. Since process() was driven by audio output,
		//  in this case there was nothing driving the process() function which responds to
		//  such gui commands. So I separated the events processing from process(), into this.
		// It should be used for things like midi events, gui events etc. - things which need to
		//  be done BEFORE all the AudioOutput::process() are called below. That does NOT include
		//  audio processing, because THAT is done at the very end of this routine.
		// This will also reset the track's processed flag.
		track->preProcessAlways();
	}
	// Pre-process the metronome.
	((AudioTrack*) metronome)->preProcessAlways();

	OutputList* ol = song->outputs();
	for (ciAudioOutput i = ol->begin(); i != ol->end(); ++i)
		(*i)->process(samplePos, offset, frames);

	// Were ANY tracks unprocessed as a result of processing all the AudioOutputs, above?
	// Not just unconnected ones, as previously done, but ones whose output path ultimately leads nowhere.
	// Those tracks were missed, until this fix.
	// Do them now. This will animate meters, and 'quietly' process some audio which needs to be done -
	//  for example synths really need to be processed, 'quietly' or not, otherwise the next time
	//  processing is 'turned on', if there was a backlog of events while it was off, then they all happen at once.
	for (ciTrack it = tl->begin(); it != tl->end(); ++it)
	{
		Track* t = *it;
		if (!t || t->isMidiTrack())
			continue;
		track = (AudioTrack*) t;
		// Ignore unprocessed tracks which have an output route, because they will be processed by
		//  whatever track(s) they are routed to.
		if (!track->processed() && track->noOutRoute() && (track->type() != Track::AUDIO_OUTPUT))
		{
			channels = track->channels();
			// Just a dummy buffer.
			float* buffer[channels];
			float data[frames * channels];
			for (int i = 0; i < channels; ++i)
				buffer[i] = data + i * frames;
			//printf("Audio::process1 calling track->copyData for track:%s\n", track->name().toLatin1());

			track->copyData(samplePos, channels, -1, -1, frames, buffer);
		}
	}
}

//---------------------------------------------------------
//   processMsg
//---------------------------------------------------------

void Audio::processMsg(AudioMsg* msg)
{
	switch (msg->id)
	{
		case AUDIO_RECORD:
			msg->snode->setRecordFlag2(msg->ival);
			//TODO: hook this and send midi cc to bcf2000
			break;
		case AUDIO_ROUTEADD:
			addRoute(msg->sroute, msg->droute);
			break;
		case AUDIO_ROUTEREMOVE:
			removeRoute(msg->sroute, msg->droute);
			break;
		case AUDIO_REMOVEROUTES: // p3.3.55
			removeAllRoutes(msg->sroute, msg->droute);
			break;
		case AUDIO_VOL:
			msg->snode->setVolume(msg->dval);
			//TODO: hook this and send midi cc to bcf2000
			break;
		case AUDIO_PAN:
			msg->snode->setPan(msg->dval);
			//TODO: hook this and send midi cc to bcf2000
			break;
		case SEQM_SET_AUX:
			msg->snode->setAuxSend(msg->sid, msg->dval);
			break;
		case AUDIO_SET_PREFADER:
			msg->snode->setPrefader(msg->ival);
			break;
		case AUDIO_SET_CHANNELS:
			msg->snode->setChannels(msg->ival);
			break;
		case AUDIO_IDLEPLUGIN:
			msg->snode->idlePlugin(msg->plugin);
			break;
		case AUDIO_ADDPLUGIN:
			msg->snode->addPlugin(msg->plugin, msg->ival);
			//TODO: Trigger song->update(SC_RACK);
			write(sigFd, "E", 1); // signal update effects to gui
			break;
		case AUDIO_SET_PLUGIN_CTRL_VAL:
			//msg->plugin->track()->setPluginCtrlVal(msg->ival, msg->dval);
			// p3.3.43
			msg->snode->setPluginCtrlVal(msg->ival, msg->dval);
			break;
		case AUDIO_SWAP_CONTROLLER_IDX:
			msg->snode->swapControllerIDX(msg->a, msg->b);
			break;
		case AUDIO_CLEAR_CONTROLLER_EVENTS:
			msg->snode->clearControllerEvents(msg->ival);
			break;
		case AUDIO_SEEK_PREV_AC_EVENT:
			msg->snode->seekPrevACEvent(msg->ival);
			break;
		case AUDIO_SEEK_NEXT_AC_EVENT:
			msg->snode->seekNextACEvent(msg->ival);
			break;
		case AUDIO_ERASE_AC_EVENT:
			msg->snode->eraseACEvent(msg->ival, msg->a);
			break;
		case AUDIO_ERASE_RANGE_AC_EVENTS:
			msg->snode->eraseRangeACEvents(msg->ival, msg->a, msg->b);
			break;
		case AUDIO_ADD_AC_EVENT:
			msg->snode->addACEvent(msg->ival, msg->a, msg->dval);
			break;
		case AUDIO_SET_SOLO:
			msg->track->setSolo((bool)msg->ival);
			//TODO: hook this and send midi cc to bcf2000
			break;

		case AUDIO_SET_SEND_METRONOME:
			msg->snode->setSendMetronome((bool)msg->ival);
			break;

		case AUDIO_SET_SEG_SIZE:
			segmentSize = msg->ival;
			sampleRate = msg->iival;
            
            for (iMidiDevice i = midiDevices.begin(); i != midiDevices.end(); ++i)
            {
                if ((*i)->deviceType() == MidiDevice::SYNTH_MIDI)
                {
                    ((SynthPluginDevice*)(*i))->segmentSizeChanged(segmentSize);
                }
            }

#if 0 //TODO
			audioOutput.segmentSizeChanged();
			for (int i = 0; i < mixerGroups; ++i)
				audioGroups[i].segmentSizeChanged();
			for (iSynthI ii = synthiInstances.begin(); ii != synthiInstances.end(); ++ii)
				(*ii)->segmentSizeChanged();
#endif
			break;
		case SEQM_RESET_DEVICES:
			for (int i = 0; i < MIDI_PORTS; ++i)
			{
				if(midiPorts[i].device())
					midiPorts[i].instrument()->reset(i, song->mtype());
			}
			break;
		case SEQM_INIT_DEVICES:
			initDevices();
			break;
		case SEQM_MIDI_LOCAL_OFF:
			sendLocalOff();
			break;
		case SEQM_PANIC:
			panic();
			break;
		case SEQM_PLAY_MIDI_EVENT:
		{
			MidiPlayEvent* ev = (MidiPlayEvent*) (msg->p1);
			midiPorts[ev->port()].sendEvent(*ev);
			// Record??
		}
			break;
		case SEQM_SET_HW_CTRL_STATE:
		{
			MidiPort* port = (MidiPort*) (msg->p1);
			port->setHwCtrlState(msg->a, msg->b, msg->c);
		}
			break;
		case SEQM_SET_HW_CTRL_STATES:
		{
			MidiPort* port = (MidiPort*) (msg->p1);
			port->setHwCtrlStates(msg->a, msg->b, msg->c, msg->ival);
		}
			break;
		case SEQM_SCAN_ALSA_MIDI_PORTS:
			alsaScanMidiPorts();
			break;
		case MIDI_SHOW_INSTR_GUI:
        case MIDI_SHOW_INSTR_NATIVE_GUI:
			midiSeq->msgUpdatePollFd();
			break;
		case SEQM_ADD_TEMPO:
		case SEQM_REMOVE_TEMPO:
		case SEQM_REMOVE_TEMPO_RANGE:
		case SEQM_SET_GLOBAL_TEMPO:
		case SEQM_SET_TEMPO:
			song->processMsg(msg);
			if (isPlaying())
			{
				if (!checkAudioDevice()) return;
				_pos.setTick(curTickPos);
				int samplePos = _pos.frame();
				syncFrame = audioDevice->framePos();
				syncTime = curTime();
				frameOffset = syncFrame - samplePos;
			}
			break;
		case SEQM_ADD_TRACK:
		case SEQM_REMOVE_TRACK:
		case SEQM_REMOVE_TRACK_GROUP:
		case SEQM_CHANGE_TRACK:
		case SEQM_ADD_PART:
		case SEQM_REMOVE_PART:
		case SEQM_REMOVE_PART_LIST:
		case SEQM_CHANGE_PART:
		case SEQM_SET_TRACK_OUT_CHAN:
		case SEQM_SET_TRACK_OUT_PORT:
		case SEQM_REMAP_PORT_DRUM_CTL_EVS:
		case SEQM_CHANGE_ALL_PORT_DRUM_CTL_EVS:
		case SEQM_PRELOAD_PROGRAM:
			midiSeq->sendMsg(msg);
			break;

		case SEQM_IDLE:
			idle = msg->a;
			midiSeq->sendMsg(msg);
			break;

		default:
			song->processMsg(msg);
			break;
	}
}

//---------------------------------------------------------
//   seek
//    - called before start play
//    - initiated from gui
//---------------------------------------------------------

void Audio::seek(const Pos& p)
{
	if (_pos == p)
	{
		if (debugMsg)
			printf("Audio::seek already there\n");
		return;
	}

	//printf("Audio::seek frame:%d\n", p.frame());
	_pos = p;
	if (!checkAudioDevice()) return;
	syncFrame = audioDevice->framePos();
	frameOffset = syncFrame - _pos.frame();
	curTickPos = _pos.tick();

	midiSeq->msgSeek(); // handle stuck notes and set
	// controller for new position

	// Don't send if external sync is on. The master, and our sync routing system will take care of that.
	if (!extSyncFlag.value())
	{

		for (int port = 0; port < MIDI_PORTS; ++port)
		{
			MidiPort* mp = &midiPorts[port];
			MidiDevice* dev = mp->device();
			if (!dev || !mp->syncInfo().MRTOut())
				continue;

			int beat = (curTickPos * 4) / config.division;

			bool isPlaying = false;
			if (state == PLAY)
				isPlaying = true;

			mp->sendStop();
			mp->sendSongpos(beat);
			if (isPlaying)
				mp->sendContinue();
		}
	}

	if (state != LOOP2 && !freewheel())
	{
		// We need to force prefetch to update,
		//  to ensure the most recent data. Things can happen to a part
		//  before play is pressed - such as part muting, part moving etc.
		// Without a force, the wrong data was being played.
		audioPrefetch->msgSeek(_pos.frame(), true);
	}

	write(sigFd, "G", 1); // signal seek to gui
}

//---------------------------------------------------------
//   writeTick
//    called from audio prefetch thread context
//    write another buffer to soundfile
//---------------------------------------------------------

void Audio::writeTick()
{
	AudioOutput* ao = song->bounceOutput;
	if (ao && song->outputs()->find(ao) != song->outputs()->end())
	{
		if (ao->recordFlag())
			ao->record();
	}
	WaveTrackList* tl = song->waves();
	for (iWaveTrack t = tl->begin(); t != tl->end(); ++t)
	{
		WaveTrack* track = *t;
		if (track->recordFlag())
			track->record();
	}
}

//---------------------------------------------------------
//   startRolling
//---------------------------------------------------------

void Audio::startRolling()
{
	if (debugMsg)
		printf("startRolling - loopCount=%d, _pos=%d\n", _loopCount, _pos.tick());

	if (_loopCount == 0)
	{
		startRecordPos = _pos;
	}

	if (song->record())
	{
		recording = true;
		TrackList* tracks = song->tracks();
		for (iTrack i = tracks->begin(); i != tracks->end(); ++i)
		{
			if ((*i)->isMidiTrack())
				continue;
			if ((*i)->type() == Track::WAVE)
				((WaveTrack*) (*i))->resetMeter();
		}
	}
	state = PLAY;
	write(sigFd, "1", 1); // Play

	// Don't send if external sync is on. The master, and our sync routing system will take care of that.
	if (!extSyncFlag.value())
	{

		for (int port = 0; port < MIDI_PORTS; ++port)
		{
			MidiPort* mp = &midiPorts[port];
			MidiDevice* dev = mp->device();
			if (!dev)
				continue;

			MidiSyncInfo& si = mp->syncInfo();

			if (si.MMCOut())
				mp->sendMMCDeferredPlay();

			if (si.MRTOut())
			{
				if (curTickPos)
					mp->sendContinue();
				else
					mp->sendStart();
			}
		}
	}

	if (precountEnableFlag
			&& song->click()
			&& !extSyncFlag.value()
			&& song->record())
	{
#if 0
		state = PRECOUNT;
		int z, n;
		if (precountFromMastertrackFlag)
			AL::sigmap.timesig(playTickPos, z, n);
		else
		{
			z = precountSigZ;
			n = precountSigN;
		}
		clickno = z * preMeasures;
		clicksMeasure = z;
		ticksBeat = (division * 4) / n;
#endif
	}
	else
	{
		//
		// compute next midi metronome click position
		//
		int bar, beat;
		unsigned tick;
		AL::sigmap.tickValues(curTickPos, &bar, &beat, &tick);
		if (tick)
			beat += 1;
		midiClick = AL::sigmap.bar2tick(bar, beat, 0);
	}

	// reenable sustain
	for (int i = 0; i < MIDI_PORTS; ++i)
	{
		MidiPort* mp = &midiPorts[i];
		for (int ch = 0; ch < MIDI_CHANNELS; ++ch)
		{
			if (mp->hwCtrlState(ch, CTRL_SUSTAIN) == 127)
			{
				if (mp->device() != NULL)
				{
					//printf("send enable sustain!!!!!!!! port %d ch %d\n", i,ch);
					MidiPlayEvent ev(0, i, ch, ME_CONTROLLER, CTRL_SUSTAIN, 127);

					// may cause problems, called from audio thread
					mp->device()->playEvents()->add(ev);
				}
			}
		}
	}
}

//---------------------------------------------------------
//   stopRolling
//---------------------------------------------------------

void Audio::stopRolling()
{
	state = STOP;
	midiSeq->msgStop();

#if 1 //TODO
	//---------------------------------------------------
	//    reset sustain
	//---------------------------------------------------


	// clear sustain
	for (int i = 0; i < MIDI_PORTS; ++i)
	{
		MidiPort* mp = &midiPorts[i];
		for (int ch = 0; ch < MIDI_CHANNELS; ++ch)
		{
			if(!mp->device())
				continue;
			if (mp->hwCtrlState(ch, CTRL_SUSTAIN) == 127)
			{
				//printf("send clear sustain!!!!!!!! port %d ch %d\n", i,ch);
				MidiPlayEvent susEv(0, i, ch, ME_CONTROLLER, CTRL_SUSTAIN, 0);
				// may cause problems, called from audio thread
				mp->device()->putEvent(susEv);
			}
			mp->sendEvent(MidiPlayEvent(0, i, ch, ME_CONTROLLER, CTRL_ALL_SOUNDS_OFF, 0), true);
			mp->sendEvent(MidiPlayEvent(0, i, ch, ME_CONTROLLER, CTRL_ALL_NOTES_OFF, 0), true);
			mp->sendEvent(MidiPlayEvent(0, i, ch, ME_CONTROLLER, CTRL_RESET_ALL_CTRL, 0), true);
		}
	}

#endif

	// Don't send if external sync is on. The master, and our sync routing system will take care of that.
	if (!extSyncFlag.value())
	{
		for (int port = 0; port < MIDI_PORTS; ++port)
		{
			MidiPort* mp = &midiPorts[port];
			MidiDevice* dev = mp->device();
			if (!dev)
				continue;

			MidiSyncInfo& si = mp->syncInfo();

			if (si.MMCOut())
			{
				mp->sendMMCStop();
			}

			if (si.MRTOut()) //
			{
				// send STOP and
				// "set song position pointer"
				mp->sendStop();
			}
		}
	}


	WaveTrackList* tracks = song->waves();
	for (iWaveTrack i = tracks->begin(); i != tracks->end(); ++i)
	{
		WaveTrack* track = *i;
		track->resetMeter();
	}
	recording = false;
	endRecordPos = _pos;
	write(sigFd, "0", 1); // STOP
}

//---------------------------------------------------------
//   recordStop
//    execution environment: gui thread
//---------------------------------------------------------

void Audio::recordStop()
{
	if (debugMsg)
		printf("recordStop - startRecordPos=%d\n", startRecordPos.tick());
	audio->msgIdle(true); // gain access to all data structures

	song->startUndo();
	WaveTrackList* wl = song->waves();

	for (iWaveTrack it = wl->begin(); it != wl->end(); ++it)
	{
		WaveTrack* track = *it;
		if (track->recordFlag() || song->bounceTrack == track)
		{
			song->cmdAddRecordedWave(track, startRecordPos, endRecordPos);
			// The track's _recFile pointer may have been kept and turned
			//  into a SndFileR and added to a new part.
			// Or _recFile may have been discarded (no new recorded part created).
			// Regardless, we are done with the pointer itself. Set to zero so
			//  song->setRecordFlag knows about it...

			track->setRecFile(0); // flush out the old file
			song->setRecordFlag(track, false); //
		}
	}
	MidiTrackList* ml = song->midis();
	for (iMidiTrack it = ml->begin(); it != ml->end(); ++it)
	{
		MidiTrack* mt = *it;
		MPEventList* mpel = mt->mpevents();
		EventList* el = mt->events();

		//---------------------------------------------------
		//    resolve NoteOff events, Controller etc.
		//---------------------------------------------------

		//buildMidiEventList(el, mpel, mt, config.division, true);
		// Do SysexMeta. Do loops.
		buildMidiEventList(el, mpel, mt, config.division, true, true);
		song->cmdAddRecordedEvents(mt, el, startRecordPos.tick());
		el->clear();
		mpel->clear();
	}

	//
	// bounce to file operates on the only
	// selected output port
	//

	AudioOutput* ao = song->bounceOutput;
	if (ao && song->outputs()->find(ao) != song->outputs()->end())
	{
		if (ao->recordFlag())
		{
			song->bounceOutput = 0;
			SndFile* sf = ao->recFile();
			if (sf)
				delete sf; // close
			ao->setRecFile(0);
			ao->setRecordFlag1(false);
			msgSetRecord(ao, false);
		}
	}
	audio->msgIdle(false);
	song->endUndo(0);
	song->setRecord(false);
}

//---------------------------------------------------------
//   curFrame
//    extrapolates current play frame on syncTime/syncFrame
//---------------------------------------------------------

unsigned int Audio::curFrame() const
{
	return lrint((curTime() - syncTime) * sampleRate) + syncFrame;
}

//---------------------------------------------------------
//   timestamp
//---------------------------------------------------------

int Audio::timestamp() const
{
	int t = curFrame() - frameOffset;
	return t;
}

//---------------------------------------------------------
//   sendMsgToGui
//---------------------------------------------------------

void Audio::sendMsgToGui(char c)
{
	write(sigFd, &c, 1);
}

