#include "stdafx.h"
#define SHAREDMEMORYSERVER_SIMPLE_MACRO_NAME
#define SHAREDMEMORYSERVER_IMPLEMENTATION
#include "SharedMemoryServer.h"
#include "trace.h"
#include "utils.h"
#include <assert.h>
#include <process.h>
#include <exception>


AVSValue __cdecl Create_SharedMemoryServer(AVSValue args, void* user_data, IScriptEnvironment* env)
{
    SharedMemoryServer_parameter_storage_t params(args);
    AVSValue aux_clips = ARG(aux_clips);
    assert(aux_clips.IsArray());
    int array_size = aux_clips.ArraySize();
    PClip* clips = new PClip[array_size + 2];
    clips[0] = ARG(child).AsClip();
    for (int i = 0; i < array_size; i++)
    {
        clips[i + 1] = aux_clips[i].AsClip();
    }
    clips[array_size + 1] = NULL;
    VideoInfo* vi_array = new VideoInfo[array_size + 1];
    for (int i = 0; i < array_size + 1; i++)
    {
        vi_array[i] = clips[i]->GetVideoInfo();
        // disable audio
        vi_array[i].audio_samples_per_second = 0;
    }
    AVSValue result;
    try
    {
        result = new SharedMemoryServer(clips, array_size + 1, vi_array, params, env);
    } catch (AvisynthError&) {
        delete [] vi_array;
        delete [] clips;
        throw;
    } catch (std::exception& e) {
        delete [] vi_array;
        delete [] clips;
        env->ThrowError("SharedMemoryServer: %s", e.what());
    } catch (...) {
        delete [] vi_array;
        delete [] clips;
        env->ThrowError("SharedMemoryServer: Unknown exception");
    }
    delete [] vi_array;
    delete [] clips;
    return result;
}

bool SharedMemoryServer::is_shutting_down() const
{
    return _shutdown || _manager.header->shutdown;
}

unsigned __stdcall SharedMemoryServer::thread_stub(void* self)
{
    return ((SharedMemoryServer*)self)->thread_proc();
}

void __inline copy_plane(char* dst, int dst_pitch, PVideoFrame& frame, int row_size, int height, int plane)
{
    int src_pitch = frame->GetPitch(plane);
    const BYTE* src = frame->GetReadPtr(plane);

    // full aligned copy when possible, dst is always aligned so don't need to check it
    int line_size = min(aligned(row_size, 16), src_pitch);

    if (line_size == src_pitch)
    {
        memcpy(dst, src, line_size * height);
    } else {
        for (int i = 0; i < height; i++)
        {
            memcpy(dst, src, line_size);
            dst += dst_pitch;
            src += src_pitch;
        }
    }
}

void trace_avs_error(AvisynthError& e)
{
    TRACE("AvisynthError: %hs", e.msg);
}

unsigned SharedMemoryServer::thread_proc()
{
    shared_memory_source_request_t request;
    while (!is_shutting_down())
    {
        {
            TwoSidedLockAcquire lock(_manager.get_lock_by_type(SharedMemorySourceManager::request_lock));
            if (is_shutting_down())
            {
                break;
            }
            request = _manager.header->request;
            _manager.header->request.response_object_id = request.response_object_id == 0 ? 1 : 0;
#if _DEBUG
            // set to invalid value
            _manager.header->request.request_type = (request_type_t)-1; 
#endif
        }

        PVideoFrame frame = NULL;
        bool parity;
        try
        {
            switch (request.request_type)
            {
            case REQ_GETFRAME:
                frame = _fetcher.GetFrame(request.clip_index, request.frame_number, _env);
                if (!frame)
                {
                    initiate_shutdown();
                    return 1;
                }
                break;
            case REQ_GETPARITY:
                parity = _fetcher.GetParity(request.clip_index, request.frame_number);
                break;
            default:
                assert(false);
                initiate_shutdown();
                return 1;
            }
        } catch (AvisynthError& e) {
            trace_avs_error(e);
        } catch (...) {
            TRACE("Unknown error occurred while handling request.");
            assert(false);
            __debugbreak();
        }
        int lock_type = SharedMemorySourceManager::response_0_lock + request.response_object_id;
        {
            TwoSidedLockAcquire lock(_manager.get_lock_by_type((SharedMemorySourceManager::lock_type_t)lock_type));
            if (is_shutting_down())
            {
                break;
            }
            shared_memory_source_response_t& response = _manager.header->response[request.response_object_id];
#ifdef _DEBUG
            response.request_type = request.request_type;
            response.clip_index = request.clip_index;
            response.frame_number = request.frame_number;
#endif
            switch (request.request_type)
            {
            case REQ_GETFRAME:
                {
                    const VideoInfo& vi = _manager.header->vi_array[request.clip_index];
                    char* buffer = (char*)_manager.header + response.buffer_offset;
                    int row_size = vi.RowSize();
                    int pitch = aligned(row_size, 16);
                    response.response_data.response_GetFrame.pitch = pitch;
                    copy_plane(buffer, pitch, frame, row_size, vi.height, 0);
                    if (vi.IsPlanar() && !vi.IsY8())
                    {
                        int offset = pitch * vi.height;
                        response.response_data.response_GetFrame.offset_u = offset;

                        int uv_row_size = vi.RowSize(PLANAR_U);
                        int uv_pitch = aligned(row_size, 16);
                        response.response_data.response_GetFrame.pitch_uv = uv_pitch;
                        int uv_height = vi.height >> vi.GetPlaneHeightSubsampling(PLANAR_U);
                        copy_plane(buffer + offset, uv_pitch, frame, uv_row_size, uv_height, PLANAR_U);
                        offset += uv_pitch * uv_height;
                        response.response_data.response_GetFrame.offset_v = offset;
                        copy_plane(buffer + offset, uv_pitch, frame, uv_row_size, uv_height, PLANAR_V);
                    }
#ifdef _DEBUG
                    _manager.check_data_buffer_integrity(request.response_object_id);
#endif
                    break;
                }
            case REQ_GETPARITY:
                response.response_data.response_GetParity.parity = parity;
                break;
            default:
                assert(false);
                initiate_shutdown();
                return 1;
            }
        }
    }
    initiate_shutdown();
    return 0;
}

void SharedMemoryServer::initiate_shutdown()
{
    _shutdown = true;
    _manager.signal_shutdown();
}

SharedMemoryServer::SharedMemoryServer(PClip clips[], int clip_count, const VideoInfo vi_array[], SharedMemoryServer_parameter_storage_t& o, IScriptEnvironment* env):
    SharedMemoryServer_parameter_storage_t(o),
    GenericVideoFilter(clips[0]),
    _thread_handle(NULL),
    _env(env),
    _shutdown(false),
    _fetcher(clips, _max_cache_frames, _cache_behind, env),
    _manager(get_shared_memory_key("LOCAL", _port), true, clip_count, vi_array)
{
    _thread_handle.replace((HANDLE)_beginthreadex(NULL, 0, thread_stub, this, 0, 0));
    if (!_thread_handle.is_valid())
    {
        env->ThrowError("SharedMemoryServer: Unable to create thread.");
    }
}

SharedMemoryServer::~SharedMemoryServer()
{
    initiate_shutdown();
    while (!_thread_handle.wait(10))
    {
        initiate_shutdown();
    }
}

PVideoFrame SharedMemoryServer::GetFrame(int n, IScriptEnvironment* env)
{
    return _fetcher.GetFrame(0, n, env);
}

bool SharedMemoryServer::GetParity(int n)
{
    return _fetcher.GetParity(0, n);
}