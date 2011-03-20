////////////////////////////////////////////////////////////////////////////////////////////////////
// I, the creator of this work, hereby release it into the public domain. This applies worldwide.
// In case this is not legally possible: I grant anyone the right to use this work for any purpose,
// without any conditions, unless such conditions are required by law.
////////////////////////////////////////////////////////////////////////////////////////////////////

// Yabu-Server

#include "yabu.h"


#include <sys/types.h>
#include <grp.h>
#include <errno.h>
#include <fcntl.h>
#include <arpa/inet.h>
#include <netinet/in.h>
#include <netdb.h>
#include <poll.h>
#include <pwd.h>
#include <signal.h>
#include <stdarg.h>
#include <time.h>
#include <stdlib.h>
#include <string.h>
#include <syslog.h>
#include <sys/socket.h>
#include <sys/stat.h>
#include <sys/wait.h>
#include <unistd.h>



struct Client;

static StringMap static_env;		// Feste Umgebung für alle Skripte
static const int MIN_UID = 20;		// Kleinste erlaubte UID


struct SrvJob: public PollObj {
   SrvJob(Client &cl, const char *cmd);
   ~SrvJob();
   void append(SrvJob ***tailp);
   SrvJob *remove(SrvJob ***tailp);
   const char *pre_exec();
   bool exec();
   int handle_input(int fd, int events);
   void handle_exit(pid_t pid, int status);
   static SrvJob *find_pid(pid_t pid);
   Client &client_;
   unsigned const cjid_;	// Job-Id des Clients
   SrvJob *next_;
   SrvJob **prevp_;
   StringMap env_;		// Environment
   Str wd_;			// Arbeitsverzeichnis
   Str cmd_;			// Das Skript
private:
   static SrvJob *pid_hash_tab[19];
   pid_t pid_;
   SrvJob *next_pid_;
   SrvJob **prevp_pid_;
   void set_pid(pid_t pid);
   void del_pid();
   static size_t pid_hash(pid_t pid);
};


// Verbindung zu einen yabu-Prozeß.

struct Client: public PollObj {
   Client(int fd, struct sockaddr_in const &sa);
   ~Client();
   static void purge();
   int handle_input(int fd, int events);
   int handle_output(int fd, int events);
   bool handle_cmd(char tag, size_t len, char *cmd);
   bool handle_uid(char *cmd);
   bool create_job(char *cmd);
   static void start_jobs();
   bool start_job();
   Client *next_;	// Liste aller Clients (Ringverkettung!)
   Client *prev_;
   IoBuffer iob_;
   SrvJob *w_head_;	// Wartende Jobs
   SrvJob **w_tail_;
   StringMap env_;
   Str wd_;
   unsigned cjid_;
   int uid_;
   Str uname_;
   int gid_;
   DirMaker dirs_;
};

// TCP-Socket, auf dem wir Verbindungen annehmen.

struct TcpServer: public PollObj {
    TcpServer(int fd);
    ~TcpServer();
    int handle_input(int fd, int events);
    static bool init();
};

static bool running_as_root = false;
static unsigned total_active;
static unsigned max_active = 0;		// Max. Anzahl parallel ausgeführter Skripte
static sockaddr_in my_addr;		// Unsere TCP-Adresse
static SrvJob *r_head = 0;		// Laufende Jobs
static SrvJob **r_tail = &r_head;
SrvJob *SrvJob::pid_hash_tab[19];


////////////////////////////////////////////////////////////////////////////////////////////////////
// Dezimaldarstellung einer IPv4-Adresse
////////////////////////////////////////////////////////////////////////////////////////////////////

static const char *addr2str(struct sockaddr_in const &addr)
{
    static char buf[30];	// xxx.xxx.xxx.xxx:xxxxx 
    unsigned a = ntohl(addr.sin_addr.s_addr);
    snprintf(buf,sizeof(buf),"%u.%u.%u.%u:%u",
        (a >> 24) & 0xff, (a>>16) & 0xff, (a>>8) & 0xff, a & 0xff,ntohs(addr.sin_port));
    return buf;
}




// =================================================================================================



SrvJob::SrvJob(Client &cl, const char *cmd)
    : 
      client_(cl),  cjid_(cl.cjid_),
      next_(0), prevp_(0), env_(cl.env_),
      wd_(cl.wd_), cmd_(cmd),
      pid_(0), next_pid_(0), prevp_pid_(0)
{
   Message(MSG_2,"[%u.%u] Init cjid=%u",client_.id_,id_,cjid_);
}

SrvJob::~SrvJob()
{
   YABU_ASSERT(prevp_ == 0);
   YABU_ASSERT(next_ == 0);
   if (pid_ > 1) {
      kill(pid_,SIGKILL);
      del_pid();
   }
}

void SrvJob::append(SrvJob ***tailp)
{
   YABU_ASSERT(*tailp != 0);
   YABU_ASSERT(prevp_ == 0);
   YABU_ASSERT(next_ == 0);
   prevp_ = *tailp;
   **tailp = this;
   *tailp = &next_;
}

SrvJob *SrvJob::remove(SrvJob ***tailp)
{
   YABU_ASSERT(*tailp != 0);
   YABU_ASSERT(prevp_ != 0);
   YABU_ASSERT((next_ == 0) ^ (*tailp != &next_));

   if (next_ == 0)		// Letztes Element?
      *tailp = prevp_;
   else
      next_->prevp_ = prevp_;
   *prevp_ = next_;
   next_ = 0;
   prevp_ = 0;
   return this;
}


////////////////////////////////////////////////////////////////////////////////////////////////////
// Ausgaben vom Skript lesen und in die Sende-Warteschlange stellen.
////////////////////////////////////////////////////////////////////////////////////////////////////

int SrvJob::handle_input(int fd, int events)
{
   char tmp[8192];
   snprintf(tmp,sizeof(tmp),"%08x",cjid_);
   int n = read(fd,tmp + 8,sizeof(tmp) - 8);
   if (n <= 0)
      return 1;
   client_.iob_.append('O',n + 8,tmp);
   return 0;
}


////////////////////////////////////////////////////////////////////////////////////////////////////
// Skript ausführen
////////////////////////////////////////////////////////////////////////////////////////////////////

bool SrvJob::exec()
{
   int pfd;
   pid_t pid;

   Message(MSG_2,"[%u.%u] Exec",client_.id_,id_);
    if (!yabu_fork(&pfd,&pid,EXEC_COLLECT_OUTPUT | EXEC_MERGE_STDERR))
       return false;
    if (pid == 0) {
        const char *err = pre_exec();
	if (err == 0) {
	   nice(5);
	   execle(shell_prog,"sh","-c",(const char *)cmd_,(void *) 0,env_.env());
	   static char tmp[200];
	   snprintf(tmp,sizeof(tmp),"%s: errno=%d (%s)\n",
	       (const char *)shell_prog,errno,strerror(errno));
	   err = tmp;
	}
	fputs(err,stderr);
	exit(123);
    }
    else {
       set_pid(pid);
       add_fd(pfd);
       return true;
    }
}

size_t SrvJob::pid_hash(pid_t pid)
{
   return pid % sizeof(pid_hash_tab) / sizeof(pid_hash_tab[0]);
}


////////////////////////////////////////////////////////////////////////////////////////////////////
// Objekt in die PID-Hashtabelle einfügen
////////////////////////////////////////////////////////////////////////////////////////////////////

void SrvJob::set_pid(pid_t pid)
{
   YABU_ASSERT(pid > 1);
   YABU_ASSERT(pid_ == 0);
   YABU_ASSERT(next_pid_ == 0);
   YABU_ASSERT(prevp_pid_ == 0);
   pid_ = pid;
   size_t hp = pid_hash(pid);
   next_pid_ = pid_hash_tab[hp];
   if (next_pid_ != 0) next_pid_->prevp_pid_ = &next_pid_;
   prevp_pid_ = pid_hash_tab + hp;
   pid_hash_tab[hp] = this;
}


////////////////////////////////////////////////////////////////////////////////////////////////////
// Entfernt das Objekt aus dem PID-Hash
////////////////////////////////////////////////////////////////////////////////////////////////////

void SrvJob::del_pid()
{
   YABU_ASSERT(pid_ > 1);
   YABU_ASSERT(prevp_pid_ != 0);
   pid_ = 0;
   if ((*prevp_pid_ = next_pid_) != 0)
	next_pid_->prevp_pid_ = prevp_pid_;
}


////////////////////////////////////////////////////////////////////////////////////////////////////
// Sucht eine PID
////////////////////////////////////////////////////////////////////////////////////////////////////

SrvJob *SrvJob::find_pid(pid_t pid)
{
   for (SrvJob *p = pid_hash_tab[pid_hash(pid)]; p != 0; p = p->next_pid_) {
      if (p->pid_ == pid)
	 return p;
   }
   return 0;
}


// =================================================================================================

// Liste aller Clients. Die Liste ist in beiden Richtungen ringartig verkettet. «clients» dient
// zugleich als Arbeitszeiger für das Starten neuer Skripte -- siehe «start_jobs()» -- und ändert
// sich somit ständig.

static Client *clients = 0;


////////////////////////////////////////////////////////////////////////////////////////////////////
// Konstruktor
////////////////////////////////////////////////////////////////////////////////////////////////////

Client::Client(int fd, struct sockaddr_in const &sa)
    : iob_(*this), w_head_(0), w_tail_(&w_head_),
      env_(static_env),
      cjid_(0), uid_(-1), gid_(-1)
{
   Message(MSG_1,"[%u] Client() %s fd=%d",id_,addr2str(sa),fd);
   if (clients == 0) {
      clients = this;
      next_ = this;
      prev_ = this;
   } else {
      next_ = clients;
      prev_ = clients->prev_;
      next_->prev_ = this;
      prev_->next_ = this;
   }
   add_fd(fd);
}


////////////////////////////////////////////////////////////////////////////////////////////////////
// Destruktor
////////////////////////////////////////////////////////////////////////////////////////////////////

Client::~Client()
{
   if (next_ == this)	// Der letzte Client
      clients = 0;
   else {
      next_->prev_ = prev_;
      prev_->next_ = next_;
      if (clients == this)
	 clients = next_;
   }

   // Alle wartenden Jobs löschen
   while (w_head_)
      delete w_head_->remove(&w_tail_);

   // Alle laufenden Jobs löschen
   for (SrvJob *j = r_head; j; )
   {
      SrvJob *n = j->next_;
      if (&j->client_ == this) {
	 j->remove(&r_tail);
	 --total_active;
	 delete j;
      }
      j = n;
   }

   Message(MSG_2,"[%u] ~Client()",id_);
}


////////////////////////////////////////////////////////////////////////////////////////////////////
// Geschlossene Verbindungen aufräumen.
////////////////////////////////////////////////////////////////////////////////////////////////////

void Client::purge()
{
   if (clients == 0) return;
   Client *cl = clients;
   do {
      if (cl->fd() < 0) {
         Client *next = cl->next_;
	 delete cl;
	 cl = (next == cl) ? 0 : next;
      }
      else
	 cl = cl->next_;
   } while (cl != clients);
}


////////////////////////////////////////////////////////////////////////////////////////////////////
// Daten von yabu empfangen
////////////////////////////////////////////////////////////////////////////////////////////////////

int Client::handle_input(int fd, int events)
{
   int rc = iob_.read(fd);
   if (rc <= 0)
      return -1;
   while (iob_.next())
      if (!handle_cmd(iob_.tag_,iob_.len_,iob_.data_))
	 return -1;
   return 0;
}


////////////////////////////////////////////////////////////////////////////////////////////////////
// Daten an yabu senden
////////////////////////////////////////////////////////////////////////////////////////////////////

int Client::handle_output(int fd, int events)
{
   iob_.write(fd);
   return 0;
}


////////////////////////////////////////////////////////////////////////////////////////////////////
// Verarbeitet ein Kommando
////////////////////////////////////////////////////////////////////////////////////////////////////

bool Client::handle_cmd(char tag, size_t len, char *cmd)
{
    bool ok = true;
    switch (tag) {
	case 'E':			// Environment
	   Message(MSG_3,"[%u] E %s",id_,cmd);
	   env_.set(cmd);
	   break;
	case 'U':			// User-Id
	   ok = handle_uid(cmd);
	   break;
	case 'D':			// Arbeitsverzeichnis
	   wd_ = cmd;
	   Message(MSG_2,"[%u] D %s",id_,cmd);
	   break;
	case 'I':			// Initialisierung, nächstes Skript
	   ok = sscanf(cmd,"%x",&cjid_) == 1;
	   //env_.clear();
	   break;
	case 'M':			// Verzeichnis erzeugen
	   ok = dirs_.create_parents(cmd,true);
	   break;
	case 'C':			// Skript
	   ok = create_job(cmd);
	   break;
	default:
	    Message(MSG_0,"[%u] unknown command 0x%02x",id_,tag);
	    ok = false;
    }
    return ok;
}


////////////////////////////////////////////////////////////////////////////////////////////////////
// Benutzeranmeldung
////////////////////////////////////////////////////////////////////////////////////////////////////

bool Client::handle_uid(char *cmd)
{
   int uid;
   if (!next_int(&uid,&cmd)) return false;
   if (uid < MIN_UID) return false;
   char *token = str_chop(&cmd);
   if (token == 0 || *token == 0) return false;
   struct passwd *pwd = auth_check(yabu_cfg_dir,uid,token);
   if (pwd != 0) {
      uid_ = uid;
      uname_ = pwd->pw_name;
      gid_ = pwd->pw_gid;
   }
   Message(MSG_1,"[%u] %s",id_,Msg::login_result(uname_,pwd != 0));
   iob_.append('U',pwd ? "+" : "-");
   return true;
}


////////////////////////////////////////////////////////////////////////////////////////////////////
// Ein Skript ausführen
////////////////////////////////////////////////////////////////////////////////////////////////////

bool Client::create_job(char *cmd)
{
   if (uid_ < MIN_UID || cmd == 0)	// Nicht angemeldet
      return false;
   SrvJob *c = new SrvJob(*this,cmd);
   c->append(&w_tail_);
   ++cjid_;				// Default für das nächste Kommando
   return true;
}


////////////////////////////////////////////////////////////////////////////////////////////////////
// Ausführung vorbereitung
////////////////////////////////////////////////////////////////////////////////////////////////////

const char *SrvJob::pre_exec()
{
   if (running_as_root) {
      // Gruppen- und Benutzer-Id setzen
      if (setgid(client_.gid_) < 0)
	 return Msg::userinit_failed(client_.uname_,"setgid",client_.gid_,errno);
      if (initgroups(client_.uname_,client_.gid_) < 0)
	 return Msg::userinit_failed(client_.uname_,"initgroups",client_.gid_,errno);
      if (setuid(client_.uid_) < 0)
	 return Msg::userinit_failed(client_.uname_,"setuid",client_.uid_,errno);
   }

   // Wechsel in das Arbeitsverzeichnis (als ausführender Benutzer)
   if (chdir(wd_) != 0)
      return Msg::cwd_failed(wd_,errno);

   return 0;
}


////////////////////////////////////////////////////////////////////////////////////////////////////
// Prozeß beendet
////////////////////////////////////////////////////////////////////////////////////////////////////

void SrvJob::handle_exit(pid_t pid, int status)
{
   Message(MSG_2,"[%u.%u] Exit status=0x%x",client_.id_,id_,status);

   // Restliche Daten aus der Pipe lesen (kann nicht blockieren)
   while (idx_ >= 0 && handle_input(fd(),POLLIN | POLLHUP) == 0);

   // Status zurückmelden
   if (WIFEXITED(status))
      client_.iob_.append('T',"%x E %x",cjid_,WEXITSTATUS(status));
   else if (WIFSIGNALED(status))
      client_.iob_.append('T',"%x S %x",cjid_,WTERMSIG(status));
   else 
      client_.iob_.append('T',"%x ? %x",cjid_,status);
   remove(&r_tail);
   --total_active;
}


////////////////////////////////////////////////////////////////////////////////////////////////////
// Das nächste wartende Skript für diesen Benutzer starten
////////////////////////////////////////////////////////////////////////////////////////////////////

bool Client::start_job()
{
   if (w_head_ == 0)
      return false;

   // Skript aus W-Queue entfernen und an R-Queue anhängen
   SrvJob *j = w_head_->remove(&w_tail_);
   j->append(&r_tail);
   ++total_active;

   // Kindprozeß starten
   j->exec();
   return true;
}


////////////////////////////////////////////////////////////////////////////////////////////////////
// Reihum von allen Benutzern neue Jobs starten, bis die maximale Anzahl erreicht ist oder keine
// weiteren Jobs auführbereit sind.
////////////////////////////////////////////////////////////////////////////////////////////////////

void Client::start_jobs()
{
   if (clients == 0) return;
   Client *cl = clients;
   bool loop = false;
   while (total_active < max_active) {
      if (cl->w_head_) {
	 cl->start_job();
	 if (cl->w_head_) loop = true;		// evtl. weiterer Durchlauf
      }
      if ((cl = cl->next_) == clients) {	// eine Runde ist komplett
	 if (!loop) break;
	 loop = false;
      }
   }

   // Startpunkt für den nächsten Aufruf merken, damit alle Benutzer gleich behandelt werden.
   clients = cl;
}


// =================================================================================================


TcpServer::TcpServer(int fd)
{
   add_fd(fd);
}

TcpServer::~TcpServer()
{
}


////////////////////////////////////////////////////////////////////////////////////////////////////
// Eine Verbindung annehmen
////////////////////////////////////////////////////////////////////////////////////////////////////

int TcpServer::handle_input(int fd, int events)
{
   struct sockaddr_in sa;
   socklen_t sa_len = sizeof(sa);
   int conn = accept(fd, (struct sockaddr *) &sa, &sa_len);
   if (conn >= 0) {
      set_close_on_exec(conn);
      new Client(conn,sa);
   }
   return 0;
}


////////////////////////////////////////////////////////////////////////////////////////////////////
// TCP-Socket öffnen und auf Verbindungen warten.
////////////////////////////////////////////////////////////////////////////////////////////////////

bool TcpServer::init()
{
   int fd = socket(AF_INET, SOCK_STREAM, 0);
   if (fd < 0) {
      YUFTL(G20,syscall_failed("socket",0));
      return false;
   }
   int yes = 1;
   set_close_on_exec(fd);
   setsockopt(fd, SOL_SOCKET, SO_REUSEADDR, &yes, sizeof(yes));
   if (bind(fd, (struct sockaddr const *) &my_addr, sizeof(my_addr)) < 0) {
      Message(MSG_W,Msg::addr_unusable(addr2str(my_addr),errno));
      close(fd);
      return false;
   }
   if (listen(fd, 5) < 0)
      YUFTL(G20,syscall_failed("listen",0));
   else
      new TcpServer(fd);
   return true;
}



// =================================================================================================

class ServerCfgReader: public CfgReader
{
public:
   ServerCfgReader(const char *fn, int prio): CfgReader(fn,prio) {}
   bool host(const char *name, struct sockaddr_in const *sa, int max_jobs, int prio,
	 const char *cfg);
};



bool ServerCfgReader::host(const char *name, struct sockaddr_in const *sa, int max_jobs, int prio,
	 const char *cfg)
{
   if (sa != 0 && !strcmp(name, my_hostname())) {	// Server-Zeile mit unserem Namen
      max_active = max_jobs > 0 ? max_jobs : 1;
      my_addr = *sa;
   }
   return true;
}


////////////////////////////////////////////////////////////////////////////////////////////////////
// Konfigurationsdatei lesen
////////////////////////////////////////////////////////////////////////////////////////////////////

static bool load_cfg()
{
   Str cfg_file(yabu_cfg_dir);
   cfg_file.append(CFG_GLOBAL);
   ServerCfgReader(cfg_file,Setting::GLOBALRC).read(false);
   if (exit_code >= 2)
      return false;
   if (max_active == 0) {
      Message(MSG_W,Msg::my_hostname_not_found(cfg_file,my_hostname()));
      return false;
   }
   return true;
}


////////////////////////////////////////////////////////////////////////////////////////////////////
// Ende eines Prozesses verarbeiten.
////////////////////////////////////////////////////////////////////////////////////////////////////

static void handle_exit(pid_t pid, int status)
{
   SrvJob *j = SrvJob::find_pid(pid);
   if (j != 0) {
      j->handle_exit(pid,status);
      delete j;
   }
}




////////////////////////////////////////////////////////////////////////////////////////////////////
// Verarbeitet Optionen auf der Kommandozeile.
////////////////////////////////////////////////////////////////////////////////////////////////////

static bool handle_options(int argc, const char *const *argv)
{
   const int prio = Setting::COMMAND_LINE;
   int i;
   int v = 0;
   for (i = 1; i < argc && *argv[i] == '-'; ++i) {
      for (const char *c = argv[i] + 1; *c; ++c) {
         switch (*c) {
	    case 'g':
               yabu_cfg_dir = argv[++i];
               break;
            case 'q':
               --v;
               break;
            case 'S':
               shell_prog.set(prio, argv[++i]);
               break;
            case 'v':
               ++v;
               break;
            case 'V':
               printf("Yabu version %s\n", YABU_VERSION);
               exit(0);
               break;
            case '?':
               MSG(MSG_0, Msg::srv_usage());
               exit(0);
               break;
            default:
               YUERR(G41,unknown_option(*c));
	       return false;
         }
         if (i >= argc) {
            YUERR(G42,missing_opt_arg(*c));
	    return false;
	 }
      }
   }
   if (i < argc) {
      Message(MSG_W,Msg::unexpected_args());
      return false;
   }
   verbosity.set(prio, v);
   return true;
}


static void to_syslog(unsigned flags, const char *msg)
{
   int prio = LOG_INFO;
   if (MSG_LEVEL(flags) < MSG_W) prio = LOG_ERR;
   else if (MSG_LEVEL(flags) > MSG_0) prio = LOG_DEBUG;
   syslog(prio,"%s",msg);
}


static void intr(int sig)
{
   exit_code = 2;
}


////////////////////////////////////////////////////////////////////////////////////////////////////
/// MCP starten. Return erfolgt nur im Arbeiterprozeß.
////////////////////////////////////////////////////////////////////////////////////////////////////

static void daemon()
{
   if (fork() != 0) exit(0);
   setsid();
   close(0);
   close(1);
   close(2);
   int wt = 1;
   pid_t p = 1;
   while (exit_code < 2) {
      if ((p = fork()) <= 1) return;
      int status;
      sleep(wt);
      time_t start_time = time(0);
      while (exit_code < 2 && wait(&status) != p)
	 ;
      if (wt < 15 && start_time + 10 > time(0))
	 ++wt;
      else
	 wt = 1;
   }
   if (p > 1)
      kill(p,SIGTERM);
   exit(0);
}



static bool init(int argc, char *argv[])
{
   running_as_root = geteuid() == 0;
   set_language();
   if (!handle_options(argc, argv))
      return false;

   sys_setup_sig_handler(intr,intr,intr);
   if (verbosity <= 0) {
      daemon();
      openlog("yabusrv",0,LOG_USER);
      set_message_handler(to_syslog);
   }

   set_language();
   struct stat sb;
   if (stat(yabu_cfg_dir,&sb) != 0 || !S_ISDIR(sb.st_mode))
      Message(MSG_E,Msg::no_dir(yabu_cfg_dir));

   // Statisches Environment setzen
   static_env.set("YABU_HOSTNAME",my_hostname());
   static_env.set("YABU_SYSTEM",my_osname());
   static_env.set("YABU_RELEASE",my_osrelease());
   static_env.set("YABU_MACHINE",my_machine());
   
   if (!load_cfg())
      return false;
   if (!TcpServer::init())
      return false;

   return true;
}


int server_main(int argc, char *argv[])
{
   if (!init(argc, argv))
      return 2;

   Message(MSG_0,Msg::server_running(YABU_VERSION, addr2str(my_addr)));
   while (exit_code < 2) {
      PollObj::poll(10000);
      Client::purge();
      int status;
      pid_t pid;
      while ((pid = waitpid(0,&status,WNOHANG)) != (pid_t) -1 && pid > 1)
	 handle_exit(pid,status);
      Client::start_jobs();
   }
   Message(MSG_0,"yabusrv exit(%d)",exit_code);
   return exit_code;
}

// vim:shiftwidth=3:cindent

