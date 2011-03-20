////////////////////////////////////////////////////////////////////////////////////////////////////
// I, the creator of this work, hereby release it into the public domain. This applies worldwide.
// In case this is not legally possible: I grant anyone the right to use this work for any purpose,
// without any conditions, unless such conditions are required by law.
////////////////////////////////////////////////////////////////////////////////////////////////////

// Systemspezifische Funktionen

#include "yabu.h"
#include <fcntl.h>
#include <netdb.h>
#include <errno.h>
#include <signal.h>
#include <unistd.h>
#include <utime.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/wait.h>
#include <sys/utsname.h>
#include <arpa/inet.h>
#include <netinet/in.h>


static struct utsname ubuf;
static bool ubuf_set = false;

static void fill_ubuf()
{
    ubuf_set = true;
    uname(&ubuf);
    char *c = strchr(ubuf.nodename, '.');
    if (c) *c = 0;
}

const char *my_osname()
{
    if (!ubuf_set) fill_ubuf();
    return ubuf.sysname;
}

const char *my_osrelease()
{
    if (!ubuf_set) fill_ubuf();
    return ubuf.release;
}

const char *my_machine()
{
    if (!ubuf_set) fill_ubuf();
    return ubuf.machine;
}

const char *my_hostname()
{
    static const char *fake_name = getenv("YABU_FAKE_HOSTNAME");
    if (fake_name) return fake_name;
    if (!ubuf_set) fill_ubuf();
    return ubuf.nodename;
}

void set_close_on_exec(int fd)
{
   long const old_flags = fcntl(fd,F_GETFD);
   if (old_flags != -1)
	fcntl(fd, F_SETFD, old_flags | FD_CLOEXEC);
}


bool resolve(struct in_addr *a, const char *name)
{
    if ((a->s_addr = inet_addr(name)) == (in_addr_t) -1) {
        struct hostent *he = gethostbyname(name);
        if (he == 0)
	    return false;
        memcpy(&a->s_addr,he->h_addr_list[0],sizeof(a->s_addr));
    }
    return true;
}



////////////////////////////////////////////////////////////////////////////////////////////////////
// Signal-Handler
////////////////////////////////////////////////////////////////////////////////////////////////////

volatile bool got_sigchld = false;
static yabu_sigh_t sigint_handler = 0;
static yabu_sigh_t sighup_handler = 0;
static yabu_sigh_t sigterm_handler = 0;

extern "C"
void yabu_signal_handler(int sig)
{
   switch (sig) {
       case SIGCHLD: got_sigchld = true; break;
       case SIGINT: sigint_handler(SIGINT); break;	
       case SIGTERM: sigterm_handler(SIGTERM); break;	
       case SIGHUP: sighup_handler(SIGHUP); break;	
   }
}

void sys_setup_sig_handler(yabu_sigh_t hup, yabu_sigh_t intr, yabu_sigh_t term)
{
   struct sigaction sa;
   memset(&sa,0,sizeof(sa));
   sa.sa_flags = SA_NOCLDSTOP;
   sa.sa_handler = yabu_signal_handler;
   sigaction(SIGCHLD,&sa,0);
   if ((sighup_handler = hup) != 0)
       sigaction(SIGHUP,&sa,0);
   if ((sigint_handler = intr) != 0)
       sigaction(SIGINT,&sa,0);
   if ((sigterm_handler = term) != 0)
       sigaction(SIGTERM,&sa,0);
}


////////////////////////////////////////////////////////////////////////////////////////////////////
// Erzeugt einen neuen Prozeß und eine Pipe zwischen dem aufrufenden und Kindprozeß.
// Der Aufrufer erhält das Lese-Ende der Pipe in «pipe_fd». Das Schreib-Ende wird im Kindprozeß
// gemäß «flags» genutzt:
//   EXEC_COLLECT_OUTPUT    Standardausgabe wird in die Pipe umgeleitet
//   EXEC_MERGE_STDERR      Zusätzlich wird stderr in die Pipe umgeleitet
//
// pipe: Dateideskriptor der Pipe (Lese-Ende)
// pid: Prozeß-Id
// flags: Siehe oben
////////////////////////////////////////////////////////////////////////////////////////////////////


bool yabu_fork(int *pipe_fd, pid_t *pid, unsigned flags)
{
   // Pipe erzeugen
   int pfd[2];
   if (pipe(pfd) < 0) {
      YUFTL(G20,syscall_failed("pipe",0));
      return false;
   }
   set_close_on_exec(pfd[0]);
   *pipe_fd = pfd[0];

   // Kindprozeß erzeugen
   switch ((*pid = fork())) {
       case (pid_t) -1:
	   YUFTL(G20,syscall_failed("fork",0));
	   close(pfd[0]);
	   close(pfd[1]);
	   return false;
	case 0:					// Kindprozeß
           close(pfd[0]);			// yabu-Ende schließen
	   if ((flags & EXEC_MERGE_STDERR) == 0)	// stderr -> stdout (???)
	       dup2(1, 2);
	   if ((flags & EXEC_COLLECT_OUTPUT) && pfd[1] != 1) {
	       dup2(pfd[1], 1);			// stdout in die Pipe umleiten
	       close(pfd[1]);
	   }
	   if (flags & EXEC_MERGE_STDERR)
	       dup2(1, 2);			// stderr nach stdout umleiten
	   break;
	default:				// yabu
	   close(pfd[1]);		 	// Kind-Ende schließen
   }

   return true;
}

int yabu_open(const char *fn, int flags)
{
   int rc;
   while ((rc = open(fn,flags)) == -1 && errno == EINTR)
      ;
   return rc;
}

int yabu_read(int fd, void *buf, size_t len)
{
   int rc;
   while ((rc = read(fd,buf,len)) == -1 && errno == EINTR);
   return rc;
}

int yabu_write(int fd, void const *buf, size_t len)
{
   int rc;
   while ((rc = write(fd,buf,len)) == -1 && errno == EINTR);
   return rc;
}


bool yabu_stat(const char *name, struct stat *sb)
{
   int rc;
   memset(sb,0,sizeof(*sb));
   while ((rc = stat(name,sb)) == -1 && errno == EINTR);
#ifdef EOVERFLOW
   if (rc == -1 && errno == EOVERFLOW)		 // Ignoriere EOVERFLOW (bei großen Dateien)
      rc = 0;
#endif
   return rc == 0;
}


void yabu_ftime(Ftime *ft, struct stat const *sb)
{
    ft->s_ = sb->st_mtime;
#if defined(__linux__) && __USE_MISC
    ft->ns_ = sb->st_mtim.tv_nsec;
#elif (defined(__linux__) && __USE_MISC == 0) || defined(__OpenBSD__) || defined(__NetBSD__)
    ft->ns_ = sb->st_mtimensec;
#elif defined(__FreeBSD__) && __BSD_VISIBLE
    ft->ns_ = sb->st_mtimespec.tv_nsec;
#elif defined(__FreeBSD__) && __BSD_VISIBLE == 0
    ft->ns_ = sb->__st_mtimensec;
#elif defined(__sun__) && (!defined(__XOPEN_OR_POSIX) || defined(__EXTENSIONS__))
    ft->ns_ = sb->st_atim.tv_nsec;
#elif defined(__sun__) && (defined(__XOPEN_OR_POSIX) && !defined(__EXTENSIONS__))
    ft->ns_ = sb->st_atim.__tv_nsec;
#else
    ft->ns_ = 0;
#endif
}

// "Create or touch"

void yabu_cot(const char *fn)
{
#ifndef O_DIRECT
#define O_DIRECT 0
#endif
    int fd = open(fn,O_CREAT | O_EXCL | O_WRONLY | O_NONBLOCK | O_DIRECT,0755);
    if (fd >= 0) {
	unlink(fn);
	close(fd);
    } else if (errno == EEXIST)
	utime(fn,0);
}

