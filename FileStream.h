#ifndef FILESTREAM_H
#define FILESTREAM_H

#include <stdio.h>
#include "Stream.h"

class FileStream : public Stream {
public:
    FILE*  f;
    size_t size;
    enum {
        READ_ACCESS = 0,
        WRITE_ACCESS = 1,
        APPEND_ACCESS = 2,
    };

    FileStream* New(const char* filename, uint32_t access);
    void        Close();
    void        Seek(int64_t offset);
    void        SeekEnd(int64_t offset);
    void        Skip(int64_t offset);
    size_t      Position();
    size_t      Length();
    size_t      ReadBytes(void* data, int n);
    size_t      WriteBytes(void* data, int n);
};

#endif /* FILESTREAM_H */
