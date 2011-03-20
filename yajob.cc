////////////////////////////////////////////////////////////////////////////////////////////////////
// I, the creator of this work, hereby release it into the public domain. This applies worldwide.
// In case this is not legally possible: I grant anyone the right to use this work for any purpose,
// without any conditions, unless such conditions are required by law.
////////////////////////////////////////////////////////////////////////////////////////////////////

// Ausführung von Kommandos (lokal oder via Yabu-Server)


#include "yabu.h"

#include <limits.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <netinet/in.h>
#include <errno.h>
#include <fcntl.h>
#include <stdlib.h>
#include <string.h>
#include <sys/wait.h>
#include <sys/uio.h>
#include <poll.h>
#include <time.h>
#include <signal.h>
#include <unistd.h>

static IntegerSetting max_output_lines("max_output_lines",-1,INT_MAX,0);
static const unsigned MAX_SERVERS = 64;		// Bis zu 63 Server
static const char TMP_FILE_PREFIX[] = "/tmp/y%";
static unsigned char const EMPTY_MASK[MAX_SERVERS / 8] = {0};	// Nicht ausführbar
static unsigned char const LOCAL_MASK[MAX_SERVERS / 8] = {1,0};	// Nur lokal ausführbar
static char EMPTY[1] = {0};
static StringList exports;			// Zu exportierende Variablen
static bool idle = false;			// (Neue) Jobs sind ausführbar


// Ein ausführbares Skript

struct Script {
   unsigned const id_;
   Target * const tgt_;				// Zugehöriges Ziel oder 0.
   bool const no_exec_;				// Ausführung simulieren
   unsigned char qmask_[MAX_SERVERS / 8];	// Verfügbare Server für dieses Skript
   Str cmds_;					// Kommandos
   unsigned const flags_;
   Str obuf_;					// Ausgaben
   Project * prj_;

   Script(Project *prj, Target *t, char tag, Str const &cmds, unsigned flags);
   ~Script();
   bool init();
   void setup_env();
   static Script *find(unsigned queue);
   bool next_step(bool prev_ok);
   char *chunk() const { return chunk_ ? chunk_ : EMPTY; }
   static bool empty();
   static void clear_queue_mask(unsigned qid);
   char **env() const { return env_.env(); }
   static void cancel_waiting();
   void set_local_env();
   void requeue();
   void cancel();
   static unsigned count_active();

private:
   char const tag_;
   static Script *whead;		// Wartende Skripte ("W-Liste")
   static Script **wtail;
   static Script *ahead;		// Skripte in Bearbeitung ("A-Liste")
   static Script **atail;
   static unsigned count;		// Anzahl aller Skripte
   StringMap env_;			// Zusätzlich zu «static_env»
   const char *host_;			// Ausführender Server
   Script *next_;
   Script **prevp_;
   char *chunk_;			// Aktueller Block - siehe «next_chunk()»
   char *rp_;				// Nächster Block - siehe «next_chunk()»
   char saved_char_;			// Gesichertes Zeichen - siehe «next_chunk()»

   void notify(notify_event_t event);
   bool is_in_list(Script *head);
   bool next_chunk();
   void activate(unsigned qid);
   void unlink();
};

Script *Script::whead = 0;
Script **Script::wtail = &whead;
Script *Script::ahead = 0;
Script **Script::atail = &ahead;
unsigned Script::count = 0;


////////////////////////////////////////////////////////////////////////////////////////////////////
// Ein lokal (d.h. durch yabu) ausgeführtes Skript
////////////////////////////////////////////////////////////////////////////////////////////////////

struct Job: public PollObj {
   Job(Script *script);
   ~Job();
   static bool reap_children(bool blocking);
   static void process_queue(bool wait);
   bool do_next_chunk(bool prev_okay);
   static void start_jobs();
   static void cancel_all();
private:
   Script * const script_;
   Job *next_;
   Job **prevp_;
   pid_t pid_;
   Str file_name_;
   void cleanup();
   void exec(const char *dir, char *chunk);
   void exec_shell(const char *shell, const char *dir, const char *arg1, const char *arg2);
   int handle_input(int fd, int events);
   static Job *find_pid(pid_t pid);
   static unsigned count;
   static Job *head;
   static Job **tail;
};

unsigned Job::count = 0;		// Anzahl aktiver (lokaler) Jobs 
static unsigned max_active = 1;		// Maximale Anzahl aktiver Jobs
Job *Job::head = 0;			// Liste aller Jobs
Job **Job::tail = &head;


// Ein Skript, das durch einen Server ausgführt wird
struct RemoteJob {
   Script *script_;
   RemoteJob *next_;
   RemoteJob **prev_;

   RemoteJob(Script *script, RemoteJob **head);
   ~RemoteJob();
};


// Warteschlange für einen Yabu-Server

struct Server: public PollObj {
   Server(const char *cfg, const char *host, struct sockaddr_in const *addr,
	 unsigned max_jobs, unsigned prio);
   void connect();
   void shutdown(int err);
   static void cancel_connecting();
   static bool select_queues(Project *prj, unsigned char *qmask, const char *cfg);
   static void start_jobs_all();
   static void shutdown_all();
   unsigned const qid_;
   const char * const cfg_;
   const char * const host_;
   const struct sockaddr_in addr_;
   const unsigned max_jobs_;
   const unsigned prio_;
   enum { CREATED, CONNECTING, LOGIN, READY, DEAD } state_;
   IoBuffer iob_;
   bool idle_;			// Nichts zu tun
private:
   unsigned n_jobs_;
   RemoteJob *head_;

   int handle_connect(int fd);
   int handle_input(int fd, int events);
   int handle_output(int fd, int events);
   void start_job();
   RemoteJob *add_job(Script *s);
   RemoteJob *find_job(unsigned jid);
   bool do_next_chunk(RemoteJob *j, bool prev_ok);
};

static Server *servers[MAX_SERVERS];
static size_t n_servers = 1;			// 0 ist die lokale Queue


static const char *local_cfg = "_local";	// Konfiguration für die lokale Warteschlange
static StringMap static_env;			// Globales Environment für alle Skripte
static const char *auth_token = "";






////////////////////////////////////////////////////////////////////////////////////////////////////
// Konstruktor
////////////////////////////////////////////////////////////////////////////////////////////////////

static unsigned last_id = 0;

Script::Script(Project *prj, Target *tgt, char tag, Str const &cmds, unsigned flags)
   :id_(++last_id), tgt_(tgt),
    no_exec_((flags & EXEC_LOCAL) ? false : no_exec), cmds_(cmds), flags_(flags),
    prj_(prj), tag_(tag),
    env_(static_env),
    host_(0), next_(0), prevp_(wtail), chunk_(0), rp_(0),
    saved_char_(0)
{
   memset(qmask_,0,sizeof(qmask_));
   rp_ = cmds_.data();
   *wtail = this;		// An W-Liste anhängen
   wtail = &next_;
   ++count;
   Message(MSG_3,"%s {%u}: CREATED",tgt ? tgt->name_ : "-",id_ );
}


////////////////////////////////////////////////////////////////////////////////////////////////////
// Destruktor
////////////////////////////////////////////////////////////////////////////////////////////////////

Script::~Script()
{
   YABU_ASSERT(count > 0);
   YABU_ASSERT(prevp_ != 0 && *prevp_ == this);
   YABU_ASSERT(next_ == 0 || next_->prevp_ == &next_);
   unlink();
   notify(NOTIFY_FAILED);	// Falls noch nicht ausgeführt
   --count;
}


////////////////////////////////////////////////////////////////////////////////////////////////////
// Bestimmt die verfügbaren Warteschlangen.
////////////////////////////////////////////////////////////////////////////////////////////////////

bool Script::init()
{
   setup_env();
   if (flags_ & EXEC_LOCAL) {
      qmask_[0] = 1;
      idle = false;
   } else if (!Server::select_queues(prj_, qmask_,tgt_ ? tgt_->build_cfg_ : var_current_cfg(prj_->vscope())  )) {
      // Vermeide aufeinanderfolgende Meldungen zur gleichen Konfiguration
      static const char *last_cfg = 0;
      if (tgt_ && tgt_->build_cfg_ && tgt_->build_cfg_ != last_cfg) {
	 last_cfg = tgt_->build_cfg_;
	 Message(MSG_W,Msg::no_queue(tgt_));
      }
      notify(NOTIFY_CANCELLED);		// Besitzer benachrichtigen
      return false;
   }
   return true;
}


////////////////////////////////////////////////////////////////////////////////////////////////////
// Bricht alle wartenden Skripte ab
////////////////////////////////////////////////////////////////////////////////////////////////////

void Script::cancel_waiting()
{
   while (whead) {
      whead->notify(NOTIFY_CANCELLED);
      delete whead;
   }
}


////////////////////////////////////////////////////////////////////////////////////////////////////
// Skript-Environment festlegen. Der Konstruktor initialisert das Environment wird mit «static_env».
// Hier setzen wir weitere Variablen, die konfigurationsabhängig und für jedes Skript verschieden
// sein können.
////////////////////////////////////////////////////////////////////////////////////////////////////

void Script::setup_env()
{
   // Für Build-Skripte: YABU_TARGET und YABU_CONFIGURATION exportieren
   if (tgt_) {
      env_.set("YABU_TARGET",tgt_->name_);
      env_.set("YABU_CONFIGURATION",tgt_->build_cfg_);
   }

   // Weitere Variablen exportieren (!export-Anweisungen)
   for (size_t i = 0; i < exports.size(); ++i) {
      char src[100];
      snprintf(src,sizeof(src),"$(%s)",exports[i]);
      Str s;
      expand_vars(prj_->vscope(),s,src,0,0,0);
      env_.set(exports[i],s);
   }
}


////////////////////////////////////////////////////////////////////////////////////////////////////
// Wird aufgerufen, wenn eine Server-Queue unbenutzbar wird (z.B. Login gescheitert).
// Löscht das zugehörige Bit aus den Masken aller wartenden Skripte. Wird die Maske
// dadurch leer, ziehen wir das Skript aus der W-Liste zurück. Bleibt nur das local-Bit übrig,
// wird das Skript lokal ausführbar.
////////////////////////////////////////////////////////////////////////////////////////////////////

void Script::clear_queue_mask(unsigned qid)
{
   unsigned idx = qid / 8;
   unsigned bit = 1 << (qid % 8);
   Script *s = whead;
   while (s) {
      Script *n = s->next_;
      s->qmask_[idx] &= ~bit;	// Bit löschen
      
      if (!memcmp(s->qmask_,EMPTY_MASK,sizeof(EMPTY_MASK))) {
         s->notify(NOTIFY_CANCELLED); // Keine Warteschlange mehr verfügbar
	 delete s;
      } 
      else if (!memcmp(s->qmask_,LOCAL_MASK,sizeof(LOCAL_MASK)))
	 idle = false;		// Skript ist lokal ausführbar geworden

      s = n;
   }
}


////////////////////////////////////////////////////////////////////////////////////////////////////
// Anzahl der aktiven Scripte
////////////////////////////////////////////////////////////////////////////////////////////////////

unsigned Script::count_active()
{
   unsigned n = 0;
   for (Script const *s = ahead; s; s = s->next_)
      ++n;
   return n;
}


////////////////////////////////////////////////////////////////////////////////////////////////////
// Liefert «true» wenn keine Skripte ausführbereit sind.
////////////////////////////////////////////////////////////////////////////////////////////////////

bool Script::empty()
{
   return Script::whead == 0 && Script::ahead == 0;
}


////////////////////////////////////////////////////////////////////////////////////////////////////
// Prüft, ob das Script in einer Liste enthalten ist
////////////////////////////////////////////////////////////////////////////////////////////////////

bool Script::is_in_list(Script *head)
{
   for (; head; head = head->next_) {
      if (head == this)
	 return true;
   }
   return false;
}


////////////////////////////////////////////////////////////////////////////////////////////////////
// Entfernt das Skript aus der (A- oder W-)Liste
////////////////////////////////////////////////////////////////////////////////////////////////////

void Script::unlink()
{
   // Aus Liste entfernen
   if (next_)
      next_->prevp_ = prevp_;
   if ((*prevp_ = next_) == 0) {
      // «this» war das letzte Element der Liste. D.h., entweder «wtail» oder «atail» zeigt
      // auf «next_» und muß angepaßt werden.
      if (wtail == &next_) wtail = prevp_;
      else if (atail == &next_) atail = prevp_;
      else YABU_ASSERT(false);
   }
}


////////////////////////////////////////////////////////////////////////////////////////////////////
// Verschiebt das Skript aus der W-Liste in die A-Liste.
////////////////////////////////////////////////////////////////////////////////////////////////////

void Script::activate(unsigned qid)
{
   YABU_ASSERT(is_in_list(whead));

   host_ = qid == 0 ? 0 : servers[qid]->host_;

   // Aus aktueller Liste entfernen...
   unlink();

   // ... und an die "aktiv"-Liste anhängen
   *atail = this;
   prevp_ = atail;
   atail = &next_;
   next_ = 0;

   // Ausgabe für den Benutzer
   notify(NOTIFY_STARTED);
}


////////////////////////////////////////////////////////////////////////////////////////////////////
// Sucht das nächste ausführbare Skript für die Warteschlange »queue» und stellt das Skript in die
// A-Liste.
// Die Warteschlange 0 (lokale Ausführung) hat niedrigere Priorität, d.h., ein Skript, das
// mindestens einem Server zugeordnet ist, wird niemals lokal ausgeführt.
////////////////////////////////////////////////////////////////////////////////////////////////////

Script *Script::find(unsigned queue)
{
   YABU_ASSERT(queue < n_servers);

   // Wenn parallel_build=no (bz.w -p) gesetzt ist, Skripte nacheinander ausführen.
   if (!parallel_build && ahead != 0)
      return 0;

   Script *j = 0;
   if (queue == 0) {
      for (j = whead; j && memcmp(LOCAL_MASK,j->qmask_,(n_servers + 7) / 8); j = j->next_);
   } else {
      size_t const idx = queue / 8;
      size_t const bit = 1 << (queue % 8);
      for (j = whead; j && (j->qmask_[idx] & bit) == 0; j = j->next_);
   }
   if (j)
      j->activate(queue);
   return j;
}


////////////////////////////////////////////////////////////////////////////////////////////////////
// Zerlegt das Skript in einzelne ausführbare Blöcke. Ein Block wird durch '{' und '}' (jeweils 
// allein in einer Zeile) gebildet. Die Klammern werden nicht Teil des Blocks. Verschachtelte 
// Klammern werden erkannt, die inneren Klammern haben aber keine Bedeutung. Ohne Klammern zählt
// jede Zeile als eigener Block.
// Beginnt die erste Zeile des Skriptes mit '#!', dann bilden die restlichen Zeilen einen einzigen
// Block.
// return: True: nächster Block bereit. False: Ende des Skripts erreicht.
////////////////////////////////////////////////////////////////////////////////////////////////////

bool Script::next_chunk()
{
   if (saved_char_ > 0 && rp_ != 0) 
      *rp_ = saved_char_;
   if (rp_ == 0 || skip_blank(&rp_) == 0) {
      chunk_ = rp_ = 0;
      return false;
   }

   chunk_ = rp_;
   if (rp_[0] == '#' && rp_[1] == '!') 		// #!... --> Rest ist ein Block
      rp_ = 0;
   else if (*rp_ == '{' && rp_[1] == '\n') {	// Mehrzeiliger Block { ... }
      int level = 1;
      chunk_ = (rp_ += 2);
      while (level > 0 && *rp_) {
         while (*rp_ != 0 && *rp_ != '\n') ++rp_;
         if (rp_[-1] == '{')
            ++level;
         else if (rp_[-1] == '}' && --level == 0)
            rp_[-1] = 0;
         if (*rp_ != 0) ++rp_;                  // Nächste Zeile
      }
      if (level > 0) {
	 YUERR(S02,nomatch("{","}"));
	 chunk_ = rp_ = 0;
	 return false;
      }
   } else if (*rp_ == '|') {			// '|'-Syntax
      chunk_ = ++rp_;
      char *wp = rp_;
      while (true) {
	 while (*rp_ != 0 && *rp_ != '\n') *wp++ = *rp_++;
	 if (*rp_ == '\n') {
	    *wp++ = *rp_++;
	    while (*rp_ == ' ' || *rp_ == '\t') ++rp_;
	    if (*rp_ != '|') {
	       wp[-1] = 0;
	       break;
	    }
	    ++rp_;
	 }
      }
   } else {					// Einzelne Zeile
      while (*rp_ != 0 && *rp_ != '\n')
         ++rp_;
      if (*rp_ != 0)
         ++rp_;
   }
   if (rp_) {
      saved_char_ = *rp_;
      *rp_ = 0;					// Block mit NUL abschließen
   }
   return true;
}


////////////////////////////////////////////////////////////////////////////////////////////////////
// Text ausgeben und auf «max_output_lines» Zeilen beschränken. Nach der Rückkehr ist «o» leer.
////////////////////////////////////////////////////////////////////////////////////////////////////

static void output(Str &o)
{
   struct iovec iov[5];
   int nio = 0;
#define ADDIO(p,l) (iov[nio].iov_base = (char *)(p), iov[nio++].iov_len = (l))

   size_t const len = o.len();
   if (len == 0) return;
   const char *txt = o;
   const char * const end = txt + len;
   if (max_output_lines > 0) {
      // Die ersten «max_output_lines» ausgeben
      int n = 0;
      const char *c = txt;
      while (n < max_output_lines && c < end) {
	 if (*c++ == '\n') ++n;
      }
      ADDIO(txt,c - txt);

      // Falls Text abgeschnitten wurde, Ellipse und die letzten drei Zeilen ausgeben
      if (c < end) {
         if (c[-1] != '\n') ADDIO("\n",1);
	 const char *d = end;
	 int m = (d[-1] == '\n') ? 0 : 1;
	 while (d > c && m < 4) {
	    if (*--d == '\n') ++m;
	 }
	 while (d < end && *d == '\n') ++d;
	 ADDIO("...\n",4);
	 ADDIO(d,end - d);
      }
   } else
      ADDIO(txt,len);		// «max_lines» = 0: alles ausgeben

   if (end[-1] != '\n') ADDIO("\n",1);
   writev(1,iov,nio);
   o.clear();
}

static void echo(const char *script) 
{
   MessageBlock(MSG_ALWAYS + MSG_SCRIPT + MSG_0, "", script);
}


////////////////////////////////////////////////////////////////////////////////////////////////////
// Bereitet die Ausführung des nächsten Blocks vor und sorgt für das Echo vor bzw. nach der
// Ausführung.
// «prev_ok»: Ergebnis der Ausführung des aktuellen Blocks
// «return»: True: nächster Block bereit, False: Ende erreicht
////////////////////////////////////////////////////////////////////////////////////////////////////

bool Script::next_step(bool prev_ok)
{
   if ((flags_ & EXEC_COLLECT_OUTPUT) == 0)
      output(obuf_);

   YabuContext yctx(tgt_ && tgt_->build_rule_ ?tgt_->build_rule_->srcline_ : 0,tgt_,0);

   // Echo nach Fehler im vorigen Block, falls nötig
   if (!prev_ok && !echo_before && echo_after_error)
      echo(chunk_);

   // Nächsten Block holen. Falls «no_exec», Ausführung des gesamten Skriptes simulieren.
   do {
      if (prev_ok && next_chunk() && echo_before)
         echo(chunk_);
   } while (no_exec_ && prev_ok && chunk_ != 0);
   if (prev_ok && chunk_ != 0)
      return true;

   // Fehler oder Ende des Skriptes erreicht.
   notify(prev_ok ? NOTIFY_OK : NOTIFY_FAILED);
   return false;
}


static const char *event_str(notify_event_t event)
{
   switch (event)
   {
       case NOTIFY_STARTED: return "STARTED";
       case NOTIFY_OK: return "SUCCESS";
       case NOTIFY_FAILED: return "FAILED";
       case NOTIFY_CANCELLED: return "CANCELLED";
   }
   return "???";
}

////////////////////////////////////////////////////////////////////////////////////////////////////
// Benachrichtigung bei Start und Ende des Skriptes
////////////////////////////////////////////////////////////////////////////////////////////////////

void Script::notify(notify_event_t event)
{
   if (prj_) {
      // Versuche, Störungen durch NFS-Caching zu vermeiden
      if (event == NOTIFY_OK && host_ && !tgt_->is_alias_)
	 yabu_cot(tgt_->name_);

      Str *o = (event != NOTIFY_STARTED && (flags_ & EXEC_COLLECT_OUTPUT)) ? &obuf_ : 0;
      Message(MSG_3,"%s {%u}: %s",tgt_ ? tgt_->name_ : "-",id_, event_str(event));
      prj_->job_notify(tgt_,tag_,event,host_,o);
      if (event != NOTIFY_STARTED)
          prj_ = 0;			// Keine weiteren Benachrichtigungen
   }
}


////////////////////////////////////////////////////////////////////////////////////////////////////
// Setzt Environmentvariablen für die lokale Ausführung.
////////////////////////////////////////////////////////////////////////////////////////////////////

void Script::set_local_env()
{
   env_.set("YABU_MACHINE",my_machine());
   env_.set("YABU_SYSTEM",my_osname());
   env_.set("YABU_RELEASE",my_osrelease());
   env_.set("YABU_HOSTNAME",my_hostname());
}


void Script::cancel()
{
   if (is_in_list(ahead))
      notify(NOTIFY_FAILED);
   else
      notify(NOTIFY_CANCELLED);
   output(obuf_);
}


////////////////////////////////////////////////////////////////////////////////////////////////////
// Sucht den Job mit einer gegebenen PID
////////////////////////////////////////////////////////////////////////////////////////////////////

Job *Job::find_pid(pid_t pid)
{
   for (Job *j = head; j; j = j->next_) {
      if (j->pid_ == pid)
	 return j;
   }
   return 0;	// Nicht gefunden
}


////////////////////////////////////////////////////////////////////////////////////////////////////
// Wartende Jobs starten, solange «max_active» nicht erreicht ist
////////////////////////////////////////////////////////////////////////////////////////////////////

void Job::start_jobs()
{
   idle = true;
   Script *s;
   while (count < max_active && (s = Script::find(0)) != 0) {
      idle = false;
      Job *j = new Job(s);
      s->set_local_env();
      j->do_next_chunk(true);		// Ausführung starten
   }
}



////////////////////////////////////////////////////////////////////////////////////////////////////
// Der Destruktor entfernt den Job aus der Liste
////////////////////////////////////////////////////////////////////////////////////////////////////

Job::~Job()
{
   cleanup();
   YABU_ASSERT(prevp_ != 0 && *prevp_ == this);
   YABU_ASSERT(next_ == 0 || next_->prevp_ == &next_);
   YABU_ASSERT(count > 0);
   if (next_)
      next_->prevp_ = prevp_;
   if ((*prevp_ = next_) == 0)
      tail = prevp_;
   --count;
   delete script_;
}


////////////////////////////////////////////////////////////////////////////////////////////////////
// Alle Jobs abbrechen
////////////////////////////////////////////////////////////////////////////////////////////////////

void Job::cancel_all()
{
   while (head) {
      if (head->pid_ > 1)
	 kill(head->pid_,SIGKILL);
      if (head->script_)
	 head->script_->cancel();
      delete head;
   }
}



////////////////////////////////////////////////////////////////////////////////////////////////////
// Liest Ausgaben des Prozesses aus der Pipe
// return: True: ok. False: Pipe wurde geschlossen.
////////////////////////////////////////////////////////////////////////////////////////////////////

int Job::handle_input(int fd, int events)
{
   int rc = 0;
   if (events & POLLIN) {
      Str &buf = script_->obuf_;
      buf.reserve(1024);
      rc = yabu_read(fd,buf.end(),buf.avail());
      if (rc > 0)
	 buf.extend(rc);
   }

   if (!(events & POLLIN) || rc <= 0)
      return 1;		// Bewirkt del_fd()
   return 0;
}




////////////////////////////////////////////////////////////////////////////////////////////////////
// Führt das Programm «shell» mit zwei Argumenten aus und erzeugt eine Pipe zum Kindprozeß. Wie
// die Pipe genutzt wird, richtet sich nach «collect_output_» und «merge_stderr_».
// shell: Shellprogramm.
// arg1: Erstes Argument.
// arg2: Zweites Argument oder 0.
////////////////////////////////////////////////////////////////////////////////////////////////////

void Job::exec_shell(const char *shell, const char *dir, const char *arg1, const char *arg2)
{
   int pipefd;
   int flags = script_->flags_;
   if (max_output_lines != 0)
      flags |= EXEC_COLLECT_OUTPUT;
   if (!yabu_fork(&pipefd,&pid_,flags))
      return;
   if (pid_ == 0) {
      if (*dir && chdir(dir) != 0) {
	fprintf(stderr,"chdir(%s): %s\n", dir, strerror(errno));
	_exit(127);
      }
      char *argv[4];
      argv[0] = const_cast<char *>(strrchr(shell, '/'));
      argv[0] = argv[0] ? argv[0] + 1 : const_cast<char *>(shell);
      argv[1] = const_cast<char *>(arg1);
      argv[2] = const_cast<char *>(arg2);
      argv[3] = 0;
      execve(shell, argv, script_->env());
      fprintf(stderr,"%s: %s\n", shell, strerror(errno));
      _exit(127);
   }
   else
      add_fd(pipefd);
}


Job::Job(Script *script)
      : script_(script), next_(0), prevp_(tail),
        pid_(-1)
{
   *tail = this;
   tail = &next_;
   ++count;
}

////////////////////////////////////////////////////////////////////////////////////////////////////
// Startet die Shell, um einen Skriptabschnitt auszuführen. Dabei sind zwei Fälle möglich:
// - Beginnt «chunk» mit "#!", dann wird der Rest der ersten Zeile als Name der Shell
//   interpretiert. Die übrigen Zeilen werden in eine temporäre Datei geschrieben, deren Name
//   die Shell als erstens (und einziges) Argument erhält.
// - Andernfalls wird die Default-Shell «shell_prog» ausgeführt. Sie erhält zwei Argumente,
//   "-c" und «chunk»
// Siehe auch «exec_shell()».
////////////////////////////////////////////////////////////////////////////////////////////////////

void Job::exec(const char *dir, char *chunk)
{
   cleanup();
   if (!strncmp(chunk, "#!", 2)) {			// Im Skript angegebene Shell benutzen
      chunk += 2;
      char *shell = chunk;
      while (*chunk != '\n' && *chunk != 0) ++chunk;
      if (*chunk != 0)
	 *chunk++ = 0;

      // Skript in temporäre Datei schreiben
      static unsigned job_id = 0;
      file_name_.printf("%s%x.%x",TMP_FILE_PREFIX,(unsigned)getpid(),++job_id);
      int fd = open(file_name_, O_WRONLY | O_CREAT | O_TRUNC, 0755);
      if (fd < 0)
	 YUERR(G10,cannot_open(file_name_));
      yabu_write(fd, chunk, strlen(chunk));
      close(fd);
      exec_shell(shell,dir,file_name_, 0);
   }
   else							// Default-Shell benutzen
      exec_shell(shell_prog,dir,"-c",chunk);
}



////////////////////////////////////////////////////////////////////////////////////////////////////
// Aufräumarbeiten vor Ausführung des nächsten Blocks bzw. nach Abschluß des letzten Blocks
////////////////////////////////////////////////////////////////////////////////////////////////////

void Job::cleanup()
{
   del_fd();			// Evtl. noch offene Pipe schließen
   if (!file_name_.empty()) {	// Evtl. vorhandenen Skriptdatei löschen
      unlink(file_name_);
      file_name_.clear();
   }
}



////////////////////////////////////////////////////////////////////////////////////////////////////
// Wertet das Ergebnis des vorigen Blocks aus und startet die Ausführung des nächsten Blocks.
// prev_ok: Ergebnis des vorigen Blocks.
// return: true: nächster Block wird ausgeführt, false: Job ist beendet, Objekt wurde zerstört.
////////////////////////////////////////////////////////////////////////////////////////////////////

bool Job::do_next_chunk(bool prev_ok)
{
   if (script_->next_step(prev_ok))
      exec(script_->prj_->aroot_,script_->chunk());
   else {
      // Fehler oder Job ist beendet
      idle = false;
      delete this;
      return false;
   }
   return true;
}



////////////////////////////////////////////////////////////////////////////////////////////////////
// Verarbeitet beendete Kindprozesse.
// return: True: Mindestens ein Job wurde abgeschlossen. False: kein Job abgeschlossen.
////////////////////////////////////////////////////////////////////////////////////////////////////

bool Job::reap_children(bool blocking)
{
   bool job_finished = false;
   // Beendete Kindprozesse verarbeiten
   int status;
   pid_t pid;
   while ((pid = waitpid(-1,&status,blocking ? 0 : WNOHANG)) > 0) {
      blocking = false;
      Job *j = find_pid(pid);
      if (j == 0)
	 continue;	// Unbekannter Prozeß
      j->pid_ = (pid_t) -1;

      // Eventuell noch einmal Daten aus der Pipe lesen
      int fd = j->fd();
      if (fd >= 0)
	 while (j->handle_input(fd,POLLIN | POLLHUP) == 0);
      if (WIFSIGNALED(status))
           MSG(MSG_W,Msg::script_terminated_on_signal(WTERMSIG(status)));
      if (!j->do_next_chunk(WIFEXITED(status) && WEXITSTATUS(status) == 0))
	 job_finished = true;
   }
   return job_finished;
}


void job_process_queue(bool wait)
{
   Job::process_queue(wait);
}


////////////////////////////////////////////////////////////////////////////////////////////////////
// Bis zu 10s auf das Ende laufender Jobs warten
////////////////////////////////////////////////////////////////////////////////////////////////////

void job_shutdown()
{
   Server::cancel_connecting();
   Script::cancel_waiting();
   time_t msgtime = time(0) + 2;
   time_t exptime = msgtime + 8;
   while (Script::count_active() > 0) {
      time_t now = time(0);
      if (now >= exptime)
	 break;
      if (now >= msgtime) {
	 MSG(MSG_0,Msg::waiting_for_jobs(Script::count_active()));
	 msgtime = now + 3;
      }
      PollObj::poll(100);
      if (Job::reap_children(false))
	 msgtime = now;
   };
   Server::shutdown_all();
   Job::cancel_all();
}


////////////////////////////////////////////////////////////////////////////////////////////////////
// Ausgaben laufender Jobs verarbeiten und neue Jobs starten.
// wait: Warten bis ein Job beendet ist.
////////////////////////////////////////////////////////////////////////////////////////////////////

void Job::process_queue(bool wait)
{
   do {
      if (exit_code <= 1) {
	 if (!idle) start_jobs();
	 Server::start_jobs_all();
      } else
	 wait = false;

      // Warte auf Input, falls nicht inzwischen ein SIGCHLD aufgetreten ist
      bool poll_ok = PollObj::poll((got_sigchld || !wait) ? 0 : 2000);
      got_sigchld = false;

      // Beendete Kindprozesse verarbeiten
      reap_children(wait && !poll_ok);
   } while (wait && !Script::empty());
}



////////////////////////////////////////////////////////////////////////////////////////////////////
// Gibt «true» zurück, wenn alle Warteschlangen leer sind.
////////////////////////////////////////////////////////////////////////////////////////////////////

bool job_queue_empty()
{
   return Script::empty();
}





////////////////////////////////////////////////////////////////////////////////////////////////////
// Der Konstruktor fügt des neue Objekt als erstes Element in die Liste «head» ein.
////////////////////////////////////////////////////////////////////////////////////////////////////

RemoteJob::RemoteJob(Script *script, RemoteJob **head)
   :script_(script), next_(*head), prev_(head)
{
   Message(MSG_2,"RemoteJob [%u]",script_->id_);
   *head = this;
   if (next_) next_->prev_ = &next_;
}


RemoteJob::~RemoteJob()
{
   *prev_ = next_;
   if (next_) next_->prev_ = prev_;
   delete script_;
}



////////////////////////////////////////////////////////////////////////////////////////////////////

Server::Server(const char *cfg, const char *host,
      struct sockaddr_in const *addr, unsigned max_jobs, unsigned prio)
   :qid_(n_servers), cfg_(str_freeze(cfg)), host_(str_freeze(host)), addr_(*addr),
    max_jobs_(max_jobs), prio_(prio), state_(CREATED), iob_(*this),idle_(false),
    n_jobs_(0), head_(0)
{
   YABU_ASSERT(n_servers < MAX_SERVERS);
   servers[n_servers++] = this;
}


////////////////////////////////////////////////////////////////////////////////////////////////////
// Verbindung zum Server herstellen. Status nach Rückkehr ist CONNECTING (Aufbau läuft), LOGIN
// (erfolgreich), oder DEAD (Fehler).
////////////////////////////////////////////////////////////////////////////////////////////////////

void Server::connect()
{
   YABU_ASSERT(state_ == CREATED);
   int fd = socket(AF_INET, SOCK_STREAM, 0);
   add_fd(fd);
   fcntl(fd,F_SETFL,fcntl(fd,F_GETFL) | O_NONBLOCK);
   int rc = ::connect(fd,(struct sockaddr *)&addr_,sizeof(addr_));
   if (rc == 0) 				// «connect()» war sofort erfolgreich
      handle_connect(fd);
   else if (errno == EINPROGRESS) {		// «connect()» läuft
      state_ = CONNECTING;
      set_events(POLLOUT);
   } else					// Fehler
      shutdown(errno);
}


////////////////////////////////////////////////////////////////////////////////////////////////////
// Alle laufenden Verbindungsversuche abbrechen. Wird bei Fehler oder SIGINT benutzt; das Warten
// auf den TCP-Timeout könnte sehr  dauern.
////////////////////////////////////////////////////////////////////////////////////////////////////

void Server::cancel_connecting()
{
   for (unsigned i = 1; i < n_servers; ++i) {
      if (servers[i]->state_ == CONNECTING)
	 servers[i]->shutdown(0);
   }
}


////////////////////////////////////////////////////////////////////////////////////////////////////
// Legt fest, welche Warteschlangen in der Konfiguration «cfg» benutzbar sind. Nach der
// Rückkehr sind für alle zulässigen Warteschlangen die zugehörigen Bits in «qmask» auf 1 und
// alle anderen Bits auf 0 gesetzt. Die Reihenfolge der Bits entspricht der in «servers»:
// (qmask[0] & 0x01)  gehört zu servers[0], (qmask[0] & 0x80) gehört zu servers[7] und
// (qmask[1] & 0x01) zu servers[8].
// Der Returnwert ist «true», wenn mindestens eine Warteschlange verfügbar ist.
////////////////////////////////////////////////////////////////////////////////////////////////////

bool Server::select_queues(Project *prj, unsigned char *qmask, const char *cfg)
{
   bool success = false;

   // Server-Warteschlangen zuerst!
   for (size_t i = 1; i < n_servers; ++i) {
      if (servers[i]->state_ != DEAD && var_cfg_compatible(prj->vscope(),servers[i]->cfg_,cfg)) {
	 servers[i]->idle_ = false;
	 success = true;
	 qmask[i / 8] |= 1 << (i % 8);
	 if (servers[i]->state_ == CREATED)
	    servers[i]->connect();
      }
   }

   // Lokale Warteschlange.
   if (var_cfg_compatible(prj->vscope(),local_cfg,cfg)) {
      qmask[0] |= 1;
      if (!success) {	
	 success = true;
	 idle = false;		// Kein Server verfügbar --> sofort lokal ausführen
      }
   }
   return success;
}


////////////////////////////////////////////////////////////////////////////////////////////////////
// Schickt den nächsten Block von Kommandos zum Server. Löscht den RemoteJob, wenn alle Kommandos
// abgearbeitet sind.
////////////////////////////////////////////////////////////////////////////////////////////////////

bool Server::do_next_chunk(RemoteJob *j, bool prev_ok)
{
   Script &s = *j->script_;
   if (!s.next_step(prev_ok)) {					// Fehler oder Job ist beendet
      YABU_ASSERT(n_jobs_ > 0);
      --n_jobs_;
      idle_ = false;
      delete j;
      return false;
   }

   iob_.append('I',"%x",j->script_->id_);
   const char *root = j->script_->prj_->aroot_;
   iob_.append('D',"%s%s%s",yabu_wd, *root ? "/" : "",root);	// Verzeichnis
   iob_.append('C',"%s",j->script_->chunk());
   return true;
}



void Server::shutdown_all()
{
   for (unsigned i = 1; i < n_servers; ++i)
      servers[i]->shutdown(0);
}




////////////////////////////////////////////////////////////////////////////////////////////////////
// Neue Jobs auf allen verfügbaren Servern starten, soweit möglich.
// TODO: Warteschlangen gemäß «max_active» und «prio» priorisieren
////////////////////////////////////////////////////////////////////////////////////////////////////

void Server::start_jobs_all()
{
   for (bool idle = false; !idle; ) {
      idle = true;
      for (unsigned i = 1; i < n_servers; ++i) {
	 servers[i]->start_job();
	 idle &= servers[i]->idle_;
      }
   }
}


////////////////////////////////////////////////////////////////////////////////////////////////////
// Neue Jobs starten, falls möglich
////////////////////////////////////////////////////////////////////////////////////////////////////

void Server::start_job()
{
   Script *script = 0;
   if (idle_ || state_ != READY || n_jobs_ >= max_jobs_ || (script = Script::find(qid_)) == 0) {
      idle_ = true;
   } else {
      ++n_jobs_;
      new RemoteJob(script,&head_);
      for (char **ep = script->env(); *ep;  ++ep)
	 iob_.append('E',"%s",*ep);
      if (auto_mkdir && !script->tgt_->is_alias_) {
	 const char *root = script->tgt_->prj_->aroot_;
	 iob_.append('M',"%s%s%s%s",yabu_wd,*root ? "/" : "",root,script->tgt_->name_);
      }
      do_next_chunk(head_,true);
   }
}


////////////////////////////////////////////////////////////////////////////////////////////////////
// Verbindung nach «connect()» initialisieren
////////////////////////////////////////////////////////////////////////////////////////////////////

int Server::handle_connect(int fd)
{
   // Ergebnis von «connect()» holen und auswerten (der Socket ist nicht-blockierend)
   int error = -1;
   socklen_t optlen = sizeof(error);
   getsockopt(fd,SOL_SOCKET,SO_ERROR,&error,&optlen);
   if (error != 0) {
      shutdown(error);
      return 1;
   }
   fcntl(fd,F_SETFL,fcntl(fd,F_GETFL) & ~O_NONBLOCK);		// Wieder blockierend machen

   // Login
   state_ = LOGIN;
   iob_.append('U',"%u %s",(unsigned)getuid(),auth_token);	// Login
   return 0;
}


////////////////////////////////////////////////////////////////////////////////////////////////////
// Ausgabefunktion für die TCP-Verbindung zum Server. Wird ausgeführt, wenn «poll()» den Socket
// zum Schreiben freigibt (POLLOUT).
////////////////////////////////////////////////////////////////////////////////////////////////////

int Server::handle_output(int fd, int events)
{
   if (state_ == CONNECTING)
      return handle_connect(fd);	// connect() is abgeschlossen
   iob_.write(fd);
   return 0;
}


////////////////////////////////////////////////////////////////////////////////////////////////////
// Sucht einen Job mit gegebener Id
////////////////////////////////////////////////////////////////////////////////////////////////////

RemoteJob *Server::find_job(unsigned jid)
{
   for (RemoteJob *j = head_; j; j = j->next_) {
      if (j->script_->id_ == jid)
	 return j;
   }
   return 0;
}


////////////////////////////////////////////////////////////////////////////////////////////////////
// Verbindung zu einem Server schließen, wartende Skripte auf andere Server umleiten.
////////////////////////////////////////////////////////////////////////////////////////////////////

void Server::shutdown(int err)
{
   if (state_ != DEAD) {
      state_ = DEAD;
      del_fd();
      if (err != 0)
	 Message(MSG_W,Msg::server_unavailable(host_,err));

      // Aktive Jobs als gescheitert melden
      while (head_) {
	 head_->script_->cancel();
	 delete head_;
      }

      Script::clear_queue_mask(qid_);
   }
}


////////////////////////////////////////////////////////////////////////////////////////////////////
// Daten von einem Server empfangen und auswerten
////////////////////////////////////////////////////////////////////////////////////////////////////

int Server::handle_input(int fd, int events)
{
    if (state_ == CONNECTING)			// Spezialfall: connect() fertig
       return handle_connect(fd);
    const int rc = iob_.read(fd);
    while (iob_.next()) {
       unsigned jid = 0;
       char how = '?';
       unsigned code = 0;
       if (iob_.tag_ == 'U') {
	  if (*iob_.data_ == '+') {
	     if (state_ == LOGIN) 
		state_ = READY;
	     idle_ = false;
	  } else {
	     Message(MSG_W,Msg::login_failed(host_));
	     shutdown(0);
	  }
       } else if (iob_.tag_ == 'T' && sscanf(iob_.data_,"%x %c %x",&jid,&how,&code) == 3) {
	  RemoteJob *j = find_job(jid);
	  YABU_ASSERT(j != 0);
	  if (j != 0)
	     do_next_chunk(j,how == 'E' && code == 0);
       } else if (iob_.tag_ == 'O' && iob_.len_ > 8) {
	  sscanf(iob_.data_,"%8x",&jid);
	  RemoteJob *j = find_job(jid);
	  if (j && max_output_lines != 0)
	     j->script_->obuf_.append(iob_.data_+8,iob_.len_-8);	// Ausgabe puffern
	  else
	      write(1,iob_.data_+8,iob_.len_-8);			// Sofort ausgeben
       }
       else 
	  printf("??? %c %s\n",iob_.tag_,iob_.data_);
    }

    if ((events & (POLLHUP | POLLERR)) || rc <= 0) {
       shutdown(ECONNRESET);
       return 1;		// Bewirkt del_fd()
    }
    return 0;
}


////////////////////////////////////////////////////////////////////////////////////////////////////
// Wird beim Einlesen der Konfigurationsdatei für jede host-Zeile aufgerufen. Hier erzeugen wir
// die Server-Warteschlangen und legen die Konfiguration für die lokale Warteschlange fest.
////////////////////////////////////////////////////////////////////////////////////////////////////

bool job_add_host(const char *name, struct sockaddr_in const *sa,
      int max_jobs, int prio, const char *cfg)
{
   if (sa && use_server) {				// Es ist ein Server
      Str s(cfg);
      s.append("-_local");
      new Server(s,name,sa,max_jobs,prio);
      if (*auth_token == 0 && *yabu_cfg_dir != 0)	// Anmelde-Token erzeugen
	 auth_token = make_auth_token(yabu_cfg_dir);
   }

   if (!strcmp(name,my_hostname())) {			// Lokale Warteschlange
      Str s(cfg);
      s.append("+_local");
      local_cfg = str_freeze(s);
      max_active = use_server ? max_jobs : 1;
   }

   return true;
}


////////////////////////////////////////////////////////////////////////////////////////////////////
// Globale Initialisierung
////////////////////////////////////////////////////////////////////////////////////////////////////

void job_init()
{
   job_export_env("USER");
   job_export_env("LOGNAME");
   job_export_env("TERM");
   job_export("YABU_CHOST",my_hostname());
   if (*yabu_cfg_dir != 0)
      job_export("YABU_CFG_DIR",yabu_cfg_dir);
   job_export("PATH","/usr/local/bin:/usr/bin:/bin");
}


////////////////////////////////////////////////////////////////////////////////////////////////////
// Definiert eine (feste) Variable im Skript-Environment
////////////////////////////////////////////////////////////////////////////////////////////////////

void job_export(const char *name, const char *value)
{
   static_env.set(name,value);
}


////////////////////////////////////////////////////////////////////////////////////////////////////
// Exportiert eine Yabu-Variable in das Skript-Environment. Die Auswertung der Variablen erfolgt
// erst beim Erzeugen des Skriptes, da der Wert im allgemeinen konfigurationsabhängig ist.
////////////////////////////////////////////////////////////////////////////////////////////////////

void job_export_var(const char *name)
{
   if (!exports.find(0,name))
      exports.append(name);
}


////////////////////////////////////////////////////////////////////////////////////////////////////
// Exportiert eine Variable aus dem Environment in das Skript-Environment.
////////////////////////////////////////////////////////////////////////////////////////////////////

void job_export_env(const char *name)
{
   char const *value = getenv(name);
   if (value)
      static_env.set(name,value);
}


////////////////////////////////////////////////////////////////////////////////////////////////////
// Erzeugt einen neuen Job und stellt ihn in die Warteschlange. Die Ausführung beginnt erst in
// «Job::process_queue()».
// t: Ziel, für das der Job ausgeführt werden soll, oder 0
// cmds: Skript
// collect_output: Ausgabe sammeln und an Notify-Funktion übergeben
// merge_stderr: stderr auf stdout umleiten
// notify: Wird nach Beendigung des Jobs aufgerufen.
////////////////////////////////////////////////////////////////////////////////////////////////////

void job_create(Project *prj, Target *t, char tag, Str const &cmds, unsigned flags)
{
   Script *s = new Script(prj, t, tag, cmds,flags);
   if (!s->init())
      delete s;
}



////////////////////////////////////////////////////////////////////////////////////////////////////
// Liefert die lokale Konfiguration aus yabu.cfg (Default ist "").
////////////////////////////////////////////////////////////////////////////////////////////////////

const char *job_local_cfg()
{
   return local_cfg;
}

// vim:sw=3 cin fileencoding=utf-8



class TokenQueue: public PollObj
{
public:
   static const unsigned MAX_SIZE;
   TokenQueue(unsigned size);
   bool get_token();
   bool return_token();
private:
   int rfd_;
   int wfd_;
   unsigned n_;
};

const unsigned TokenQueue::MAX_SIZE = 1024;

TokenQueue::TokenQueue(unsigned size)
   :rfd_(-1), wfd_(-1), n_(0)
{
   static const char VAR_NAME[] = "YABU_JOB_CONTROL_PIPE";
   if (const char *env = getenv(VAR_NAME)) {
      int rfd, wfd;
      if (sscanf(env,"%d,%d",&rfd,&wfd) == 2 ) {
	 rfd_ = rfd;
	 wfd_ = wfd;
      }
   }

   int pfd[2];
   pipe(pfd);
   rfd_ = pfd[0];
   wfd_ = pfd[1];
   if (size > MAX_SIZE) size = MAX_SIZE;
   unsigned char dummy[MAX_SIZE];
   memset(dummy,rfd_ & 0xFF,size);
   write(wfd_,dummy,size);
   static char env_var[50];
   snprintf(env_var,sizeof(env_var),"%s=%d,%d",VAR_NAME,rfd_,wfd_);
   putenv(env_var);
}


bool TokenQueue::get_token()
{
   unsigned char dummy;
   int rc = read(rfd_,&dummy,1);
   if (rc != 1 || dummy != (rfd_ & 0xFF))
      return false;
   ++n_;
   return true;
}

bool TokenQueue::return_token()
{
   if (n_ == 0)
      return false;
   unsigned char const dummy = rfd_ & 0xFF;
   int rc;
   do {
       rc = write(rfd_,&dummy,1);
   } while (rc == -1 && errno == EINTR);
   if (rc != 1)
      return false;
   --n_;
   return true;
}


// vim:sw=3 cin fileencoding=utf-8

