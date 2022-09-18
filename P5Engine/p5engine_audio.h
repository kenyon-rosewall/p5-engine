#pragma once

#ifndef P5ENGINE_AUDIO_H
#define P5ENGINE_AUDIO_H

struct playing_sound
{
	v2 CurrentVolume;
	v2 dCurrentVolume;
	v2 TargetVolume;

	sound_id ID;
	i32 SamplesPlayed;
	playing_sound* Next;
};

struct audio_state
{
	memory_arena* PermArena;
	playing_sound* FirstPlayingSound;
	playing_sound* FirstFreePlayingSound;
};

#endif // P5ENGINE_AUDIO_H	