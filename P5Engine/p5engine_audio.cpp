
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
	PlayingSound->dSample = 1.0f;
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
ChangePitch(audio_state* AudioSats, playing_sound* Sound, f32 dSample)
{
	Sound->dSample = dSample;
}

internal void
OutputPlayingSounds(audio_state* AudioState, game_sound_output_buffer* SoundBuffer, game_assets* Assets, memory_arena* TempArena)
{
	temporary_memory MixerMemory = BeginTemporaryMemory(TempArena);

	Assert((SoundBuffer->SampleCount & 7) == 0);
	u32 SampleCount8 = SoundBuffer->SampleCount / 8;
	u32 SampleCount4 = SoundBuffer->SampleCount / 4;

	__m128* RealChannel0 = PushArray(TempArena, SampleCount4, __m128, 16);
	__m128* RealChannel1 = PushArray(TempArena, SampleCount4, __m128, 16);

	f32 SecondsPerSample = 1.0f / (f32)SoundBuffer->SamplesPerSecond;
	i32 const AudioStateOutputChannelCount = 2;

	// NOTE: Clear out the mixer channels
	__m128 Zero = _mm_set1_ps(0.0f);
	{
		__m128* Dest0 = RealChannel0;
		__m128* Dest1 = RealChannel1;
		for (u32 SampleIndex = 0; SampleIndex < SampleCount4; ++SampleIndex)
		{
			_mm_store_ps((float*)Dest0++, Zero);
			_mm_store_ps((float*)Dest1++, Zero);
		}
	}

	// NOTE: Sum all sounds
	for (playing_sound** PlayingSoundPtr = &AudioState->FirstPlayingSound; *PlayingSoundPtr; )
	{
		playing_sound* PlayingSound = *PlayingSoundPtr;
		b32 SoundFinished = false;

		u32 TotalSamplesToMix8 = SampleCount8;
		__m128* Dest0 = RealChannel0;
		__m128* Dest1 = RealChannel1;

		while (TotalSamplesToMix8 && !SoundFinished)
		{
			loaded_sound* LoadedSound = GetSound(Assets, PlayingSound->ID);
			if (LoadedSound)
			{
				asset_sound_info* Info = GetSoundInfo(Assets, PlayingSound->ID);
				PrefetchSound(Assets, Info->NextIDToPlay);

				v2 Volume = PlayingSound->CurrentVolume;
				v2 dVolume = SecondsPerSample * PlayingSound->dCurrentVolume;
				v2 dVolume8 = 8.0f * dVolume;
				f32 dSample = PlayingSound->dSample;
				f32 dSample8 = 8.0f * dSample;

				__m128 MasterVolume4_0 = _mm_set1_ps(AudioState->MasterVolume.E[0]);
				__m128 MasterVolume4_1 = _mm_set1_ps(AudioState->MasterVolume.E[1]);
				__m128 Volume4_0 = _mm_setr_ps(Volume.E[0] + 0.0f * dVolume.E[0],
											   Volume.E[0] + 1.0f * dVolume.E[0], 
											   Volume.E[0] + 2.0f * dVolume.E[0], 
											   Volume.E[0] + 3.0f * dVolume.E[0]);
				__m128 dVolume4_0 = _mm_set1_ps(dVolume.E[0]);
				__m128 dVolume84_0 = _mm_set1_ps(dVolume8.E[0]);
				__m128 Volume4_1 = _mm_setr_ps(Volume.E[1] + 0.0f * dVolume.E[1],
											   Volume.E[1] + 1.0f * dVolume.E[1],
											   Volume.E[1] + 2.0f * dVolume.E[1],
											   Volume.E[1] + 3.0f * dVolume.E[1]);
				__m128 dVolume4_1 = _mm_set1_ps(dVolume.E[1]);
				__m128 dVolume84_1 = _mm_set1_ps(dVolume8.E[1]);

				Assert(PlayingSound->SamplesPlayed >= 0);

				u32 SamplesToMix8 = TotalSamplesToMix8;
				f32 RealSamplesRemainingInSound8 = (LoadedSound->SampleCount - PlayingSound->SamplesPlayed) / dSample8;
				u32 SamplesRemainingInSound8 = RoundReal32ToInt32(RealSamplesRemainingInSound8);
				if (SamplesToMix8 > SamplesRemainingInSound8)
				{
					SamplesToMix8 = SamplesRemainingInSound8;
				}

				b32 VolumeEnded[AudioStateOutputChannelCount] = {};
				for (u32 ChannelIndex = 0; ChannelIndex < ArrayCount(VolumeEnded); ++ChannelIndex)
				{
					// TODO: Fixe the "both volumes end at the same time" bug
					if (dVolume8.E[ChannelIndex] != 0.0f)
					{
						f32 DeltaVolume = (PlayingSound->TargetVolume.E[ChannelIndex] - Volume.E[ChannelIndex]);
						u32 VolumeSampleCount8 = (u32)((DeltaVolume / dVolume8.E[ChannelIndex]) + 0.5f);
						if (SamplesToMix8 > VolumeSampleCount8)
						{
							SamplesToMix8 = VolumeSampleCount8;
							VolumeEnded[ChannelIndex] = true;
						}
					}
				}

				// TODO: Handle stereo
				f32 SamplePosition = PlayingSound->SamplesPlayed;
				for (u32 LoopIndex = 0; LoopIndex < SamplesToMix8; ++LoopIndex)
				{
#if 0
					f32 OffsetSamplePosition = SamplePosition + (f32)SampleOffset * dSample;
					u32 SampleIndex = FloorReal32ToInt32(OffsetSamplePosition);
					f32 Frac = OffsetSamplePosition - (f32)SampleIndex;

					f32 Sample0 = (f32)LoadedSound->Samples[0][SampleIndex];
					f32 Sample1 = (f32)LoadedSound->Samples[0][SampleIndex + 1];
					f32 SampleValue = Lerp(Sample0, Frac, Sample1);
#else
					__m128 SampleValue_0 = _mm_setr_ps(LoadedSound->Samples[0][RoundReal32ToInt32(SamplePosition + 0.0f * dSample)],
													   LoadedSound->Samples[0][RoundReal32ToInt32(SamplePosition + 1.0f * dSample)], 
													   LoadedSound->Samples[0][RoundReal32ToInt32(SamplePosition + 2.0f * dSample)], 
													   LoadedSound->Samples[0][RoundReal32ToInt32(SamplePosition + 3.0f * dSample)]);
					__m128 SampleValue_1 = _mm_setr_ps(LoadedSound->Samples[0][RoundReal32ToInt32(SamplePosition + 4.0f * dSample)],
													   LoadedSound->Samples[0][RoundReal32ToInt32(SamplePosition + 5.0f * dSample)],
													   LoadedSound->Samples[0][RoundReal32ToInt32(SamplePosition + 6.0f * dSample)],
													   LoadedSound->Samples[0][RoundReal32ToInt32(SamplePosition + 7.0f * dSample)]);
#endif
					__m128 D0_0 = _mm_load_ps((float*)&Dest0[0]);
					__m128 D0_1 = _mm_load_ps((float*)&Dest0[1]);
					__m128 D1_0 = _mm_load_ps((float*)&Dest1[0]);
					__m128 D1_1 = _mm_load_ps((float*)&Dest1[1]);

					D0_0 = _mm_add_ps(D0_0, _mm_mul_ps(_mm_mul_ps(MasterVolume4_0, Volume4_0), SampleValue_0));
					D0_1 = _mm_add_ps(D0_1, _mm_mul_ps(_mm_mul_ps(MasterVolume4_0, _mm_add_ps(dVolume4_0, Volume4_0)), SampleValue_1));
					D1_0 = _mm_add_ps(D1_0, _mm_mul_ps(_mm_mul_ps(MasterVolume4_1, Volume4_1), SampleValue_0));
					D1_1 = _mm_add_ps(D1_1, _mm_mul_ps(_mm_mul_ps(MasterVolume4_1, _mm_add_ps(dVolume4_1, Volume4_1)), SampleValue_1));
					
					_mm_store_ps((float*)&Dest0[0], D0_0);
					_mm_store_ps((float*)&Dest0[1], D0_1);
					_mm_store_ps((float*)&Dest1[0], D1_0);
					_mm_store_ps((float*)&Dest1[1], D1_1);

					Dest0 += 2;
					Dest1 += 2;
					Volume4_0 = _mm_add_ps(Volume4_0, dVolume84_0);
					Volume4_1 = _mm_add_ps(Volume4_1, dVolume84_1);
					Volume += dVolume8;
					SamplePosition += dSample8;
				}

				PlayingSound->CurrentVolume = Volume;
				for (u32 ChannelIndex = 0; ChannelIndex < ArrayCount(VolumeEnded); ++ChannelIndex)
				{
					if (VolumeEnded[ChannelIndex])
					{
						PlayingSound->CurrentVolume.E[ChannelIndex] = PlayingSound->TargetVolume.E[ChannelIndex];
						PlayingSound->dCurrentVolume.E[ChannelIndex] = 0.0f;
					}
				}

				PlayingSound->SamplesPlayed = SamplePosition;
				Assert(TotalSamplesToMix8 >= SamplesToMix8);
				TotalSamplesToMix8 -= SamplesToMix8;

				if ((u32)PlayingSound->SamplesPlayed >= LoadedSound->SampleCount)
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
		__m128* Source0 = RealChannel0;
		__m128* Source1 = RealChannel1;

		__m128i* SampleOut = (__m128i*)SoundBuffer->Samples;
		Assert(SampleCount4 > 0);
		for (u32 SampleIndex = 0; SampleIndex < SampleCount4; ++SampleIndex)
		{
			__m128 S0 = _mm_load_ps((float*)Source0++);
			__m128 S1 = _mm_load_ps((float*)Source1++);

			__m128i L = _mm_cvtps_epi32(S0);
			__m128i R = _mm_cvtps_epi32(S1);

			__m128i LR0 = _mm_unpacklo_epi32(L, R);
			__m128i LR1 = _mm_unpackhi_epi32(L, R);

			__m128i S01 = _mm_packs_epi32(LR0, LR1);

			*SampleOut++ = S01;
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

	AudioState->MasterVolume = V2(1.0f, 1.0f);
}