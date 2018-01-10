#ifndef WIN32_FOX_H
#define WIN32_FOX_H

struct win32_offscreen_buffer
{
    BITMAPINFO info;
    void *memory;
    int width;
    int height;
    int pitch;
    int bytesPerPixel;
};

//client window size
struct win32_window_dimension
{
    int width;
    int height;  
};

struct win32_sound_output
{
    int samplesPerSecond;                
    uint32 runningSampleIndex;
    int bytesPerSample;
    DWORD secondaryBufferSize;
    DWORD safetyBytes;
    //TODO : bytesPerSeconds?
};

struct win32_debug_time_marker
{
    DWORD playCursor;
    DWORD writeCursor;
};

struct win32_game_code
{
    HMODULE gameCodeDLL;
    FILETIME DLLLastWriteTime;

    // IMPORTANT : Either of the callbacks can be 0
    // You must check before calling.
    game_update_and_render *updateAndRender;
    game_get_sound_samples *getSoundSamples;

    bool32 isValid;
};

#define WIN32_STATE_FILE_NAME_COUNT MAX_PATH
struct win32_state
{
    //TODO : Never use MAX_PATH because it can be dangerous.
    char exeFileName[MAX_PATH];   
    char *onePastLastSlash;
};
#endif