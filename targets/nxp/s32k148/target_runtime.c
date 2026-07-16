#include <stddef.h>

void *memset(void *destination, int value, size_t count)
{
    unsigned char *bytes = (unsigned char *)destination;
    size_t index;
    for (index = 0u; index < count; ++index)
    {
        bytes[index] = (unsigned char)value;
    }
    return destination;
}

void *memcpy(void *destination, const void *source, size_t count)
{
    unsigned char *to = (unsigned char *)destination;
    const unsigned char *from = (const unsigned char *)source;
    size_t index;
    for (index = 0u; index < count; ++index)
    {
        to[index] = from[index];
    }
    return destination;
}

void __aeabi_memclr(void *destination, size_t count)
{
    (void)memset(destination, 0, count);
}

void __aeabi_memclr4(void *destination, size_t count)
{
    (void)memset(destination, 0, count);
}

void __aeabi_memclr8(void *destination, size_t count)
{
    (void)memset(destination, 0, count);
}

void __aeabi_memcpy(void *destination, const void *source, size_t count)
{
    (void)memcpy(destination, source, count);
}

void __aeabi_memcpy4(void *destination, const void *source, size_t count)
{
    (void)memcpy(destination, source, count);
}

void __aeabi_memcpy8(void *destination, const void *source, size_t count)
{
    (void)memcpy(destination, source, count);
}
