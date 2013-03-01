#ifndef WINSTRDEFS_H
#define WINSTRDEFS_H

#if defined(_WIN32) || defined(_WIN64)
  #define snprintf _snprintf
  #define vsnprintf _vsnprintf
  #define strcasecmp _stricmp
  #define strncasecmp _strnicmp
#endif

#endif//WINSTRDEFS_H
