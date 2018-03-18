/******************************************************************************
File:   win32_projecth.cpp
Author: GyuHyeon Lee
Email:  email: evulstudio@gmail.com
Date:   24/05/2017
Info:   This contains windows platform layer.

Notice: (C) Copyright 2017 by GyuHyeon, Lee. All Rights Reserved. $
******************************************************************************/
/*****
    Windows Data Types
    https://msdn.microsoft.com/en-us/library/windows/desktop/aa383751(v=vs.85).aspx

    system Error Codes
    https://msdn.microsoft.com/en-us/library/windows/desktop/ms681382(v=vs.85).aspx

    IDirectSound8
    https://msdn.microsoft.com/ko-kr/library/windows/desktop/ee418055(v=vs.85).aspx
*****/

/*****
    TODO: THIS IS NOT A FINAL PATFORM LAYER!!

    - Remove the cursor spinning thing because we are not loading anything initially
    - Saved game location //Create temporary folder for debugging purpose?
    - Getting a handle to our own excutable file
    - Asset loading path
    - Thrading (launch a thred)
    - Raw Input (support for multiple keyboards)
    - ClipCursor(multimonitor support)
    - QueryCancelAutoplay
    - WM_ACTIVATEAPP (for when we are not the active application)
    - Blit speed improvements (bitblt)
    - Hardware accelration (OpenGL or Direct3D or BOTH??)
    - GetKeyboadLayout (for French keyboard, international WASD support)
    = ChangeDisplaySetting option if we detect slow fullscreen blit??
    - OpenGL Stuff? How can we get that Opengl things widthen win32 playform layer?

    JUST A PARTIAL LIST OF STUFF!!
*****/

//Always mind the orders - it matters!

#include "fox.h"
#include "fox_intrinsics.h"

#include <windows.h>
#include <stdio.h>
#include <malloc.h>
// These are not linked directly = We are using GetProaddress to get the specific function
// Because some of the functions may not be inside the game!
#include <xinput.h>
#include <dsound.h>
// OpenGL for Windows(win32)
#include <gl/gl.h>

#include "win32_fox.h"

//Global Variables
global_variable bool32 globalRunning;
global_variable win32_offscreen_buffer globalBackBuffer;
global_variable LPDIRECTSOUNDBUFFER globalSecondaryBuffer;
global_variable int64 perfCountFrequency;
global_variable bool32 globalDEBUGShowCursor;

global_variable WINDOWPLACEMENT globalWindowPosition = {sizeof(globalWindowPosition)};

/*****
    Load XInput functions ourselves because if we don't do this
    the game will not just start without controller(player can just use keyboard!).

    #define X_INPUT_XXX_STATE(name) is to make XInputXXXStateStub 
    so that we can initialize the function pointer and don't make it NULL.

    We also need to typedef it so that we can have function pointer with that type.

    #define XInputXXXState XInputXXXState_ is for our own sake
    so that we can just write XInputXXXState instead of XInputXXXState_. 
*****/
//XInputGetState
#define X_INPUT_GET_STATE(name) DWORD WINAPI name(DWORD dwUserIndex, XINPUT_STATE *pState)
typedef X_INPUT_GET_STATE(x_input_get_state);
X_INPUT_GET_STATE(XInputGetStateStub)   
{
    //If the function pointer of the xinputgetstate points this,
    //it means the device is not connected.
    return ERROR_DEVICE_NOT_CONNECTED;
}
global_variable x_input_get_state *XInputGetState_ = XInputGetStateStub;
#define XInputGetState XInputGetState_

//XInputSetState
#define X_INPUT_SET_STATE(name) DWORD WINAPI name(DWORD dwUserIndex, XINPUT_VIBRATION *pViration)
typedef X_INPUT_SET_STATE(x_input_set_state);
X_INPUT_SET_STATE(XInputSetStateStub)
{
    return ERROR_DEVICE_NOT_CONNECTED;
}
global_variable x_input_set_state *XInputSetState_ = XInputSetStateStub;
#define XInputSetState XInputSetState_

//NOTE : This function loads xinput library and functions
internal void
Win32LoadXInput(void)
{
    //TODO : Find out what windows has certain type of xinput dll.
    HMODULE XInputLibrary = LoadLibraryA("xinput1_4.dll");

    //Try to load diffrent xinput dlls
    if(!XInputLibrary)
    {
        XInputLibrary = LoadLibraryA("xinput1_3.dll");
        if(!XInputLibrary)
            XInputLibrary = LoadLibraryA("xinput9_1_0.dll");
    }
    /*****
        FARPROC WINAPI GetProcAddress(
        _In_ HMODULE hModule,
        _In_ LPCSTR  lpProcName
        );

        Inside the loaded library(using LoadLibrary), 
        find the function or variable with certain name(lpProcName). 
    *****/
    if(XInputLibrary)
    {
        XInputGetState_ = (x_input_get_state *)GetProcAddress(XInputLibrary, "XInputGetState");      
        XInputSetState_ = (x_input_set_state *)GetProcAddress(XInputLibrary, "XInputSetState");
    }
}

internal FILETIME
Win32GetFileTime(char *fileName)
{
    FILETIME lastWriteTime = {};
    WIN32_FILE_ATTRIBUTE_DATA data;

    if(GetFileAttributesEx(fileName, GetFileExInfoStandard, &data))
    {
        lastWriteTime = data.ftLastWriteTime;
    }

    return lastWriteTime;
}

internal win32_game_code
Win32LoadGameCode(char *sourceDLLName, char *tempDLLName, char *lockFileName)
{
    win32_game_code result = {};

    WIN32_FILE_ATTRIBUTE_DATA ignored;
    if(!    GetFileAttributesEx(lockFileName, GetFileExInfoStandard, &ignored))
    {
        result.DLLLastWriteTime = Win32GetFileTime(sourceDLLName);

        CopyFile(sourceDLLName, tempDLLName, FALSE);

        result.gameCodeDLL = LoadLibraryA(tempDLLName);

        if(result.gameCodeDLL)
        {
            result.updateAndRender = (game_update_and_render *)GetProcAddress(result.gameCodeDLL, "GameUpdateAndRender");
            result.getSoundSamples = (game_get_sound_samples *)GetProcAddress(result.gameCodeDLL, "GameGetSoundSamples");

            result.isValid = (result.updateAndRender && result.getSoundSamples);
        }
    }

    if(!result.isValid)
    {
        result.updateAndRender = 0;
        result.getSoundSamples = 0;        
    }

    return result;
}

internal void
Win32UnloadGameCode(win32_game_code *gameCode)
{
    if(gameCode->gameCodeDLL)
    {
        FreeLibrary(gameCode->gameCodeDLL);
    }

    gameCode->isValid = false;
    gameCode->updateAndRender = 0;
    gameCode->getSoundSamples = 0;
}

DEBUG_PLATFORM_FREE_FILE_MEMORY(DEBUGPlatformFreeFileMemory)
{
    if(memory)
    {
        VirtualFree(memory, 0, MEM_RELEASE);
    }
}

DEBUG_PLATFORM_READ_ENTIRE_FILE(DEBUGPlatformReadEntireFile)
{
    debug_read_file_result result = {};

    HANDLE fileHandle = CreateFileA(fileName, GENERIC_READ, FILE_SHARE_READ, 0, OPEN_EXISTING, 0, 0);

    if(fileHandle != INVALID_HANDLE_VALUE)
    {
		LARGE_INTEGER fileSize = {};
        /*
        Using GetFileSizeEx instead of GetFileSize because GetFileSize can only hold 32 bit value
        - which means up to 4 gigabytes.
        */
        if(GetFileSizeEx(fileHandle, &fileSize))
        {
            uint32 fileSize32 = SafeTruncateUInt64(fileSize.QuadPart);
            result.content = VirtualAlloc(0, fileSize32, MEM_RESERVE|MEM_COMMIT, PAGE_READWRITE);
            if(result.content)
            {
                DWORD bytesRead;

                //fileSize32 == bytesRead is for the case
                //when the file size has been changed while we were reading the file.
                if(ReadFile(fileHandle, result.content, fileSize32, &bytesRead, 0) && 
                    fileSize32 == bytesRead)
                {
                    result.contentSize = bytesRead;
                }
                else
                {
                    //if it failed, free the allocated memory
                    //so that the user can know something bad happened.
                    DEBUGPlatformFreeFileMemory(thread, result.content);
                    result.content = 0;
                    result.contentSize = 0;
                }//if(ReadFile)
            }
            else
            {

            }//if(result.content)
        }
        else
        {

        }//if(GetFileSizeEx)

        CloseHandle(fileHandle);
    }
    else
    {
    }//if(fileHandle)

    return result;
}

DEBUG_PLATFORM_WRTIE_ENTIRE_FILE(DEBUGPlatformWriteEntireFile)
{
    bool32 result = 0;

    HANDLE fileHandle = CreateFileA(fileName, GENERIC_WRITE, 0, 0, CREATE_ALWAYS, 0, 0);
    if(fileHandle != INVALID_HANDLE_VALUE)
    {
        DWORD bytesWritten;
        if(WriteFile(fileHandle, memory, memorySize, &bytesWritten, 0))
        {
           result = (bytesWritten == memorySize); 
        }
        else
        {
        }//if(WriteFile)
        
        CloseHandle(fileHandle);
    }
    else
    {
    }//if(fileHandle)

    return result;
}

internal void
Win32InitOpenGL(HWND window)
{
    HDC windowDC = GetDC(window);

    // Setup the opengl by specifying what we are trying to do
    // what pixel format we are using, are we display for the screen..
    PIXELFORMATDESCRIPTOR pixelFormat = {};
    pixelFormat.nSize = sizeof(pixelFormat);
    pixelFormat.nVersion = 1;
    pixelFormat.dwFlags = PFD_SUPPORT_OPENGL | PFD_DRAW_TO_WINDOW | PFD_DOUBLEBUFFER;
    pixelFormat.cColorBits = 32;
    pixelFormat.cAlphaBits = 8;

    int suggestedPixelFormatIndex = ChoosePixelFormat(windowDC, &pixelFormat);
#if 1

    PIXELFORMATDESCRIPTOR suggestedPixelFormat = {};
    DescribePixelFormat(windowDC, suggestedPixelFormatIndex,
                        sizeof(PIXELFORMATDESCRIPTOR), &suggestedPixelFormat);

    SetPixelFormat(windowDC, suggestedPixelFormatIndex, &suggestedPixelFormat);
#endif
    // Opengl Rendering Context
    // This is specific for the thread, so a thread that calls the opengl funcitons
    // MUST have called this!
    HGLRC openglRC = wglCreateContext(windowDC);
    if(wglMakeCurrent(windowDC, openglRC))
    {
        // NOTE : Setting up Opengl succeeded
    } 
    else
    {
        InvalidCodePath;
        // Failed, should have never come down here
    }

    ReleaseDC(window, windowDC);
}

#define DIRECT_SOUND_CREATE(name) HRESULT WINAPI name(LPCGUID pcGuidDevice, LPDIRECTSOUND *ppDS, LPUNKNOWN pUnkOuter)
typedef DIRECT_SOUND_CREATE(direct_sound_create); 

//Set primary & secondary sound buffer
internal void
Win32InitDSound(HWND hWnd, int32 samplesPerSec, int32 secondaryBufferSize)
{
    HMODULE DSoundLibrary = LoadLibraryA("dsound.dll");
    if(DSoundLibrary)
    {
        // NOTE: Get a DirectSound object! - cooperative
        direct_sound_create *DirectSoundCreate = (direct_sound_create *)
            GetProcAddress(DSoundLibrary, "DirectSoundCreate");

        // TODO: Double-check that this works on XP
        // TODO: Should we use direct sound 7 instead of 8 for the XP? 
        LPDIRECTSOUND directSound;//directSound object because of c++
        if(DirectSoundCreate && SUCCEEDED(DirectSoundCreate(0, &directSound, 0)))
        {
            //Set the format so that to let the computer know
            //how it should read the sound buffers
            WAVEFORMATEX waveFormat = {};
            //PCM - Pulse Code Modulation
            //It's a method used to digitally represent analog.
            //It is the standard way in computer.
            waveFormat.wFormatTag = WAVE_FORMAT_PCM;
            waveFormat.nChannels = 2;
            waveFormat.nSamplesPerSec = samplesPerSec;            
            waveFormat.wBitsPerSample = 16;
            waveFormat.nBlockAlign = (waveFormat.nChannels * waveFormat.wBitsPerSample) / 8;
            waveFormat.nAvgBytesPerSec = waveFormat.nSamplesPerSec * waveFormat.nBlockAlign;
            waveFormat.cbSize = 0;

            //DSSCL_PRIORITY level allows us to set the format(SetFormat funtion).
            //If used DSSCL_NORMAL, the output format is limited.
            if(SUCCEEDED(directSound->SetCooperativeLevel(hWnd, DSSCL_PRIORITY)))
            {
                //Primary buffer description.
                DSBUFFERDESC bufferDesc = {};
                bufferDesc.dwSize = sizeof(bufferDesc);
                bufferDesc.dwFlags = DSBCAPS_PRIMARYBUFFER;

                LPDIRECTSOUNDBUFFER primaryBuffer;
                if(SUCCEEDED(directSound->CreateSoundBuffer(&bufferDesc, &primaryBuffer, 0)))
                {
                    if(SUCCEEDED(primaryBuffer->SetFormat(&waveFormat)))
                    {
                    }
                    else
                    {
                        //Setting format for the primary buffer failed.
                    }
                }
                else
                {
                    //Creating Primary Buffer Failed.                    
                }
            }
            else
            {
                //Function SetCooperativeLevel() failed.
            }

            //Create Secondary buffer.
            DSBUFFERDESC bufferDesc = {};
            bufferDesc.dwSize = sizeof(bufferDesc);
            bufferDesc.dwFlags = DSBCAPS_GETCURRENTPOSITION2;
            bufferDesc.dwBufferBytes = secondaryBufferSize;
            bufferDesc.lpwfxFormat = &waveFormat;
            if(SUCCEEDED(directSound->CreateSoundBuffer(&bufferDesc, &globalSecondaryBuffer, 0)))
            {
				//Creating secondary sound buffer succeeded
            }
            else
            {
				//Creating secondary sound buffer failed
				//TODO : Logging
            }
        }
        else
        {
			//TODO : Logging
        }
    }
    else
    {       
    }
}

internal void
Win32ClearSoundBuffer(win32_sound_output *soundOutput)
{
    VOID *region1;
    DWORD region1Size;
    VOID *region2;
    DWORD region2Size;
    if(SUCCEEDED(globalSecondaryBuffer->Lock(0, soundOutput->secondaryBufferSize,
                                        &region1, &region1Size, 
                                        &region2, &region2Size, 
                                        0)))
    {
        //because we want to initalize this byte by byte,
        //cast it in uint8
        uint8 *destByte = (uint8 *)region1; 
        for(DWORD byteIndex = 0;
            byteIndex < region1Size;
            byteIndex++)
        {
            *destByte++ = 0;
        }

        destByte = (uint8 *)region2; 
        for(DWORD byteIndex = 0;
            byteIndex < region2Size;
            byteIndex++)
        {
            *destByte++ = 0;
        }
        
        globalSecondaryBuffer->Unlock(region1, region1Size, region2, region2Size);
    }
}

internal void
Win32FillSoundBuffer(win32_sound_output *soundOutput, DWORD byteToLock, DWORD bytesToWrite, 
                    game_sound_output_buffer *sourceBuffer)
{

    //TOOD : More strenuous test!
    VOID *region1;
    DWORD region1Size;
    VOID *region2;
    DWORD region2Size;

    if(SUCCEEDED(globalSecondaryBuffer->Lock(byteToLock, bytesToWrite,
                                        &region1, &region1Size, 
                                        &region2, &region2Size, 
                                        0)))
    {
        //targetSample = Where in the memory are we going to write.
        DWORD region1SampleCount = region1Size / soundOutput->bytesPerSample;        
        int16 *targetSample = (int16 *)region1;
        int16 *sourceSamples = sourceBuffer->samples;
        for(DWORD sampleIndex = 0;
            sampleIndex < region1SampleCount;
            ++sampleIndex)
        {
            *targetSample++ = *sourceSamples++;//left
            *targetSample++ = *sourceSamples++;//right
            ++soundOutput->runningSampleIndex;
        }

        DWORD region2SampleCount = region2Size / soundOutput->bytesPerSample;                        
        targetSample = (int16 *)region2;
        for(DWORD sampleIndex = 0;
            sampleIndex < region2SampleCount;
            ++sampleIndex)
        {
            *targetSample++ = *sourceSamples++;//left
            *targetSample++ = *sourceSamples++;//right;
            ++soundOutput->runningSampleIndex;                            
        }

        //After writing to the secondary buffer, unlock it.
        globalSecondaryBuffer->Unlock(region1, region1Size, region2, region2Size);
    }
}

win32_window_dimension 
Win32GetWindowDimension(HWND window)
{
    win32_window_dimension result;

    /*
        structure RECT
        left, top, right, bottom
    */
    RECT clientRect;
    GetClientRect(window, &clientRect);
    result.width = clientRect.right - clientRect.left;
    result.height = clientRect.bottom - clientRect.top;

    return result;
}
    
void Win32ResizeDIBSection(win32_offscreen_buffer *buffer, int width, int height)
{
    if(buffer->memory)
    {
        VirtualFree(buffer->memory, 0, MEM_RELEASE);
    }

    buffer->width = width;
    buffer->height = height;

    // NOTE: When the biHeight field is negative, this is the clue to
    // Windows to treat this bitmap as top-down, not bottom-up, meaning that
    // the first three bytes of the image are the color for the top left pixel
    // in the bitmap, not the bottom left!
    buffer->info.bmiHeader.biSize = sizeof(buffer->info.bmiHeader);
    buffer->info.bmiHeader.biWidth = buffer->width;
    buffer->info.bmiHeader.biHeight = buffer->height;
    buffer->info.bmiHeader.biPlanes = 1;
    buffer->info.bmiHeader.biBitCount = 32;
    buffer->info.bmiHeader.biCompression = BI_RGB;

    buffer->bytesPerPixel = 4;

    int bitmapMemorySize = buffer->width * buffer->height * buffer->bytesPerPixel;
    buffer->memory = VirtualAlloc(buffer->memory, bitmapMemorySize, MEM_COMMIT, PAGE_READWRITE);
    buffer->pitch = width * buffer->bytesPerPixel;
}

void Win32DisplayBuffer(HDC deviceContext, 
                        int windowWidth, int windowHeight,
                        win32_offscreen_buffer *buffer)
{
#if 0
    int32 offsetX = 0;
    int32 offsetY = 0;

    if(windowWidth >= buffer->width * 2 &&
        windowHeight >= buffer->height * 2)
    {
        StretchDIBits(deviceContext, 
            offsetX, offsetY, 2 * buffer->width, 2 * buffer->height,
            offsetX, offsetY, buffer->width, buffer->height,
            buffer->memory,
            &buffer->info,
            DIB_RGB_COLORS, SRCCOPY);
    }
    else
    {
        PatBlt(deviceContext, 0, 0 , windowWidth, offsetY, BLACKNESS);
        PatBlt(deviceContext, 0, offsetY + buffer->height , windowWidth, windowHeight, BLACKNESS);
        PatBlt(deviceContext, 0, 0 , offsetX, windowHeight, BLACKNESS);
        PatBlt(deviceContext, offsetX + buffer->width, 0 , windowWidth, windowHeight, BLACKNESS);

        // For prototyping purposes, we're going to always blit 1 to 1 pixels
        StretchDIBits(deviceContext, 
                    offsetX, offsetY, buffer->width, buffer->height,
                    offsetX, offsetY, buffer->width, buffer->height,
                    buffer->memory,
                    &buffer->info,
                    DIB_RGB_COLORS, SRCCOPY);
    }

#endif
    glViewport(0, 0, windowWidth, windowHeight);
    glClearColor(1.0f, 0.0f, 0.0f, 1.0f);
    glClear(GL_COLOR_BUFFER_BIT);
    SwapBuffers(deviceContext);
}

internal
void Win32ProcessKeyboardMessage(game_button_state *newState, bool32 isDown)
{
    newState->endedDown = isDown;
    ++newState->halfTransitionCount;
}

internal
real32 Win32ProcessStickValue(SHORT thumbValue, SHORT deadZoneThreshold)
{
    real32 result = 0.0f;

    if(thumbValue < -deadZoneThreshold)
    {
        result = (real32)thumbValue / 32768.0f;
    }
    else if(thumbValue > deadZoneThreshold)
    {
        result = (real32)thumbValue / 32767.0f;
    }

    return result;
}

internal
void Win32ProcessXInputDigitalButton(game_button_state *oldState, game_button_state *newState,
                                DWORD XInputButtonState, DWORD buttonBit)
{
    newState->endedDown = ((XInputButtonState & buttonBit) == buttonBit);
    newState->halfTransitionCount = (oldState->endedDown != newState->endedDown)? 1 : 0;
}

inline LARGE_INTEGER
Win32GetWallClock(void)
{
    LARGE_INTEGER result;
    QueryPerformanceCounter(&result);
    return result;
}

inline real32 
Win32GetSecondsElpased(LARGE_INTEGER start, LARGE_INTEGER end)
{
    return (real32)(end.QuadPart - start.QuadPart) / (real32)perfCountFrequency;
}

internal void
Win32DebugDrawVertical(win32_offscreen_buffer *backBuffer, int x, int top, int bottom, uint32 color)
{
    uint8 *pixel = (uint8 *)backBuffer->memory + 
                            x * backBuffer->bytesPerPixel + 
                            top * backBuffer->pitch;
    for(int y = top;
        y < bottom;
        ++y)
    {
        *(uint32 *)pixel = color;
        pixel += backBuffer->pitch;
    }
}

internal void
Win32DebugSyncDisplay(win32_offscreen_buffer *backBuffer, 
                    int timeMarkerCount, win32_debug_time_marker *timeMarkers, 
                    win32_sound_output *soundOutput,
                    real32 targetSecondsPerFrame)
{
    int padX = 16;
    int padY = 16;

    int top = padY;
    int bottom = backBuffer->height - padY;

    //gap between each playcursor
    real32 gap = (real32)(backBuffer->width - 2 * padX) / (real32)soundOutput->secondaryBufferSize;
    for(int timeMarkerIndex = 0;
        timeMarkerIndex < timeMarkerCount;
        timeMarkerIndex++)
    {
        int playCursorX = (int)(gap * (real32)timeMarkers[timeMarkerIndex].playCursor);
        int writeCursorX = (int)(gap * (real32)timeMarkers[timeMarkerIndex].writeCursor);        
        Win32DebugDrawVertical(backBuffer, playCursorX, top, bottom, 0xFFFFFFFF);
        Win32DebugDrawVertical(backBuffer, writeCursorX, top, bottom, 0xFFFF0000);
    }
}

LRESULT CALLBACK 
MainWindowProc(HWND hWnd,
                UINT uMsg,
                WPARAM wParam,
                LPARAM lParam)
{
    LRESULT result = 0;
    
    switch(uMsg)
    {
        //Whenever the user presses the close button.
        case WM_CLOSE:
        {
            globalRunning = false;
        } break;

        case WM_DESTROY:
        {
        } break;

        case WM_SIZE:
        {
        } break;

        case WM_ACTIVATEAPP:
        {
        } break;
        case WM_SYSKEYDOWN:
        case WM_SYSKEYUP:
        case WM_KEYDOWN:
        case WM_KEYUP:
        {
            Assert("keyboard input should not come in here!!");
        }break;
        case WM_PAINT:
        {
            /*****
            typedef struct tagPAINTSTRUCT {
                HDC  hdc;
                BOOL fErase; //whether the background should be deleted or not
                RECT rcPaint; //upper left and lower right of the rectangle in which the painting is requested.
                BOOL fRestore; //reserved.
                BOOL fIncUpdate; //reserved.
                BYTE rgbReserved[32]; //reserved.
                } PAINTSTRUCT, *PPAINTSTRUCT;
            *****/
            PAINTSTRUCT paintStruct;
            HDC deviceContext = BeginPaint(hWnd, &paintStruct);

            win32_window_dimension dimension = Win32GetWindowDimension(hWnd);
            Win32DisplayBuffer(deviceContext, dimension.width, dimension.height, &globalBackBuffer);
            EndPaint(hWnd, &paintStruct);
        }
        
        case WM_SETCURSOR:
        {
            if(globalDEBUGShowCursor)
            {
                result = DefWindowProc(hWnd, uMsg, wParam, lParam); 
            }
            else
            {
                SetCursor(0);
            }
        }break;

        default:
        {
            result = DefWindowProc(hWnd, uMsg, wParam, lParam);
        }
    }

    return result;
}

internal void
ToggleFullSceen(HWND window)
{
    // NOTE : This follows Raymond Chen's prescription
    // for fullscreen toggling, see:
    // http://blogs.msdn.com/b/oldnewthing/archive/2010/04/12/9994016.aspx

    DWORD style = GetWindowLong(window, GWL_STYLE);
    if(style & WS_OVERLAPPEDWINDOW)
    {
        MONITORINFO monitorInfo = {sizeof(monitorInfo)};
        if(GetWindowPlacement(window, &globalWindowPosition) && 
            GetMonitorInfo(MonitorFromWindow(window, MONITOR_DEFAULTTOPRIMARY), &monitorInfo))
        {
            // Change window style to non OVERLAPPEDWINDOW
            SetWindowLong(window, GWL_STYLE, style & ~WS_OVERLAPPEDWINDOW);
            /*
                NOTE : Set the window starting from the top left corner of the monitor
                        and big as the monitor to make the fullscreen
            */
            SetWindowPos(window, HWND_TOP,
                         monitorInfo.rcMonitor.left, monitorInfo.rcMonitor.top,
                         monitorInfo.rcMonitor.right - monitorInfo.rcMonitor.left,
                         monitorInfo.rcMonitor.bottom - monitorInfo.rcMonitor.top,
                         SWP_NOOWNERZORDER | SWP_FRAMECHANGED);
        }
    }
    else
    {
        SetWindowLong(window, GWL_STYLE, style | WS_OVERLAPPEDWINDOW);
        SetWindowPlacement(window, &globalWindowPosition);
        SetWindowPos(window, 0, 0, 0, 0, 0,
                     SWP_NOMOVE | SWP_NOSIZE | SWP_NOZORDER |
                     SWP_NOOWNERZORDER | SWP_FRAMECHANGED);
    }
}

internal void 
Win32ProcessPendingMessages(game_controller *keyboardController)
{
    MSG msg;
    while(PeekMessageA(&msg, 0, 0, 0, PM_REMOVE))
    {
        switch(msg.message)
        {
            case WM_SYSKEYDOWN:
            case WM_SYSKEYUP:
            case WM_KEYDOWN:
            case WM_KEYUP:
            {
                uint32 vkCode = (uint32)msg.wParam;

                bool wasDown = ((msg.lParam & (1 << 30)) != 0);
                bool isDown = ((msg.lParam & (1 << 31)) == 0);
                
                //If the key state had been changed
                if(wasDown != isDown)
                {
                    if(vkCode == 'W')
                    {
                        Win32ProcessKeyboardMessage(&keyboardController->moveUp, isDown);
                    }
                    else if(vkCode == 'A')
                    {
                        Win32ProcessKeyboardMessage(&keyboardController->moveLeft, isDown);
                    }
                    else if(vkCode == 'S')
                    {
                        Win32ProcessKeyboardMessage(&keyboardController->moveDown, isDown);
                    }
                    else if(vkCode == 'D')
                    {
                        Win32ProcessKeyboardMessage(&keyboardController->moveRight, isDown);
                    }
                    else if(vkCode == 'Q')
                    {
                    }
                    else if(vkCode == 'E')
                    {
                    }
                    else if(vkCode == VK_UP)
                    {
                        Win32ProcessKeyboardMessage(&keyboardController->actionUp, isDown);
                    }
                    else if(vkCode == VK_LEFT)
                    {
                        Win32ProcessKeyboardMessage(&keyboardController->actionLeft, isDown);
                    }
                    else if(vkCode == VK_DOWN)
                    {
                        Win32ProcessKeyboardMessage(&keyboardController->actionDown, isDown);
                    }
                    else if(vkCode == VK_RIGHT)
                    {
                        Win32ProcessKeyboardMessage(&keyboardController->actionRight, isDown);
                    }
                    else if(vkCode == VK_ESCAPE)
                    {
                        Win32ProcessKeyboardMessage(&keyboardController->back, isDown);
                    }
                    else if(vkCode == VK_SPACE)
                    {
                        Win32ProcessKeyboardMessage(&keyboardController->start, isDown);                        
                    }

                    if(isDown)
                    {
                        bool32 altKeyWasDown = msg.lParam & (1 << 29);

                        if(vkCode == VK_F4 && altKeyWasDown)
                        {
                            globalRunning = false;
                        }
                        if(vkCode == VK_RETURN && altKeyWasDown)
                        {
                            if(msg.hwnd)
                            {
                                ToggleFullSceen(msg.hwnd);
                            }
                        }
                    }
                }
            }break;
        }

        TranslateMessage(&msg);
        DispatchMessage(&msg);
    }
}

void
CatStrings(char *sourceA, size_t sourceACount,
            char *sourceB, size_t sourceBCount,
            char *dest, size_t destCount)
{
    for(size_t index = 0;
        index < sourceACount;
        ++index)
    {
        *dest++ = *sourceA++;
    }

    for(size_t index = 0;
        index < sourceBCount;
        ++index)
    {
        *dest++ = *sourceB++;
    }
    
    *dest++ = 0;//null termination
}

internal void
Win32GetEXEFileName(win32_state *state)
{
    //This gives us where is our exe file, including the name of exe too.
    DWORD sizeOfFileName = GetModuleFileName(0, state->exeFileName, sizeof(state->exeFileName));
    state->onePastLastSlash = state->exeFileName;
    //because we want to only know where is the exe, we have to truncate the buffer.
    for(char *scan = state->exeFileName;
        *scan;
        ++scan)
    {
        if(*scan == '\\')
        {
            state->onePastLastSlash = scan + 1;
        }
    }
}

internal int
StringLength(char *string)
{
    int count = 0;
    while(*string++)
    {
        ++count;
    }
    return count;
}

internal void
Win32BuildEXEPathFileName(win32_state *win32State, char *fileName,
                            int destCount, char *dest)
{
    CatStrings(win32State->exeFileName, win32State->onePastLastSlash - win32State->exeFileName,
            fileName, StringLength(fileName),
            dest, destCount);
}

internal void
HandleDebugCycleCounter(game_memory *gameMemory)
{
    OutputDebugStringA("DEBUG CYCLE COUNT : \n");
#if FOX_DEBUG
    for(int counterIndex = 0;
        counterIndex < ArrayCount(gameMemory->counters);
        ++counterIndex)
    {
        debug_cycle_counter *counter = gameMemory->counters + counterIndex; 
        if(counter->hitCount)
        {
            char buffer[512];
            sprintf_s(buffer, " %d : %I64ucycles, %dhit, %I64ucycles/hit\n",
                    counterIndex,  
                    counter->cycleCount, 
                    counter->hitCount, 
                    counter->cycleCount/counter->hitCount);
            OutputDebugStringA(buffer);

            counter->cycleCount = 0;
            counter->hitCount = 0;
        }
    }
#endif
}


int CALLBACK 
WinMain(HINSTANCE hInstance,
        HINSTANCE HPrevInstance,
        LPSTR lpCmdLine,
        int nCmdShow)
{
    //Because the frequency doesn't change, we can just compute here.
    LARGE_INTEGER perfCountFreqResult;
    QueryPerformanceFrequency(&perfCountFreqResult);
    perfCountFrequency = perfCountFreqResult.QuadPart;


    UINT desiredSchedulerMS = 1;
    //if we can adjust granularity in certain computer, we can use sleep
    //if not, we should not, so we are using this variable.
    bool32 isSleepGranular = timeBeginPeriod(desiredSchedulerMS) == TIMERR_NOERROR;

    /*Get the DLL path*/
    win32_state win32State = {};
    Win32GetEXEFileName(&win32State);

    //Get the source dll path    
    char sourceDLLFullPath[MAX_PATH];
    Win32BuildEXEPathFileName(&win32State, "fox.dll", sizeof(sourceDLLFullPath), sourceDLLFullPath);

    //Get the temp source dll path
    char tempDLLFullPath[MAX_PATH];
    Win32BuildEXEPathFileName(&win32State, "fox_temp.dll", sizeof(tempDLLFullPath), tempDLLFullPath);

    char gameCodeLockFullPath[MAX_PATH];
    Win32BuildEXEPathFileName(&win32State, "lock.tmp", sizeof(gameCodeLockFullPath), gameCodeLockFullPath);

    /*Load XInput Library*/
    Win32LoadXInput();

    // TODO : How can we set this up with opengl?
    int32 screenWidth = 960;
    int32 screenHeight = 540;
    /* NOTE : 1080p display mode is 1920x1080 -> Half of that is 960x540
       1920 -> 2048 = 2048-1920 -> 128 pixels
       1080 -> 2048 = 2048-1080 -> pixels 968
       1024 + 128 = 1152
    */
    /*Set the initial screen resolution and allocate backbuffer with that size*/
    Win32ResizeDIBSection(&globalBackBuffer, screenWidth, screenHeight);

#if PROJECTH_DEBUG
    globalDEBUGShowCursor = true;
#endif

    /*Make window class*/
    WNDCLASSEXA windowClass = {};
    windowClass.cbSize = sizeof(windowClass); //size of this structure
    windowClass.style = CS_HREDRAW|CS_VREDRAW;
    windowClass.lpfnWndProc = MainWindowProc; //Function Pointer to the Windows Procedure function
    windowClass.hInstance = hInstance;
    windowClass.lpszClassName = "FoxWindowClass";
    windowClass.hCursor = LoadCursor(0, IDC_ARROW);

    if(RegisterClassExA(&windowClass))
    {
        HWND hWnd = 
            CreateWindowExA(
                0, 
                windowClass.lpszClassName, 
                "FoxTail",
                WS_OVERLAPPEDWINDOW|WS_VISIBLE,
                CW_USEDEFAULT,
                CW_USEDEFAULT,
                CW_USEDEFAULT,
                CW_USEDEFAULT,
                0,
                0,
                hInstance,
                0);

        if(hWnd)
        {
            HDC refreshDC = GetDC(hWnd);
            int monitorRefreshHz = 60;
            int win32RefreshRate = GetDeviceCaps(refreshDC, VREFRESH);
            ReleaseDC(hWnd, refreshDC);
            if(win32RefreshRate > 1)
            {
                monitorRefreshHz = win32RefreshRate;
            }
            int gameUpdateHz = monitorRefreshHz / 2;
            real32 targetSecondsPerFrame = 1.0f / (real32)gameUpdateHz;

            // OpenGL
            Win32InitOpenGL(hWnd);
            win32_sound_output soundOutput = {0};

            soundOutput.samplesPerSecond = 48000;                        
            soundOutput.bytesPerSample = sizeof(int16) * 2;//because we are in stereo
            soundOutput.secondaryBufferSize = soundOutput.samplesPerSecond * soundOutput.bytesPerSample;
            soundOutput.safetyBytes = (DWORD)(((real32)soundOutput.samplesPerSecond*(real32)soundOutput.bytesPerSample / gameUpdateHz)/3.0f);     
            Win32InitDSound(hWnd, soundOutput.samplesPerSecond, soundOutput.secondaryBufferSize);
            Win32ClearSoundBuffer(&soundOutput);

            globalSecondaryBuffer->Play(0, 0, DSBPLAY_LOOPING);

            int16 *samples = (int16 *)VirtualAlloc(0, soundOutput.secondaryBufferSize, 
                                                    MEM_RESERVE|MEM_COMMIT, PAGE_READWRITE);


//If possible, set the base address to where we want for debugging purpose
#if PROJECTH_DEBUG
            LPVOID baseAddress = (LPVOID)Terabytes(2);
#else
            LPVOID baseAddress = 0;
#endif
            game_memory gameMemory = {};
            // TODO : See how much this game actually needs
            gameMemory.permanentStorageSize = Megabytes(64);
            gameMemory.transientStorageSize = Gigabytes(1);
            gameMemory.debugPlatformFreeFileMemory = DEBUGPlatformFreeFileMemory;            
            gameMemory.debugPlatformReadEntireFile = DEBUGPlatformReadEntireFile;
            gameMemory.debugPlatformWriteEntireFile = DEBUGPlatformWriteEntireFile;
            uint64 totalSize = gameMemory.permanentStorageSize + gameMemory.transientStorageSize;
            // TODO :Use MEM_LARGE_PAGES. This need many pre-functions so this is todo.
            
            gameMemory.permanentStorage = VirtualAlloc(baseAddress, (size_t)totalSize,
                                                        MEM_RESERVE|MEM_COMMIT, PAGE_READWRITE);
            gameMemory.transientStorage = ((uint8 *)gameMemory.permanentStorage + gameMemory.permanentStorageSize);

            if(samples && gameMemory.permanentStorage && gameMemory.transientStorage)
            {
                game_input inputs[2] = {0};
                game_input *newInput = &inputs[0];
                game_input *oldInput = &inputs[1];

                LARGE_INTEGER lastCounter = Win32GetWallClock();
                LARGE_INTEGER flipWallClock = Win32GetWallClock();

                //Query Counter
                //Counter is for knowing the real time
                //Cycle shows how much CPU cycle has looped each frame

                int debugTimeMarkerIndex = 0;
                win32_debug_time_marker debugTimeMarkers[30] = {0};
                
                DWORD audioLatencyBytes = 0;
                real32 audioLatencySeconds = 0;
                //At the first frame, sound should not be valid.
                bool32 isSoundValid = false;

                win32_game_code gameCode = {};
                gameCode = Win32LoadGameCode(sourceDLLFullPath, tempDLLFullPath, gameCodeLockFullPath);

                globalRunning = true;                

                uint64 lastCycleCount = __rdtsc();                
                while(globalRunning)
                {
                    newInput->dtForFrame = targetSecondsPerFrame;
                    
                    FILETIME newDLLWriteTime = Win32GetFileTime(sourceDLLFullPath);
                    if(CompareFileTime(&newDLLWriteTime, &gameCode.DLLLastWriteTime) != 0)
                    {
                        Win32UnloadGameCode(&gameCode);
                        gameCode = Win32LoadGameCode(sourceDLLFullPath, tempDLLFullPath, gameCodeLockFullPath);
                    }

                    game_controller *oldKeyboardController = &oldInput->controllers[0];
                    game_controller *newKeyboardController = &newInput->controllers[0];
                    
                    //TODO : Zeroing macros?
                    game_controller zeroController = {};
                    *newKeyboardController = zeroController;
                    for(int buttonIndex = 0;
                        buttonIndex < ArrayCount(newKeyboardController->buttons);
                        ++buttonIndex)
                    {
                        //Copy the old button states
                        newKeyboardController->buttons[buttonIndex].endedDown = 
                            oldKeyboardController->buttons[buttonIndex].endedDown;   
                    }

                    //Process message first so that we know the input                    
                    Win32ProcessPendingMessages(newKeyboardController);
                    
                    POINT mouseP;
                    GetCursorPos(&mouseP);
                    ScreenToClient(hWnd, &mouseP);
                    newInput->mouseX = mouseP.x;
                    newInput->mouseY = mouseP.y;
                    newInput->mouseZ = 0; // TODO : Support mousewheel?
                    Win32ProcessKeyboardMessage(&newInput->mouseButtons[0],
                                                GetKeyState(VK_LBUTTON) & (1 << 15));
                    Win32ProcessKeyboardMessage(&newInput->mouseButtons[1],
                                                GetKeyState(VK_MBUTTON) & (1 << 15));
                    Win32ProcessKeyboardMessage(&newInput->mouseButtons[2],
                                                GetKeyState(VK_RBUTTON) & (1 << 15));
                    Win32ProcessKeyboardMessage(&newInput->mouseButtons[3],
                                                GetKeyState(VK_XBUTTON1) & (1 << 15));
                    Win32ProcessKeyboardMessage(&newInput->mouseButtons[4],
                                                GetKeyState(VK_XBUTTON2) & (1 << 15));

                    int maxControllerCount = XUSER_MAX_COUNT;
                    if(maxControllerCount > ArrayCount(newInput->controllers))
                    {
                        //If there are too many controllers, always process only 
                        //fixed amount of controllers
                        maxControllerCount = ArrayCount(newInput->controllers);
                    }
                    //Process Mulitple Controllers
                    for(int controllerIndex = 0;
                        controllerIndex < maxControllerCount;
                        ++controllerIndex)
                    {
                        XINPUT_STATE controllerState;
                        
                        //To process without the keyboard(which is controllers[0])
                        int ourControllerIndex = controllerIndex + 1;
                        game_controller *oldController = &oldInput->controllers[ourControllerIndex];
                        game_controller *newController = &newInput->controllers[ourControllerIndex];                    
                        if(XInputGetState(controllerIndex, &controllerState) == ERROR_SUCCESS)
                        {
                            newController->isConnected = true;
                            newController->isAnalog = oldController->isAnalog;                            
                            //Just not to write whole 'controllerState.Gamepad' thing
                            XINPUT_GAMEPAD *pad = &controllerState.Gamepad;
                        
                            newController->averageStickX = Win32ProcessStickValue(pad->sThumbLX, XINPUT_GAMEPAD_LEFT_THUMB_DEADZONE);                           
                            newController->averageStickY = Win32ProcessStickValue(pad->sThumbLY, XINPUT_GAMEPAD_LEFT_THUMB_DEADZONE);
                            if(newController->averageStickX != 0.0f || newController->averageStickY != 0.0f)
                            {
                                newController->isAnalog = true;
                            }
                            //Player can also use DPAD 
                            if(pad->wButtons & XINPUT_GAMEPAD_DPAD_UP)
                            {
                                newController->averageStickY = 1.0f;
                                newController->isAnalog = false;
                            }
                            if(pad->wButtons & XINPUT_GAMEPAD_DPAD_DOWN)
                            {
                                newController->averageStickY = -1.0f;
                                newController->isAnalog = false;
                            }
                            if(pad->wButtons & XINPUT_GAMEPAD_DPAD_LEFT)
                            {
                                newController->isAnalog = false;
                                newController->averageStickX = -1.0f;
                            }
                            if(pad->wButtons & XINPUT_GAMEPAD_DPAD_RIGHT)
                            {
                                newController->isAnalog = false;
                                newController->averageStickX = 1.0f;
                            }

                            // bool32 leftShoulder = (pad->wButtons & XINPUT_GAMEPAD_LEFT_SHOULDER);
                            // bool32 rightShoulder = (pad->wButtons & XINPUT_GAMEPAD_RIGHT_SHOULDER);

                            //Convert the stick value(anlog value) into the digital buttons
                            real32 Threshold = 0.5f;
                            Win32ProcessXInputDigitalButton(&oldController->moveLeft, &newController->moveLeft,
                                ((newController->averageStickX < -Threshold)? 1 : 0), 1);
                            Win32ProcessXInputDigitalButton(&oldController->moveRight, &newController->moveRight,
                                ((newController->averageStickX > Threshold)? 1 : 0), 1);
                            Win32ProcessXInputDigitalButton(&oldController->moveUp, &newController->moveUp,
                                ((newController->averageStickY > Threshold)? 1 : 0), 1);
                            Win32ProcessXInputDigitalButton(&oldController->moveDown, &newController->moveDown,
                                ((newController->averageStickY < -Threshold)? 1 : 0), 1);
                            
                            //process digital buttons
                            Win32ProcessXInputDigitalButton(&oldController->actionUp, &newController->actionUp,
                                                            pad->wButtons, XINPUT_GAMEPAD_Y);
                            Win32ProcessXInputDigitalButton(&oldController->actionDown, &newController->actionDown,
                                                            pad->wButtons, XINPUT_GAMEPAD_A);                                                        
                            Win32ProcessXInputDigitalButton(&oldController->actionLeft, &newController->actionLeft,
                                                            pad->wButtons, XINPUT_GAMEPAD_X);
                            Win32ProcessXInputDigitalButton(&oldController->actionRight, &newController->actionRight,
                                                            pad->wButtons, XINPUT_GAMEPAD_B);
                            Win32ProcessXInputDigitalButton(&oldController->start, &newController->start,
                                                            pad->wButtons, XINPUT_GAMEPAD_START);
                            Win32ProcessXInputDigitalButton(&oldController->back, &newController->back,
                                                            pad->wButtons, XINPUT_GAMEPAD_BACK);
                        }
                        else
                        {
                            newController->isConnected = false;
                            //Something went wrong with the contoller!
                            //Maybe it's not connected(unplugged), or something else happened.
                        }
                    }

                    thread_context thread = {};
                    game_offscreen_buffer gameBuffer = {};
                    gameBuffer.memory = globalBackBuffer.memory;
                    gameBuffer.width = globalBackBuffer.width;
                    gameBuffer.height = globalBackBuffer.height;
                    gameBuffer.pitch = globalBackBuffer.pitch;
                    gameBuffer.bytesPerPixel = 4;
                    
                    // Get the update and render function so that we can update the game
                    if(gameCode.updateAndRender)
                    {
                        gameCode.updateAndRender(&thread, &gameMemory, &gameBuffer, newInput);

                        // Clear Timer
                        HandleDebugCycleCounter(&gameMemory);
                    }

                    LARGE_INTEGER audioWallClock = Win32GetWallClock();
                    real32 fromBeginToAudioSeconds = Win32GetSecondsElpased(flipWallClock, audioWallClock);
                        /*NOTE :
                            Here is how sound output computation works!!!
                            
                            When we wake up to write audio, we will look and see what the
                            playcursor position is and we will forecast ahead where we think
                            playcursor will be on the next frame boundary.

                            We will then look to see if the writecursor is before that
                            - which means low audio latency-.
                            If it is, we will write up to the next frame boundary from
                            the writecurosr. So,  the target fill position is
                            (from the writecursor to this frame boundary) + (one whole frame).

                            If the write cursor is after the next frame boundary
                            -which means high audio latnecy-,
                            then we assume we can never sync the audio perfectly,
                            so we will write one frame's worth of audio plush
                            number of guard samples(1ms or something determined to be safe) 
                        */
                    DWORD playCursor;
                    DWORD writeCursor;
                    if(globalSecondaryBuffer->GetCurrentPosition(&playCursor, &writeCursor) == DS_OK)
                    {
                        if(!isSoundValid)
                        {
                            soundOutput.runningSampleIndex = writeCursor / soundOutput.bytesPerSample;
                            isSoundValid = true;
                        }

                        DWORD byteToLock = ((soundOutput.runningSampleIndex*soundOutput.bytesPerSample) %
                                            soundOutput.secondaryBufferSize);

                        DWORD expectedSoundBytesPerFrame =
                            (soundOutput.samplesPerSecond*soundOutput.bytesPerSample) / gameUpdateHz;
                        real32 secondsLeftUntilFlip = (targetSecondsPerFrame - fromBeginToAudioSeconds);
                        //Where the video flip(going to the next frame) happenes
                        DWORD expectedBytesUntilFlip = (DWORD)((secondsLeftUntilFlip/targetSecondsPerFrame)*(real32)expectedSoundBytesPerFrame);

                        DWORD expectedFrameBoundaryByte = playCursor + expectedBytesUntilFlip;
                    
                        DWORD safeWriteCursor = writeCursor;
                        if(safeWriteCursor < playCursor)
                        {
                            safeWriteCursor += soundOutput.secondaryBufferSize;
                        }
                        safeWriteCursor += soundOutput.safetyBytes;
                    
                        bool32 audioCardIsLowLatency = (safeWriteCursor < expectedFrameBoundaryByte);                        

                        DWORD targetCursor = 0;
                        if(audioCardIsLowLatency)
                        {
                            targetCursor = (expectedFrameBoundaryByte + expectedSoundBytesPerFrame);
                        }
                        else
                        {
                            targetCursor = (writeCursor + expectedSoundBytesPerFrame +
                                            soundOutput.safetyBytes);
                        }
                        targetCursor = (targetCursor % soundOutput.secondaryBufferSize);
                    
                        DWORD bytesToWrite = 0;
                        if(byteToLock > targetCursor)
                        {
                            bytesToWrite = (soundOutput.secondaryBufferSize - byteToLock);
                            bytesToWrite += targetCursor;
                        }
                        else
                        {
                            bytesToWrite = targetCursor - byteToLock;
                        }

                        game_sound_output_buffer soundBuffer = {};
                        soundBuffer.samplesPerSecond = soundOutput.samplesPerSecond;
                        soundBuffer.sampleCount = bytesToWrite / soundOutput.bytesPerSample;
                        soundBuffer.samples = samples;
                        if(gameCode.getSoundSamples)
                        {
                            // Get the sound samples from the game code
                            gameCode.getSoundSamples(&thread, &gameMemory, &soundBuffer);
                        }

                        Win32FillSoundBuffer(&soundOutput, byteToLock, bytesToWrite, &soundBuffer);
                    }
                    else
                    {
                        isSoundValid = false;
                    }

                    LARGE_INTEGER workCounter = Win32GetWallClock();
                    real32 workSecondsElapsed = Win32GetSecondsElpased(lastCounter, workCounter);

                    real32 secondsElapsedForFrame = workSecondsElapsed;
                    if(secondsElapsedForFrame < targetSecondsPerFrame)
                    {
                        if(isSleepGranular)
                        {
                            
                            DWORD sleepMS = (DWORD)(1000.0f * (targetSecondsPerFrame - secondsElapsedForFrame));
                            if(sleepMS > 0)
                            {
                                Sleep(sleepMS);
                            }
                        }

                        real32 testSecondsElapsedForFrame = Win32GetSecondsElpased(lastCounter, Win32GetWallClock());
                        while(secondsElapsedForFrame < targetSecondsPerFrame)
                        {
                            secondsElapsedForFrame = Win32GetSecondsElpased(lastCounter, Win32GetWallClock());
                        }
                    }
                    else
                    {
                        //missed frame rate!!
                        // TODO : Logging system
                    }

                    LARGE_INTEGER endCounter = Win32GetWallClock();
                    real32 counterElapsed = (real32)(endCounter.QuadPart - lastCounter.QuadPart);
                    lastCounter = endCounter;

                    //Display using the graphics buffer
                    win32_window_dimension dimension = Win32GetWindowDimension(hWnd);
                    
                    HDC deviceContext = GetDC(hWnd); 
                    Win32DisplayBuffer(deviceContext, dimension.width, dimension.height, &globalBackBuffer);
                    ReleaseDC(hWnd, deviceContext);
                    flipWallClock = Win32GetWallClock();
#if PROJECTH_DEBUG
                    // DWORD playCursor;
                    // DWORD writeCursor;
                    // if(globalSecondaryBuffer->GetCurrentPosition(&playCursor, &writeCursor) == DS_OK)
                    // {
                    //     win32_debug_time_marker *marker = &debugTimeMarkers[debugTimeMarkerIndex++];
                    //     if(debugTimeMarkerIndex == ArrayCount(debugTimeMarkers))
                    //     {
                    //         debugTimeMarkerIndex = 0;
                    //     }
                    //     marker->playCursor = playCursor;
                    //     marker->writeCursor = writeCursor;
                    // }
#endif

                    game_input *temp = newInput;
                    newInput = oldInput;
                    oldInput = temp;
                    
                    uint64 endCycleCount = __rdtsc();
                    uint64 cyclesElapsed = endCycleCount - lastCycleCount;
                    lastCycleCount = endCycleCount;

#if 0
                    real32 msPerFrame = (1000.0f * ((real32)counterElapsed / (real32)cyclesElapsed));
                    real32 fps = perfCountFrequency / (real32)counterElapsed;
                    real32 mcpf = ((real32)cyclesElapsed / (1000.0f * 1000.0f));

                    //Shows debug performance(fps, mcfs,..... )
                    char buffer[256];
                    sprintf_s(buffer, "%.02fms/f, %.02fFPS, %.02fmc/f\n", msPerFrame, fps, mcpf);
                    OutputDebugStringA(buffer);
#endif

                }
            }
        }
        else
        {
            //creating window failed.
        }
    }
    else
    {
        //Registering Windows Class Failed.
    }

    return 0;
}