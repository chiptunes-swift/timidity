#ifndef ___PLAYMIDI_H_
#define ___PLAYMIDI_H_
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

   playmidi.h

   */

typedef struct {
  int32 time;
  uint8 type, channel, a, b;
} MidiEvent;

#define REVERB_MAX_DELAY_OUT (3 * play_mode->rate / 2)

struct ReverbControls
{
    int32 lastin;
    int level;
    int rvid;
};

#define MIDI_EVENT_NOTE(ep) (ISDRUMCHANNEL((ep)->channel) ? (ep)->a : \
			     (((int)(ep)->a + note_key_offset + \
			       channel[ep->channel].key_shift) & 0x7f))

#define MIDI_EVENT_TIME(ep) ((int32)((ep)->time * midi_time_ratio + 0.5))

/* Midi events */
enum midi_event_t
{
    ME_NONE,

    /* MIDI events */
    ME_NOTEOFF,
    ME_NOTEON,
    ME_KEYPRESSURE,
    ME_PROGRAM,
    ME_CHANNEL_PRESSURE,
    ME_PITCHWHEEL,

    /* Controls */
    ME_TONE_BANK_MSB,
    ME_TONE_BANK_LSB,
    ME_MODULATION_WHEEL,
    ME_MAINVOLUME,
    ME_PAN,
    ME_EXPRESSION,
    ME_SUSTAIN,
    ME_PORTAMENTO_TIME,
    ME_PORTAMENTO,
    ME_DATA_ENTRY_MSB,
#if 0
    ME_DATA_ENTRY_LSB,		/* Not supported */
    ME_SUSTENUTO,		/* Not supported */
    ME_SOFT_PEDAL,		/* Not supported */
    ME_HARMONIC_CONTENT,	/* Not supported */
    ME_RELEASE_TIME,		/* Not supported */
    ME_ATTACK_TIME,		/* Not supported */
    ME_BRIGHTNESS,		/* Not supported */
#endif
    ME_REVERB_EFFECT,
    ME_CHORUS_EFFECT,
#if 0
    ME_VARIATION_EFFECT,	/* Not supported */
#endif
    ME_RPN_INC,
    ME_RPN_DEC,
    ME_NRPN_LSB,
    ME_NRPN_MSB,
    ME_RPN_LSB,
    ME_RPN_MSB,
    ME_ALL_SOUNDS_OFF,
    ME_RESET_CONTROLLERS,
    ME_ALL_NOTES_OFF,
    ME_MONO,
    ME_POLY,

    /* TiMidity Extensionals */
#if 0
    ME_VOLUME_ONOFF,		/* Not supported */
#endif
    ME_RANDOM_PAN,
    ME_SET_PATCH,		/* Install special instrument */
    ME_DRUMPART,
    ME_KEYSHIFT,

    /* Global channel events */

    ME_TEMPO,
    ME_CHORUS_TEXT,
    ME_LYRIC,
    ME_MARKER,
    ME_INSERT_TEXT, /* for SC */
    ME_TEXT,
    ME_KARAOKE_LYRIC, /* for KAR format */
    ME_MASTER_VOLUME,
    ME_RESET,			/* Reset and change system mode */
    ME_NOTE_STEP,

    ME_PATCH_OFFS,		/* Change special instrument sample position
				 * SampleID, LSB, MSB
				 */
    ME_WRD,			/* for WRD tracer */
    ME_TIMESIG,			/* Time signature */

    ME_LAST = 254,		/* Last sequence of MIDI list */
    ME_EOT = 255		/* End of MIDI */
};

#define GLOBAL_CHANNEL_EVENT_TYPE(type)	\
	((type) == ME_NONE || (type) >= ME_TEMPO)

enum rpn_data_address_t /* NRPN/RPN */
{
    NRPN_ADDR_0108,
    NRPN_ADDR_0109,
    NRPN_ADDR_010A,
    NRPN_ADDR_0120,
    NRPN_ADDR_0121,
    NRPN_ADDR_0163,
    NRPN_ADDR_0164,
    NRPN_ADDR_0166,
    NRPN_ADDR_1400,
    NRPN_ADDR_1500,
    NRPN_ADDR_1600,
    NRPN_ADDR_1700,
    NRPN_ADDR_1800,
    NRPN_ADDR_1900,
    NRPN_ADDR_1A00,
    NRPN_ADDR_1C00,
    NRPN_ADDR_1D00,
    NRPN_ADDR_1E00,
    NRPN_ADDR_1F00,
    RPN_ADDR_0000,
    RPN_ADDR_0001,
    RPN_ADDR_0002,
    RPN_ADDR_7F7F,
    RPN_ADDR_FFFF,
    RPN_MAX_DATA_ADDR
};

struct DrumParts
{
    int drum_panning;
    int32 drum_envelope_rate[6]; /* Not supported */

/* Not supported:
 * Drum Filter Cutoff
 * Drum Filter Resonance
 * Drum Attack Time
 * Drum Decay Time
 * Coarse Pitch of Drum/Fine Pitch of Drum
 * Level of Drum
 * Reverb Send Level of Drum
 * Chorus Send Level of Drum
 * Variation Send Level of Drum
 */
};

typedef struct {
  int8 bank_msb, bank_lsb, bank, program, volume,
	expression, sustain, panning, mono, portamento, modulation_wheel;

  uint8 chorus_level;
  /* Special sample ID. (0 means Normal sample) */
  uint8 special_sample;
  int8 key_shift;

  int pitchbend;

  /* chorus, reverb... Coming soon to a 300-MHz, eight-way superscalar
     processor near you */

  FLOAT_T
    pitchfactor; /* precomputed pitch bend factor to save some fdiv's */

  /* for portamento */
  int porta_control_ratio, porta_dpb;
  int32 portamento_time, last_note_fine;

  struct DrumParts *drums[128];

  int32 vibrato_ratio, vibrato_delay;
  int vibrato_depth;

  uint8 rpnmap[RPN_MAX_DATA_ADDR]; /* pseudo RPN address map */
  uint8 lastlrpn, lastmrpn,
      nrpn; /* 0:RPN, 1:NRPN */
  struct ReverbControls *rb;

  /* Not used current version */
  int32 envelope_rate[6]; /* for Envelope Generator
			   * 0: Attack rate
			   * 1: Decay rate
			   * 3: Release rate
			   */
  int mapID;
  AlternateAssign *altassign;
  int rpn_7f7f_flag;
} Channel;

/* Causes the instrument's default panning to be used. */
#define NO_PANNING -1

typedef struct {
  uint8
    status, channel, note, velocity;
  int vid;
  Sample *sample;
  int32
    orig_frequency, frequency,
    sample_offset, sample_increment,
    envelope_volume, envelope_target, envelope_increment,
    tremolo_sweep, tremolo_sweep_position,
    tremolo_phase, tremolo_phase_increment,
    vibrato_sweep, vibrato_sweep_position;

  final_volume_t left_mix, right_mix;

  FLOAT_T
    left_amp, right_amp, tremolo_volume;
  int32
    vibrato_sample_increment[VIBRATO_SAMPLE_INCREMENTS], vibrato_delay;
  int
    vibrato_phase, orig_vibrato_control_ratio, vibrato_control_ratio,
    vibrato_depth, vibrato_control_counter,
    envelope_stage, control_counter, panning, panned, modulation_wheel;

  /* for portamento */
  int porta_control_ratio, porta_control_counter, porta_dpb;
  int32 porta_pb;

  int delay; /* Note ON delay samples */
  int32 timeout;
  struct cache_hash *cache;
} Voice;

/* Voice status options: */
#define VOICE_FREE	(1<<0)
#define VOICE_ON	(1<<1)
#define VOICE_SUSTAINED	(1<<2)
#define VOICE_OFF	(1<<3)
#define VOICE_DIE	(1<<4)

/* Voice panned options: */
#define PANNED_MYSTERY 0
#define PANNED_LEFT 1
#define PANNED_RIGHT 2
#define PANNED_CENTER 3
/* Anything but PANNED_MYSTERY only uses the left volume */

#define ISDRUMCHANNEL(c)  IS_SET_CHANNELMASK(drumchannels, c)

extern Channel channel[];
extern Voice voice[];

extern int32 control_ratio, amp_with_poly, amplification;

extern ChannelBitMask default_drumchannel_mask;
extern ChannelBitMask drumchannel_mask;
extern ChannelBitMask default_drumchannels;
extern ChannelBitMask drumchannels;

extern int adjust_panning_immediately;
extern int voices, upper_voices;
extern int note_key_offset;
extern FLOAT_T midi_time_ratio;
extern int opt_modulation_wheel;
extern int opt_portamento;
extern int opt_nrpn_vibrato;
extern int opt_reverb_control;
extern int opt_chorus_control;
extern int opt_channel_pressure;
extern int opt_overlap_voice_allow;
extern int32 current_play_tempo;
extern int opt_realtime_playing;
extern int check_eot_flag;
extern int special_tonebank;
extern int default_tonebank;
extern int playmidi_seek_flag;

extern int play_midi_file(char *fn);
extern void dumb_pass_playing_list(int number_of_files, char *list_of_files[]);
extern void default_ctl_lyric(int lyricid);
extern int check_apply_control(void);
extern void recompute_freq(int v);
extern int midi_drumpart_change(int ch, int isdrum);
extern void ctl_note_event(int noteID);
extern void ctl_mode_event(int type, int trace, long arg1, long arg2);
extern char *channel_instrum_name(int ch);

#endif /* ___PLAYMIDI_H_ */
