#include "ByteQueue.h"

namespace IPCDemo
{
ByteQueue::ByteQueue()
    : m_buffer(new UINT8[1024])
    , m_size(1024)
    , m_offset(0)
    , m_current(0)
{
}

ByteQueue::~ByteQueue() = default;

UINT32  ByteQueue::size() const
{
    return m_current;
}

void ByteQueue::Reset()
{
    m_offset = 0;
    m_current = 0;
}

void ByteQueue::Push(const UINT8* data, INT32 size)
{
    size_t total = m_current + size;
    if (total > m_size)
    {
        size_t newsize = 2 * m_size;
        while (total > newsize && newsize > m_size)
            newsize *= 2;
        unique_ptr<UINT8[]> buffer(new UINT8[newsize]);
        if (m_current > 0)
        {
            memcpy(buffer.get(), front(), m_current);
        }
        m_buffer = std::move(buffer);
        m_size = newsize;
        m_offset = 0;
    }
    else if ((m_offset + m_current + size) > m_size)
    {
        memmove(m_buffer.get(), front(), m_current);
        m_offset = 0;
    }
    memcpy(front() + m_current, data, size);
    m_current += size;
}

void ByteQueue::Peek(const UINT8** data, INT32* size) const
{
    *data = front();
    *size = m_current;
}

void ByteQueue::Pop(INT32 size)
{
    m_offset += size;
    m_current -= size;
    if (m_offset == m_size)
    {
        m_offset = 0;
    }
}

UINT8* ByteQueue::front() const
{
    return m_buffer.get() + m_offset;
}
} // namespace MediaSDK
