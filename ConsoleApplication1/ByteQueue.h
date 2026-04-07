#pragma once
#include "Common.h"
using namespace std;

namespace IPCDemo
{
class ByteQueue
{
public:
    ByteQueue();
    ~ByteQueue();
    UINT32  size() const;
    void    Reset();
    void    Push(const UINT8* data, INT32 size);
    void    Peek(const UINT8** data, INT32* size) const;
    void    Pop(INT32 count);

private:
    UINT8*              front() const;
    UINT32              m_size;
    UINT32              m_offset;
    INT32               m_current;
    unique_ptr<UINT8[]> m_buffer;
};
} // namespace MediaSDK
