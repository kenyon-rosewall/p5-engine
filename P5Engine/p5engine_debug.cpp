
// TODO: Stop using stdio
#include <stdio.h>

global_variable f32 LeftEdge;
global_variable f32 AtY;
global_variable f32 FontScale;
global_variable font_id FontID;

internal void
DEBUGReset(game_assets* Assets, u32 Width, u32 Height)
{
	TIMED_BLOCK();

	asset_vector MatchVector = {};
	asset_vector WeightVector = {};
	MatchVector.E[(u32)asset_tag_id::FontType] = (f32)asset_font_type::Debug;
	WeightVector.E[(u32)asset_tag_id::FontType] = 1.0f;
	FontID = GetBestMatchFontFrom(Assets, asset_type_id::Font, &MatchVector, &WeightVector);

	FontScale = 1.0f;
	Orthographic(DEBUGRenderGroup, Width, Height, 1.0f);
	LeftEdge = -0.5f * Width;

	p5a_font* Info = GetFontInfo(Assets, FontID);
	AtY = 0.5 * Height - FontScale * GetStartingBaselineY(Info);
}

inline b32
IsHex(char Char)
{
	b32 Result = (((Char >= '0') && (Char <= '9')) ||
		((Char >= 'A') && (Char <= 'F')));

	return(Result);
}

inline u32
GetHex(char Char)
{
	u32 Result = 0;

	if ((Char >= '0') && (Char <= '9'))
	{
		Result = Char - '0';
	}
	else if ((Char >= 'A') && (Char <= 'F'))
	{
		Result = 0xA + (Char - 'A');
	}

	return(Result);
}

internal void
DEBUGTextLine(char* String)
{
	if (DEBUGRenderGroup)
	{
		render_group* RenderGroup = DEBUGRenderGroup;

		loaded_font* Font = PushFont(RenderGroup, FontID);
		if (Font)
		{
			p5a_font* Info = GetFontInfo(RenderGroup->Assets, FontID);

			u32 PreviousCodePoint = 0;
			f32 CharScale = FontScale;
			v4 Color = V4(1, 1, 1, 1);
			f32 AtX = LeftEdge;
			for (char* At = String;
				*At;
				)
			{
				if ((At[0] == '\\') &&
					(At[1] == '#') &&
					(At[2] != 0) &&
					(At[3] != 0) &&
					(At[4] != 0))
				{
					f32 CScale = 1.0f / 9.0f;
					Color = V4(
						Clamp01(CScale * (f32)(At[2] - '0')),
						Clamp01(CScale * (f32)(At[3] - '0')),
						Clamp01(CScale * (f32)(At[4] - '0')),
						1.0f
					);

					At += 5;
				}
				else if ((At[0] == '\\') &&
					(At[1] == '^') &&
					(At[2] != 0))
				{
					f32 CScale = 1.0f / 9.0f;
					CharScale = FontScale * Clamp01(CScale * (f32)(At[2] - '0'));

					At += 3;
				}
				else
				{
					u32 Codepoint = *At;

					if ((At[0] == '\\') &&
						(IsHex(At[1])) &&
						(IsHex(At[2])) &&
						(IsHex(At[3])) &&
						(IsHex(At[4])))
					{
						Codepoint = ((GetHex(At[1]) << 12) |
							(GetHex(At[2]) << 8) |
							(GetHex(At[3]) << 4) |
							(GetHex(At[4]) << 0));

						At += 4;
					}

					f32 AdvanceX = CharScale * GetHorizontalAdvanceForPair(Info, Font, PreviousCodePoint, Codepoint);
					AtX += AdvanceX;

					if (Codepoint != ' ')
					{
						bitmap_id BitmapID = GetBitmapForGlyph(RenderGroup->Assets, Info, Font, Codepoint);
						p5a_bitmap* Info = GetBitmapInfo(RenderGroup->Assets, BitmapID);
						PushBitmap(RenderGroup, BitmapID, V3(AtX, AtY, 0), CharScale * (f32)Info->Dim[1], Color);
					}

					PreviousCodePoint = Codepoint;
					++At;
				}
			}

			AtY -= GetLineAdvanceFor(Info) * FontScale;
		}
	}
}

struct debug_statistic
{
	f64 Min;
	f64 Max;
	f64 Avg;
	u32 Count;
};

inline void
BeginDebugStatistic(debug_statistic* Stat)
{
	Stat->Min = Real32Maximum;
	Stat->Max = -Real32Maximum;
	Stat->Avg = 0.0f;
	Stat->Count = 0;
}

inline void
AccumDebugStatistic(debug_statistic* Stat, f64 Value)
{
	++Stat->Count;

	if (Stat->Min > Value)
	{
		Stat->Min = Value;
	}

	if (Stat->Max < Value)
	{
		Stat->Max = Value;
	}

	Stat->Avg += Value;
}

inline void
EndDebugStatistic(debug_statistic* Stat)
{
	if (Stat->Count != 0)
	{
		Stat->Avg /= (f64)Stat->Count;
	}
	else
	{
		Stat->Min = 0.0f;
		Stat->Max = 0.0f;
	}
}

internal void
DEBUGOverlay(game_memory* Memory)
{
	debug_state* DebugState = (debug_state*)Memory->DebugStorage;
	if (DebugState && DEBUGRenderGroup)
	{
		render_group* RenderGroup = DEBUGRenderGroup;

		// TODO: Layout / cached font info / etc. for real debug display
		loaded_font* Font = PushFont(RenderGroup, FontID);

		if (Font)
		{
			p5a_font* Info = GetFontInfo(RenderGroup->Assets, FontID);
			for (u32 CounterIndex = 0;
				CounterIndex < DebugState->CounterCount;
				++CounterIndex)
			{
				debug_counter_state* Counter = DebugState->CounterStates + CounterIndex;

				debug_statistic HitCount, CycleCount, CycleOverHit;
				BeginDebugStatistic(&HitCount);
				BeginDebugStatistic(&CycleCount);
				BeginDebugStatistic(&CycleOverHit);
				for (u32 SnapshotIndex = 0;
					SnapshotIndex < DEBUG_SNAPSHOT_COUNT;
					++SnapshotIndex)
				{
					AccumDebugStatistic(&HitCount, Counter->Snapshots[SnapshotIndex].HitCount);
					AccumDebugStatistic(&CycleCount, (u32)Counter->Snapshots[SnapshotIndex].CycleCount);

					f64 HOC = 0.0f;
					if (Counter->Snapshots[SnapshotIndex].HitCount)
					{
						HOC = ((f64)Counter->Snapshots[SnapshotIndex].CycleCount / (f64)Counter->Snapshots[SnapshotIndex].HitCount);
					}
					AccumDebugStatistic(&CycleOverHit, HOC);
				}
				EndDebugStatistic(&HitCount);
				EndDebugStatistic(&CycleCount);
				EndDebugStatistic(&CycleOverHit);

				if (Counter->FunctionName)
				{
					if (CycleCount.Max > 0.0f)
					{
						f32 BarWidth = 4.0f;
						f32 ChartLeft = 210.0f;
						f32 ChartMinY = AtY;
						f32 ChartHeight = Info->AscenderHeight * FontScale;
						f32 Scale = 1.0f / CycleCount.Max;
						for (u32 SnapshotIndex = 0;
							SnapshotIndex < DEBUG_SNAPSHOT_COUNT;
							++SnapshotIndex)
						{
							f32 ThisProportion = Scale * (f32)Counter->Snapshots[SnapshotIndex].CycleCount;
							f32 ThisHeight = ChartHeight * ThisProportion;

							PushRect(
								RenderGroup, V3(ChartLeft + BarWidth * (f32)SnapshotIndex + 0.5f * BarWidth, ChartMinY + 0.5f * ThisHeight, 0.0f),
								V2(BarWidth, ThisHeight), V4(ThisProportion, 1, 0, 1)
							);
						}
					}

					// if (HitCount.Max > 0.0f)
					{
#if 1
						char TextBuffer[256];
						_snprintf_s(
							TextBuffer, sizeof(TextBuffer),
							"%24s(%4d): %10ucy %8uh %10ucy/h",
							Counter->FunctionName,
							Counter->LineNumber,
							(u32)CycleCount.Avg,
							(u32)HitCount.Avg,
							(u32)(CycleOverHit.Avg)
						);

						DEBUGTextLine(TextBuffer);
#endif
					}
				}
			}

			// DEBUGTextLine("\\5C0F\\8033\\6728\\514E");

			f32 BarWidth = 8.0f;
			f32 BarSpacing = 10.0f;
			f32 ChartLeft = LeftEdge + 10.0f;
			f32 ChartHeight = 300.0f;
			f32 ChartWidth = BarSpacing * (f32)DEBUG_SNAPSHOT_COUNT;
			f32 ChartMinY = AtY - (ChartHeight + 20.0f);
			f32 Scale = 1.0f / 0.03333f;

			v3 Colors[] =
			{
				{1, 0, 0},
				{0, 1, 0},
				{0, 0, 1},
				{1, 1, 0},
				{0, 1, 1},
				{1, 0, 1},
				{1, 0.5f, 0},
				{1, 0, 0.5f},
				{0.5f, 1, 0},
				{0, 1, 0.5f},
				{0.5f, 0, 1},
				{0, 0.5f, 1},
			};

			f32 ExampleNumber = 0.0f;
			for (u32 SnapshotIndex = 0;
				SnapshotIndex < DEBUG_SNAPSHOT_COUNT;
				++SnapshotIndex)
			{
				debug_frame_end_info* Info = DebugState->FrameEndInfos + SnapshotIndex;

				f32 StackY = ChartMinY;
				f32 PrevTimestampSeconds = 0.0f;
				for (u32 TimestampIndex = 0;
					TimestampIndex < Info->TimestampCount;
					++TimestampIndex)
				{
					debug_frame_timestamp* Timestamp = Info->Timestamps + TimestampIndex;

					f32 ThisSecondsElapsed = Timestamp->Seconds - PrevTimestampSeconds;
					PrevTimestampSeconds = Timestamp->Seconds;
					v3 Color = Colors[TimestampIndex % ArrayCount(Colors)]; 

					f32 ThisProportion = Scale * ThisSecondsElapsed;
					f32 ThisHeight = ChartHeight * ThisProportion;
					
					PushRect(
						RenderGroup,
						V3(ChartLeft + BarSpacing * (f32)SnapshotIndex + 0.5f * BarWidth, StackY + 0.5f * ThisHeight, 0.0f),
						V2(BarWidth, ThisHeight), ToV4(Color, 1)
					);

					StackY += ThisHeight;
				}
			}

			PushRect(
				RenderGroup, V3(ChartLeft + 0.5f * ChartWidth, ChartMinY + ChartHeight, 0.0f),
				V2(ChartWidth, 1.0f), V4(1, 1, 1, 1)
			);
		}
	}
}

#define DebugRecords_Main_Count __COUNTER__
extern u32 DebugRecords_Optimized_Count;

debug_table GlobalDebugTable;

internal void
UpdateDebugRecords(debug_state* DebugState, u32 CounterCount, debug_record* Counters)
{
	for (u32 CounterIndex = 0;
		CounterIndex < CounterCount;
		++CounterIndex)
	{
		debug_record* Source = Counters + CounterIndex;
		debug_counter_state* Dest = DebugState->CounterStates + DebugState->CounterCount++;

		u64 HitCount_CycleCount = AtomicExchangeU64(&Source->HitCount_CycleCount, 0);
		Dest->Filename = Source->Filename;
		Dest->FunctionName = Source->FunctionName;
		Dest->LineNumber = Source->LineNumber;
		Dest->Snapshots[DebugState->SnapshotIndex].HitCount = (u32)(HitCount_CycleCount >> 32);
		Dest->Snapshots[DebugState->SnapshotIndex].CycleCount = (u32)(HitCount_CycleCount & 0xFFFFFFFF);
	}
}

internal void
CollateDebugRecords(debug_state* DebugState, u32 EventCount, debug_event* Events)
{
#define DebugRecords_Platform_Count 0
	DebugState->CounterCount = (
		DebugRecords_Main_Count + 
		DebugRecords_Optimized_Count + 
		DebugRecords_Platform_Count
	);
	
	for (u32 CounterIndex = 0;
		CounterIndex < DebugState->CounterCount;
		++CounterIndex)
	{
		debug_counter_state* Dest = DebugState->CounterStates + CounterIndex;
		Dest->Snapshots[DebugState->SnapshotIndex].HitCount = 0;
		Dest->Snapshots[DebugState->SnapshotIndex].CycleCount = 0;
	}

	debug_counter_state* CounterArray[3] =
	{
		DebugState->CounterStates,
		DebugState->CounterStates + DebugRecords_Main_Count,
		DebugState->CounterStates + DebugRecords_Main_Count + DebugRecords_Optimized_Count,
	};
	
	for (u32 EventIndex = 0;
		EventIndex < EventCount;
		++EventIndex)
	{
		debug_event* Event = Events + EventIndex;

		debug_counter_state* Dest = CounterArray[Event->TranslationUnit] + Event->DebugRecordIndex;

		debug_record* Source = GlobalDebugTable.Records[Event->TranslationUnit] + Event->DebugRecordIndex;
		Dest->Filename = Source->Filename;
		Dest->FunctionName = Source->FunctionName;
		Dest->LineNumber = Source->LineNumber;

		if (Event->Type == (u8)debug_event_type::BeginBlock)
		{
			++Dest->Snapshots[DebugState->SnapshotIndex].HitCount;
			Dest->Snapshots[DebugState->SnapshotIndex].CycleCount -= Event->Clock;
		}
		else
		{
			Assert(Event->Type == (u8)debug_event_type::EndBlock);

			Dest->Snapshots[DebugState->SnapshotIndex].CycleCount += Event->Clock;
		}
	}
}

extern "C" GAME_DEBUG_FRAME_END(GameDEBUGFrameEnd)
{
	GlobalDebugTable.CurrentEventArrayIndex = !GlobalDebugTable.CurrentEventArrayIndex;
	u64 ArrayIndex_EventIndex = AtomicExchangeU64(&GlobalDebugTable.EventArrayIndex_EventIndex, 
		(u64)GlobalDebugTable.CurrentEventArrayIndex << 32);

	u32 EventArrayIndex = ArrayIndex_EventIndex >> 32;
	u32 EventCount = ArrayIndex_EventIndex & 0xFFFFFFFF;

	debug_state* DebugState = (debug_state*)Memory->DebugStorage;
	if (DebugState)
	{
		DebugState->CounterCount = 0;

		CollateDebugRecords(DebugState, EventCount, GlobalDebugTable.Events[EventArrayIndex]);

		DebugState->FrameEndInfos[DebugState->SnapshotIndex] = *Info;

		++DebugState->SnapshotIndex;
		if (DebugState->SnapshotIndex >= DEBUG_SNAPSHOT_COUNT)
		{
			DebugState->SnapshotIndex = 0;
		}
	}
}