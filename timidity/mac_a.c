/* 
    TiMidity++ -- MIDI to WAVE converter and player
    Copyright (C) 1999 Masanao Izumo <mo@goice.co.jp>
    Copyright (C) 1995 Tuukka Toivonen <tt@cgs.fi>

    This program is free software; you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation; either version 2 of the License, or
    (at your option) any later version.

    This program is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU General Public License for more details.

    You should have received a copy of the GNU General Public License
    along with this program; if not, write to the Free Software
    Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.

	Macintosh interface for TiMidity
	by T.Nogami	<t-nogami@happy.email.ne.jp>
		
    mac_a.c
    Macintosh audio driver
*/


#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>

#include <sound.h>
#include <SoundInput.h>
#include <Threads.h>

#include "timidity.h"
#include "common.h"
#include "output.h"
#include "controls.h"
#include "miditrace.h"

#include "mac_main.h"
#include "mac_util.h"

extern int default_play_event(void *);
static int open_output(void); /* 0=success, 1=warning, -1=fatal error */
static void close_output(void);
static void output_data(int32 *buf, int32 count);
static int  flush_output(void);
static void purge_output(void);
static int32 current_samples(void);
static int play_loop(void);


/* export the playback mode */

#define dpm mac_play_mode

PlayMode dpm = {
  44100, PE_16BIT|PE_SIGNED, PF_NEED_INSTRUMENTS|PF_CAN_TRACE,
  NULL,			//file descriptor
  {0,0,0,0,0},	/*no extra parameters so far*/
  "mac audio driver", 'm',
  "",			//device file name
  default_play_event,
  open_output,
  close_output,
  output_data,
  flush_output,
  purge_output,
  current_samples,
  play_loop
};

#define	MACBUFSIZE	((AUDIO_BUFFER_SIZE)*4+100)
						/* *4 means 16bit stereo */
						/* 100 means sound header */
#define	MACBUFNUM	(MAC_SOUNDBUF_QLENGTH/2+1)
#define	FLUSH_END	-1

Handle	soundHandle[MACBUFNUM];
int	nextBuf=0;

SndChannelPtr	gSndCannel=0;
short			mac_amplitude=0x00FF;
unsigned long	start_tic;
volatile static int32	play_counter;
static int				filling_flag, flushing_flag;
/* ******************************************************************* */
static SndChannelPtr MyCreateSndChannel(short synth, long initOptions,
					SndCallBackUPP userRoutine,	short	queueLength)
{
	SndChannelPtr	mySndChan; // {pointer to a sound channel}
	OSErr			myErr;
						// {Allocate memory for sound channel.}
	mySndChan = (SndChannelPtr)NewPtr(
				sizeof(SndChannel) + (queueLength-stdQLength)*sizeof(SndCommand) );
	if( mySndChan != 0 ){
		mySndChan->qLength = queueLength;	// {set number of commands in queue}
											// {Create a new sound channel.}
		myErr = SndNewChannel(&mySndChan, synth, initOptions, userRoutine);
		if( myErr != noErr ){			// {couldn't allocate channel}
			DisposePtr((Ptr)mySndChan); // {free memory already allocated}
			mySndChan = 0;				// {return NIL}
		}
		else
			mySndChan->userInfo = 0;	// {reset userInfo field}
	}
	return mySndChan; 					// {return new sound channel}
}

// ***************************************
static void initCounter()
{
	start_tic=TickCount();
	play_counter=0;
	filling_flag=0;
	play_mode->extra_param[0]=0;
	flushing_flag=0;
}

static pascal void callback( SndChannelPtr /*chan*/, SndCommand * cmd)
{
	if( cmd->param2==FLUSH_END ){
		flushing_flag=0;
	}else{
		play_counter+= cmd->param2;
	}
}

static int open_output (void)
{
	int		i;
	SndCommand	theCmd;

	gSndCannel=MyCreateSndChannel(sampledSynth, 0,
					NewSndCallBackProc(callback), MAC_SOUNDBUF_QLENGTH);
	if( gSndCannel==0 )
			{ StopAlertMessage("\pCan't open Sound Channel"); ExitToShell(); }
					
	dpm.encoding |= PE_16BIT;
    dpm.encoding |= PE_SIGNED;
    dpm.encoding &= ~PE_ULAW;
	dpm.encoding &= ~PE_ALAW;
    dpm.encoding &= ~PE_BYTESWAP;

	for( i=0; i<MACBUFNUM; i++) /*making sound buffer*/
	{
		soundHandle[i]=NewHandle(MACBUFSIZE);
		if( soundHandle[i]==0 ) return -1;
		HLock(soundHandle[i]);
	}
	
	theCmd.cmd=ampCmd;	/*setting volume*/
	theCmd.param1=mac_amplitude;
	SndDoCommand(gSndCannel, &theCmd, 0);
	initCounter();
	return 0;
}

static void filling_end()
{
	if( filling_flag && do_initial_filling){
		filling_flag=0;
		if( skin_state!=PAUSE ){
			SndCommand		theCmd;
			theCmd.cmd=resumeCmd; SndDoImmediate(gSndCannel, &theCmd);
		}
	}
	if( gCursorIsWatch ){
		InitCursor();	gCursorIsWatch=false;
	}
}

static void QuingSndCommand(SndChannelPtr chan, const SndCommand *cmd)
{
	OSErr err;
	
	for(;;)/* wait for successful quing */
	{
		err= SndDoCommand(chan, cmd, 1);
		if( err==noErr ){ gBusy=true; break; }/*buffer has more rooms*/
		else if( err==queueFull )
		{
			gBusy=false;
				//end of INITIAL FILLING
			filling_end();
			trace_loop();
			YieldToAnyThread();
		}
		else	/*queueFull 以外のerrなら終了*/
			mac_ErrorExit("\pSound out error--quit");			
	}
}

static void output_data (int32 *buf, int32 count)
{
	short			err,headerLen;
	long			len = count;
	const int32 	samples = count;
	long			offset;
	SndCommand		theCmd;
	
			// start INITIAL FILLING
	if( play_counter==0 && filling_flag==0 && do_initial_filling){
		filling_flag=1;
		theCmd.cmd=pauseCmd; SndDoImmediate(gSndCannel, &theCmd);
	}

	if (!(dpm.encoding & PE_MONO)) /* Stereo sample */
	{
		count *= 2;
		len *= 2;
	}

	if (dpm.encoding & PE_16BIT)
		len *= 2;
	else{
		ctl->cmsg(CMSG_ERROR, VERB_NORMAL,
		  "Sorry, not support 8bit sound.");
		ExitToShell();
	}
	
	s32tos16 (buf, count);	/*power mac always 16bit*/
	
	if( count>MACBUFSIZE)
		mac_ErrorExit("\pTiMidity Error--sound packet is too large");	
	err= SetupSndHeader((SndListHandle)soundHandle[nextBuf],
			dpm.encoding & PE_MONO? 1:2,
					dpm.rate<<16, 16, 'NONE',
							60, len, &headerLen);
	if( err )
		mac_ErrorExit("\pTiMidity Error--Cannot make Sound");
	BlockMoveData(buf, *(soundHandle[nextBuf])+headerLen, len);
	
	err= GetSoundHeaderOffset((SndListHandle)(soundHandle[nextBuf]), &offset);
	if( err )
		mac_ErrorExit("\pTiMidity Error--Cannot make Sound");
	theCmd.cmd= bufferCmd;
	theCmd.param2=(long)( *(soundHandle[nextBuf])+offset);
	
	QuingSndCommand(gSndCannel, &theCmd);
	nextBuf++;	if( nextBuf>=MACBUFNUM ) nextBuf=0;

	theCmd.cmd= callBackCmd;	// post set
	theCmd.param1= 0;
	theCmd.param2= samples;
	QuingSndCommand(gSndCannel, &theCmd);
	play_mode->extra_param[0] += samples;
}

static void close_output (void)
{
	int i;
	
	purge_output();
	
	for( i=0; i<MACBUFNUM; i++)
		DisposeHandle(soundHandle[i]);
	
	SndDisposeChannel(gSndCannel, 0); gSndCannel=0;
	initCounter();
}

static int flush_output (void)
{
	int			ret=RC_NONE;
	SndCommand	theCmd;

	flushing_flag=1;
	theCmd.cmd= callBackCmd;
	theCmd.param1= 0;
	theCmd.param2= FLUSH_END;
	QuingSndCommand(gSndCannel, &theCmd);
	
	filling_end();
	for(;;){
		trace_loop();
		YieldToAnyThread();
		//ctl->current_time(current_samples());
   		if( ! flushing_flag ){ //end of midi
   			ret= RC_NONE;
   			break;
   		}else if( mac_rc!=RC_NONE ){
  			ret= mac_rc;
  			break;
   		}
   	}
   	initCounter();
   	return ret;
}

static void fade_output()
{
	unsigned int	fade_start_tick=TickCount();
	int				i;
	SndCommand		theCmd;
	
	for( i=0; i<=30; i++ ){
		theCmd.cmd=ampCmd;
		theCmd.param1=mac_amplitude*(30-i)/30;		/*amplitude->0*/
		SndDoImmediate(gSndCannel, &theCmd);
		while( TickCount() < fade_start_tick+i )
				YieldToAnyThread();
	}
}

static void purge_output (void)
{
	OSErr		err;
	SndCommand	theCmd;
	
	if( skin_state==PLAYING ) fade_output();
	theCmd.cmd=flushCmd;	/*clear buffer*/
	err= SndDoImmediate(gSndCannel, &theCmd);
	theCmd.cmd=quietCmd;
	err= SndDoImmediate(gSndCannel, &theCmd);

	theCmd.cmd=ampCmd;
	theCmd.param1=0;		/*amplitude->0*/
	SndDoImmediate(gSndCannel, &theCmd);

	theCmd.cmd=resumeCmd;
	err= SndDoImmediate(gSndCannel, &theCmd);
	
	theCmd.cmd=waitCmd;
	theCmd.param1=2000*0.5; /* wait 0.5 sec */
	SndDoCommand(gSndCannel, &theCmd, 1);

	theCmd.cmd=ampCmd;
	theCmd.param1=mac_amplitude;
	SndDoCommand(gSndCannel, &theCmd,0);
	
	filling_end();
	initCounter();
}

static int play_loop(void)
{
	return 0;
}

static int32 current_samples(void)
{
	return play_counter;
}
/* ************************************************************* */


