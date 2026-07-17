#ifndef RTS_VERSION_H
#define RTS_VERSION_H

#define RTS_VERSION_MAJOR 1u
#define RTS_VERSION_MINOR 0u
#define RTS_VERSION_PATCH 0u
#define RTS_VERSION_STRING "1.0.0"

#define RTS_VERSION_ENCODE(major, minor, patch) \
    ((((major) & 0xffu) << 24u) | (((minor) & 0xffu) << 16u) | \
     ((patch) & 0xffffu))

#define RTS_VERSION \
    RTS_VERSION_ENCODE(RTS_VERSION_MAJOR, RTS_VERSION_MINOR, \
                       RTS_VERSION_PATCH)

#endif /* RTS_VERSION_H */
