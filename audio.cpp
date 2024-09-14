// @Fixme: @Fixme: Right now, music is frame dependent
// @Fixme: @Fixme: Right now, music is frame dependent
// @Fixme: @Fixme: Right now, music is frame dependent
// @Fixme: @Fixme: Right now, music is frame dependent

#include "audio.h"

#include <miniaudio.h>
#include <dr_wav.h>
#include <dr_mp3.h>
#include <stb_vorbis.c>

#define AUDIO_DEVICE_CHANNELS    2
#define AUDIO_DEVICE_FORMAT      ma_format_f32
#define AUDIO_DEVICE_SAMPLE_RATE 44100

#define IM_CALLOC(n, size)       calloc(n, size)
#define IM_FREE(p)               free(p)

#define TRACELOG(level, ...)     printf(__VA_ARGS__)

#define array_size(arr)          (sizeof(arr) / sizeof(arr[0]))

enum class Audio_File_Type : u32
{
    NONE = 0, // No context loaded
    WAV,
    OGG,
    MP3,
};

enum class LOG
{
    ALL = 0,
    TRACE,
    DEBUG,
    INFO,
    WARNING,
    ERROR,
    FATAL,
    NONE
};

enum class Audio_Buffer_Usage
{
    STATIC = 0,
    STREAM
};

// 2 sub-buffers because we are doing stereo
const u32 AUDIO_SUB_BUFFERS_SIZE = 2;

struct Audio_Buffer
{
    ma_data_converter converter;
    Audio_Callback*   callback;

    float volume;
    float pitch;
    float pan;

    bool playing;
    bool paused;
    bool looping;

    Audio_Buffer_Usage usage;

    bool is_sub_buffer_processed[AUDIO_SUB_BUFFERS_SIZE];

    uint32_t size_in_frames;
    uint32_t frame_cursor_position;
    uint32_t frames_processed;

    uint8_t *data;

    // linked-list like, pointers are items in the audio_buffer_pool
    Audio_Buffer *next;
    Audio_Buffer *prev;
};

constexpr u32 AUDIO_BUFFER_POOL_CAP = 32;
Audio_Buffer audio_buffer_pool[AUDIO_BUFFER_POOL_CAP];

ma_context    audio_context;
ma_device     audio_device;
ma_mutex      audio_lock;
bool          audio_is_ready;
u64           audio_pcm_buffer_size;
void         *audio_pcm_buffer;

Audio_Buffer *audio_buffer_first; // first audio buffer in the list
Audio_Buffer *audio_buffer_last; // last audio buffer in the list
i32           audio_buffer_default_size = 0;



//
// helper functions
//
bool is_file_ext(const char *filename, const char *ext)
{
    bool result = false;

    // @Fixme: use our own method of file path searching and such
    const char *file_ext = strrchr(filename, '.');

    if (file_ext != NULL)
    {
        result = (strcmp(file_ext, ext) == 0);
    }

    return result;
}

void on_log(void *user_data, ma_uint32 level, const char *message)
{
    // @Note: all log messages from miniaudio are errors
    TRACELOG(LOG::WARNING, "miniaudio: %s\n", message);
}






// ----------------------------------------------------------------------
// AUDIO BUFFER FUNCTIONS
// ----------------------------------------------------------------------
bool is_audio_buffer_playing(Audio_Buffer *buffer);
// init new audio buffer (filled with silence)
Audio_Buffer *load_audio_buffer(ma_format format, ma_uint32 channels, ma_uint32 sample_rate,
                                ma_uint32 size_in_frames, Audio_Buffer_Usage usage);
// unload audio buffer
void unload_audio_buffer(Audio_Buffer *buffer);
// track audio buffer to linked list next position
void track_audio_buffer(Audio_Buffer *buffer);
// untrack audio buffer from linked list
void untrack_audio_buffer(Audio_Buffer *buffer);
// stop and audio buffer
void stop_audio_buffer(Audio_Buffer *buffer);
void play_audio_buffer(Audio_Buffer *buffer);

Audio_Buffer *allocate_audio_buffer()
{
    u32 last_allocated = AUDIO_BUFFER_POOL_CAP - 1;

    while (true)
    {
        last_allocated = (last_allocated + 1) % AUDIO_BUFFER_POOL_CAP;

        // checking if the data is NULL
        if (audio_buffer_pool[last_allocated].data == NULL)
        {
            break;
        }
    }

    return &audio_buffer_pool[last_allocated];
}

bool is_audio_buffer_playing(Audio_Buffer *buffer)
{
    return buffer != NULL && buffer->playing && !buffer->paused;
}

Audio_Buffer *load_audio_buffer(ma_format format, ma_uint32 channels, ma_uint32 sample_rate,
                                ma_uint32 size_in_frames, Audio_Buffer_Usage usage)
{
    Audio_Buffer *audio_buffer = allocate_audio_buffer();

    if (size_in_frames > 0)
    {
        audio_buffer->data = (uint8_t*)IM_CALLOC(size_in_frames * channels * ma_get_bytes_per_sample(format), 1);
    }

    // audio data runs througha format converter
    ma_data_converter_config converter_config =
        ma_data_converter_config_init(format, AUDIO_DEVICE_FORMAT,
                                      channels, AUDIO_DEVICE_CHANNELS,
                                      sample_rate, audio_device.sampleRate);
    converter_config.allowDynamicSampleRate = true;

    if (ma_data_converter_init(&converter_config, NULL, &audio_buffer->converter) != MA_SUCCESS)
    {
        TRACELOG(LOG::WARNING, "AUDIO: Failed to create data conversion pipeline\n");
        IM_FREE(audio_buffer);
        return NULL;
    }

    // init audio buffer values
    audio_buffer->volume = 1.0f;
    audio_buffer->pitch  = 1.0f;
    audio_buffer->pan    = 0.5f;

    audio_buffer->callback = NULL;

    audio_buffer->playing = false;
    audio_buffer->paused  = false;
    audio_buffer->looping = false;

    audio_buffer->usage = usage;

    audio_buffer->frame_cursor_position = 0;
    audio_buffer->size_in_frames        = size_in_frames;


    track_audio_buffer(audio_buffer);

    return audio_buffer;
}

void unload_audio_buffer(Audio_Buffer *buffer)
{
    if (buffer != NULL)
    {
        ma_data_converter_uninit(&buffer->converter, NULL);

        untrack_audio_buffer(buffer);

        IM_FREE(buffer->data);
        IM_FREE(buffer);
    }
}

void track_audio_buffer(Audio_Buffer *buffer)
{
    ma_mutex_lock(&audio_lock);

    // not good, as this is not contiguous in memory so cache will miss cycle
    if (audio_buffer_first == NULL)
    {
        audio_buffer_first = buffer;
    }
    else
    {
        audio_buffer_last->next = buffer;
        buffer->prev = audio_buffer_last;
    }

    audio_buffer_last = buffer;

    ma_mutex_unlock(&audio_lock);
}

// detach the current buffer from the track (linked list)
void untrack_audio_buffer(Audio_Buffer *buffer)
{
    ma_mutex_lock(&audio_lock);

    if (buffer->prev == NULL)
    {
        audio_buffer_first = buffer->next;
    }
    else
    {
        buffer->prev->next = buffer->next;
    }

    if (buffer->next == NULL)
    {
        audio_buffer_last = buffer->prev;
    }
    else
    {
        buffer->next->prev = buffer->prev;
    }

    buffer->prev = NULL;
    buffer->next = NULL;

    ma_mutex_lock(&audio_lock);
}

void stop_audio_buffer(Audio_Buffer *buffer)
{
    if (buffer == NULL) return;

    if (is_audio_buffer_playing(buffer))
    {
        buffer->playing = false;
        buffer->paused = false;
        buffer->frame_cursor_position = 0;
        buffer->frames_processed = 0;
        buffer->is_sub_buffer_processed[0] = true;
        buffer->is_sub_buffer_processed[1] = true;
    }
}

// play an audio buffer
// @Note:
// buffer is restarted to the start
void play_audio_buffer(Audio_Buffer *buffer)
{
    if (buffer == NULL) return;

    buffer->playing = true;
    buffer->paused  = false;
    buffer->frame_cursor_position = 0;
}











// we read audio data from an Audio_Buffer object in internal format
ma_uint32 read_audio_buffer_frames_in_internal_format(Audio_Buffer *buffer, void *frames_out,
                                                             ma_uint32 frame_count)
{
    // using audio buffer callback
    if (buffer->callback)
    {
        buffer->callback(frames_out, frame_count);
        buffer->frames_processed += frame_count;

        return frame_count;
    }

    ma_uint32 sub_buffer_size_in_frames = (buffer->size_in_frames > 1) ?
        (buffer->size_in_frames / 2) : buffer->size_in_frames;

    ma_uint32 current_sub_buffer_index  =
        buffer->frame_cursor_position / sub_buffer_size_in_frames;

    if (current_sub_buffer_index > 1)
    {
        return 0;
    }

    // another thread can update the processed state of buffers, so
    // we take a copy here to try and avoid potential synchronization problems
    bool is_sub_buffer_processed[2] = {0};
    is_sub_buffer_processed[0] = buffer->is_sub_buffer_processed[0];
    is_sub_buffer_processed[1] = buffer->is_sub_buffer_processed[1];

    ma_uint32 frame_size_in_bytes = ma_get_bytes_per_frame(
        buffer->converter.formatIn, buffer->converter.channelsIn);

    // fill out every frame until we find a buffer that's marked as processed
    // then fill the raminder with 0
    ma_uint32 frames_read = 0;

    while (true)
    {
        // for static buffer,  we fill as much data as possible
        // for streaming buffer, we fill half of the buffer that are processed
        //             the unprocessed halves must keep their audio data intact
        if (buffer->usage == Audio_Buffer_Usage::STATIC)
        {
            if (frames_read >= frame_count)
            {
                break;
            }
        }
        else
        {
            if (is_sub_buffer_processed[current_sub_buffer_index])
            {
                break;
            }
        }

        ma_uint32 total_frames_remaining = frame_count - frames_read;
        if (total_frames_remaining == 0)
        {
            break;
        }

        ma_uint32 frames_remaining_in_output_buffer;
        if (buffer->usage == Audio_Buffer_Usage::STATIC)
        {
            frames_remaining_in_output_buffer =
                buffer->size_in_frames - buffer->frame_cursor_position;
        }
        else
        {
            ma_uint32 first_frame_index_of_this_sub_buffer =
                sub_buffer_size_in_frames * current_sub_buffer_index;

            frames_remaining_in_output_buffer = sub_buffer_size_in_frames -
                (buffer->frame_cursor_position - first_frame_index_of_this_sub_buffer);
        }

        ma_uint32 frames_to_read = total_frames_remaining;
        if (frames_to_read > frames_remaining_in_output_buffer)
        {
            frames_to_read = frames_remaining_in_output_buffer;
        }

        memcpy((uint8_t*)frames_out + (frames_read * frame_size_in_bytes),
               buffer->data + (buffer->frame_cursor_position * frame_size_in_bytes),
               frames_to_read * frame_size_in_bytes);

        buffer->frame_cursor_position =
            (buffer->frame_cursor_position + frames_to_read) % buffer->size_in_frames;

        frames_read += frames_to_read;

        // if we've read to the end of the buffer,
        // mark it as processed
        if (frames_to_read == frames_remaining_in_output_buffer)
        {
            buffer->is_sub_buffer_processed[current_sub_buffer_index] = true;
            is_sub_buffer_processed[current_sub_buffer_index] = true;

            current_sub_buffer_index = (current_sub_buffer_index + 1) % 2;

            // break from this loop if we're not looping
            if (!buffer->looping)
            {
                stop_audio_buffer(buffer);
                break;
            }
        }
    }

    // after we finished with the loop, we fill the excess with 0
    ma_uint32 total_frames_remaining = frame_count - frames_read;
    if (total_frames_remaining > 0)
    {
        memset((uint8_t*)frames_out + (frames_read * frame_size_in_bytes),
               0,
               total_frames_remaining * frame_size_in_bytes);

        // @Note: For static buffers we can fill the remaining frames with silence for
        // safety, but we don't want to report those frames as "read"
        // The reason for this is that the caller uses the return value
        // to know whether a non-looping sound has finished playback.
        if (buffer->usage != Audio_Buffer_Usage::STATIC)
        {
            frames_read += total_frames_remaining;
        }
    }

    return frames_read;
}

// ----------------------------------------------------------------------
// @Important:
// - our mixing function is simply an accumulation
// - all the mixing takes place here
// - this function will be called when miniaudio needs more data
// - sends audio data to device callback function
// ----------------------------------------------------------------------
void on_send_audio_data_to_device(ma_device *device, void *frames_out,
                                  const void *frames_input, ma_uint32 frame_count)
{
    memset(frames_out, 0, frame_count * device->playback.channels * ma_get_bytes_per_sample(device->playback.format));

    // @Fixme: using mutex makes this not real-time
    ma_mutex_lock(&audio_lock);

    for (Audio_Buffer *buffer = audio_buffer_first;
         buffer != NULL;
         buffer = buffer->next)
    {
        // ignore stopped or paused sounds
        if (!buffer->playing || buffer->paused)
        {
            continue;
        }

        ma_uint32 frames_read = 0;

        while (true)
        {
            if (frames_read >= frame_count)
            {
                break;
            }

            // read as much data as we can from the stream
            ma_uint32 frames_to_read = (frame_count - frames_read);

            constexpr uint32_t TEMP_BUFFER_CAP = 1024;
            while (frames_to_read > 0)
            {
                // frames out for stereo
                float temp_buffer[TEMP_BUFFER_CAP] = {0};

                ma_uint32 frames_to_read_right_now = frames_to_read;

                if (frames_to_read_right_now > TEMP_BUFFER_CAP / AUDIO_DEVICE_CHANNELS)
                {
                    frames_to_read_right_now = TEMP_BUFFER_CAP / AUDIO_DEVICE_CHANNELS;
                }

                // @Note: reads audio data from the buffer object in device mixing format
                // data will be in a format appropriate for mixing
                // ----------------------------------------
                ma_uint32 frames_just_read;
                {
                    // we continously convert the data from the buffer's internal format
                    // to the mixing format, which should be defined by the output format
                    // of the data converter

                    // do this until there are frames_to_read_right_now frames in the output

                    // @Important: NEVER READ MORE INPUT DATA THAN IS REQUIRED for the
                    // specified number of output frames.
                    // We use ma_data_converter_get_required_input_frame_count() to
                    // get enough frames.

                    constexpr size_t INPUT_BUFFER_CAP = 4096;
                    ma_uint8 input_buffer[INPUT_BUFFER_CAP] = {0};

                    const ma_uint32 INPUT_BUFFER_FRAME_CAP = sizeof(input_buffer) /
                        ma_get_bytes_per_frame(buffer->converter.formatIn,
                                               buffer->converter.channelsIn);

                    ma_uint32 total_output_frames = 0;
                    while (total_output_frames < frames_to_read_right_now)
                    {
                        ma_uint64 output_frames_to_process_this_iteration =
                            frames_to_read_right_now - total_output_frames;

                        ma_uint64 input_frames_to_process_this_iteration = 0;

                        (void)ma_data_converter_get_required_input_frame_count(
                            &buffer->converter,
                            output_frames_to_process_this_iteration,
                            &input_frames_to_process_this_iteration);

                        if (input_frames_to_process_this_iteration > INPUT_BUFFER_FRAME_CAP)
                        {
                            input_frames_to_process_this_iteration = INPUT_BUFFER_FRAME_CAP;
                        }

                        float *running_frames_out =
                            temp_buffer + (total_output_frames * buffer->converter.channelsOut);

                        // convert the data to our mixing format
                        ma_uint64 output_frames_processed_this_iteration = output_frames_to_process_this_iteration;
                        ma_uint64 input_frames_processed_this_iteration =
                            read_audio_buffer_frames_in_internal_format(buffer, input_buffer,
                                                                        static_cast<ma_uint32>(
                                                                            input_frames_to_process_this_iteration));

                        ma_data_converter_process_pcm_frames(&buffer->converter,
                                                             input_buffer, &input_frames_processed_this_iteration,
                                                             running_frames_out,
                                                             &output_frames_processed_this_iteration);

                        total_output_frames += (ma_uint32)output_frames_processed_this_iteration; // Safe cast

                        if (input_frames_processed_this_iteration < input_frames_to_process_this_iteration)
                        {
                            break; // run out of buffer data
                        }

                        // ideally, this branch will never occur.
                        // this ensures that we get out of the loop 
                        // when no input and no output frames are processed
                        if (input_frames_processed_this_iteration == 0 &&
                            output_frames_processed_this_iteration == 0)
                        {
                            break;
                        }
                    }

                    // assign value to the frames_just_read variable
                    frames_just_read = total_output_frames;
                }
                // ----------------------------------------
                

                // if we read some frames in the earlier scope
                if (frames_just_read > 0)
                {
                    float *f32_frames_out = (float*)frames_out + (frames_read * audio_device.playback.channels);
                    float *f32_frames_in  = temp_buffer;
                    ma_uint32 frame_count = frames_just_read;

                    // @Todo: might want to deal with processor later

                    // @Note: mix audio frames
                    {
                        const float LOCAL_VOLUME = buffer->volume;
                        const ma_uint32 CHANNELS = audio_device.playback.channels;

                        // @Note: if we consider panning
                        if (CHANNELS == 2)
                        {
                            // @Note: the length of the buffer is normalized,
                            // .i.e, [0.0f..1.0f]
                            const float LEFT  = buffer->pan;
                            const float RIGHT = 1.0f - LEFT;

                            // @Note: fast sine approximation in [0..1] for pan law:
                            // y = 0.5f * x * (3 - x*x)
                            const float levels[2] = {
                                LOCAL_VOLUME * 0.5f * LEFT * (3.0f - LEFT * LEFT),
                                LOCAL_VOLUME * 0.5f * RIGHT * (3.0f - RIGHT * RIGHT)
                            };

                            float *frame_out      = f32_frames_out;
                            const float *frame_in = f32_frames_in;

                            for (ma_uint32 frame = 0; frame < frame_count; ++frame)
                            {
                                frame_out[0] += frame_in[0] * levels[0];
                                frame_out[1] += frame_in[1] * levels[1];

                                // shift by two because we just assigned 2 entries
                                frame_out += 2;
                                frame_in  += 2;
                            }
                        }
                        // @Note: if we don't consider panning
                        else
                        {
                            for (ma_uint32 frame = 0; frame < frame_count; ++frame)
                            {
                                for (ma_uint32 c = 0; c < CHANNELS; ++c)
                                {
                                    float *frame_out = f32_frames_out + (frame * CHANNELS);
                                    float *frame_in  = f32_frames_in + (frame * CHANNELS);

                                    // output = input * volume of the provided output (usually 0)
                                    frame_out[c] += frame_in[c] * LOCAL_VOLUME;
                                }
                            }
                        }
                    }

                    frames_to_read -= frames_just_read;
                    frames_read += frames_just_read;
                }



                if (!buffer->playing)
                {
                    frames_read = frame_count;
                    break;
                }



                // if we werene't able to read all the frames we requested,
                // break because we run out of frames to read
                if (frames_just_read < frames_to_read_right_now)
                {
                    if (!buffer->looping)
                    {
                        stop_audio_buffer(buffer);
                        break;
                    }
                    else
                    {
                        // should never get here, because the looping will ensure that
                        // the data read will wrap around
                        // anyhow, if it hits here, move the cursor position back to the
                        // start and continue the loop
                        buffer->frame_cursor_position = 0;
                        continue;
                    }
                }
            }


            // if we weren't able to read every frame we'll need to break from the loop
            // not doing this could result in an infinite loop
            if (frames_to_read > 0)
            {
                break;
            }
        }
    }

    // @Todo: deal with processors stuff here, later... (around line 2555 of raudio.c)

    ma_mutex_unlock(&audio_lock);
}









// ----------------------------------------------------------------------
// AUDIO DEVICE MANAGEMENT FUNCTIONS 
// ----------------------------------------------------------------------
void init_audio_device()
{
    // audio context
    ma_context_config context_config = ma_context_config_init();
    ma_log_callback_init(on_log, NULL);

    if (ma_context_init(NULL, 0, &context_config, &audio_context) != MA_SUCCESS)
    {
        TRACELOG(LOG::WARNING, "AUDIO: Failed to initialized context\n");
        return;
    }

    // audio device
    // @Note: using the default device. format is f32 because it simplifies mixing.
    ma_device_config config   = ma_device_config_init(ma_device_type_playback); // using playback type
    config.playback.pDeviceID = NULL; // set NULL to use the default playback AUDIO
    config.playback.format    = AUDIO_DEVICE_FORMAT;
    config.playback.channels  = AUDIO_DEVICE_CHANNELS;
    config.capture.pDeviceID  = NULL; // set NULL for the default capture AUDIO
    config.capture.format     = ma_format_s16; // using signed 16 bits format (enforce every file data to this)
    config.capture.channels   = 1;
    config.sampleRate         = AUDIO_DEVICE_SAMPLE_RATE;
    config.dataCallback       = on_send_audio_data_to_device;
    config.pUserData          = NULL;

    if (ma_device_init(&audio_context, &config, &audio_device) != MA_SUCCESS)
    {
        TRACELOG(LOG::WARNING, "AUDIO: Failed to initialized playback device\n");
        ma_context_uninit(&audio_context);
        return;
    }

    // mixing happens on a separate thread which means we need to synchronize.
    // @Fixme: USING A MUTEX HERE to make things simple, but may want to look at something
    // a bit smarter later on to keep everything real-time, if that's necessary.
    if (ma_mutex_init(&audio_lock) != MA_SUCCESS)
    {
        TRACELOG(LOG::WARNING, "AUDIO: Failed to create mutex for mixing\n");
        ma_device_uninit(&audio_device);
        ma_context_uninit(&audio_context);
        return;
    }

    // @Fixme: CURRENTLY KEEP THE DEVICE RUNNING THE WHOLE TIME.
    // might want to do something a bit smarter by only run the device if there is at least one sound being played
    if (ma_device_start(&audio_device) != MA_SUCCESS)
    {
        TRACELOG(LOG::WARNING, "AUDIO: Failed to start playback device\n");
        ma_device_uninit(&audio_device);
        ma_context_uninit(&audio_context);
        return;
    }

    TRACELOG(LOG::INFO, "AUDIO: Device initialized successfully\n");
    TRACELOG(LOG::INFO, "    > Backend:       miniaudio / %s\n", ma_get_backend_name(audio_context.backend));
    TRACELOG(LOG::INFO, "    > Format:        %s -> %s\n", ma_get_format_name(audio_device.playback.format),
             ma_get_format_name(audio_device.playback.internalFormat));
    TRACELOG(LOG::INFO, "    > Channels:      %d -> %d\n", audio_device.playback.channels,
             audio_device.playback.internalChannels);
    TRACELOG(LOG::INFO, "    > Sample rate:   %d -> %d\n", audio_device.sampleRate,
             audio_device.playback.internalSampleRate);
    TRACELOG(LOG::INFO, "    > Periods size:  %d\n", audio_device.playback.internalPeriodSizeInFrames *
             audio_device.playback.internalPeriods);

    // turn on the green light for other functions
    audio_is_ready = true;
}

void close_audio_device()
{
    if (audio_is_ready)
    {
        ma_mutex_uninit(&audio_lock);
        ma_device_uninit(&audio_device);
        ma_context_uninit(&audio_context);

        audio_is_ready = false;

        IM_FREE(audio_pcm_buffer);
        audio_pcm_buffer = NULL;
        audio_pcm_buffer_size = 0;

        TRACELOG(LOG::INFO, "AUDIO: Device closed successfully\n");
    }
    else
    {
        TRACELOG(LOG::WARNING, "AUDIO: Device could not be closed, not currently initialized\n");
    }
}

// ----------------------------------------------------------------------
// AUDIO STREAM MANAGEMENT FUNCTIONS
// ----------------------------------------------------------------------
// load audio stream (to stream audio pcm data)
Audio_Stream load_audio_stream(uint32_t sample_rate, uint32_t sample_size, uint32_t channels)
{
    Audio_Stream stream = {0};

    stream.sample_rate = sample_rate;
    stream.sample_size = sample_size;
    stream.channels    = channels;

    ma_format format_in = sample_size == 8 ? ma_format_u8 // use unsigned 8-bit for sample size 8
        : sample_size == 16 ? ma_format_s16 // use signed 16-bit for sample size 16
        : ma_format_f32; // everything else uses floating point 32-bit

    // size of a streaming buffer must be at least double the size of a period
    uint32_t period_size = audio_device.playback.internalPeriodSizeInFrames;

    // if the buffer is not set, compute one that would give us
    // a buffer good enough for a decent frame rate
    constexpr uint32_t SUBSTITUTE_FRAME_RATE = 30;
    uint32_t sub_buffer_size = audio_buffer_default_size == 0 ?
        audio_device.sampleRate / SUBSTITUTE_FRAME_RATE
        : audio_buffer_default_size;

    if (sub_buffer_size < period_size)
    {
        sub_buffer_size = period_size;
    }

    // create a DOUBLE AUDIO BUFFER of defined size
    stream.buffer = load_audio_buffer(format_in, stream.channels, stream.sample_rate,
                                      sub_buffer_size * 2, Audio_Buffer_Usage::STREAM);

    if (stream.buffer != NULL)
    {
        stream.buffer->looping = true; // loop by default for streaming buffers
        TRACELOG(LOG::INFO, "STREAM: Initialized successfully (%i Hz, %i bit, %s)\n",
                 stream.sample_rate, stream.sample_size,
                 stream.channels == 1 ? "Mono" : "Stereo");
    }
    else
    {
        TRACELOG(LOG::WARNING, "STREAM: Failed to load audio buffer, stream could not be created\n");
    }

    return stream;
}

// unload audio stream and free memory
void unload_audio_stream(Audio_Stream stream)
{
    unload_audio_buffer(stream.buffer);
    TRACELOG(LOG::INFO, "STREAM: Unloaded audio stream data from RAM\n");
}

// play audio stream
// @Note: this function resets the cursor position of the audio stream's buffer
void play_audio_stream(Audio_Stream stream)
{
    play_audio_buffer(stream.buffer);
}

// update auduio stream's buffers with data
// @Note:
// if there are multiple buffers in one stream source:
// - only update one buffer of the stream source: dequeue the buffer -> update it -> enqueue it back
// - to dequeue a buffer, it needs to be processed. so check it with is_audio_stream_processed()
void update_audio_stream(Audio_Stream stream, void *data, uint32_t frame_count)
{
    if (stream.buffer != NULL)
    {
        // @Fixme: what about mono-channels???

        // @Note: check if buffer is processed
        if (stream.buffer->is_sub_buffer_processed[0] || stream.buffer->is_sub_buffer_processed[1])
        {
            ma_uint32 sub_buffer_to_update = 0;

            if (stream.buffer->is_sub_buffer_processed[0] && stream.buffer->is_sub_buffer_processed[1])
            {
                // both buffers are available for updating.
                // update the first one and make sure the cursor is moved back to the front.
                sub_buffer_to_update = 0;
                stream.buffer->frame_cursor_position = 0;
            }
            else
            {
                // just update whichever sub-buffer is processed.
                sub_buffer_to_update = (stream.buffer->is_sub_buffer_processed[0])? 0 : 1;
            }

            ma_uint32 sub_buffer_size_in_frames = stream.buffer->size_in_frames / 2;
            uint8_t *sub_buffer = stream.buffer->data + ((sub_buffer_size_in_frames * stream.channels *
                                                          (stream.sample_size / 8)) * sub_buffer_to_update);

            // total frames processed in buffer is always the complete size, filled with 0 if required
            stream.buffer->frames_processed += sub_buffer_size_in_frames;

            // does this API expect a whole buffer to be updated in one go?
            // assuming so, but if not will need to change this logic.
            if (sub_buffer_size_in_frames >= (ma_uint32)frame_count)
            {
                ma_uint32 frames_to_write = (ma_uint32)frame_count;

                ma_uint32 bytes_to_write = frames_to_write * stream.channels * (stream.sample_size / 8);
                memcpy(sub_buffer, data, bytes_to_write);

                // any leftover frames should be filled with zeros.
                ma_uint32 left_over_frame_count = sub_buffer_size_in_frames - frames_to_write;

                if (left_over_frame_count > 0)
                {
                    memset(sub_buffer + bytes_to_write, 0,
                           left_over_frame_count * stream.channels * (stream.sample_size / 8));
                }

                stream.buffer->is_sub_buffer_processed[sub_buffer_to_update] = false;
            }
            else
            {
                TRACELOG(LOG_WARNING, "STREAM: Attempting to write too many frames to buffer\n");
            }
        }
        else
        {
            TRACELOG(LOG_WARNING, "STREAM: Buffer not available for updating\n");
        }
    }

}

void stop_audio_stream(Audio_Stream stream)
{
    stop_audio_buffer(stream.buffer);
}





// ----------------------------------------------------------------------
// MUSIC MANAGEMENT FUNCTIONS
// ----------------------------------------------------------------------
// load music stream (chunks at a time)
Music load_music_stream(const char *filename)
{
    Music music = {0};
    bool music_loaded = false;

    // WAV files
    if (is_file_ext(filename, ".wav"))
    {
        drwav *context_wav = (drwav*)IM_CALLOC(1, sizeof(drwav));
        bool success = drwav_init_file(context_wav, filename, NULL);

        music.context_type = Audio_File_Type::WAV;
        music.context_data = context_wav;

        if (success)
        {
            int32_t sample_size = context_wav->bitsPerSample;
            if (sample_size == 24)
            {
                sample_size = 16; // forced conversion to s16 in update_music_stream()
            }

            music.stream = load_audio_stream(context_wav->sampleRate, sample_size, context_wav->channels);

            music.frame_count = (uint32_t)context_wav->totalPCMFrameCount;
            music.looping = true;

            music_loaded = true;
        }
    }
    // OGG files
    else if (is_file_ext(filename, ".ogg"))
    {
        music.context_type = Audio_File_Type::OGG;
        music.context_data = stb_vorbis_open_filename(filename, NULL, NULL);

        if (music.context_data != NULL)
        {
            stb_vorbis_info info = stb_vorbis_get_info(static_cast<stb_vorbis*>(music.context_data));

            // OGG bit rate defaults to 16 bit,
            // this is enough for compressed format (which is s16)
            music.stream = load_audio_stream(info.sample_rate, 16, info.channels);

            // @Warning: re-read this part of the code
            music.frame_count = (uint32_t)stb_vorbis_stream_length_in_samples((stb_vorbis*)music.context_data);
            music.looping = true; // enable looping

            music_loaded = true;
        }
    }
    // MP3 files
    else if (is_file_ext(filename, ".mp3"))
    {
        drmp3 *context_mp3 = (drmp3*)IM_CALLOC(1, sizeof(drmp3));
        int32_t result = drmp3_init_file(context_mp3, filename, NULL);

        music.context_type = Audio_File_Type::MP3;
        music.context_data = context_mp3;

        if (result > 0)
        {
            music.stream = load_audio_stream(context_mp3->sampleRate, 32, context_mp3->channels);
            music.frame_count = (uint32_t)drmp3_get_pcm_frame_count(context_mp3);
            music.looping = true;

            music_loaded = true;
        }
    }
    else
    {
        TRACELOG(LOG::WARNING, "STREAM: File format %s is not supported yet\n", filename);
    }


    // if not loaded successfully
    if (!music_loaded)
    {
        if (music.context_type == Audio_File_Type::WAV)
        {
            drwav_uninit((drwav*)music.context_data);
            IM_FREE(music.context_data);
        }
        else if (music.context_type == Audio_File_Type::OGG)
        {
            stb_vorbis_close((stb_vorbis*)music.context_data);
        }
        else if (music.context_type == Audio_File_Type::MP3)
        {
            drmp3_uninit((drmp3*)music.context_data);
            IM_FREE(music.context_data);
        }

        music.context_data = NULL;
        TRACELOG(LOG::WARNING, "FILEIO: Music file %s could not be opened\n", filename);
    }
    // if succeeded
    else
    {
        TRACELOG(LOG::INFO, "FILEIO: Music file %s loaded successfully\n", filename);
        TRACELOG(LOG::INFO, "    > Sample rate:   %i Hz\n", music.stream.sample_rate);
        TRACELOG(LOG::INFO, "    > Sample size:   %i bits\n", music.stream.sample_size);
        TRACELOG(LOG::INFO, "    > Channels:      %i (%s)\n", music.stream.channels,
                 (music.stream.channels == 1) ? "Mono"
                 : (music.stream.channels == 2) ? "Stereo" : "Multi");
        TRACELOG(LOG::INFO, "    > Total frames:  %i\n", music.frame_count);
    }

    return music;
}


void unload_music_stream(Music *music)
{
    unload_audio_stream(music->stream);

    if (music->context_data != NULL)
    {
        if (music->context_type == Audio_File_Type::WAV)
        {
            drwav_uninit((drwav*)music->context_data);
            IM_FREE(music->context_data);
        }
        else if (music->context_type == Audio_File_Type::OGG)
        {
            stb_vorbis_close((stb_vorbis*)music->context_data);
        }
        else if (music->context_type == Audio_File_Type::MP3)
        {
            drmp3_uninit((drmp3*)music->context_data);
            IM_FREE(music->context_data);
        }
    }
}


void play_music_stream(Music *music)
{
    if (music->stream.buffer != NULL)
    {
        // @Note: for music streams, we need to maintain the frame cursor position
        ma_uint32 frame_cursor_position = music->stream.buffer->frame_cursor_position;
        play_audio_stream(music->stream); // this function resets the cursor position
        // @Note: since it reset the frame cursor position, we set it again
        music->stream.buffer->frame_cursor_position = frame_cursor_position;
    }
}


void stop_music_stream(Music *music)
{
    stop_audio_stream(music->stream);

    switch (music->context_type)
    {
        case Audio_File_Type::WAV:
        {
            drwav_seek_to_pcm_frame((drwav*)music->context_data, 0);
        } break;

        case Audio_File_Type::OGG:
        {
            stb_vorbis_seek_start((stb_vorbis*)music->context_data);
        } break;

        case Audio_File_Type::MP3:
        {
            drmp3_seek_to_start_of_stream((drmp3*)music->context_data);
        } break;

        default: break;
    }
}


// refill music buffers if data has been processed
void update_music_stream(Music *music)
{
    if (music->stream.buffer == NULL)
    {
        return;
    }

    uint32_t sub_buffer_size_in_frames = music->stream.buffer->size_in_frames / 2;

    // on the first call of this function, we lazily pre-allocated a temporary
    // bufer to read audio files or audio memory data in
    int32_t frame_size = music->stream.channels * music->stream.sample_size / 8; // size of one frame
    uint32_t pcm_size  = sub_buffer_size_in_frames * frame_size;

    if (audio_pcm_buffer_size < pcm_size)
    {
        IM_FREE(audio_pcm_buffer);
        audio_pcm_buffer      = IM_CALLOC(1, pcm_size);
        audio_pcm_buffer_size = pcm_size;
    }

    // check both sub-buffers to see if they need to be refilled
    for (int32_t i = 0; i < AUDIO_SUB_BUFFERS_SIZE; ++i)
    {
        if ((music->stream.buffer != NULL) &&
            !music->stream.buffer->is_sub_buffer_processed[i])
        {
            continue;
        }

        // frames not processed (to be processed)
        uint32_t frames_left = music->frame_count - music->stream.buffer->frames_processed;
        // total frames to be streamed
        uint32_t frames_to_stream = 0;

        if ((frames_left >= sub_buffer_size_in_frames) || music->looping)
        {
            frames_to_stream = sub_buffer_size_in_frames;
        }
        else
        {
            frames_to_stream = frames_left;
        }

        int32_t frame_count_still_needed = frames_to_stream;
        int32_t frame_count_read_total   = 0;

        switch (music->context_type)
        {
            case Audio_File_Type::WAV:
            {
                if (music->stream.sample_size == 16)
                {
                    while (true)
                    {
                        int32_t frame_count_read = (int32_t)drwav_read_pcm_frames_s16(
                            (drwav*)music->context_data,
                            frame_count_still_needed,
                            (int16_t*)((int8_t*)audio_pcm_buffer +
                                       frame_count_read_total * frame_size));
                        frame_count_read_total += frame_count_read;
                        frame_count_still_needed -= frame_count_read;

                        if (frame_count_still_needed == 0)
                        {
                            break;
                        }
                        else
                        {
                            drwav_seek_to_pcm_frame((drwav*)music->context_data, 0);
                        }
                    }
                }
                else if (music->stream.sample_size == 32)
                {
                    while (true)
                    {
                        int32_t frame_count_read = (int32_t)drwav_read_pcm_frames_f32(
                            (drwav*)music->context_data,
                            frame_count_still_needed,
                            (float*)((int8_t*)audio_pcm_buffer +
                                     frame_count_read_total * frame_size));
                        frame_count_read_total += frame_count_read;
                        frame_count_still_needed -= frame_count_read;

                        if (frame_count_still_needed == 0)
                        {
                            break;
                        }
                        else
                        {
                            drwav_seek_to_pcm_frame((drwav*)music->context_data, 0);
                        }
                    }
                }
            } break;


            case Audio_File_Type::OGG:
            {
                while (true)
                {
                    int32_t frame_count_read = stb_vorbis_get_samples_short_interleaved(
                        (stb_vorbis*)music->context_data,
                        music->stream.channels,
                        (int16_t*)((int8_t*)audio_pcm_buffer +
                                   frame_count_read_total * frame_size),
                        frame_count_still_needed * music->stream.channels);
                    frame_count_read_total += frame_count_read;
                    frame_count_still_needed -= frame_count_read;

                    if (frame_count_still_needed == 0)
                    {
                        break;
                    }
                    else
                    {
                        stb_vorbis_seek_start((stb_vorbis*)music->context_data);
                    }
                }
            } break;


            case Audio_File_Type::MP3:
            {
                while (true)
                {
                    int32_t frame_count_read = (int32_t)drmp3_read_pcm_frames_f32(
                        (drmp3*)music->context_data,
                        frame_count_still_needed,
                        (float*)((int8_t*)audio_pcm_buffer +
                                 frame_count_read_total * frame_size));
                    frame_count_read_total += frame_count_read;
                    frame_count_still_needed -= frame_count_read;
                    if (frame_count_still_needed == 0)
                    {
                        break;
                    }
                    else
                    {
                        drmp3_seek_to_start_of_stream((drmp3*)music->context_data);
                    }
                }
            } break;


            default: break;
        }

        update_audio_stream(music->stream, audio_pcm_buffer, frames_to_stream);

        music->stream.buffer->frames_processed =
            music->stream.buffer->frames_processed % music->frame_count;

        if (frames_left <= sub_buffer_size_in_frames)
        {
            if (!music->looping)
            {
                // if not loop, the end the streaming by filling the latest frames
                // from input
                stop_music_stream(music);
                return;
            }
        }
    }
}
