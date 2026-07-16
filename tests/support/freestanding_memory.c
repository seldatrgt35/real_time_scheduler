#include <stddef.h>

void *memset(void *destination, int value, size_t count)
{
    unsigned char *bytes = destination;
    size_t index;

    for (index = 0u; index < count; ++index)
    {
        bytes[index] = (unsigned char)value;
    }
    return destination;
}

void *memcpy(void *restrict destination,
             const void *restrict source,
             size_t count)
{
    unsigned char *destination_bytes = destination;
    const unsigned char *source_bytes = source;
    size_t index;

    for (index = 0u; index < count; ++index)
    {
        destination_bytes[index] = source_bytes[index];
    }
    return destination;
}
