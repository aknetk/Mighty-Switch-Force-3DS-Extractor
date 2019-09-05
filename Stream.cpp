#include "Stream.h"

#define READ_TYPE_MACRO(type) \
    type data; \
    ReadBytes(&data, sizeof(data));

void     Stream::Close() {
    delete this;
}
void     Stream::Seek(int64_t offset) {
}
void     Stream::SeekEnd(int64_t offset) {
}
void     Stream::Skip(int64_t offset) {
}
size_t   Stream::Position() {
    return 0;
}
size_t   Stream::Length() {
    return 0;
}

size_t   Stream::ReadBytes(void* data, int n) {
    return 0;
}
uint8_t  Stream::ReadByte() {
    READ_TYPE_MACRO(uint8_t);
    return data;
}
uint16_t Stream::ReadUInt16() {
    READ_TYPE_MACRO(uint16_t);
    return data;
}
uint16_t Stream::ReadUInt16BE() {
    return (uint16_t)(ReadByte() << 8 | ReadByte());
}
uint32_t Stream::ReadUInt32() {
    READ_TYPE_MACRO(uint32_t);
    return data;
}
uint32_t Stream::ReadUInt32BE() {
    return (uint32_t)(ReadByte() << 24 | ReadByte() << 16 | ReadByte() << 8 | ReadByte());
}
uint64_t Stream::ReadUInt64() {
    READ_TYPE_MACRO(uint64_t);
    return data;
}
int16_t  Stream::ReadInt16() {
    READ_TYPE_MACRO(int16_t);
    return data;
}
int16_t  Stream::ReadInt16BE() {
    return (int16_t)(ReadByte() << 8 | ReadByte());
}
int32_t  Stream::ReadInt32() {
    READ_TYPE_MACRO(int32_t);
    return data;
}
int32_t  Stream::ReadInt32BE() {
    return (int32_t)(ReadByte() << 24 | ReadByte() << 16 | ReadByte() << 8 | ReadByte());
}
int64_t  Stream::ReadInt64() {
    READ_TYPE_MACRO(int64_t);
    return data;
}
float    Stream::ReadFloat() {
    READ_TYPE_MACRO(float);
    return data;
}
char*    Stream::ReadLine() {
    uint8_t byte;
    size_t start = Position();
    while ((byte = ReadByte()) != '\n' && byte);

    size_t size = Position() - start;

    char* data = (char*)malloc(size + 1);
    Skip(-size);
    ReadBytes(data, size);
    data[size] = 0;

    return data;
}
char*    Stream::ReadString() {
    size_t start = Position();
    while (ReadByte());

    size_t size = Position() - start;

    char* data = (char*)malloc(size + 1);
    Skip(-size);
    ReadBytes(data, size);
    data[size] = 0;

    return data;
}
char*    Stream::ReadHeaderedString() {
    uint8_t size = ReadByte();

    char* data = (char*)malloc(size + 1);
    ReadBytes(data, size);
    data[size] = 0;

    return data;
}

size_t   Stream::WriteBytes(void* data, int n) {
    return 0;
}
void     Stream::WriteByte(uint8_t data) {
    WriteBytes(&data, sizeof(data));
}
void     Stream::WriteUInt16(uint16_t data) {
    WriteBytes(&data, sizeof(data));
}
void     Stream::WriteUInt16BE(uint16_t data) {
    WriteByte(data >> 8 & 0xFF);
    WriteByte(data & 0xFF);
}
void     Stream::WriteUInt32(uint32_t data) {
    WriteBytes(&data, sizeof(data));
}
void     Stream::WriteUInt32BE(uint32_t data) {
    WriteByte(data >> 24 & 0xFF);
    WriteByte(data >> 16 & 0xFF);
    WriteByte(data >> 8 & 0xFF);
    WriteByte(data & 0xFF);
}
void     Stream::WriteInt16(int16_t data) {
    WriteBytes(&data, sizeof(data));
}
void     Stream::WriteInt16BE(int16_t data) {
    WriteUInt16BE((uint16_t)data);
}
void     Stream::WriteInt32(int32_t data) {
    WriteBytes(&data, sizeof(data));
}
void     Stream::WriteInt32BE(int32_t data) {
    WriteUInt32BE((int32_t)data);
}
void     Stream::WriteFloat(float data) {
    WriteBytes(&data, sizeof(data));
}
void     Stream::WriteString(char* string) {
    size_t size = strlen(string) + 1;
    WriteBytes(string, size);
}
void     Stream::WriteHeaderedString(char* string) {
    size_t size = strlen(string) + 1;
    WriteByte((uint8_t)size);
    WriteBytes(string, size);
}

void     Stream::CopyTo(Stream* dest) {
    size_t length = Length();
    void*  memory = Memory::Malloc(length);

    Seek(0);
    ReadBytes(memory, length);
    dest->WriteBytes(memory, length);
    dest->Seek(0);

    free(memory);
}

Stream::~Stream() {
}
