////////////////////////////////////////////////////////////////////////////////////////////////////
// I, the creator of this work, hereby release it into the public domain. This applies worldwide.
// In case this is not legally possible: I grant anyone the right to use this work for any purpose,
// without any conditions, unless such conditions are required by law.
////////////////////////////////////////////////////////////////////////////////////////////////////

// Lesen von Konfigurationsdateien

#include "yabu.h"


#include <errno.h>
#include <unistd.h>
#include <string.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <netinet/in.h>


////////////////////////////////////////////////////////////////////////////////////////////////////
// Eine Zeile verarbeiten
////////////////////////////////////////////////////////////////////////////////////////////////////

void CfgReader::process_line(char *c)
{
   if (!strcmp(c,"!settings"))
      mode_ = PREFS;
   else if (!strcmp(c,"!options"))
      mode_ = OPTIONS;
   else if (!strcmp(c,"!servers"))
      mode_ = SERVERS;
   else if (*c == '!')
      mode_ = IGNORE;
   else switch (mode_) {
      case PREFS: Setting::parse_line(c,prio_); break;
      case SERVERS: do_server(c); break;
      case OPTIONS: var_parse_global_options(c); break;
      case IGNORE: break;
   }
}


////////////////////////////////////////////////////////////////////////////////////////////////////
// Eine Zeile im Abschnit !servers verarbeiten
////////////////////////////////////////////////////////////////////////////////////////////////////

bool CfgReader::do_server(char *lbuf)
{
    size_t argc;
    char *argv[20];
    static const size_t MAX_ARGC = sizeof(argv) / sizeof(argv[0]);
    char *c = lbuf;
    for (argc = 0; argc < MAX_ARGC && (argv[argc] = str_chop(&c)) != 0; ++argc);
    if (!strcmp(argv[0], "host"))
	return do_host(argc, argv);
    YUERR(S01,syntax_error(argv[0]));
    return false;
}


////////////////////////////////////////////////////////////////////////////////////////////////////
// Eine "host"-Zeile verarbeiten.
////////////////////////////////////////////////////////////////////////////////////////////////////

bool CfgReader::do_host(size_t argc, char **argv)
{
   if (argc < 2) return false;

   // 1. Argument: host[:[addr[:port]]]
   char *name = argv[1];
   char *c = 0;
   struct sockaddr_in sa;
   memset(&sa,0,sizeof(sa));
   if ((c = strchr(name,':')) != 0) {
      *c++ = 0;
      sa.sin_family = AF_INET;
      char * const addr = *c ? c : name;	// Default ist «addr» = «name»
      const char * port = DEFAULT_PORT;
      if ((c = strchr(addr,':')) != 0) {
	 *c++ = 0;
	 port = c;
      }
      int p = -1;
      if (!str2int(&p,port) || p < 1 || p > 65534 || !resolve(&sa.sin_addr,addr)) {
	 Message(MSG_W,"Ungültige Adresse '%s:%s'",addr,port);
	 return false;
      }
      sa.sin_port = ntohs(p);
   }

   // Weitere optionale Argumente
   char const *cfg = "";
   int max = 1;
   int prio = 1;
   for (size_t i = 2; i < argc; ++i) {
      bool ok = true;
      if (!strncmp(argv[i],"cfg=",4)) {
	 cfg = argv[i] + 4;
	 ok = var_cfg_is_valid(0,cfg);		// T:[??????]
      }
      else if (!strncmp(argv[i],"max=",4))
	 ok = str2int(&max,argv[i] + 4);
      else if (!strncmp(argv[i],"prio=",5))
	 ok = str2int(&prio,argv[i] + 5);
      else
	 ok = false;
      if (!ok) return false;
   }
   if (max < 1) max = 1;
   return host(name,sa.sin_family ? &sa : 0,max,prio,cfg);
}



// vim:shiftwidth=3:cindent
