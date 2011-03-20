////////////////////////////////////////////////////////////////////////////////////////////////////
// I, the creator of this work, hereby release it into the public domain. This applies worldwide.
// In case this is not legally possible: I grant anyone the right to use this work for any purpose,
// without any conditions, unless such conditions are required by law.
////////////////////////////////////////////////////////////////////////////////////////////////////

// Hilfsfunktionen zur Benutzerauthentisierung

#include "yabu.h"

#include <errno.h>
#include <fcntl.h>
#include <pwd.h>
#include <time.h>
#include <stdlib.h>
#include <string.h>
#include <sys/time.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>


const size_t TOKEN_SIZE = 16;



static void rnd_fill(void *buf, size_t len)
{
    unsigned X[5];
    int fd = open("/dev/urandom",O_RDONLY);
    if (fd > 0) {
	read(fd,X,sizeof(X));
	close(fd);
    }
    struct timeval t;
    gettimeofday(&t,0);
    X[0] ^= t.tv_usec;
    X[1] ^= t.tv_sec;
    X[2] ^= getpid();
    X[3] ^= getppid();
    X[4] ^= clock();
    unsigned char *wp = (unsigned char *)buf;
    while (len > 0) {
	for (unsigned n = 0; n < 5; ++n) {
	    X[n] = 69069 * X[n] + 13;
	    X[(n+1)%5] ^= (X[n] << 7) | (X[n] << 25);
	}
        *wp++ = (char) (X[0] % 94 + 33);
        --len;
    }
}

static const char *token_file(const char *cfg_dir, const char *user)
{
    static char fn[500];
    snprintf(fn,sizeof(fn),"%s/auth/%s",cfg_dir,user);
    return fn;
}

const char *make_auth_token(const char *cfg_dir)
{
   static char token[TOKEN_SIZE + 1];
   if (token[0] != 0)
       return token;
   token[TOKEN_SIZE] = 0;

   char const *name = getenv("LOGNAME");
   if (name == 0) name = getenv("USER");
   if (name == 0) return "";
   const char * const fn = token_file(cfg_dir,name);
   struct stat sb;
   if (   stat(fn,&sb) == 0 && (size_t) sb.st_size == TOKEN_SIZE
       && sb.st_mtime + 32000 >= time(0) && (sb.st_mode & 077) == 0) {
      // Vorhandenes Token benutzen
      int fd = open(fn,O_RDONLY);
      if (fd >= 0) {
	  int len = read(fd,token,TOKEN_SIZE);
	  close(fd);
	  if (len == (int) TOKEN_SIZE)
	      return token;
      }
   }

   // Neues Token erzeugen
   int fd = open(fn,O_CREAT | O_TRUNC | O_RDWR,0600);
   if (fd < 0) return "";
   if (fchmod(fd,0600) < 0) return "";
   rnd_fill(token,TOKEN_SIZE);
   write(fd,token,TOKEN_SIZE);
   close(fd);
   return token;
}


////////////////////////////////////////////////////////////////////////////////////////////////////
// Prüft das Kennwort eines Benutzers. Rückgabewert ist im Erfolgsfalle wie bei «getpwuid()»,
// ansonsten 0.
////////////////////////////////////////////////////////////////////////////////////////////////////

struct passwd *auth_check(const char *cfg_dir, unsigned uid, const char *token)
{
   struct passwd *pwd = getpwuid(uid);
   if (pwd == 0) {
      Message(MSG_W,"getpwuid(%d): errno=%d (%s)",uid,errno,strerror(errno));
      return 0;
   }

   const char * const fn = token_file(cfg_dir,pwd->pw_name);
   uid_t old_uid = geteuid();
   seteuid(uid);
   int fd = open(fn,O_RDONLY);	// Öffne als Benutzer (NFS)
   seteuid(old_uid);
   char token2[TOKEN_SIZE+1];
   token2[TOKEN_SIZE] = 0;
   if (fd == -1 || read(fd,token2,TOKEN_SIZE) != (int) TOKEN_SIZE) {
      MSG(MSG_W,Msg::cannot_read(fn,errno));
      pwd = 0;
   }
   if (pwd && strcmp(token2,token) != 0) {
       MSG(MSG_W,Msg::wrong_token(pwd->pw_name));
       pwd = 0;
   }

   struct stat sb;
   if (   pwd && (fstat(fd,&sb) == -1 || (size_t) sb.st_size != TOKEN_SIZE
       || (unsigned) sb.st_uid != uid || (sb.st_mode & 077) != 0)) {
      MSG(MSG_W,Msg::bad_file_attributes(fn));
      pwd = 0;
   }
   close(fd);
   return pwd;
}

