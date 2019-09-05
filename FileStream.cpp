#include "FileStream.h"

FileStream* FileStream::New(const char* filename, Uint32 access) {
    FileStream* stream = new FileStream;
    if (!stream) {
        return NULL;
    }

    const char* accessString = NULL;

    switch (access) {
        case FileStream::READ_ACCESS: accessString = "rb"; break;
        case FileStream::WRITE_ACCESS: accessString = "wb"; break;
        case FileStream::APPEND_ACCESS: accessString = "ab"; break;
    }

    stream->f = fopen(filename, accessString);

    #ifdef SWITCH_ROMFS
    if (!stream->f) {
        char romfsPath[256];
        sprintf(romfsPath, "romfs:/%s", filename);
        stream->f = fopen(romfsPath, accessString);
    }
    #endif

    if (!stream->f)
        goto FREE;

    fseek(stream->f, 0, SEEK_END);
    stream->size = ftell(stream->f);
    fseek(stream->f, 0, SEEK_SET);

    return stream;

    FREE:
        delete stream;
        return NULL;
}

void        FileStream::Close() {
    fclose(f);
    f = NULL;
    Stream::Close();
}
void        FileStream::Seek(int64_t offset) {
    fseek(f, offset, SEEK_SET);
}
void        FileStream::SeekEnd(int64_t offset) {
    fseek(f, offset, SEEK_END);
}
void        FileStream::Skip(int64_t offset) {
    fseek(f, offset, SEEK_CUR);
}
size_t      FileStream::Position() {
    return ftell(f);
}
size_t      FileStream::Length() {
    return size;
}

size_t      FileStream::ReadBytes(void* data, int n) {
    // if (!f) Log::Print(Log::LOG_ERROR, "Attempt to read from closed stream.")
    return fread(data, 1, n, f);
}

size_t      FileStream::WriteBytes(void* data, int n) {
    return fwrite(data, 1, n, f);
}
