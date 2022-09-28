
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

	Assert((SoundBuffer->SampleCount & 3) == 0);
	u32 ChunkCount = SoundBuffer->SampleCount / 4;

	__m128* RealChannel0 = PushArray(TempArena, ChunkCount, __m128, 16);
	__m128* RealChannel1 = PushArray(TempArena, ChunkCount, __m128, 16);

	f32 SecondsPerSample = 1.0f / (f32)SoundBuffer->SamplesPerSecond;
	i32 const AudioStateOutputChannelCount = 2;

	__m128 Zero = _mm_set1_ps(0.0f);
	__m128 One = _mm_set1_ps(1.0f);

	// NOTE: Clear out the mixer channels
	{
		__m128* Dest0 = RealChannel0;
		__m128* Dest1 = RealChannel1;
		for (u32 SampleIndex = 0; SampleIndex < ChunkCount; ++SampleIndex)
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

		u32 TotalChunksToMix = ChunkCount;
		__m128* Dest0 = RealChannel0;
		__m128* Dest1 = RealChannel1;

		while (TotalChunksToMix && !SoundFinished)
		{
			loaded_sound* LoadedSound = GetSound(Assets, PlayingSound->ID);
			if (LoadedSound)
			{
				sound_id NextSoundInChain = GetNextSoundInChain(Assets, PlayingSound->ID);
				PrefetchSound(Assets, NextSoundInChain);

				v2 Volume = PlayingSound->CurrentVolume;
				v2 dVolume = SecondsPerSample * PlayingSound->dCurrentVolume;
				v2 dVolumeChunk = 4.0f * dVolume;
				f32 dSample = PlayingSound->dSample;
				f32 dSampleChunk = 4.0f * dSample;

				// NOTE: Channel 0
				__m128 MasterVolume0 = _mm_set1_ps(AudioState->MasterVolume.E[0]);
				__m128 Volume0 = _mm_setr_ps(Volume.E[0] + 0.0f * dVolume.E[0],
					Volume.E[0] + 1.0f * dVolume.E[0],
					Volume.E[0] + 2.0f * dVolume.E[0],
					Volume.E[0] + 3.0f * dVolume.E[0]);
				__m128 dVolume0 = _mm_set1_ps(dVolume.E[0]);
				__m128 dVolumeChunk0 = _mm_set1_ps(dVolumeChunk.E[0]);

				// NOTE: Channel 1
				__m128 MasterVolume1 = _mm_set1_ps(AudioState->MasterVolume.E[1]);
				__m128 Volume1 = _mm_setr_ps(Volume.E[1] + 0.0f * dVolume.E[1],
					Volume.E[1] + 1.0f * dVolume.E[1],
					Volume.E[1] + 2.0f * dVolume.E[1],
					Volume.E[1] + 3.0f * dVolume.E[1]);
				__m128 dVolume1 = _mm_set1_ps(dVolume.E[1]);
				__m128 dVolumeChunk1 = _mm_set1_ps(dVolumeChunk.E[1]);

				Assert(PlayingSound->SamplesPlayed >= 0.0f);

				u32 ChunksToMix = TotalChunksToMix;
				f32 RealChunksRemainingInSound = (LoadedSound->SampleCount - RoundReal32ToInt32(PlayingSound->SamplesPlayed)) / dSampleChunk;
				u32 ChunksRemainingInSound = RoundReal32ToInt32(RealChunksRemainingInSound);
				b32 InputSamplesEnded = false;
				if (ChunksToMix > ChunksRemainingInSound)
				{
					ChunksToMix = ChunksRemainingInSound;
				}

				u32 VolumeEndsAt[AudioStateOutputChannelCount] = {};
				for (u32 ChannelIndex = 0; ChannelIndex < ArrayCount(VolumeEndsAt); ++ChannelIndex)
				{
					// TODO: Fixe the "both volumes end at the same time" bug
					if (dVolumeChunk.E[ChannelIndex] != 0.0f)
					{
						f32 DeltaVolume = (PlayingSound->TargetVolume.E[ChannelIndex] - Volume.E[ChannelIndex]);
						u32 VolumeChunkCount = (u32)((DeltaVolume / dVolumeChunk.E[ChannelIndex]) + 0.5f);
						if (ChunksToMix > VolumeChunkCount)
						{
							ChunksToMix = VolumeChunkCount;
							VolumeEndsAt[ChannelIndex] = VolumeChunkCount;
						}
					}
				}

				// TODO: Handle stereo
				f32 BeginSamplePosition = PlayingSound->SamplesPlayed;
				f32 EndSamplePosition = BeginSamplePosition + ChunksToMix * dSampleChunk;
				f32 LoopIndexC = (EndSamplePosition - BeginSamplePosition) / (f32)ChunksToMix;
				for (u32 LoopIndex = 0; LoopIndex < ChunksToMix; ++LoopIndex)
				{
					f32 SamplePosition = BeginSamplePosition + LoopIndexC * (f32)LoopIndex;
#if 1
					__m128 SamplePos = _mm_setr_ps(SamplePosition + 0.0f * dSample,
						SamplePosition + 1.0f * dSample,
						SamplePosition + 2.0f * dSample,
						SamplePosition + 3.0f * dSample);
					__m128i SampleIndex = _mm_cvttps_epi32(SamplePos);
					__m128 Frac = _mm_sub_ps(SamplePos, _mm_cvtepi32_ps(SampleIndex));

					__m128 SampleValueF = _mm_setr_ps(LoadedSound->Samples[0][((i32*)&SampleIndex)[0]],
						LoadedSound->Samples[0][((i32*)&SampleIndex)[1]],
						LoadedSound->Samples[0][((i32*)&SampleIndex)[2]],
						LoadedSound->Samples[0][((i32*)&SampleIndex)[3]]);
					__m128 SampleValueC = _mm_setr_ps(LoadedSound->Samples[0][((i32*)&SampleIndex)[0] + 1],
						LoadedSound->Samples[0][((i32*)&SampleIndex)[1] + 1],
						LoadedSound->Samples[0][((i32*)&SampleIndex)[2] + 1],
						LoadedSound->Samples[0][((i32*)&SampleIndex)[3] + 1]);

					__m128 SampleValue = _mm_add_ps(_mm_mul_ps(_mm_sub_ps(One, Frac), SampleValueF),
						_mm_mul_ps(Frac, SampleValueC));
#else
					__m128 SampleValue = _mm_setr_ps(LoadedSound->Samples[0][RoundReal32ToInt32(SamplePosition + 0.0f * dSample)],
						LoadedSound->Samples[0][RoundReal32ToInt32(SamplePosition + 1.0f * dSample)],
						LoadedSound->Samples[0][RoundReal32ToInt32(SamplePosition + 2.0f * dSample)],
						LoadedSound->Samples[0][RoundReal32ToInt32(SamplePosition + 3.0f * dSample)]);
#endif
					__m128 D0 = _mm_load_ps((float*)&Dest0[0]);
					__m128 D1 = _mm_load_ps((float*)&Dest1[0]);

					D0 = _mm_add_ps(D0, _mm_mul_ps(_mm_mul_ps(MasterVolume0, Volume0), SampleValue));
					D1 = _mm_add_ps(D1, _mm_mul_ps(_mm_mul_ps(MasterVolume1, Volume1), SampleValue));

					_mm_store_ps((float*)&Dest0[0], D0);
					_mm_store_ps((float*)&Dest1[0], D1);

					++Dest0;
					++Dest1;
					Volume0 = _mm_add_ps(Volume0, dVolumeChunk0);
					Volume1 = _mm_add_ps(Volume1, dVolumeChunk1);
				}

				PlayingSound->CurrentVolume.E[0] = ((f32*)&Volume0)[0];
				PlayingSound->CurrentVolume.E[1] = ((f32*)&Volume0)[1];
				for (u32 ChannelIndex = 0; ChannelIndex < ArrayCount(VolumeEndsAt); ++ChannelIndex)
				{
					if (ChunksToMix == VolumeEndsAt[ChannelIndex])
					{
						PlayingSound->CurrentVolume.E[ChannelIndex] = PlayingSound->TargetVolume.E[ChannelIndex];
						PlayingSound->dCurrentVolume.E[ChannelIndex] = 0.0f;
					}
				}

				PlayingSound->SamplesPlayed = EndSamplePosition;
				Assert(TotalChunksToMix >= ChunksToMix);
				TotalChunksToMix -= ChunksToMix;

				if (ChunksToMix == ChunksRemainingInSound)
				{
					if (IsValid(NextSoundInChain))
					{
						PlayingSound->ID = NextSoundInChain;
						Assert(PlayingSound->SamplesPlayed >= LoadedSound->SampleCount);
						PlayingSound->SamplesPlayed -= (f32)LoadedSound->SampleCount;
						if (PlayingSound->SamplesPlayed < 0)
						{
							PlayingSound->SamplesPlayed = 0.0f;
						}
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

	// NOTE: Convert to 16-bit
	{
		__m128* Source0 = RealChannel0;
		__m128* Source1 = RealChannel1;

		__m128i* SampleOut = (__m128i*)SoundBuffer->Samples;
		Assert(ChunkCount > 0);
		for (u32 SampleIndex = 0; SampleIndex < ChunkCount; ++SampleIndex)
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