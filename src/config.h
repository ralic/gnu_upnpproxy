#define HAVE_INET6 1
#define HAVE_GETOPT_LONG 1
#define _GNU_SOURCE
#define _XOPEN_SOURCE

#ifdef __APPLE__

#define HAVE_STRNDUP 0
#define HAVE_STRNLEN 0
#define HAVE_GETLINE 0
#define HAVE_GETC_UNLOCKED 1
#define HAVE_UNGETC_UNLOCKED 0

#else

#define HAVE_STRNDUP 1
#define HAVE_STRNLEN 1
#define HAVE_GETLINE 1
#define HAVE_GETC_UNLOCKED 1
#define HAVE_UNGETC_UNLOCKED 0

#endif
