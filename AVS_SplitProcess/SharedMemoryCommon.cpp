#define UNICODE
#define _UNICODE

#include "stdafx.h"
#include "SharedMemoryCommon.h"

#include <stdio.h>
#include <stdexcept>
#include <assert.h>
#include "utils.h"

using namespace std;

sys_string get_shared_memory_key(const char* key1, int key2)
{
    if (strlen(key1) > 256)
    {
        throw invalid_argument("key1 is too long.");
    }
    SYSCHAR buffer[1024];
    memset(buffer, 0, sizeof(buffer));
    _sntprintf(buffer, ARRAYSIZE(buffer) - 1, L"SharedMemorySource_%hs_%d", key1, key2);
    return sys_string(buffer);
}


void add_guard_bytes(void* address, DWORD buffer_size)
{
    for (int i = 0; i < 4; i++)
    {
        *(((int*)address) - i - 1) = 0xDEADC0DE;
    }
    for (int i = 0; i < 4; i++)
    {
        *(((int*)address + buffer_size) + i) = 0xDEADBEEF;
    }
}

void check_guard_bytes(void* address, DWORD buffer_size)
{
#if _DEBUG
    for (int i = 0; i < 4; i++)
    {
        assert(*(((int*)address) - i - 1) == 0xDEADC0DE);
    }
    for (int i = 0; i < 4; i++)
    {
        assert(*(((int*)address + buffer_size) + i) == 0xDEADBEEF);
    }
#endif
}

void SharedMemorySourceManager::init_server(const SYSCHAR* mapping_name, int clip_count, const VideoInfo vi_array[])
{
    assert(clip_count > 0);
    DWORD data_buffer_size = 0;
    for (int i = 0; i < clip_count; i++)
    {
        DWORD clip_buffer_size = aligned(vi_array[i].RowSize()) * vi_array[i].height;
        if (vi_array[i].IsPlanar() && !vi_array[i].IsY8())
        {
            clip_buffer_size += aligned(vi_array[i].RowSize(PLANAR_U)) * 
                (vi_array[i].height >> vi_array[i].GetPlaneHeightSubsampling(PLANAR_U)) * 2;
        }
        if (clip_buffer_size > data_buffer_size)
        {
            data_buffer_size = clip_buffer_size;
        }
    }

    assert(data_buffer_size > 0);

    // add some space for aligning
    data_buffer_size += 2048;
    DWORD mapping_size = sizeof(shared_memory_source_header_t) + sizeof(VideoInfo) * clip_count + data_buffer_size;
    _mapping_handle.replace(CreateFileMapping(INVALID_HANDLE_VALUE, NULL, PAGE_READWRITE, 0, mapping_size, mapping_name));
    if (!_mapping_handle.is_valid())
    {
        DWORD error_code = GetLastError();
        assert(false);
        throw runtime_error("Unable to create file mapping object.");
    }
    if (GetLastError() == ERROR_ALREADY_EXISTS)
    {
        throw runtime_error("The file mapping object already exists.");
    }
    map_view();

    memset(&header, 0, sizeof(shared_memory_source_header_t));
    header->signature = SHARED_MEMORY_SOURCE_SIGNATURE;
    header->clip_count = clip_count;
    header->data_buffer_size = data_buffer_size;
    for (int i = 0; i < clip_count; i++)
    {
        header->vi_array[i] = vi_array[i];
    }
    char* data_buffer_address = (char*)header + sizeof(shared_memory_source_header_t) + sizeof(VideoInfo) * clip_count + 64;
    data_buffer_address = aligned(data_buffer_address);
    add_guard_bytes(data_buffer_address, data_buffer_size);
    header->response[0].buffer_offset = data_buffer_address - (char*)header;

    data_buffer_address += 64 + data_buffer_size;
    data_buffer_address = aligned(data_buffer_address);
    add_guard_bytes(data_buffer_address, data_buffer_size);
    header->response[1].buffer_offset = data_buffer_address - (char*)header;

    _request_lock.switch_to_other_side();
}

void SharedMemorySourceManager::map_view()
{
    header = (shared_memory_source_header_t*)MapViewOfFile(_mapping_handle.get(), FILE_MAP_WRITE, 0, 0, 0);
    if (!header)
    {
        DWORD error_code = GetLastError();
        assert(false);
        throw runtime_error("MapViewOfFile failed.");
    }
    if (header->signature != SHARED_MEMORY_SOURCE_SIGNATURE)
    {
        assert(false);
        throw runtime_error("Invalid shared memory object.");
    }
}

void SharedMemorySourceManager::check_data_buffer_integrity(int response_object_id)
{
    assert(response_object_id >= 0 && response_object_id <= 1);
    check_guard_bytes(header + header->response[response_object_id].buffer_offset, header->data_buffer_size);
}

void SharedMemorySourceManager::signal_shutdown()
{
    if (_is_server)
    {
        header->shutdown = true;
        while (header->client_count > 0)
        {
            _request_lock.signal_all();
            _response_0_lock.signal_all();
            _response_1_lock.signal_all();
            Sleep(0);
        }
    }
}

void SharedMemorySourceManager::init_client(const SYSCHAR* mapping_name)
{
    _mapping_handle.replace(OpenFileMapping(FILE_MAP_WRITE, FALSE, mapping_name));
    if (!_mapping_handle.is_valid())
    {
        DWORD error_code = GetLastError();
        assert(false);
        throw runtime_error("Unable to open the file mapping object, maybe the server is closed.");
    }
    map_view();
    InterlockedIncrement(&header->client_count);
}

SharedMemorySourceManager::SharedMemorySourceManager(const sys_string key, bool is_server, int clip_count, const VideoInfo vi_array[]) :
    _is_server(is_server),
    _mapping_handle(NULL),
    header(NULL),
    _request_lock(key.c_str(), TEXT("RequestLock"), is_server),
    _response_0_lock(key.c_str(), TEXT("Response0Lock"), is_server),
    _response_1_lock(key.c_str(), TEXT("Response1Lock"), is_server)
{
    if (clip_count <= 0)
    {
        throw invalid_argument("clip_count must be greater than 0.");
    }

    tstring mapping_name(key);
    mapping_name.append(TEXT("_SharedMemoryObject"));

    if (is_server)
    {
        init_server(mapping_name.c_str(), clip_count, vi_array);
    } else {
        init_client(mapping_name.c_str());
    }
}

SharedMemorySourceManager::~SharedMemorySourceManager()
{
    signal_shutdown();
    if (!_is_server)
    {
        InterlockedDecrement(&header->client_count);
    }
    UnmapViewOfFile(header);
    header = NULL;
}

TwoSidedLock& SharedMemorySourceManager::get_lock_by_type(lock_type_t lock_type)
{
    switch (lock_type)
    {
    case request_lock:
        return _request_lock;
        break;
    case response_0_lock:
        return _response_0_lock;
        break;
    case response_1_lock:
        return _response_1_lock;
        break;
    default:
        assert(false);
        throw runtime_error("Shouldn't reach here!");
    }
}
