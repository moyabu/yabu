////////////////////////////////////////////////////////////////////////////////////////////////////
// I, the creator of this work, hereby release it into the public domain. This applies worldwide.
// In case this is not legally possible: I grant anyone the right to use this work for any purpose,
// without any conditions, unless such conditions are required by law.
////////////////////////////////////////////////////////////////////////////////////////////////////

#include "yabu.h"

#include <errno.h>
#include <time.h>
#include <unistd.h>
#include <signal.h>
#include <sys/stat.h>


volatile int exit_code = -1;			// Exit-Code von Yabu
char yabu_wd[1024] = "/";			// Arbeitsverzeichnis
const time_t yabu_start_time = time(0);
static const char *dump_options = "";		// Argument von -D
TsAlgo_t global_ts_algo = TSA_DEFAULT;		// Vergleichsalgorithmus (-y)

// ~/.yaburc auswerten (-r).
static BooleanSetting use_yaburc("use_yaburc", true);

// Globales Konfigurationsverzeichnis (-g)
const char *yabu_cfg_dir = "";

// Name der Beschreibungsdatei (-f).
static const char *buildfile = "Buildfile";

// Statusdatei (Buildfile.state) benutzen (/-s)
BooleanSetting use_state_file("use_state_file", true);

// Alle Kommandos ausgeben (-e).
BooleanSetting echo_before("echo", false);

// Fehlgeschlagene Kommandos ausgeben
BooleanSetting echo_after_error("echo_after_error", true);

// Fehlende Verzeichnisse automatisch erstellen (einschalten mit -m).
BooleanSetting auto_mkdir("auto_mkdir", false);

// Server benutzen (-j/-J).
BooleanSetting use_server("use_server", true);

// Skripte parallel ausführen (-p/-P)
BooleanSetting parallel_build("parallel_build", true);

// Autodepend-Skripte benutzen (abschaltbar mit -a).
BooleanSetting use_auto_depend("auto_dependencies", true);

static const char **targets = 0;	// Ziele auf der Kommandozeile
static size_t num_targets = 0;		// Anzahl der Argumente in targets
int no_exec = 0;			// Keine Kommandos ausführen (-n).

StringSetting shell_prog("shell", "/bin/sh"); // Zur Ausführung von Skripten (-S).


// Auf der Kommandozeile angegebene Konfiguration (-c).
const char *user_options = 0;


////////////////////////////////////////////////////////////////////////////////////////////////////
// Verarbeitet Kommandozeilenoptionen, beginnend mit «argv[0]». Rückgabewert ist der Index des
// ersten "echten" Argumentes oder gleich «argc» wenn «argv» nur Optionen enthält.
////////////////////////////////////////////////////////////////////////////////////////////////////

static unsigned handle_options(int argc, const char *const *argv)
{
   const int prio = Setting::COMMAND_LINE;
   int i;
   const char *tsalgo = 0;
   int v = 0;
   for (i = 0; i < argc && *argv[i] == '-'; ++i) {
      for (const char *c = argv[i] + 1; *c; ++c) {
         switch (*c) {
            case 'a': use_auto_depend.set(prio, false); break;
            case 'A': use_auto_depend.set(prio, true); break;
            case 'c': user_options = argv[++i]; break;
            case 'D': dump_options = argv[++i]; break;
            case 'e': echo_before.set(prio, true); break;
            case 'E': echo_before.set(prio, false); break;
            case 'f': buildfile = argv[++i]; break;
	    case 'g': yabu_cfg_dir = argv[++i]; break;
	    case 'j': use_server.set(prio, false); break;
	    case 'J': use_server.set(prio, true); break;
            case 'k': max_warnings.set(prio, 0); break;
            case 'K': max_warnings.set(prio, -1); break;
            case 'm': auto_mkdir.set(prio, true); break;
            case 'M': auto_mkdir.set(prio, false); break;
            case 'n': ++no_exec; echo_before.set(prio - 1, "true"); break;
	    case 'p': parallel_build.set(prio, false); break;
	    case 'P': parallel_build.set(prio, true); break;
            case 'q': --v; break;
            case 'r': use_yaburc.set(prio, false); break;
            case 'R': use_yaburc.set(prio, true); break;
            case 's': use_state_file.set(prio, false); break;
            case 'S': shell_prog.set(prio, argv[++i]); break;
            case 'v': ++v; break;
            case 'V': printf("Yabu version %s\n", YABU_VERSION); exit(0); break;
            case 'y': tsalgo = argv[++i]; break;
            case '?': MSG(MSG_0, Msg::usage()); exit(0); break;
            default: YUERR(G41,unknown_option(*c));
         }
         if (i >= argc)
            YUERR(G42,missing_opt_arg(*c));
      }
   }
   if (v != 0) verbosity.set(prio, v);
   if (verbosity > 3)
      echo_before.set(prio,true);	// -vvv impliziert -e

   if (tsalgo) {
      if (!strcmp(tsalgo, "cksum"))
         global_ts_algo = TSA_CKSUM;
      else if (!strcmp(tsalgo, "mt"))
         global_ts_algo = TSA_MTIME;
      else if (!strcmp(tsalgo, "mtid"))
         global_ts_algo = TSA_MTIME_ID;
      else
	YUERR(G43,bad_ts_algo(tsalgo));
   }
   return i;
}




class ClientCfgReader: public CfgReader
{
public:
   ClientCfgReader(const char *fn, int prio) :CfgReader(fn,prio) {}
   bool host(const char *name, struct sockaddr_in const *sa, int max_jobs, int prio,
	 const char *cfg) {
      return job_add_host(name,sa,max_jobs,prio,cfg);
   }
};


////////////////////////////////////////////////////////////////////////////////////////////////////
// Kommandozeile und ~/.yabrc verarbeiten.
// argc: Anzahl der Argumente.
// argv: Argumente.
////////////////////////////////////////////////////////////////////////////////////////////////////

static void parse_args(int argc, char *argv[])
{
   if ((yabu_cfg_dir = getenv("YABU_CFG_DIR")) == 0)
      yabu_cfg_dir = "";
   int i = handle_options(argc - 1, argv + 1) + 1;
   num_targets = argc - i;
   targets = (const char **) argv + i;

   // Globale Konfigurationsdatei lesen
   // Vor .yaburc, damit man global use_yaburc=false setzen kann.
   if (*yabu_cfg_dir != 0) {
      struct stat sb;
      if (!yabu_stat(yabu_cfg_dir,&sb) || !S_ISDIR(sb.st_mode)) {
	 Message(MSG_E,Msg::no_dir(yabu_cfg_dir));
	 return;
      }
      Str dir(yabu_cfg_dir);
      ClientCfgReader(dir.append(CFG_GLOBAL),Setting::GLOBALRC).read(false);
   }

   // ~/.yaburc lesen
   char const *home = getenv("HOME");
   if (exit_code <= 1 && use_yaburc && home) {
      Str dir(home);
      ClientCfgReader(dir.append("/.yaburc"),Setting::RCFILE).read(true);
   }
}




bool dump_req(char c)
{
   return strchr(dump_options,c) || strchr(dump_options,'A');
}





static void intr(int sig)
{
   exit_code = 2;
   if (sig == SIGINT)
      puts("*** INTR ***");
}

////////////////////////////////////////////////////////////////////////////////////////////////////
// main() für yabu
////////////////////////////////////////////////////////////////////////////////////////////////////

static int yabu_main(int argc, char *argv[])
{
   sys_setup_sig_handler(intr,intr,intr);
   set_language();
   if (getcwd(yabu_wd,sizeof(yabu_wd)) == 0)
      YUFTL(G20,syscall_failed("getcwd",0));
   parse_args(argc, argv);
   set_language();               // Sprache wurde eventuell in .yaburc geändert
   if (dump_req('p')) Setting::dump();
   Project *main_prj = new Project(0,"","",buildfile,0);
   var_set_system_variables();

   if (exit_code <= 1) {
      job_init();
   }

   // Primäre Ziele auswählen
   if (exit_code <= 1) {
      if (num_targets <= 0)
	 main_prj->select_tgt("all", 0);
      else {
	 for (size_t i = 0; exit_code <= 1 && i < num_targets; ++i)
	    main_prj->select_tgt(targets[i], 0);
      }
   }

   // Hauptschleife
   while (exit_code < 2 && !job_queue_empty())
      job_process_queue(true);
   job_shutdown();

   // Alle noch ausgewählten Ziele abbrechen. Das ist "normal", wenn Phase 2 wegen
   // eines Fehlers vorzeitig beendet wurde. Wenn aber kein Fehler vorliegt, dann
   // sollten hier keine Ziele mehr übrig sein.
   Project::cancel_all(exit_code < 1 ? MSG_E : MSG_1);

   if (dump_req('t')) main_prj->dump_tgts();
   if (exit_code <= 1 && use_state_file && !no_exec)
      main_prj->state_file_write();
   if (exit_code <= 1)
      exit_code = (Target::total_failed > 0 || Target::total_cancelled > 0) ? 1 : 0;
   Target::statistics();
   return exit_code;
}


////////////////////////////////////////////////////////////////////////////////////////////////////
// main()
////////////////////////////////////////////////////////////////////////////////////////////////////

int main(int argc, char *argv[])
{
    char *c = strrchr(argv[0],'/');
    if (c) ++c; else c = argv[0];
    if (!strcmp(c,"yabusrv"))
	return server_main(argc,argv);
    else
	return yabu_main(argc,argv);
}


// vim:sw=3 cin fileencoding=utf-8
