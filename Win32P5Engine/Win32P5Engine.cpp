/*
TODO: THIS IS NOT A FINAL PLATFORM LAYER!!!

 - Saved game locations
 - Asset loading path
 - Threading (launch a thread)
 - Raw Input (support for multiple keyboards)
 - ClipCursor() (for multimonitor support)
 - Make the right calls so Windows doesn't think we're "still loading" for a bit after we actually start
 - QueryCancelAutoplay
 - WM_ACTIVATEAPP (for when we are not the active application)
 - Blit speed improvements (BitBlt)
 - Hardware acceleration (OpenGL or Direct3D or BOTH??)
 - GetKeyboardLayout (for French keyboards, international WASD support)
 - ChangeDisplaySettings option if we detect slow fullscrleen blit?

Just a partial list of stuff!!
*/

#include "../P5Engine/p5engine.h"

#include <Windows.h>
#include <strsafe.h>
#include <stdio.h>
#include <Xinput.h>
#include <dsound.h>

#include "Win32P5Engine.h"

// TODO: This is a global for now
global_variable b32 GlobalRunning;
global_variable b32 GlobalPause;
global_variable win32_offscreen_buffer GlobalBackbuffer;
global_variable LPDIRECTSOUNDBUFFER GlobalSecondaryBuffer;
global_variable i64 PerfCountFrequency;
global_variable b32 DEBUGGlobalShowCursor;
global_variable WINDOWPLACEMENT GlobalWindowPosition = { sizeof(GlobalWindowPosition) };

// NOTE: XInputGetState
#define X_INPUT_GET_STATE(name) DWORD WINAPI name(DWORD dwUserIndex, XINPUT_STATE *pState)
typedef X_INPUT_GET_STATE(x_input_get_state);
X_INPUT_GET_STATE(XInputGetStateStub)
{
	return(ERROR_DEVICE_NOT_CONNECTED);
}
global_variable x_input_get_state* XInputGetState_ = XInputGetStateStub;
#define XInputGetState XInputGetState_

// NOTE: XInputSetState 
#define X_INPUT_SET_STATE(name) DWORD WINAPI name(DWORD dwUserIndex, XINPUT_VIBRATION *pVibration)
typedef X_INPUT_SET_STATE(x_input_set_state);
X_INPUT_SET_STATE(XInputSetStateStub)
{
	return(ERROR_DEVICE_NOT_CONNECTED);
}
global_variable x_input_set_state* XInputSetState_ = XInputSetStateStub;
#define XInputSetState XInputSetState_

// NOTE: DirectSoundCreate
#define DIRECT_SOUND_CREATE(name) HRESULT WINAPI name(LPCGUID pcGuidDevice, LPDIRECTSOUND *ppDS, LPUNKNOWN pUnkOuter)
typedef DIRECT_SOUND_CREATE(direct_sound_create);


internal void
CatStrings(size_t SourceACount, char* SourceA, size_t SourceBCount, char* SourceB, size_t DestCount, char* Dest)
{
	// TODO: Dest bounds checking!
	for (int Index = 0; Index < SourceACount; ++Index)
	{
		*Dest++ = *SourceA++;
	}

	for (int Index = 0; Index < SourceBCount; ++Index)
	{
		*Dest++ = *SourceB++;
	}

	*Dest++ = 0;
}

internal void
Win32GetEXEFileName(win32_state* State)
{
	// NOTE: Never use MAX_PATH in code that is user-facing, because it
	// can be dangerous and lead to bad results.
	DWORD SizeOfFilename = GetModuleFileNameA(0, State->EXEFileName, sizeof(State->EXEFileName));
	State->OnePastLastEXEFileNameSlash = State->EXEFileName;
	for (char* Scan = State->EXEFileName; *Scan; ++Scan)
	{
		if (*Scan == '\\')
		{
			State->OnePastLastEXEFileNameSlash = Scan + 1;
		}
	}
}

internal int
StringLength(char* String)
{
	int PieceCount = 0;

	while (*String++)
	{
		++PieceCount;
	}

	return(PieceCount);
}

internal void
Win32BuildEXEPathFileName(win32_state* State, char* FileName, int DestCount, char* Dest)
{
	CatStrings(State->OnePastLastEXEFileNameSlash - State->EXEFileName, State->EXEFileName, StringLength(FileName), FileName, DestCount, Dest);
}

DEBUG_PLATFORM_FREE_FILE_MEMORY(DEBUGPlatformFreeFileMemory)
{
	if (Memory)
	{
		VirtualFree(Memory, 0, MEM_RELEASE);
	}
}

DEBUG_PLATFORM_READ_ENTIRE_FILE(DEBUGPlatformReadEntireFile)
{
	debug_read_file_result Result = {};

	HANDLE FileHandle = CreateFileA(Filename, GENERIC_READ, FILE_SHARE_READ, 0, OPEN_EXISTING, 0, 0);
	if (FileHandle != INVALID_HANDLE_VALUE)
	{
		LARGE_INTEGER FileSize;
		if (GetFileSizeEx(FileHandle, &FileSize))
		{
			u32 FileSize32 = SafeTruncateUInt64(FileSize.QuadPart);
			Result.Contents = VirtualAlloc(0, FileSize.QuadPart, MEM_RESERVE | MEM_COMMIT, PAGE_READWRITE);
			if (Result.Contents)
			{
				DWORD BytesRead;
				if (ReadFile(FileHandle, Result.Contents, FileSize32, &BytesRead, 0) && (FileSize32 == BytesRead))
				{
					// NOTE: File read successfully
					Result.ContentsSize = FileSize32;
				}
				else
				{
					DEBUGPlatformFreeFileMemory(Result.Contents);
					Result.Contents = 0;
				}
			}
			else
			{
				// TODO: Logging
			}
		}
		else
		{
			// TODO: Logging
		}

		CloseHandle(FileHandle);
	}
	else
	{
		// TODO: Logging
	}

	return(Result);
}

DEBUG_PLATFORM_WRITE_ENTIRE_FILE(DEBUGPlatformWriteEntireFile)
{
	b32 Result = false;

	HANDLE FileHandle = CreateFileA(Filename, GENERIC_WRITE, 0, 0, CREATE_ALWAYS, 0, 0);
	if (FileHandle != INVALID_HANDLE_VALUE)
	{
		DWORD BytesWritten;
		if (WriteFile(FileHandle, Memory, MemorySize, &BytesWritten, 0))
		{
			// NOTE: File read successfully
			Result = (BytesWritten == MemorySize);
		}
		else
		{
			// TODO: Logging
		}

		CloseHandle(FileHandle);
	}
	else
	{
		// TODO: Logging
	}

	return(Result);
}

inline FILETIME
Win32GetLastWriteTime(char* Filename)
{
	FILETIME LastWriteTime = {};

	WIN32_FILE_ATTRIBUTE_DATA Data = {};
	if (GetFileAttributesEx(Filename, GetFileExInfoStandard, &Data))
	{
		LastWriteTime = Data.ftLastWriteTime;
	}

	return(LastWriteTime);
}

internal win32_game_code
Win32LoadGameCode(char *SourceDLLName, char* TempDLLName)
{
	win32_game_code Result = {};

	Result.DLLLastWriteTime = Win32GetLastWriteTime(SourceDLLName);

	CopyFileA(SourceDLLName, TempDLLName, FALSE);

	Result.GameCodeDLL = LoadLibraryA(TempDLLName);
	if (Result.GameCodeDLL)
	{
		Result.UpdateAndRender = (game_update_and_render*)GetProcAddress(Result.GameCodeDLL, "GameUpdateAndRender");
		Result.GetSoundSamples = (game_get_sound_samples*)GetProcAddress(Result.GameCodeDLL, "GameGetSoundSamples");

		Result.IsValid = (Result.UpdateAndRender && Result.GetSoundSamples);
	}

	if (!Result.IsValid)
	{
		Result.UpdateAndRender = 0;
		Result.GetSoundSamples = 0;
	}

	return(Result);
}

internal void
Win32UnloadGameCode(win32_game_code* GameCode)
{
	if (GameCode->GameCodeDLL)
	{
		FreeLibrary(GameCode->GameCodeDLL);
		GameCode->GameCodeDLL = 0;
	}

	GameCode->IsValid = false;
	GameCode->UpdateAndRender = 0;
	GameCode->GetSoundSamples = 0;
}

internal void
Win32LoadXInput(void)
{
	// TODO: Test this on Windows 8
	HMODULE XInputLibrary = LoadLibraryA("xinput1_4.dll");
	if (!XInputLibrary)
	{
		// TODO: Diagnostic
		HMODULE XInputLibrary = LoadLibraryA("xinput1_3.dll");
	}

	if (XInputLibrary)
	{
		XInputGetState = (x_input_get_state*)GetProcAddress(XInputLibrary, "XInputGetState");
		if (!XInputGetState)
		{
			XInputGetState = XInputGetStateStub;
		}
		else
		{
			// TODO: Diagnostic
		}

		XInputSetState = (x_input_set_state*)GetProcAddress(XInputLibrary, "XInputSetState");
		if (!XInputSetState)
		{
			XInputSetState = XInputSetStateStub;
		}
		else
		{
			// TODO: Diagnostic
		}
	}
	else
	{
		// TODO: Diagnostic
	}
}

internal void
Win32InitDSound(HWND Window, i32 SamplesPerSecond, i32 BufferSize)
{
	HMODULE DSoundLibrary = LoadLibraryA("dsound.dll");

	if (DSoundLibrary) {
		direct_sound_create* DirectSoundCreate = (direct_sound_create*)GetProcAddress(DSoundLibrary, "DirectSoundCreate");

		// TODO: Double-check that this works on XP - DirectSound8 or 7??
		LPDIRECTSOUND DirectSound;
		if (DirectSoundCreate && SUCCEEDED(DirectSoundCreate(0, &DirectSound, 0)))
		{
			WAVEFORMATEX WaveFormat = {};
			WaveFormat.wFormatTag = WAVE_FORMAT_PCM;
			WaveFormat.nChannels = 2;
			WaveFormat.nSamplesPerSec = SamplesPerSecond;
			WaveFormat.wBitsPerSample = 16;
			WaveFormat.nBlockAlign = (WaveFormat.nChannels * WaveFormat.wBitsPerSample) / 8;
			WaveFormat.nAvgBytesPerSec = SamplesPerSecond * WaveFormat.nBlockAlign;
			WaveFormat.cbSize = 0;

			if (SUCCEEDED(DirectSound->SetCooperativeLevel(Window, DSSCL_PRIORITY)))
			{
				DSBUFFERDESC BufferDescription = {};
				BufferDescription.dwSize = sizeof(BufferDescription);
				BufferDescription.dwFlags = DSBCAPS_PRIMARYBUFFER;

				// TODO: DSBCAPS_GLOBALFOCUS?
				LPDIRECTSOUNDBUFFER PrimaryBuffer;
				if (SUCCEEDED(DirectSound->CreateSoundBuffer(&BufferDescription, &PrimaryBuffer, 0)))
				{
					HRESULT Error = PrimaryBuffer->SetFormat(&WaveFormat);
					if (SUCCEEDED(Error))
					{
						OutputDebugStringA("Primary buffer format was set.\n");
					}
					else
					{
						// TODO: Diagnostic
					}
				}
				else
				{
					// TODO: Diagnostic
				}
			}
			else
			{
				// TODO: Diagnostic
			}

			// TODO: In release mode, should we not specify DSBCAPS_GLOBALFOCUS?

			// TODO: DSBCAPS_GETCURRENTPOSITION2
			DSBUFFERDESC BufferDescription = {};
			BufferDescription.dwSize = sizeof(BufferDescription);
			BufferDescription.dwFlags = DSBCAPS_GETCURRENTPOSITION2;
#if P5ENGINE_INTERNAL
			BufferDescription.dwFlags |= DSBCAPS_GLOBALFOCUS;
#endif
			BufferDescription.dwBufferBytes = BufferSize;
			BufferDescription.lpwfxFormat = &WaveFormat;

			HRESULT Error = DirectSound->CreateSoundBuffer(&BufferDescription, &GlobalSecondaryBuffer, 0);
			if (SUCCEEDED(Error))
			{
				OutputDebugStringA("Secondary buffer format was set.\n");
			}
			else
			{
				// TODO: Diagnostic
			}
		}
		else
		{
			// TODO: Diagnostic
		}
	}
	else
	{
		// TODO: Diagnostic
	}
}

internal win32_window_dimension
Win32GetWindowDimension(HWND Window)
{
	win32_window_dimension Result = {};

	RECT ClientRect;
	GetClientRect(Window, &ClientRect);
	Result.Width = ClientRect.right - ClientRect.left;
	Result.Height = ClientRect.bottom - ClientRect.top;

	return(Result);
}

internal void
Win32ResizeDIBSection(win32_offscreen_buffer* Buffer, int Width, int Height)
{
	// TODO: Bulletproof this.
	// Maybe don't free first, free after, then free first if that fails.

	if (Buffer->Memory)
	{
		VirtualFree(Buffer->Memory, 0, MEM_RELEASE);
	}

	Buffer->Width = Width;
	Buffer->Height = Height;
	int BytesPerPixel = 4;
	Buffer->BytesPerPixel = BytesPerPixel;

	// NOTE: When the biHeight field is negative, this is the clue to Windows
	// to treat this bitmap as top-down, not bottom-up, meaning that the first
	// three bytes of the image are the color for the top left pixel in the
	// bitmap, not the bottom left!
	Buffer->Info.bmiHeader.biSize = sizeof(Buffer->Info.bmiHeader);
	Buffer->Info.bmiHeader.biWidth = Buffer->Width;
	Buffer->Info.bmiHeader.biHeight = Buffer->Height;
	Buffer->Info.bmiHeader.biPlanes = 1;
	Buffer->Info.bmiHeader.biBitCount = 32;
	Buffer->Info.bmiHeader.biCompression = BI_RGB;

	// NOTE: Thank you to Chris Hecker of Spy Party fame
	// for clarifying the deal with StretchDIBits and BitBlt!
	// No more DC for us.
	Buffer->Pitch = Align16(Width * BytesPerPixel);
	int BitmapMemorySize = Buffer->Pitch * Buffer->Height;
	Buffer->Memory = VirtualAlloc(0, BitmapMemorySize, MEM_RESERVE|MEM_COMMIT, PAGE_READWRITE);
}

internal void
Win32DisplayBufferInWindow(HDC DeviceContext, win32_offscreen_buffer* Buffer, int WindowWidth, int WindowHeight)
{
	if ((WindowWidth >= Buffer->Width * 2) && (WindowHeight >= Buffer->Height * 2))
	{
		// TODO: Centering / black bars?

		StretchDIBits(DeviceContext,
			0, 0, 2 * Buffer->Width, 2 * Buffer->Height,
			0, 0, Buffer->Width, Buffer->Height,
			Buffer->Memory, &Buffer->Info, DIB_RGB_COLORS, SRCCOPY);
	}
	else
	{
#if 0
		int OffsetX = 50;
		int OffsetY = 50;

		PatBlt(DeviceContext, 0, 0, WindowWidth, OffsetY, BLACKNESS);
		PatBlt(DeviceContext, 0, OffsetY + Buffer->Height, WindowWidth, WindowHeight, BLACKNESS);
		PatBlt(DeviceContext, 0, 0, OffsetX, WindowHeight, BLACKNESS);
		PatBlt(DeviceContext, OffsetX + Buffer->Width, 0, WindowWidth, WindowHeight, BLACKNESS);
#else
		int OffsetX = 0;
		int OffsetY = 0;
#endif

		// NOTE: For prototyping purposes, we're going to always blit
		// 1-to-1 piyxels to make sure we don't introduce artifacts with
		// stretching while we are learning to code the renderer!
		StretchDIBits(DeviceContext,
			OffsetX, OffsetY, Buffer->Width, Buffer->Height,
			0, 0, Buffer->Width, Buffer->Height,
			Buffer->Memory, &Buffer->Info, DIB_RGB_COLORS, SRCCOPY);
	}
}

LRESULT CALLBACK
Win32MainWindowCallback(HWND Window, UINT Message, WPARAM WParam, LPARAM LParam)
{
	LRESULT Result = 0;

	switch (Message)
	{
		case WM_CLOSE:
		{
			// TODO: Handle this with a message to the user
			GlobalRunning = false;
		} break;
		
		case WM_SETCURSOR:
		{
			if (DEBUGGlobalShowCursor)
			{
				Result = DefWindowProcA(Window, Message, WParam, LParam);
			}
			else
			{
				SetCursor(0);
			}
		} break;

		case WM_ACTIVATEAPP:
		{
			OutputDebugStringA("WM_ACTIVATEAPP\n");
		} break;

		case WM_DESTROY:
		{
			// TODO: Handle this as an error - recreate window?
			GlobalRunning = false;
		} break;

		case WM_PAINT:
		{
			PAINTSTRUCT Paint;
			HDC DeviceContext = BeginPaint(Window, &Paint);

			win32_window_dimension Dim = Win32GetWindowDimension(Window);
			Win32DisplayBufferInWindow(DeviceContext, &GlobalBackbuffer, Dim.Width, Dim.Height);

			EndPaint(Window, &Paint);
		} break;

		case WM_SYSKEYDOWN:
		case WM_SYSKEYUP:
		case WM_KEYDOWN:
		case WM_KEYUP:
		{
			Assert(!"Keyboard input came in througha non-dispatch message!");
		} break;

		default:
		{
			Result = DefWindowProcA(Window, Message, WParam, LParam);
		} break;

	}

	return(Result);
}

internal void
Win32ClearBuffer(win32_sound_output* SoundOutput)
{
	VOID* Region1;
	DWORD Region1Size;
	VOID* Region2;
	DWORD Region2Size;
	if (SUCCEEDED(GlobalSecondaryBuffer->Lock(0, SoundOutput->SecondaryBufferSize, &Region1, &Region1Size, &Region2, &Region2Size, 0)))
	{
		// TODO: assert that Region1Size/Region2Size is valid
		u08* DestSample = (u08*)Region1;
		for (DWORD ByteIndex = 0; ByteIndex < Region1Size; ++ByteIndex)
		{
			*DestSample++ = 0;
		}

		DestSample = (u08*)Region2;
		for (DWORD ByteIndex = 0; ByteIndex < Region2Size; ++ByteIndex)
		{
			*DestSample++ = 0;
		}

		GlobalSecondaryBuffer->Unlock(Region1, Region1Size, Region2, Region2Size);
	}
}

internal void
Win32FillSoundBuffer(win32_sound_output* SoundOutput, DWORD ByteToLock, DWORD BytesToWrite, game_sound_output_buffer* SourceBuffer)
{
	// TODO: More strenuous test!
	VOID* Region1;
	DWORD Region1Size;
	VOID* Region2;
	DWORD Region2Size;
	if (SUCCEEDED(GlobalSecondaryBuffer->Lock(ByteToLock, BytesToWrite, &Region1, &Region1Size, &Region2, &Region2Size, 0)))
	{
		// TODO: assert that Region1Size/Region2Size is valid

		// TODO: Collapse two loops
		DWORD Region1SampleCount = Region1Size / SoundOutput->BytesPerSample;
		i16* DestSample = (i16*)Region1;
		i16* SourceSample = SourceBuffer->Samples;
		for (DWORD SampleIndex = 0; SampleIndex < Region1SampleCount; ++SampleIndex)
		{
			*DestSample++ = *SourceSample++;
			*DestSample++ = *SourceSample++;
			++SoundOutput->RunningSampleIndex;
		}

		DWORD Region2SampleCount = Region2Size / SoundOutput->BytesPerSample;
		DestSample = (i16*)Region2;
		for (DWORD SampleIndex = 0; SampleIndex < Region2SampleCount; ++SampleIndex)
		{
			*DestSample++ = *SourceSample++;
			*DestSample++ = *SourceSample++;
			++SoundOutput->RunningSampleIndex;
		}

		GlobalSecondaryBuffer->Unlock(Region1, Region1Size, Region2, Region2Size);
	}
}

internal void
Win32ProcessKeyboardMessage(game_button_state* NewState, b32 IsDown)
{
	if (NewState->EndedDown != IsDown)
	{
		NewState->EndedDown = IsDown;
		++NewState->HalfTransitionCount;
	}
}

internal void
Win32ProcessXInputDigitalButton(DWORD XInputButtonState, DWORD ButtonBit, game_button_state* OldState, game_button_state* NewState)
{
	NewState->EndedDown = ((XInputButtonState & ButtonBit) == ButtonBit);
	NewState->HalfTransitionCount = (OldState->EndedDown != NewState->EndedDown) ? 1 : 0;
}

internal f32
Win32ProcessXInputStickValue(SHORT Value, SHORT DeadZone)
{
	f32 Result = 0;

	if (Value < -DeadZone)
	{
		Result = (f32)((Value + DeadZone) / (32768.0f - DeadZone));
	}
	else if (Value > DeadZone)
	{
		Result = (f32)((Value - DeadZone) / (32767.0f - DeadZone));
	}

	return(Result);
}

internal void
Win32GetInputFileLocation(win32_state* State, b32 InputStream, int SlotIndex, int DestCount, char* Dest)
{
	char Temp[64];
	wsprintf(Temp, "p5_input_recording_%d_%s.p5i", SlotIndex, InputStream ? "input" : "state");
	Win32BuildEXEPathFileName(State, Temp, DestCount, Dest);
}

internal win32_replay_buffer*
Win32GetReplayBuffer(win32_state* State, int unsigned Index)
{
	Assert(Index < ArrayCount(State->ReplayBuffers));
	win32_replay_buffer* Result = &State->ReplayBuffers[Index];
	return(Result);
}

internal void
Win32BeginRecordingInput(win32_state* State, int InputRecordingIndex)
{
	win32_replay_buffer* ReplayBuffer = Win32GetReplayBuffer(State, InputRecordingIndex);
	if (ReplayBuffer->MemoryBlock)
	{
		State->InputRecordingIndex = InputRecordingIndex;

		char Filename[WIN32_STATE_FILE_NAME_COUNT];
		Win32GetInputFileLocation(State, true, InputRecordingIndex, sizeof(Filename), Filename);
		State->RecordingHandle = CreateFileA(Filename, GENERIC_WRITE, 0, 0, CREATE_ALWAYS, 0, 0);

		CopyMemory(ReplayBuffer->MemoryBlock, State->GameMemoryBlock, State->TotalSize);
	}
}

internal void
Win32EndRecordingInput(win32_state* State)
{
	CloseHandle(State->RecordingHandle);
	State->InputRecordingIndex = 0;
}

internal void
Win32BeginInputPlayback(win32_state* State, int InputPlayingIndex)
{
	win32_replay_buffer* ReplayBuffer = Win32GetReplayBuffer(State, InputPlayingIndex);
	if (ReplayBuffer->MemoryBlock)
	{
		State->InputPlayingIndex = InputPlayingIndex;

		char Filename[WIN32_STATE_FILE_NAME_COUNT];
		Win32GetInputFileLocation(State, true, InputPlayingIndex, sizeof(Filename), Filename);
		State->PlaybackHandle = CreateFileA(Filename, GENERIC_READ, 0, 0, OPEN_EXISTING, 0, 0);

		CopyMemory(State->GameMemoryBlock, ReplayBuffer->MemoryBlock, State->TotalSize);
	}
}

internal void
Win32EndInputPlayback(win32_state* State)
{
	CloseHandle(State->PlaybackHandle);
	State->InputPlayingIndex = 0;
}

internal void
Win32RecordInput(win32_state* State, game_input* NewInput)
{
	DWORD BytesWritten;
	WriteFile(State->RecordingHandle, NewInput, sizeof(*NewInput), &BytesWritten, 0);
}

internal void
Win32PlayBackInput(win32_state* State, game_input* NewInput)
{
	DWORD BytesRead = 0;
	if (ReadFile(State->PlaybackHandle, NewInput, sizeof(*NewInput), &BytesRead, 0))
	{
		if (BytesRead == 0)
		{
			int PlayingIndex = State->InputPlayingIndex;
			Win32EndInputPlayback(State);
			Win32BeginInputPlayback(State, PlayingIndex);
		}
	}
}

internal void
ToggleFullscreen(HWND Window)
{
	//  NOTE: This follows Raymond Chen's prescription
	// for fullscreen toggling, see: 
	// https://devblogs.microsoft.com/oldnewthing/20100412-00/?p=14353

	DWORD Style = GetWindowLongA(Window, GWL_STYLE);
	if (Style & WS_OVERLAPPEDWINDOW)
	{
		MONITORINFO MonitorInfo = { sizeof(MonitorInfo) };
		if (GetWindowPlacement(Window, &GlobalWindowPosition) && GetMonitorInfo(MonitorFromWindow(Window, MONITOR_DEFAULTTOPRIMARY), &MonitorInfo))
		{
			SetWindowLongA(Window, GWL_STYLE, Style & ~WS_OVERLAPPEDWINDOW);
			SetWindowPos(Window, HWND_TOP,
				MonitorInfo.rcMonitor.left, MonitorInfo.rcMonitor.top,
				MonitorInfo.rcMonitor.right - MonitorInfo.rcMonitor.left,
				MonitorInfo.rcMonitor.bottom - MonitorInfo.rcMonitor.top,
				SWP_NOOWNERZORDER | SWP_FRAMECHANGED);
		}
	}
	else
	{
		SetWindowLongA(Window, GWL_STYLE, Style | WS_OVERLAPPEDWINDOW);
		SetWindowPlacement(Window, &GlobalWindowPosition);
		SetWindowPos(Window, 0, 0, 0, 0, 0,
			SWP_NOMOVE | SWP_NOSIZE | SWP_NOZORDER | SWP_NOOWNERZORDER | SWP_FRAMECHANGED);
	}
}

internal void
Win32ProcessPendingMessages(game_controller_input* KeyboardController, win32_state* State)
{
	MSG Message;

	while (PeekMessageA(&Message, 0, 0, 0, PM_REMOVE))
	{
		switch (Message.message)
		{
			case WM_SYSKEYDOWN:
			case WM_SYSKEYUP:
			case WM_KEYDOWN:
			case WM_KEYUP:
			{
				u32 VKCode = (u32)Message.wParam;

				// NOTE: Since we are comparing WasDown to IsDown,
				// we MUST use == and != to convert these bit tests to actual
				// 0 or 1 values.
				bool WasDown = ((Message.lParam & (1 << 30)) != 0);
				bool IsDown = ((Message.lParam & ((u64)1 << 31)) == 0);

				if (WasDown != IsDown)
				{
					if (!GlobalPause)
					{
						if (VKCode == 188)
						{
							Win32ProcessKeyboardMessage(&KeyboardController->MoveUp, IsDown);
						}
						else if (VKCode == 'A')
						{
							Win32ProcessKeyboardMessage(&KeyboardController->MoveLeft, IsDown);
						}
						else if (VKCode == 'O')
						{
							Win32ProcessKeyboardMessage(&KeyboardController->MoveDown, IsDown);
						}
						else if (VKCode == 'E')
						{
							Win32ProcessKeyboardMessage(&KeyboardController->MoveRight, IsDown);
						}
						else if (VKCode == 222)
						{
							Win32ProcessKeyboardMessage(&KeyboardController->LeftShoulder, IsDown);
						}
						else if (VKCode == 190)
						{
							Win32ProcessKeyboardMessage(&KeyboardController->RightShoulder, IsDown);
						}
						else if (VKCode == VK_UP)
						{
							Win32ProcessKeyboardMessage(&KeyboardController->ActionUp, IsDown);
						}
						else if (VKCode == VK_LEFT)
						{
							Win32ProcessKeyboardMessage(&KeyboardController->ActionLeft, IsDown);
						}
						else if (VKCode == VK_DOWN)
						{
							Win32ProcessKeyboardMessage(&KeyboardController->ActionDown, IsDown);
						}
						else if (VKCode == VK_RIGHT)
						{
							Win32ProcessKeyboardMessage(&KeyboardController->ActionRight, IsDown);
						}
						else if (VKCode == VK_ESCAPE)
						{
							Win32ProcessKeyboardMessage(&KeyboardController->Back, IsDown);
						}
						else if (VKCode == VK_RETURN)
						{
							Win32ProcessKeyboardMessage(&KeyboardController->Start, IsDown);
						}
					}
	#if P5ENGINE_INTERNAL
					if (VKCode == 'L')
					{
						if (IsDown)
						{
							GlobalPause = !GlobalPause;
							for (int Index = 0; Index < ArrayCount(KeyboardController->Buttons); ++Index)
							{
								KeyboardController->Buttons[Index].EndedDown = 0;
							}
						}
					}
					if (VKCode == 'N')
					{
						if (IsDown)
						{
							if (State->InputPlayingIndex == 0)
							{
								if (State->InputRecordingIndex == 0)
								{
									Win32BeginRecordingInput(State, 1);
								}
								else
								{
									Win32EndRecordingInput(State);
									Win32BeginInputPlayback(State, 1);
								}
							}
							else
							{
								Win32EndInputPlayback(State);
							}
						}
					}
	#endif
					if (IsDown)
					{
						b32 AltKeyWasDown = (Message.lParam & (1 << 29));
						if ((VKCode == VK_F4) && AltKeyWasDown)
						{
							GlobalRunning = false;
						}
						if ((VKCode == VK_RETURN) && AltKeyWasDown)
						{
							if (Message.hwnd)
							{
								ToggleFullscreen(Message.hwnd);
							}
						}
					}
				}
			} break;

			case WM_QUIT:
			{
				GlobalRunning = false;
			} break;

			default:
			{
				TranslateMessage(&Message);
				DispatchMessageA(&Message);
			} break;
		}
	}
}

inline LARGE_INTEGER
Win32GetWallClock()
{
	LARGE_INTEGER Result = {};
	QueryPerformanceCounter(&Result);

	return(Result);
}

inline f32
Win32GetSecondsElapsed(LARGE_INTEGER Start, LARGE_INTEGER End)
{
	f32 Result = ((f32)(End.QuadPart - Start.QuadPart) / (f32)PerfCountFrequency);

	return(Result);
}

internal void
HandleDebugCycleCounters(game_memory* GameMemory)
{
#if P5ENGINE_INTERNAL
	OutputDebugStringA("DEBUG CYCLE COUNTS:\n");
	for (int CounterIndex = 0; CounterIndex < ArrayCount(GameMemory->Counters); ++CounterIndex)
	{
		debug_cycle_counter* Counter = GameMemory->Counters + CounterIndex;

		if (Counter->HitCount)
		{
			char TextBuffer[256];
			_snprintf_s(TextBuffer, sizeof(TextBuffer),
				"  %d: %I64ucy %uh %I64ucy/h\n",
				CounterIndex,
				Counter->CycleCount,
				Counter->HitCount,
				Counter->CycleCount / Counter->HitCount
			);
			OutputDebugStringA(TextBuffer);

			Counter->CycleCount = 0;
			Counter->HitCount = 0;
		}
	}
#endif
}

struct platform_work_queue_entry
{
	platform_work_queue_callback* Callback;
	void* Data;
};

struct platform_work_queue
{
	u32 volatile NextEntryToRead;
	u32 volatile NextEntryToWrite;

	u32 volatile CompletionGoal;
	u32 volatile CompletionCount;
	
	HANDLE SemaphoreHandle;

	platform_work_queue_entry Entries[256];
};

void
Win32AddEntry(platform_work_queue* Queue, platform_work_queue_callback* Callback, void* Data)
{
	u32 NextEntryToWrite = Queue->NextEntryToWrite;
	u32 NewNextEntryToWrite = (NextEntryToWrite + 1) % ArrayCount(Queue->Entries);
	Assert(NewNextEntryToWrite != Queue->NextEntryToRead);

	platform_work_queue_entry* Entry = Queue->Entries + NextEntryToWrite;
	Entry->Callback = Callback;
	Entry->Data = Data;
	++Queue->CompletionGoal;

	_WriteBarrier();

	Queue->NextEntryToWrite = NewNextEntryToWrite;

	ReleaseSemaphore(Queue->SemaphoreHandle, 1, 0);
}

internal b32
Win32DoNextWorkQueueEntry(platform_work_queue* Queue)
{
	b32 WeShouldSleep = false;

	u32 NextEntryToRead = Queue->NextEntryToRead;
	u32 NewNextEntryToRead = (NextEntryToRead + 1) % ArrayCount(Queue->Entries);
	if (NextEntryToRead != Queue->NextEntryToWrite)
	{
		u32 Index = InterlockedCompareExchange((LONG volatile*)&Queue->NextEntryToRead, NewNextEntryToRead, NextEntryToRead);

		if (Index == NextEntryToRead)
		{
			platform_work_queue_entry* Entry = Queue->Entries + Index;
			Entry->Callback(Queue, Entry->Data);
			InterlockedIncrement((LONG volatile*)&Queue->CompletionCount);
		}
	}
	else
	{
		WeShouldSleep = true;
	}

	return(WeShouldSleep);
}

void
Win32CompleteAllWork(platform_work_queue* Queue)
{
	while (Queue->CompletionGoal != Queue->CompletionCount)
	{
		Win32DoNextWorkQueueEntry(Queue);
	}

	Queue->CompletionGoal = 0;
	Queue->CompletionCount = 0;
}

DWORD WINAPI
ThreadProc(LPVOID lpParameter)
{
	platform_work_queue* Queue = (platform_work_queue*)lpParameter;

	for (;;)
	{
		if (Win32DoNextWorkQueueEntry(Queue))
		{
			WaitForSingleObjectEx(Queue->SemaphoreHandle, INFINITE, FALSE);
		}
	}

	// return(0);
}

internal
PLATFORM_WORK_QUEUE_CALLBACK(DoWorkerWork)
{
	char Buffer[256];
	wsprintf(Buffer, "=== Thread %u: %s ===\n", GetCurrentThreadId(), (char*)Data);
	OutputDebugStringA(Buffer);
}

internal void
Win32MakeQueue(platform_work_queue* Queue, u32 ThreadCount)
{
	Queue->CompletionGoal = 0;
	Queue->CompletionCount = 0;
	Queue->NextEntryToWrite = 0;
	Queue->NextEntryToRead = 0;

	u32 InitialCount = 0;
	Queue->SemaphoreHandle = CreateSemaphoreEx(0, InitialCount, ThreadCount, 0, 0, SEMAPHORE_ALL_ACCESS);

	for (u32 ThreadIndex = 0; ThreadIndex < ThreadCount; ++ThreadIndex)
	{
		DWORD ThreadID;
		HANDLE ThreadHandle = CreateThread(0, 0, ThreadProc, Queue, 0, &ThreadID);
		CloseHandle(ThreadHandle);
	}
}

int CALLBACK
WinMain(HINSTANCE Instance, HINSTANCE PrevInstance, PSTR CommandLine, int ShowCode)
{
	win32_state Win32State = {};

	platform_work_queue HighPriorityQueue;
	Win32MakeQueue(&HighPriorityQueue, 6);
	platform_work_queue LowPriorityQueue;
	Win32MakeQueue(&LowPriorityQueue, 2);

	LARGE_INTEGER PerfCountFrequencyResult;
	QueryPerformanceFrequency(&PerfCountFrequencyResult);
	PerfCountFrequency = PerfCountFrequencyResult.QuadPart;

	Win32GetEXEFileName(&Win32State);

	char SourceGameCodeDLLFullPath[WIN32_STATE_FILE_NAME_COUNT];
	char SourceGameCodeDLLFilename[] = "P5Engine.dll";
	Win32BuildEXEPathFileName(&Win32State, SourceGameCodeDLLFilename, sizeof(SourceGameCodeDLLFullPath), SourceGameCodeDLLFullPath);

	char TempGameCodeDLLFullPath[WIN32_STATE_FILE_NAME_COUNT];
	char TempGameCodeDLLFilename[] = "P5Engine_tmp.dll";
	Win32BuildEXEPathFileName(&Win32State, TempGameCodeDLLFilename, sizeof(TempGameCodeDLLFullPath), TempGameCodeDLLFullPath);

	// NOTE: Set the Windows scheduler granularity to 1ms
	// so that our Sleep() can be more granular.
	UINT DesriredSchedulerMS = 1;
	b32 SleepIsGranular = (timeBeginPeriod(DesriredSchedulerMS) == TIMERR_NOERROR);

	Win32LoadXInput();

	int WindowSizeX = 960;
	int WindowSizeY = 540;
	Win32ResizeDIBSection(&GlobalBackbuffer, WindowSizeX, WindowSizeY);

#if P5ENGINE_INTERNAL
	DEBUGGlobalShowCursor = true;
#endif

	WNDCLASSA WindowClass = {};
	WindowClass.style = CS_HREDRAW | CS_VREDRAW;
	WindowClass.lpfnWndProc = Win32MainWindowCallback;
	WindowClass.hInstance = Instance;
	WindowClass.lpszClassName = "P5EngineWindowClass";
	WindowClass.hCursor = LoadCursorA(0, IDC_ARROW);
	//WindowClass.hIcon = ;

	if (RegisterClassA(&WindowClass))
	{
		// NOTE: Why do I have to add these numbers to make the window the right size?
		int WindowWidth = WindowSizeX + 16;
		int WindowHeight = WindowSizeY + 40;

		RECT DesktopRect;
		GetClientRect(GetDesktopWindow(), &DesktopRect);
		DesktopRect.left = (DesktopRect.right / 2) - (WindowWidth / 2);
		DesktopRect.top = (DesktopRect.bottom / 2) - (WindowHeight / 2);
		HWND Window = CreateWindowExA(0, WindowClass.lpszClassName, "P5 Engine", WS_OVERLAPPEDWINDOW | WS_VISIBLE,
			DesktopRect.left, DesktopRect.top, WindowSizeX+16, WindowSizeY+40, 0, 0, Instance, 0);

		if (Window)
		{
			// NOTE: Since we specified CS_OWNDC, we can just 
			// get one device context and use it forever because we 
			// are not sharing it with anyone.
			HDC DeviceContext = GetDC(Window);

			int MonitorRefreshHz = 60;
			int Win32RefreshRate = GetDeviceCaps(DeviceContext, VREFRESH);
			if (Win32RefreshRate > 1)
			{
				MonitorRefreshHz = Win32RefreshRate;
			}
			f32 GameUpdateHz = (f32)(MonitorRefreshHz / 2.0f);
			f32 TargetSecondsPerFrame = 1.0f / GameUpdateHz;

			// NOTE: Sound test
			win32_sound_output SoundOutput = {};
			SoundOutput.SamplesPerSecond = 44100;
			SoundOutput.BytesPerSample = sizeof(i16) * 2;
			SoundOutput.SecondaryBufferSize = SoundOutput.SamplesPerSecond * SoundOutput.BytesPerSample;
			// TODO: Actually compute this variance and see
			// what the lowest reasonable value is.
			SoundOutput.SafetyBytes = (int)(((f32)SoundOutput.SamplesPerSecond * (f32)SoundOutput.BytesPerSample / MonitorRefreshHz));
			Win32InitDSound(Window, SoundOutput.SamplesPerSecond, SoundOutput.SecondaryBufferSize);
			Win32ClearBuffer(&SoundOutput);
			GlobalSecondaryBuffer->Play(0, 0, DSBPLAY_LOOPING);

			GlobalRunning = true;

			// TODO: Pool with bitmap VirtualAlloc
			// TODO: Remove MaxPossibleOverrun
			u32 MaxPossibleOverrun = 2 * 8 * sizeof(u16);
			i16* Samples = (i16*)VirtualAlloc(0, SoundOutput.SecondaryBufferSize + MaxPossibleOverrun, MEM_RESERVE | MEM_COMMIT, PAGE_READWRITE);

#if P5ENGINE_INTERNAL
			LPVOID BaseAddress = (LPVOID)Terabytes(2);
#else
			LPVOID BaseAddress = 0;
#endif
			game_memory GameMemory = {};
			GameMemory.PermanentStorageSize = Megabytes(256);
			GameMemory.TransientStorageSize = Gigabytes(1);
			GameMemory.HighPriorityQueue = &HighPriorityQueue;
			GameMemory.LowPriorityQueue = &LowPriorityQueue;
			GameMemory.PlatformAddEntry = Win32AddEntry;
			GameMemory.PlatformCompleteAllWork = Win32CompleteAllWork;
			GameMemory.DEBUGPlatformFreeFileMemory = DEBUGPlatformFreeFileMemory;
			GameMemory.DEBUGPlatformReadEntireFile = DEBUGPlatformReadEntireFile;
			GameMemory.DEBUGPlatformWriteEntireFile = DEBUGPlatformWriteEntireFile;

			// TODO: Handle various memory footprints (USING SYSTEM METRICS)
			
			// TODO: Use MEM_LARGE_PAGES and call adjust token 
			// privileges when not on Windows XP?
			
			// TODO: TransientStorage needs to be broken up into 
			// game transient and cache transient, and only the 
			// former need be saved for state playback.
			Win32State.TotalSize = GameMemory.PermanentStorageSize + GameMemory.TransientStorageSize;
			Win32State.GameMemoryBlock = VirtualAlloc(BaseAddress, (size_t)Win32State.TotalSize, MEM_RESERVE | MEM_COMMIT, PAGE_READWRITE);
			GameMemory.PermanentStorage = Win32State.GameMemoryBlock;
			GameMemory.TransientStorage = ((u08*)GameMemory.PermanentStorage + GameMemory.PermanentStorageSize);

			for (int ReplayIndex = 0; ReplayIndex < ArrayCount(Win32State.ReplayBuffers); ++ReplayIndex)
			{
				win32_replay_buffer* ReplayBuffer = &Win32State.ReplayBuffers[ReplayIndex];

				// TODO: Recording system still seems to take too long
				// on record start - find out what windows is doing and if 
				// we can speed up / defer some of that processing.

				Win32GetInputFileLocation(&Win32State, false, ReplayIndex, sizeof(ReplayBuffer->Filename), ReplayBuffer->Filename);
				ReplayBuffer->FileHandle = CreateFileA(ReplayBuffer->Filename, GENERIC_READ | GENERIC_WRITE, 0, 0, CREATE_ALWAYS, 0, 0);
				DWORD Error = GetLastError();

				DWORD MaxSizeHigh = (Win32State.TotalSize >> 32);
				DWORD MaxSizeLow = (Win32State.TotalSize & 0xFFFFFFFF);
				ReplayBuffer->MemoryMap = CreateFileMapping(ReplayBuffer->FileHandle, 0, PAGE_READWRITE, MaxSizeHigh, MaxSizeLow, 0);
				ReplayBuffer->MemoryBlock = MapViewOfFile(ReplayBuffer->MemoryMap, FILE_MAP_ALL_ACCESS, 0, 0, Win32State.TotalSize);

				if (ReplayBuffer->MemoryBlock)
				{
					
				}
				else
				{
					// TODO: Logging
				}
			}

			if (Samples && GameMemory.PermanentStorage && GameMemory.TransientStorage)
			{
				game_input Input[2] = {};
				game_input* NewInput = &Input[0];
				game_input* OldInput = &Input[1];

				LARGE_INTEGER LastCounter = Win32GetWallClock();
				LARGE_INTEGER FlipWallClock = Win32GetWallClock();

				int DebugTimeMarkerIndex = 0;
				win32_debug_time_marker DebugTimeMarkers[30] = {};

				DWORD AudioLatencyBytes = 0;
				f32 AudioLatencySeconds = 0;
				b32 SoundIsValid = false;

				win32_game_code Game = Win32LoadGameCode(SourceGameCodeDLLFullPath, TempGameCodeDLLFullPath);

				u64 LastCycleCount = __rdtsc();
				while (GlobalRunning)
				{
					NewInput->dtForFrame = TargetSecondsPerFrame;

					NewInput->ExecutableReloaded = false;
					FILETIME NewDLLWriteTime = Win32GetLastWriteTime(SourceGameCodeDLLFullPath);
					if (CompareFileTime(&NewDLLWriteTime, &Game.DLLLastWriteTime) != 0)
					{
						Win32CompleteAllWork(&HighPriorityQueue);
						Win32CompleteAllWork(&LowPriorityQueue);
						Win32UnloadGameCode(&Game);
						Game = Win32LoadGameCode(SourceGameCodeDLLFullPath, TempGameCodeDLLFullPath);
						NewInput->ExecutableReloaded = true;
					}

					// TODO: Zeroing macro
					// TODO: We can't zero everything because the up/down state will
					// be wrong!!!
					game_controller_input* OldKeyboardController = GetController(OldInput, 0);
					game_controller_input* NewKeyboardController = GetController(NewInput, 0);
					game_controller_input ZeroController = {};
					*NewKeyboardController = ZeroController;
					NewKeyboardController->IsConnected = true;
					for (int ButtonIndex = 0; ButtonIndex < ArrayCount(NewKeyboardController->Buttons); ++ButtonIndex)
					{
						NewKeyboardController->Buttons[ButtonIndex].EndedDown = OldKeyboardController->Buttons[ButtonIndex].EndedDown;
					}

					Win32ProcessPendingMessages(NewKeyboardController, &Win32State);

					if (!GlobalPause)
					{
						POINT MouseP;
						GetCursorPos(&MouseP);
						ScreenToClient(Window, &MouseP);
						NewInput->MouseX = MouseP.x;
						NewInput->MouseY = MouseP.y;
						NewInput->MouseZ = 0; // TODO: Support mousewheel
						Win32ProcessKeyboardMessage(&NewInput->MouseButtons[0], GetKeyState(VK_LBUTTON) & (1 << 15));
						Win32ProcessKeyboardMessage(&NewInput->MouseButtons[1], GetKeyState(VK_MBUTTON) & (1 << 15));
						Win32ProcessKeyboardMessage(&NewInput->MouseButtons[2], GetKeyState(VK_RBUTTON) & (1 << 15));
						Win32ProcessKeyboardMessage(&NewInput->MouseButtons[3], GetKeyState(VK_XBUTTON1) & (1 << 15));
						Win32ProcessKeyboardMessage(&NewInput->MouseButtons[4], GetKeyState(VK_XBUTTON2) & (1 << 15));

						// TODO: Need to not poll disconnected controllers to avoid
						// xinput frame rate hit on older libraries...
						// TODO: Should we pull this more frequently?
						DWORD MaxControllerCount = XUSER_MAX_COUNT;
						if (MaxControllerCount > (ArrayCount(NewInput->Controllers) - 1))
						{
							MaxControllerCount = (ArrayCount(NewInput->Controllers) - 1);
						}

						for (DWORD ControllerIndex = 0; ControllerIndex < MaxControllerCount; ++ControllerIndex)
						{
							DWORD OurControllerIndex = ControllerIndex + 1;
							game_controller_input* OldController = &OldInput->Controllers[OurControllerIndex];
							game_controller_input* NewController = &NewInput->Controllers[OurControllerIndex];

							XINPUT_STATE ControllerState;
							if (XInputGetState(ControllerIndex, &ControllerState) == ERROR_SUCCESS)
							{
								NewController->IsConnected = true;
								NewController->IsAnalogL = OldController->IsAnalogL;
								NewController->IsAnalogR = true;

								// TODO: See if ControllerState.dwPacketNumber increments too rapidly
								XINPUT_GAMEPAD* Pad = &ControllerState.Gamepad;

								// TODO: This is a square deadzone, check XInput to
								// verify that the deadzone is "round" and show how to do
								// round deadzone processing.
								NewController->StickAverageLX = Win32ProcessXInputStickValue(Pad->sThumbLX, XINPUT_GAMEPAD_LEFT_THUMB_DEADZONE);
								NewController->StickAverageLY = Win32ProcessXInputStickValue(Pad->sThumbLY, XINPUT_GAMEPAD_LEFT_THUMB_DEADZONE);
								NewController->StickAverageRX = Win32ProcessXInputStickValue(Pad->sThumbRX, XINPUT_GAMEPAD_RIGHT_THUMB_DEADZONE);
								NewController->StickAverageRY = Win32ProcessXInputStickValue(Pad->sThumbRY, XINPUT_GAMEPAD_RIGHT_THUMB_DEADZONE);
								if ((NewController->StickAverageLX != 0.0f) || (NewController->StickAverageLY != 0.0f))
								{
									NewController->IsAnalogL = true;
								}

								if (Pad->wButtons & XINPUT_GAMEPAD_DPAD_UP)
								{
									NewController->StickAverageLY = 1.0f;
									NewController->IsAnalogL = false;
								}

								if (Pad->wButtons & XINPUT_GAMEPAD_DPAD_DOWN)
								{
									NewController->StickAverageLY = -1.0f;
									NewController->IsAnalogL = false;
								}

								if (Pad->wButtons & XINPUT_GAMEPAD_DPAD_LEFT)
								{
									NewController->StickAverageLX = -1.0f;
									NewController->IsAnalogL = false;
								}

								if (Pad->wButtons & XINPUT_GAMEPAD_DPAD_RIGHT)
								{
									NewController->StickAverageLX = 1.0f;
									NewController->IsAnalogL = false;
								}

								f32 Threshold = 0.5f;
								DWORD FakeMoveUp = (NewController->StickAverageLY > Threshold) ? 1 : 0;
								DWORD FakeMoveDown = (NewController->StickAverageLY < -Threshold) ? 1 : 0;
								DWORD FakeMoveLeft = (NewController->StickAverageLX < -Threshold) ? 1 : 0;
								DWORD FakeMoveRight = (NewController->StickAverageLX > Threshold) ? 1 : 0;
								Win32ProcessXInputDigitalButton(FakeMoveUp, 1, &OldController->MoveUp, &NewController->MoveUp);
								Win32ProcessXInputDigitalButton(FakeMoveDown, 1, &OldController->MoveDown, &NewController->MoveDown);
								Win32ProcessXInputDigitalButton(FakeMoveLeft, 1, &OldController->MoveLeft, &NewController->MoveLeft);
								Win32ProcessXInputDigitalButton(FakeMoveRight, 1, &OldController->MoveRight, &NewController->MoveRight);

								Win32ProcessXInputDigitalButton(Pad->wButtons, XINPUT_GAMEPAD_A, &OldController->ActionDown, &NewController->ActionDown);
								Win32ProcessXInputDigitalButton(Pad->wButtons, XINPUT_GAMEPAD_B, &OldController->ActionRight, &NewController->ActionRight);
								Win32ProcessXInputDigitalButton(Pad->wButtons, XINPUT_GAMEPAD_X, &OldController->ActionLeft, &NewController->ActionLeft);
								Win32ProcessXInputDigitalButton(Pad->wButtons, XINPUT_GAMEPAD_Y, &OldController->ActionUp, &NewController->ActionUp);

								Win32ProcessXInputDigitalButton(Pad->wButtons, XINPUT_GAMEPAD_LEFT_SHOULDER, &OldController->LeftShoulder, &NewController->LeftShoulder);
								Win32ProcessXInputDigitalButton(Pad->wButtons, XINPUT_GAMEPAD_RIGHT_SHOULDER, &OldController->RightShoulder, &NewController->RightShoulder);

								Win32ProcessXInputDigitalButton(Pad->wButtons, XINPUT_GAMEPAD_BACK, &OldController->Back, &NewController->Back);
								Win32ProcessXInputDigitalButton(Pad->wButtons, XINPUT_GAMEPAD_START, &OldController->Start, &NewController->Start);
							}
							else
							{
								NewController->IsConnected = false;
							}
						}

						thread_context Context = {};

						game_offscreen_buffer Buffer = {};
						Buffer.Memory = GlobalBackbuffer.Memory;
						Buffer.Width = GlobalBackbuffer.Width;
						Buffer.Height = GlobalBackbuffer.Height;
						Buffer.Pitch = GlobalBackbuffer.Pitch;

						if (Win32State.InputRecordingIndex)
						{
							Win32RecordInput(&Win32State, NewInput);
						}


						if (Win32State.InputPlayingIndex)
						{
							Win32PlayBackInput(&Win32State, NewInput);
						}

						if (Game.UpdateAndRender)
						{
							Game.UpdateAndRender(&GameMemory, NewInput, &Buffer);
							HandleDebugCycleCounters(&GameMemory);
						}

						LARGE_INTEGER AudioCounter = Win32GetWallClock();
						f32 FromBeginToAudioSeconds = Win32GetSecondsElapsed(FlipWallClock, AudioCounter);

						DWORD PlayCursor;
						DWORD WriteCursor;
						if (SUCCEEDED(GlobalSecondaryBuffer->GetCurrentPosition(&PlayCursor, &WriteCursor)))
						{
							/*	NOTE:

								Here is how sound output computation works.

								We define a safety value that is the number of samples we think our
								game update loop may vary by (let's say up to 2ms).

								When we wake up to write audio, we will look and see what the play
								cursor position is and we will forecast ahead where we think the
								play cursor will be on the next frame boundary.

								We will then look to see if the write cursor is before that by at
								least our safety value. If it is, the target fill position is that
								frame boundary plus one frame. This gives us perfect audio sync in
								the case of a card that has low enough latency.

								If the write cursor is after the next frame boundary, then we assume
								we can never sync the audio perfectly, so we will write one frame's
								worth of audio plus the safety margin's worth of guard samples.
							*/

							if (!SoundIsValid)
							{
								SoundOutput.RunningSampleIndex = WriteCursor / SoundOutput.BytesPerSample;
								SoundIsValid = true;
							}

							DWORD ByteToLock = (SoundOutput.RunningSampleIndex * SoundOutput.BytesPerSample) % SoundOutput.SecondaryBufferSize;

							DWORD ExpectedSoundBytesPerFrame = (DWORD)((f32)(SoundOutput.SamplesPerSecond * SoundOutput.BytesPerSample) / GameUpdateHz);
							f32 SecondsLeftUntilFlip = (TargetSecondsPerFrame - FromBeginToAudioSeconds);
							DWORD ExpectedBytesUntilFlip = (DWORD)((SecondsLeftUntilFlip / TargetSecondsPerFrame) * (f32)ExpectedSoundBytesPerFrame);
							DWORD ExpectedFrameBoundaryByte = PlayCursor + ExpectedBytesUntilFlip;

							DWORD SafeWriteCursor = WriteCursor;
							if (SafeWriteCursor < PlayCursor)
							{
								SafeWriteCursor += SoundOutput.SecondaryBufferSize;
							}
							Assert(SafeWriteCursor >= PlayCursor);
							SafeWriteCursor += SoundOutput.SafetyBytes;

							b32 AudioCardIsLowLatency = (SafeWriteCursor < ExpectedFrameBoundaryByte);

							DWORD TargetCursor = 0;
							if (AudioCardIsLowLatency)
							{
								TargetCursor = ExpectedFrameBoundaryByte + ExpectedSoundBytesPerFrame;
							}
							else
							{
								TargetCursor = WriteCursor + ExpectedSoundBytesPerFrame + SoundOutput.SafetyBytes;
							}
							TargetCursor = (TargetCursor % SoundOutput.SecondaryBufferSize);

							DWORD BytesToWrite = 0;
							if (ByteToLock > TargetCursor)
							{
								BytesToWrite = SoundOutput.SecondaryBufferSize - ByteToLock;
								BytesToWrite += TargetCursor;
							}
							else
							{
								BytesToWrite = TargetCursor - ByteToLock;
							}

							game_sound_output_buffer SoundBuffer = {};
							SoundBuffer.SamplesPerSecond = SoundOutput.SamplesPerSecond;
							SoundBuffer.SampleCount = Align8(BytesToWrite / SoundOutput.BytesPerSample);
							BytesToWrite = SoundBuffer.SampleCount * SoundOutput.BytesPerSample;
							SoundBuffer.Samples = Samples;

							if (Game.GetSoundSamples)
							{
								Game.GetSoundSamples(&GameMemory, &SoundBuffer);
							}

#if P5ENGINE_INTERNAL
							win32_debug_time_marker* Marker = &DebugTimeMarkers[DebugTimeMarkerIndex];
							Marker->OutputPlayCursor = PlayCursor;
							Marker->OutputWriteCursor = WriteCursor;
							Marker->OutputLocation = ByteToLock;
							Marker->OutputByteCount = BytesToWrite;
							Marker->ExpectedFlipPlayCursor = ExpectedFrameBoundaryByte;

							DWORD UnwrappedWriteCursor = WriteCursor;
							if (UnwrappedWriteCursor < PlayCursor)
							{
								UnwrappedWriteCursor += SoundOutput.SecondaryBufferSize;
							}
							AudioLatencyBytes = UnwrappedWriteCursor - PlayCursor;
							AudioLatencySeconds = ((f32)AudioLatencyBytes / (f32)SoundOutput.BytesPerSample) / (f32)SoundOutput.SamplesPerSecond;
#endif
							Win32FillSoundBuffer(&SoundOutput, ByteToLock, BytesToWrite, &SoundBuffer);
						}
						else
						{
							SoundIsValid = false;
						}

						LARGE_INTEGER WorkCounter = Win32GetWallClock();
						f32 WorkSecondsElapsed = Win32GetSecondsElapsed(LastCounter, WorkCounter);

						// TODO: NOT TESTED YET! PROBABLY BUGGY!!!!!
						f32 SecondsElapsedForFrame = WorkSecondsElapsed;
						if (SecondsElapsedForFrame < TargetSecondsPerFrame)
						{
							if (SleepIsGranular)
							{
								DWORD SleepMS = (DWORD)(1000.0f * (TargetSecondsPerFrame - SecondsElapsedForFrame));
								if (SleepMS > 0)
								{
									Sleep(SleepMS);
								}
							}

							f32 TestSecondsElapsedForFrame = Win32GetSecondsElapsed(LastCounter, Win32GetWallClock());
							if (TestSecondsElapsedForFrame < TargetSecondsPerFrame)
							{
								// TODO: LOG MISSED SLEEP HERE
							}

							while (SecondsElapsedForFrame < TargetSecondsPerFrame)
							{
								SecondsElapsedForFrame = Win32GetSecondsElapsed(LastCounter, Win32GetWallClock());
							}
						}
						else
						{
							// TODO: MISSED FRAME RATE!
							// TODO: Logging
						}

						LARGE_INTEGER EndCounter = Win32GetWallClock();
						f32 MSPerFrame = 1000.0f * Win32GetSecondsElapsed(LastCounter, EndCounter);
						LastCounter = EndCounter;

						win32_window_dimension Dim = Win32GetWindowDimension(Window);
						// HDC DeviceContext = GetDC(Window);
						Win32DisplayBufferInWindow(DeviceContext, &GlobalBackbuffer, Dim.Width, Dim.Height);
						// ReleaseDC(Window, DeviceContext);

						FlipWallClock = Win32GetWallClock();
#if P5ENGINE_INTERNAL
						{
							DWORD PlayCursor;
							DWORD WriteCursor;
							if (SUCCEEDED(GlobalSecondaryBuffer->GetCurrentPosition(&PlayCursor, &WriteCursor)))
							{
								Assert(DebugTimeMarkerIndex < ArrayCount(DebugTimeMarkers));
								win32_debug_time_marker* Marker = &DebugTimeMarkers[DebugTimeMarkerIndex];

								Marker->FlipPlayCursor = PlayCursor;
								Marker->FlipWriteCursor = WriteCursor;
							}
						}
#endif

						game_input* Temp = NewInput;
						NewInput = OldInput;
						OldInput = Temp;
						// TODO: Should I clear these here?

#if P5ENGINE_INTERNAL
						++DebugTimeMarkerIndex;
						if (DebugTimeMarkerIndex == ArrayCount(DebugTimeMarkers))
						{
							DebugTimeMarkerIndex = 0;
						}
#endif
					}
				}
			}
			else
			{
				// TODO: Logging Could Not Allocate Memory
			}
		}
		else
		{
			// TODO: Logging No Window Returned
		}
	}
	else
	{
		// TODO: Logging Cannot Register WindowClass
	}

	return(0);
}
