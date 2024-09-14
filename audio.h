#pragma once

#include "common.h"

using Audio_Callback = void (void *buffer_data, u32 frames);

//
// audio device managment functions
//
void init_audio_device();
void close_audio_device();

// forward declare Audio_Buffer struct here (checkout audio.cpp for more details)
struct Audio_Buffer;

struct Audio_Stream
{
    Audio_Buffer* buffer;      // pointer to internal data used by audio system
    u32           sample_rate; // frequency (samples per second)
    u32           sample_size; // bits per sample: 8, 16, 32
    u32           channels;    // (1-mono, 2-stereo, ...)
};

//
// audio stream managment functions
//
Audio_Stream load_audio_stream(u32 sample_rate, u32 sample_size, u32 channels);
void unload_audio_stream(Audio_Stream stream);
void play_audio_stream(Audio_Stream stream);
void update_audio_stream(Audio_Stream stream, void *data, u32 frame_count);
void stop_audio_stream(Audio_Stream stream);

enum class Audio_File_Type : u32;

// song audio files, like music and ambient noise (anything longer than 10s)
struct Music
{
    Audio_Stream    stream;
    u32             frame_count;  // total number of frames (considering channels)
    bool            looping;
    Audio_File_Type context_type; // type of music context (audio filetype)
    void*           context_data; // audio context data, depends on type
};

//
// music management functions
//
Music load_music_stream(const char *filename);
void unload_music_stream(Music *music);
void play_music_stream(Music *music);
void update_music_stream(Music *music);
