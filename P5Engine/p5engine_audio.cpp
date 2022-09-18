
internal void
OutputTestSineWave(game_state* GameState, game_sound_output_buffer* SoundBuffer, int ToneHz)
{
	s16 ToneVolume = 2000;
	int WavePeriod = SoundBuffer->SamplesPerSecond / ToneHz;

	s16* SampleOut = SoundBuffer->Samples;
	for (int SampleIndex = 0; SampleIndex < SoundBuffer->SampleCount; ++SampleIndex)
	{
#if 1
		f32 SineValue = sinf(GameState->tSine);
		s16 SampleValue = (s16)(SineValue * ToneVolume);
#else
		int SampleValue = 0;
#endif

		*SampleOut++ = SampleValue;
		*SampleOut++ = SampleValue;

#if 1
		GameState->tSine += Tau32 * 1.0f / (f32)WavePeriod;
		if (GameState->tSine > Tau32)
		{
			GameState->tSine -= Tau32;
		}
#endif
	}
}

internal playing_sound*
PlaySound(audio_state* AudioState, sound_id SoundID)
{
	if (!AudioState->FirstFreePlayingSound)
	{
		AudioState->FirstFreePlayingSound = PushStruct(AudioState->PermArena, playing_sound);
		AudioState->FirstFreePlayingSound->Next = 0;
	}

	playing_sound* PlayingSound = AudioState->FirstFreePlayingSound;
	AudioState->FirstFreePlayingSound = PlayingSound->Next;

	PlayingSound->SamplesPlayed = 0;
	PlayingSound->Volume[0] = 1.0f;
	PlayingSound->Volume[1] = 1.0f;
	PlayingSound->ID = SoundID;

	PlayingSound->Next = AudioState->FirstPlayingSound;
	AudioState->FirstPlayingSound = PlayingSound;

	return(PlayingSound);
}

internal void
OutputPlayingSounds(audio_state* AudioState, game_sound_output_buffer* SoundBuffer, game_assets* Assets, memory_arena* TempArena)
{
	temporary_memory MixerMemory = BeginTemporaryMemory(TempArena);

	f32* RealChannel0 = PushArray(TempArena, SoundBuffer->SampleCount, f32);
	f32* RealChannel1 = PushArray(TempArena, SoundBuffer->SampleCount, f32);

	// NOTE: Clear out the mixer channels
	{
		f32* Dest0 = RealChannel0;
		f32* Dest1 = RealChannel1;
		for (int SampleIndex = 0; SampleIndex < SoundBuffer->SampleCount; ++SampleIndex)
		{
			*Dest0++ = 0.0f;
			*Dest1++ = 0.0f;
		}
	}

	// NOTE: Sum all sounds
	for (playing_sound** PlayingSoundPtr = &AudioState->FirstPlayingSound; *PlayingSoundPtr; )
	{
		playing_sound* PlayingSound = *PlayingSoundPtr;
		b32 SoundFinished = false;

		u32 TotalSamplesToMix = SoundBuffer->SampleCount;
		f32* Dest0 = RealChannel0;
		f32* Dest1 = RealChannel1;

		while (TotalSamplesToMix && !SoundFinished)
		{
			loaded_sound* LoadedSound = GetSound(Assets, PlayingSound->ID);
			if (LoadedSound)
			{
				asset_sound_info* Info = GetSoundInfo(Assets, PlayingSound->ID);
				PrefetchSound(Assets, Info->NextIDToPlay);

				// TODO: Handle stereo
				f32 Volume0 = PlayingSound->Volume[0];
				f32 Volume1 = PlayingSound->Volume[1];

				Assert(PlayingSound->SamplesPlayed >= 0);

				u32 SamplesToMix = TotalSamplesToMix;
				u32 SamplesRemainingInSound = LoadedSound->SampleCount - PlayingSound->SamplesPlayed;
				if (SamplesToMix > SamplesRemainingInSound)
				{
					SamplesToMix = SamplesRemainingInSound;
				}

				for (u32 SampleIndex = PlayingSound->SamplesPlayed; SampleIndex < (PlayingSound->SamplesPlayed + SamplesToMix); ++SampleIndex)
				{
					f32 SampleValue = LoadedSound->Samples[0][SampleIndex];
					*Dest0++ += Volume0 * SampleValue;
					*Dest1++ += Volume1 * SampleValue;
				}

				Assert(TotalSamplesToMix >= SamplesToMix);
				PlayingSound->SamplesPlayed += SamplesToMix;
				TotalSamplesToMix -= SamplesToMix;

				if ((u32)PlayingSound->SamplesPlayed == LoadedSound->SampleCount)
				{
					if (IsValid(Info->NextIDToPlay))
					{
						PlayingSound->ID = Info->NextIDToPlay;
						PlayingSound->SamplesPlayed = 0;
					}
					else
					{
						SoundFinished = true;
					}
				}
				else
				{
					Assert(TotalSamplesToMix == 0);
				}
			}
			else
			{
				LoadSound(Assets, PlayingSound->ID);
				break;
			}

			if (SoundFinished)
			{
				*PlayingSoundPtr = PlayingSound->Next;
				PlayingSound->Next = AudioState->FirstFreePlayingSound;
				AudioState->FirstFreePlayingSound = PlayingSound;
			}
			else
			{
				PlayingSoundPtr = &PlayingSound->Next;
			}
		}
	}

	// NOTE: Convert to 16-bit
	{
		f32* Source0 = RealChannel0;
		f32* Source1 = RealChannel1;

		s16* SampleOut = SoundBuffer->Samples;
		for (int SampleIndex = 0; SampleIndex < SoundBuffer->SampleCount; ++SampleIndex)
		{
			*SampleOut++ = (s16)(*Source0++ + 0.5f);
			*SampleOut++ = (s16)(*Source1++ + 0.5f);
		}
	}

	EndTemporaryMemory(MixerMemory);
}

internal void
InitializeAudioState(audio_state* AudioState, memory_arena* Arena)
{
	AudioState->PermArena = Arena;
	AudioState->FirstFreePlayingSound = 0;
	AudioState->FirstPlayingSound = 0;
}