#ifndef STREAM_H
#define STREAM_H

#include <cstdint>

class Stream {
public:
    virtual void     Close();
    virtual void     Seek(int64_t offset);
    virtual void     SeekEnd(int64_t offset);
    virtual void     Skip(int64_t offset);
    virtual size_t   Position();
    virtual size_t   Length();
    virtual size_t   ReadBytes(void* data, int n);
            uint8_t  ReadByte();
            uint16_t ReadUInt16();
            uint16_t ReadUInt16BE();
            uint32_t ReadUInt32();
            uint32_t ReadUInt32BE();
            uint64_t ReadUInt64();
            int16_t  ReadInt16();
            int16_t  ReadInt16BE();
            int32_t  ReadInt32();
            int32_t  ReadInt32BE();
            int64_t  ReadInt64();
            float    ReadFloat();
            char*    ReadLine();
            char*    ReadString();
            char*    ReadHeaderedString();
    virtual size_t   WriteBytes(void* data, int n);
            void     WriteByte(uint8_t data);
            void     WriteUInt16(uint16_t data);
            void     WriteUInt16BE(uint16_t data);
            void     WriteUInt32(uint32_t data);
            void     WriteUInt32BE(uint32_t data);
            void     WriteInt16(int16_t data);
            void     WriteInt16BE(int16_t data);
            void     WriteInt32(int32_t data);
            void     WriteInt32BE(int32_t data);
            void     WriteFloat(float data);
            void     WriteString(char* string);
            void     WriteHeaderedString(char* string);
            void     CopyTo(Stream* dest);
    virtual          ~Stream();
};

#endif /* STREAM_H */
