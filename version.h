#ifndef VERSION_H
#define VERSION_H

#define VERSION_MAJOR 0
#define VERSION_MINOR 1


#define STR(x) STR_(x)
#define STR_(x) #x

#define VERSION_STR STR(VERSION_MAJOR.VERSION_MINOR)

#define RC_VERSION VERSION_MAJOR,0,0,VERSION_MINOR
#define RC_VERSION_STR STR(RC_VERSION)

const UInt32 kPluginVersion = (VERSION_MAJOR << 16) | VERSION_MINOR;

#endif//VERSION_H
