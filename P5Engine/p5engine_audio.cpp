
internal void
OutputTestSineWave(game_state* GameState, game_sound_output_buffer* SoundBuffer, int ToneHz)
{
	i16 ToneVolume = 2000;
	int WavePeriod = SoundBuffer->SamplesPerSecond / ToneHz;

	i16* SampleOut = SoundBuffer->Samples;
	for (int SampleIndex = 0; SampleIndex < SoundBuffer->SampleCount; ++SampleIndex)
	{
#if 1
		f32 SineValue = sinf(GameState->tSine);
		i16 SampleValue = (i16)(SineValue * ToneVolume);
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
	PlayingSound->CurrentVolume = PlayingSound->TargetVolume = V2(1.0f, 1.0f);
	PlayingSound->dCurrentVolume = V2(0, 0);
	PlayingSound->ID = SoundID;

	PlayingSound->Next = AudioState->FirstPlayingSound;
	AudioState->FirstPlayingSound = PlayingSound;

	return(PlayingSound);
}

internal void
ChangeVolume(audio_state* AudioState, playing_sound* Sound, f32 FadeDurationInSeconds, v2 Volume)
{
	if (FadeDurationInSeconds <= 0.0f)
	{
		Sound->CurrentVolume = Sound->TargetVolume = Volume;
	}
	else
	{
		f32 OneOverFade = 1.0f / FadeDurationInSeconds;
		Sound->TargetVolume = Volume;
		Sound->dCurrentVolume = OneOverFade * (Sound->TargetVolume - Sound->CurrentVolume);
	}
}

internal void
OutputPlayingSounds(audio_state* AudioState, game_sound_output_buffer* SoundBuffer, game_assets* Assets, memory_arena* TempArena)
{
	temporary_memory MixerMemory = BeginTemporaryMemory(TempArena);

	f32* RealChannel0 = PushArray(TempArena, SoundBuffer->SampleCount, f32);
	f32* RealChannel1 = PushArray(TempArena, SoundBuffer->SampleCount, f32);

	f32 SecondsPerSample = 1.0f / (f32)SoundBuffer->SamplesPerSecond;
	i32 const AudioStateOutputChannelCount = 2;

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

				v2 Volume = PlayingSound->CurrentVolume;
				v2 dVolume = SecondsPerSample * PlayingSound->dCurrentVolume;

				Assert(PlayingSound->SamplesPlayed >= 0);

				u32 SamplesToMix = TotalSamplesToMix;
				u32 SamplesRemainingInSound = LoadedSound->SampleCount - PlayingSound->SamplesPlayed;
				if (SamplesToMix > SamplesRemainingInSound)
				{
					SamplesToMix = SamplesRemainingInSound;
				}

				b32 VolumeEnded[AudioStateOutputChannelCount] = {};
				for (u32 ChannelIndex = 0; ChannelIndex < ArrayCount(VolumeEnded); ++ChannelIndex)
				{
					if (dVolume.E[ChannelIndex] != 0.0f)
					{
						f32 DeltaVolume = (PlayingSound->TargetVolume.E[ChannelIndex] - Volume.E[ChannelIndex]);
						u32 VolumeSampleCount = (u32)((DeltaVolume / dVolume.E[ChannelIndex]) + 0.5f);
						if (SamplesToMix > VolumeSampleCount)
						{
							SamplesToMix = VolumeSampleCount;
							VolumeEnded[ChannelIndex] = true;
						}
					}
				}

				// TODO: Handle stereo
				for (u32 SampleIndex = PlayingSound->SamplesPlayed; SampleIndex < (PlayingSound->SamplesPlayed + SamplesToMix); ++SampleIndex)
				{
					f32 SampleValue = LoadedSound->Samples[0][SampleIndex];
					*Dest0++ += Volume.E[0] * SampleValue;
					*Dest1++ += Volume.E[1] * SampleValue;

					Volume += dVolume;
				}

				PlayingSound->CurrentVolume = Volume;

				// TODO: This is not correct yet. Need to truncate the loop.
				for (u32 ChannelIndex = 0; ChannelIndex < ArrayCount(VolumeEnded); ++ChannelIndex)
				{
					if (VolumeEnded[ChannelIndex])
					{
						PlayingSound->CurrentVolume.E[ChannelIndex] = PlayingSound->TargetVolume.E[ChannelIndex];
						PlayingSound->dCurrentVolume.E[ChannelIndex] = 0.0f;
					}
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

		i16* SampleOut = SoundBuffer->Samples;
		for (int SampleIndex = 0; SampleIndex < SoundBuffer->SampleCount; ++SampleIndex)
		{
			*SampleOut++ = (i16)(*Source0++ + 0.5f);
			*SampleOut++ = (i16)(*Source1++ + 0.5f);
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