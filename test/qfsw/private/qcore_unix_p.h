#include <errno.h>

#define EINTR_LOOP(var, cmd)                    \
     do {                                        \
         var = cmd;                              \
     } while (var == -1 && errno == EINTR)


 // don't call QT_OPEN or ::open
 // call qt_safe_open
 static inline int qt_safe_open(const char *pathname, int flags, mode_t mode = 0777)
 {        
 #ifdef O_CLOEXEC
     flags |= O_CLOEXEC;
 #endif   
     register int fd;
     EINTR_LOOP(fd, QT_OPEN(pathname, flags, mode));
          
     // unknown flags are ignored, so we have no way of verifying if
     // O_CLOEXEC was accepted
     if (fd != -1)
         ::fcntl(fd, F_SETFD, FD_CLOEXEC);
     return fd;
 }        
 #undef QT_OPEN
 #define QT_OPEN         qt_safe_open
