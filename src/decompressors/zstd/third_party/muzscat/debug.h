#ifndef DEBUG_H_12987983217
#define DEBUG_H_12987983217

#define DEBUG_STATIC_ASSERT(c) (void)sizeof(char[(c) ? 1 : -1])
#define DEBUGLEVEL 0
#ifndef assert
#    define assert(condition) ((void)0)
#endif
#define RAWLOG(l, ...)      {}    /* disabled */
#define DEBUGLOG(l, ...)    {}    /* disabled */

#endif /* DEBUG_H_12987983217 */
