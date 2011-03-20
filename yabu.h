////////////////////////////////////////////////////////////////////////////////////////////////////
// I, the creator of this work, hereby release it into the public domain. This applies worldwide.
// In case this is not legally possible: I grant anyone the right to use this work for any purpose,
// without any conditions, unless such conditions are required by law.
////////////////////////////////////////////////////////////////////////////////////////////////////

#ifndef _YABU_H_
#define  _YABU_H_

// Globale Funktionen und Definitionen

#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdarg.h>
#include <sys/types.h>

#if defined(__GNUC__)
#define ATTR_PRINTF(fmt,va) __attribute__((format(printf,fmt,va)))
#else
#define ATTR_PRINTF(fmt,va)
#endif

#ifndef YABU_VERSION
#define YABU_VERSION "0.00"
#endif

#define CFG_GLOBAL "/yabu.cfg"


struct Variable;
struct Rule;
class Project;
class VarScope;

enum TsAlgo_t		// Vergleichsalgorithmus
{
   TSA_DEFAULT,         // Vorgabe aus Buildfile.state oder MTIME benutzen
   TSA_MTIME,           // "klassische" Zeitstempel (wie Make)
   TSA_MTIME_ID,        // Zeitstempel wie Prüfsumme behandeln
   TSA_CKSUM            // Prüfsummen
};


enum VarMode_t 		// Zuweisungsmodus
{
  SET,                  // Normale Zuweisung (=)
  APPEND,               // Anhängen (+=)
  MERGE                 // Anhängen wenn noch nicht enthalten (?=)
};


struct SrcLine	       // Eine Quellzeile
{
  const char *file;    // Dateiname.
  unsigned line;       // Zeilennummer.
  char const *text;    // Inhalt der Zeile.
  SrcLine const *up;
};

struct Ftime {		// Änderungszeit einer Datei
   unsigned s_, ns_;
   Ftime() :s_(0), ns_(0) {}
   Ftime &operator =(unsigned rhs) { s_ = rhs; ns_ = 0; return *this; }
   friend bool operator==(Ftime const &a, Ftime const &b) { return a.s_ == b.s_ && a.ns_ == b.ns_; }
   friend bool operator!=(Ftime const &a, Ftime const &b) { return !(a == b); }
   friend bool operator<(Ftime const &a, Ftime const &b) {
      return a.s_ < b.s_ || (a.s_ == b.s_ && a.ns_ < b.ns_); }
   friend bool operator>(Ftime const &a, Ftime const &b) {
      return a.s_ > b.s_ || (a.s_ == b.s_ && a.ns_ > b.ns_); }

   friend bool operator==(Ftime const &a, unsigned b) { return a.s_ == b && a.ns_ == 0; }
   friend bool operator!=(Ftime const &a, unsigned b) { return a.s_ != b || a.ns_ != 0; }
   friend bool operator<(Ftime const &a, unsigned b) { return a.s_ < b && a.ns_ == 0; }
   friend bool operator>=(Ftime const &a, unsigned b) { return a.s_ >= b || a.ns_ != 0; }
};

// ===== yamap.cc ==================================================================================

class StringMap {
public:
   StringMap();
   StringMap(const StringMap &src);
   ~StringMap();
   StringMap &operator=(const StringMap &rhs);
   void clear();
   void swap(StringMap &x);
   bool set(const char *key, const char *val);
   bool set(const char *key_val);
   char **env() const;
   const char *operator[] (const char *key) const;
   bool contains(const char *key) const;
   unsigned size() const { return n_ - 1; }
private:
   size_t n_;		// Größe von «buf_» (= Anzahl der Variablen + 1)
   char **buf_;		// «n_-1» Einträge und eine 0 zum Abschluß
   bool set(const char *key, size_t key_len, const char *val);
};


// ===== yastr.cc =================================================================================

extern unsigned char yabu_cclass[256];
#define YABU_CCLASS(c,x) (yabu_cclass[(unsigned char)(c)] & (x))
#define IS_IDENT(c)  (YABU_CCLASS((c),0x80))
#define IS_DIGIT(c)  (YABU_CCLASS((c),0x40))
#define IS_SPACE(c)  (YABU_CCLASS((c),0x20))
#define IS_XDIGIT(c) (YABU_CCLASS((c),0x10))


extern const char DDIG[10][2];

class Str {
public:
    // Konstruktor/Destruktor
    ~Str();
    Str();
    Str(const char *src);
    Str(const char *src, size_t len);
    Str(Str const &src);

    // Nicht-modifizierend
    size_t len() const { return ((size_t*)s_)[-1]; }
    size_t capacity() const { return ((size_t*)s_)[-2]; }
    size_t avail() const { return capacity() - len(); }
    bool empty() const { return len() == 0; }
    operator const char *() const { return s_; }
    char last() const { return len() == 0 ? 0 : s_[len() - 1]; }
    char *end() { return s_ + len(); }

    // Modifizierend
    void clear();
    char *data();
    void reserve(size_t min_avail);
    void extend(size_t n);
    void swap(Str &other);
    Str &operator=(Str const &rhs);
    Str &append(const char *src, size_t len);
    Str &append(const char *src);
    Str &append(Str const &src);
    Str &vprintf(const char *fmt, va_list args);
    Str &printf(const char *fmt, ...) ATTR_PRINTF (2, 3);
    Str &vprintfa(const char *fmt, va_list args);
    Str &printfa(const char *fmt, ...) ATTR_PRINTF (2, 3);
private:
    char *s_;
};


////////////////////////////////////////////////////////////////////////////////////////////////////
// Eine Liste von Strings.
////////////////////////////////////////////////////////////////////////////////////////////////////

class StringList
{
public:
   StringList();
   ~StringList();
   const char *operator[] (unsigned n) const;
   unsigned size() const { return size_; }
   bool match(char tag, const char *p, const char *t);
   bool find(unsigned *pos, const char *val) const;
   void append(const char *s);
   void insert(const char *s);
   void remove(unsigned pos);
   void clear();
   void swap(StringList & l);
private:
   StringList(StringList const &l);    // Nicht impl.
   void operator=(StringList const &l); // Nicht impl.
   size_t size_;
   char const **data_;
};


const char *str_freeze(const char *s, unsigned len);
const char *str_freeze(const char *s);
const char *next_str(const char **rp, const char *sep);
const char *next_word(const char **rp);
const char *next_name(const char **rp);
bool skip_str(const char **rp, const char *str);
char skip_blank(const char **rp);
inline char skip_blank(char **rp) { return skip_blank((const char**)rp); }
bool next_int(int *val, const char **rp);
bool next_int(int *val, char **rp);
bool str2int(int *val, const char *s);
unsigned calc_indent(const char *c);
const char *remove_indent(const char **cp, unsigned indent);
char *str_chop(char **rp);
void expand_substrings(Str &buf, const char *s, const StringList *args, char args_tag);
void unescape(char *str);
void escape(Str &out, const char *src);

class Crc {
public:
   Crc(void const *data, size_t len);
   Crc &feed(void const *data, size_t len);
   Crc &feed(const char *s);
   unsigned final();
private:
   unsigned x_;
   unsigned len_;
};

struct Assignment {
   const char *name;
   const char *cfg;
   VarMode_t mode;
   const char *value;
   bool split(const char *buf);
};

struct VariableName {
   const char *name;
   const char *short_name;
   char tr_mode;
   char ph;
   const char *pat;
   const char *repl;
   bool split(const char **buf);
};


// ===== yadir.cc ==================================================================================

class DirMaker {
public:
    bool create_parents(const char *path, bool silent);
private:
    StringMap dirs_;    // Bereits angelegte Verzeichnisse
};



// ===== yapref.cc =================================================================================

////////////////////////////////////////////////////////////////////////////////////////////////////
// Abstrakte Basisklasse für Programmeinstellungen (über ~/.yaburc, Kommandozeile bzw. !settings).
////////////////////////////////////////////////////////////////////////////////////////////////////

class Setting
{
public:
   static const int DEFAULT = 0;        // Prioritätswerte
   static const int GLOBALRC = 4;
   static const int RCFILE = 5;
   static const int BUILDFILE = 10;
   static const int COMMAND_LINE = 20;
   static const int MAX = 100;

   Setting(const char *name);
   virtual ~Setting();
   static void parse_line(const char *line, int prio);
   static void set(const char *name, int prio, const char *value);
   void set(int prio, const char *value);
   static void dump();
protected:
   bool can_set(int prio);
private:
   const char *const name_;
   Setting *next_;
   int prio_;
   virtual void set_impl(const char *value) = 0;
   virtual Str printable_value() const = 0;
};


////////////////////////////////////////////////////////////////////////////////////////////////////
// Programmeinstellung (Text).
////////////////////////////////////////////////////////////////////////////////////////////////////

class StringSetting:public Setting
{
public:
   StringSetting(const char *name, const char *dflt = 0);
   virtual ~StringSetting() { }
   operator const char *() const { return value_; }
protected:
   virtual void set_impl(const char *value);
private:
   const char *value_;
   Str printable_value() const;
};


////////////////////////////////////////////////////////////////////////////////////////////////////
// Programmeinstellung (Integer).
////////////////////////////////////////////////////////////////////////////////////////////////////

class IntegerSetting: public Setting
{
public:
   IntegerSetting (const char *name, int min, int max, int dflt);
   virtual ~IntegerSetting () { }
   operator  int() const { return value_; }
   void set(int prio, int val) { if (can_set (prio)) value_ = val; }
private:
   int const min_;
   int const max_;
   int value_;
   void set_impl(const char *value);
   Str printable_value() const;
};


////////////////////////////////////////////////////////////////////////////////////////////////////
// Programmeinstellung (bool).
////////////////////////////////////////////////////////////////////////////////////////////////////

class BooleanSetting: public Setting
{
public:
   BooleanSetting (const char *name, bool dflt = false);
   virtual ~BooleanSetting() { }
   operator  bool() const { return value_; }
   void set(int prio, bool val) { if (can_set(prio)) value_ = val; }
private:
   bool value_;
   void set_impl(const char *value);
   Str printable_value() const;
};


// ===== yabu.cc ==================================================================================

extern int no_exec;
extern char yabu_wd[];
extern IntegerSetting max_warnings;
extern StringSetting shell_prog;
extern const char *yabu_cfg_dir;
extern BooleanSetting echo_before;
extern BooleanSetting echo_after_error;
extern BooleanSetting auto_mkdir;
extern BooleanSetting use_server;
extern BooleanSetting parallel_build;
extern BooleanSetting use_auto_depend;
extern const time_t yabu_start_time;
extern BooleanSetting use_state_file;
extern TsAlgo_t global_ts_algo;


extern const char *user_options;
bool dump_req(char c);

// ===== yastate.cc ===============================================================================

class StateFileReader {
public:
   StateFileReader(const char *file_name);
   ~StateFileReader();
   bool next();
   StringList argv_;
   bool invalid_;
   static bool decode(unsigned &val, const char *s);
   static bool decode(Ftime &t, const char *s);
};

class StateFileWriter {
public:
   StateFileWriter(const char *file_name);
   ~StateFileWriter();
   void begin(const char *c);
   void append(const char *c);
   void append(unsigned val);
   void append(Ftime const &val);
};



// ===== yavar.cc =================================================================================

class VarScope;

class CfgFreeze         // Konfiguration festhalten/wiederherstellen
{
public:
   CfgFreeze(VarScope *scope);
   ~CfgFreeze();
private:
   unsigned char *cfg_;
   VarScope * const scope_;
};

class VarLock
{
public:
   VarLock(VarScope *scope, const char *name);
   ~VarLock ();
   void release();
private:
   struct Variable *var_;
};

VarScope *var_create_scope();

void var_dump_options(VarScope *scope, const char *prj_root);
bool var_cfg_is_valid(VarScope *scope, const char *cfg);
void var_create_option(VarScope *scope, const char *group, const char *name, bool no_check = false);
bool var_parse_global_options(const char *c);
bool var_parse_options(VarScope *scope, const char *c);
bool var_cfg_compatible(VarScope *scope, const char *from, const char *to);
bool var_try_cfg_change(VarScope *scope, const char *cfg);
void var_cfg_change(VarScope *scope, const char *cfg);
const char *var_current_cfg(VarScope *scope);
void var_set_system_variables();

void var_assign(VarScope *scope, const char *name, const char *options1, const char *options2,
      VarMode_t mode, const char *value, const SrcLine *sl);
bool var_get(VarScope *scope, Str &val, const char *name, const char *defval = 0);
bool var_get_global(Str &val, const char *name, const char *defval = 0);
bool var_is_valid_name(const char *name);
bool var_check_name(const char *name);
void freeze_system_vars(bool freeze);
void var_dump(VarScope *scope, const char *prj_root);

void expand_vars(VarScope *scope, Str &buf, const char *s, const StringList *args, char args_tag,
      const StringList *files);


// ===== yatgt.h ==================================================================================

struct TargetGroup;
struct Dependency;

struct Target
{
   static const unsigned T0 = 2;// Kleinstmögliche "echte" Änderungszeit

   static unsigned total_built;
   static unsigned total_failed;
   static unsigned total_utd;
   static unsigned total_cancelled;
   static Target *sel_head;		// Für Phase 2 ausgewählte Ziele
   static Target **sel_tail;

   Project * const prj_;
   const char *const name_;		// Relativ zum Projektverzeichnis «prj_->root_»
   Ftime time_;  			// Änderungszeit (falls >= T0)
   const char *build_cfg_;		// Konfiguration, in der das Ziel erreicht wurde.
   enum Status 	{
      IGNORED,				// Nicht ausgewählt
      SELECTING,			// Wird gerade ausgewählt
      SELECTED,				// Ausgewählt
      BUILDING,				// Skript wird ausgeführt
      BUILT,				// Erreicht
      FAILED				// Nicht erreicht
   } status_;				// Für die Statistik
   Dependency *req_by_;			// Warum ausgewählt?
   Target *older_than_;			// Warum neu erzeugt?
   Dependency *srcs_;			// Quellen (Ziele, von denen «this» abhängt)
   Dependency *tgts_;			// Ziele, die von «this» anhängen
   Rule *build_rule_;			// Ausgewählte Regel oder 0
   bool is_alias_;			// Alias-Ziel (::)
   Str build_script_;			// Skript (alle Variablen ersetzt)
   Str ad_script_;			// Auto-Depend-Skript
   StringList build_files_;		// Werte für $(0), $(1), ...
   StringList build_args_;		// Werte für %1, %2, ...
   unsigned rule_id_;			// Signatur der Regel: Wert aus Buildfile.state
   unsigned rule_id_new_;		// Signatur der Regel: Neuer Wert

   static const char *status_str(Status st);
   void dump() const;
   static void statistics();
   Ftime get_file_time(TsAlgo_t tsa);
   bool is_outdated(TsAlgo_t tsa);
   void delete_auto_sources();
   void begin_select(Dependency *req_by);
   bool end_select();
   bool is_selected() const { return sel_prevp_ != 0; }
   void set_building();
   bool is_leaf();

   bool is_regular_file_;
   Target *sel_next_;		// Liste der ausgewählten Ziele
   Target **sel_prevp_;
   TargetGroup *group_;		// Gruppe (bei Skripten mit mehreren Ausgabedateien) oder 0
   Target *next_in_group_;	// Verlinkung innerhalb der Gruppe

   Target(Project *prj, const char *name);
   Target(const Target & t);     // Nicht impl.
   void deselect(Status status);
   void set_group(Rule *r, StringList const &args);
private:
   Target();			// Nicht implementiert
};


struct TargetGroup {
   TargetGroup(const char *id) :id_(id), fail_all_(true), head_(0), tailp_(&head_) , locked_by_(0) {}
   friend int compare(const char *s, const TargetGroup * const *g) { return strcmp(s,(*g)->id_); }
   void lock(Target *t) { if (locked_by_ == 0) locked_by_ = t; }
   void unlock() { locked_by_ = 0; }
   const char * const id_;	// Name der Gruppe
   bool fail_all_;		// Fehler übeträgt sich auf alle Mitglieder
   Target *head_;		// Mitgliederliste
   Target **tailp_;		// Listenende
   Target *locked_by_;
};


struct Serial: public TargetGroup {
   static void create(const char *id, StringList &tgts);
   static Serial *find(const char *name);
private:
   Serial(const char *id);
   bool match(const char *tgt) const;
   Serial *next_;
   StringList tgts_;
   static Serial *head;
   static Serial **tail;
};


struct ConfigureRule {
   ConfigureRule(ConfigureRule ***tail, const char *cfg, StringList &tgts);
   static const char *get_cfg(const ConfigureRule *head, const char *target);
private:
   const char *const cfg_;
   StringList tgts_;
   ConfigureRule *next_;
};


// ===== yadep.h ==================================================================================

class Dependency {			// Quelle-Ziel-Beziehung
public:
    Target * const tgt_;
    Target * const src_;
    Rule const *rule_;
    bool deleted_;
    Ftime last_src_time_;		// Zeitstempel der Quelle (aus Buildfile.state)
    Dependency *next_tgt_;
    Dependency *next_src_;
    static Dependency *create(Target *tgt, Target *src, Rule const *rule);
    static void destroy(Dependency *dep);
private:
    Dependency(Target *tgt, Target *src, Rule const *rule);
    ~Dependency() {}
};



// ===== yarule.cc =================================================================================

struct Rule {
   const SrcLine * const srcline_;	// Definition
   const char *targets_;		// Liste der Ziele.
   const char *sources_;		// Liste der Quellen.
   const char *config_;			// Konfigurationsauswahl.
   const SrcLine *script_beg_;
   const SrcLine *script_end_;
   const SrcLine *adscript_beg_;
   const SrcLine *adscript_end_;
   bool is_alias_;
   bool create_only_;			// Ziel nicht überschreiben (:?)
   Rule *next_;				// Nächste Regel in der Liste.

   Rule(const SrcLine *sl);
   void dump() const;
};


// ===== yajob.cc ==================================================================================

typedef enum { NOTIFY_STARTED, NOTIFY_OK, NOTIFY_FAILED, NOTIFY_CANCELLED } notify_event_t;

void job_export(const char *name, const char *value);
void job_export_var(const char *name);
void job_export_env(const char *name);
void job_process_queue(bool wait);
void job_create(Project *prj, Target *t, char tag, Str const &cmds, unsigned flags);
bool job_queue_empty();
void job_init();
void job_shutdown();
bool job_add_host(const char *name, struct sockaddr_in const *sa,
      int max_jobs, int prio, const char *cfg);
const char *job_local_cfg();



// ===== yaprj.cc ==================================================================================

extern const Rule YABU_INTERNAL_RULE;

struct CfgCommand {
   CfgCommand(SrcLine const *s, char const *sel, SrcLine const *beg, SrcLine const *end);
   CfgCommand *next_;
   SrcLine const *src_;
   const char *selector_;
   SrcLine const *beg_;
   SrcLine const *end_;
};

class Project {
public:
   const char * const aroot_;		// Relativ zu «yabu_wd» (leer oder endet mit "/")
   const char * const rroot_;		// Relativ zu «parent_->aroot_» (leer oder endet mit "/")
   const unsigned depth_;		// Anzahl der '..' in «rroot_»
   const char * const buildfile_;	// Relativ zum Stammverzeichnis
   const char * state_file_;		// Relativ zu «yabu_wd»
   const SrcLine * const src_;		// Position der !project-Anweisung
private:
   Project * const parent_;
   CfgCommand *ac_;			// !configure-Anweisungen (1. und 2. Form)
   CfgCommand **ac_tail_;
   Project *next_;
   Project *prjs_head_;			// Unterprojekte
   Project **prjs_tail_;
   VarScope *vscope_;
   Rule *rules_head_;			// Alle Regeln
   Rule **rules_tail_;
   ConfigureRule *cfg_rules_head_;	// !configure-Anweisungen (3. Form)
   ConfigureRule **cfg_rules_tail_;
   Target **all_tgts_;			// Alle Ziele (alphabetisch)
   size_t n_tgts_;			// Anzahl Elemente in «all_tgts»
   TsAlgo_t ts_algo_;
   bool discard_build_times_;		// Zeitangaben aus Buildfile.state verwerfen
   enum { NEW, OK, INVALID } state_;
   SrcLine const *eoi_; 	// Ende der Eingabe (letzte Zeile + 1).
   SrcLine const *cur_;		// Aktuelle Position während der Verarbeitung.

   bool state_file_read(const StringList &args);
   static void select_tgt(Target *tgt, Dependency *req_by);
   void dump_rules();
   void clear_all_build_times();
   void select_rule(Target * t);
   void exec(Target *t);
   void prepare_build(Target *t);
   bool build_special(Target *t);
   void add_sources(Target*t, StringList &srcs, Rule const *r);
   static void set_done(Target *t, unsigned *counter);
   static void try_build(Target *t);
   static void unlock_group(Target *t);
   static void fail_tgt(Target *t);
   static void cancel_tgt(Target *t, unsigned flags);
   void split_srcs(StringList &files, const char *srcs, StringList const &args);
   bool match_rule(Rule *r, int *score, const char **cfg, StringList &args, StringList &srcs,
      Target const *tgt);
   bool init();
   void state_file_read();
   void state_file_delete();
   Target *get_tgt(const char *name, bool may_create);
   const char *exec_ac_script(CfgCommand const *cmd);
   const char *exec_ac_switch(CfgCommand const *cmd);

   bool get_script(const char **type, const SrcLine **bp, const SrcLine **ep);
   void do_options(const char *c);
   void do_configuration(const char *c);
   void do_configure(const char *c);
   void do_settings(const char *c);
   void do_export(const char *c);
   void do_rule();
   void do_project(const char *c);
   void do_serialize(const char *c);

public:
   Project(Project *parent, const char *aroot, const char *rroot,
	 const char *buildfile, const SrcLine *src);
   void add_prj(const char *root, const char *buildfile, const SrcLine *src);
   void add_rule(Rule *r);
   void add_cfg_rule(const char *cfg, StringList &tgts);
   void select_tgt(const char *tgt, Dependency *req_by);
   void job_notify(Target *t, char tag, notify_event_t event, const char *host, Str *output);
   void exec_local(Str &output, Str const &script, const char *title);
   static void cancel_all(unsigned msg_level);
   void set_static_options();
   VarScope *vscope() const {return vscope_;}
   void dump_tgts();
   void state_file_write();
   void parse_buildfile(SrcLine const *begin, SrcLine const *end);
};


// ===== yamsg.cc ==================================================================================
//
extern volatile int exit_code;
extern BooleanSetting StopOnFailure;
extern IntegerSetting verbosity;

void set_message_handler(void (*h)(unsigned flags, const char *txt));

////////////////////////////////////////////////////////////////////////////////////////////////////
// Programmkontext für Fehlermeldungen. Abstrakte Basisklasse.
////////////////////////////////////////////////////////////////////////////////////////////////////

class YabuContext
{
public:
   YabuContext(SrcLine const *src, Target const *tgt, char const *msg);
   ~YabuContext();
private:
   SrcLine const *src_;
   Target const *tgt_;
   char const *msg_;
};


#define YABU_ASSERT(cond) \
   do { if (!(cond)) YUFTL(G03,assertion_failed(__FILE__,__LINE__,#cond)); } while (false)

#define MSG_4  7U
#define MSG_3  6U
#define MSG_2  5U
#define MSG_1  4U
#define MSG_0  3U
#define MSG_W  2U
#define MSG_E  1U
#define MSG_F  0U
#define MSG_ALWAYS 0x100	// Ausgabe unabhängig von «verbosity»
#define MSG_SCRIPT 0x1000	// Gruppe "Skripte"
#define MSG_LEVEL(x) (((x) & 0x07))
#define MSG_OK(flags) (((flags) & MSG_ALWAYS) || MSG_LEVEL(flags) <= verbosity + MSG_0)

#define MSG(lvl,txt) (MSG_OK(lvl) && Message(lvl,"%s",txt))

#define YUFTL(id,msg) yabu_error(#id,MSG_F,Msg::id##_##msg)
#define YUERR(id,msg) yabu_error(#id,MSG_E,Msg::id##_##msg)
#define YUWRN(id,msg) yabu_error(#id,MSG_W,Msg::id##_##msg)

void yabu_error(const char *id, unsigned flags, const char *msg);

bool Message(unsigned level, const char *msg, ...) ATTR_PRINTF (2, 3);
void MessageBlock(unsigned level, const char *prefix, const char *text);
void set_language();
void dump_source(Str &buf, SrcLine const *s);



namespace Msg
{
   const char *Format(const char *fmt, ...) ATTR_PRINTF(1, 2);

   const char *G03_assertion_failed(const char *fn, int line, const char *txt);
   const char *G10_cannot_open(const char *name);
   const char *G20_syscall_failed(const char *func, const char *arg);
   const char *G30_script_failed(const char *target, const char *script_type);
   const char *G31_not_built(const char *target);
   const char *G32_target_exists(const char *name);
   const char *G41_unknown_option(char opt);
   const char *G42_missing_opt_arg(char opt);
   const char *G43_bad_ts_algo(char const *arg);
   const char *G44_need_state_file(const char *algo);
   const char *L01_undefined(char kind, const char *name);
   const char *L02_unknown_arg(const char *arg, const char *macro);
   const char *L03_redefined(char kind, const char *name, const SrcLine *src);
   const char *L04_circular_dependency(char kind, const char *name);
   const char *L05_nonregular_leaf(const char *target);
   const char *L09_too_many_args(const char *name);
   const char *L11_too_many_placeholders(char tag);
   const char *L12_options_finalized();
   const char *L13_assignment_to_system_variable(const char *name);
   const char *L14_no_match(const char *vname, const char *val, const char *pat);
   const char *L24_conflicting_options(const char *name, const char *other_name);
   const char *L31_ambiguous_rules(const char *target, const SrcLine * r1, const SrcLine * r2);
   const char *L33_no_rule(const char *target);
   const char *L35_conflicting_values(const char *var,const char *cfg,const SrcLine *s1,
	 const SrcLine *s2);
   const char *L40_too_many_nested_includes();
   const char *L42_conflicting_prj_root(const char *root, const Project *prj);
   const char *L43_invalid_prj_root(const char *root);
   const char *S01_syntax_error(const char *what = 0);
   const char *S02_nomatch(const char *bcmd, const char *ecmd);
   const char *S03_missing_digit(char ch);
   const char *S04_missing_script();
   const char *S07_bad_bool_value(const char *s);
   const char *S08_bad_int_value(const char *s);
   const char *S09_int_out_of_range(int val, int min, int max);
   const char *addr_unusable(const char *addr,int err);
   const char *already_built(const char *name);
   const char *archive_corrupted(const char *name);
   const char *archive_not_found(const char *ar_name, int err);
   const char *bad_file_attributes(const char *fn);
   const char *building(const Target *tgt, const char *host, const char *script_type);
   const char *cannot_read(const char *fn, int err);
   const char *configure_result(const char *cfg);
   const char *cwd_failed(const char *dir, int err);
   const char *depends_on(const char *target, const char *source, bool reg);
   const char *discarding_state_file(const char *fn);
   const char *is_outdated(const char *target, const char *source);
   const char *leaf_exists(const char *target);
   const char *line(const char *fn, const unsigned line);
   const char *line(const SrcLine *src);
   const char *login_failed(const char *host);
   const char *login_result(const char *name, bool ok);
   const char *my_hostname_not_found(const char *cf,const char *hn);
   const char *no_dir(const char *dir);
   const char *no_queue(Target const *t);
   const char *options();
   const char *process_line(const char *name);
   const char *reading_file(const char *fn, unsigned line, unsigned line2);
   const char *reset_mtime_after_error(const char *target);
   const char *rule_changed(const char *tgt);
   const char *rules();
   const char *rule_unusable(const SrcLine *s, const char *cfg);
   const char *script_terminated_on_signal(int sig);
   const char *selecting(const char *t);
   const char *server_running(const char *ver, const char *addr);
   const char *server_unavailable(const char *host, int err);
   const char *sources();
   const char *srv_usage();
   const char *target_cancelled(const char *name);
   const char *target_is_up_to_date(const char *target);
   const char *target_missing(const char *target);
   const char *targets();
   const char *target_stats_failed(unsigned ok, unsigned failed, unsigned cancelled);
   const char *target_stats(unsigned total, unsigned utd, unsigned ok, unsigned ttime);
   const char *unexpected_args();
   const char *usage();
   const char *using_rule(const SrcLine *sl, const char *target);
   const char *userinit_failed(const char *name, const char *f, int arg, int err);
   const char *variables();
   const char *waiting_for_jobs(unsigned n);
   const char *wrong_token(const char *name);
}




////////////////////////////////////////////////////////////////////////////////////////////////////
// Die Klasse «Preprocessor» implementiert die Vorverarbeitungsschritte, die nicht bereits von
// «FileReader» ausgeführt werden. Dazu gehören .foreach- und .include-Anweisungen, Ersetzung von
// (PP-)Variablen sowie die Verarbeitung von Makros.
// 
// Die Eingabe (von «FileReader» geliefert) ist ein Bereich von Quellzeilen. Die Ausgabe besteht
// aus einem Block oder mehreren Blöcken von Quellzeilen, die einzeln an «Project»
// weitergegeben werden. Zum Beispiel erzeugt jeder Durchlauf einer .foreach-Schleife einen
// separaten Ausgabeblock.
////////////////////////////////////////////////////////////////////////////////////////////////////

class Macro;

class Preprocessor
{
public:
   Preprocessor(Project *prj, SrcLine const *begin, SrcLine const *end, Preprocessor *parent,
	 bool first_time);
   ~Preprocessor();
   void execute();
private:
   Project * const prj_;
   SrcLine const * const eoi_;  	// Ende der Eingabe (letzte Zeile + 1)
   Preprocessor * const parent_;       	// Kontext (bei verschachtelten Anweisungen)
   bool const first_time_;		// Datei wird zum ersten Mal gelesen
   SrcLine *caller_;
   SrcLine const *cur_;        		// Aktuelle Eingabezeile
   SrcLine const *sob_;      		// Beginn des aktuellen Anweisungsblocks
   Macro *local_macros_;
   StringMap vars_;	   		// Lokale Variablen
   SrcLine *obuf_;			// Aktueller Ausgabeblock
   unsigned obuf_size_;			// Anzahl der Zeilen in «obuf_»
   unsigned obuf_capacity_;		// maximale Anzahl der Zeilen in «obuf_»
   bool flushing_;

   void do_include_output();
   void do_include(const char *c);
   void do_foreach(const char *c);
   void do_define(const char *c);
   bool do_assignment(const char *c);
   bool do_macro(const char *c);
   bool do_once();
   void find_block_end(const SrcLine * begin, const char *bcmd, const char *ecmd);
   const char *find_var(const char *name) const;
   bool expand_vars(Str &buf, const char *txt);
   void expand_vars(const char **cp);
   void emit(SrcLine const *src, const char *txt);
   void flush();
   bool get_script(Str &script);
};



////////////////////////////////////////////////////////////////////////////////////////////////////
// Eine Quelldatei. Abstrakte Basisklasse.
////////////////////////////////////////////////////////////////////////////////////////////////////

class FileReader
{
public:
   FileReader(const char *file_name, const char *root);
   virtual ~FileReader();
   void read(bool ign_missing);
protected:
   SrcLine src_;
   FILE *file_;
   bool first_time_;
private:
   const char *file_name_;
   char *lbuf_;			// Zeilenpuffer
   unsigned lbsize_;		// Puffergröße
   bool next_line();
   virtual void process_line(char *c) = 0;
   virtual void finish() { }
   virtual bool is_comment(const char *c) const;
};


////////////////////////////////////////////////////////////////////////////////////////////////////
// Ein Buildfile.
////////////////////////////////////////////////////////////////////////////////////////////////////


class BuildfileReader: public FileReader
{
public:
   BuildfileReader(Project *prj, const char *file_name, Preprocessor *parent);
   ~BuildfileReader();
private:
   Project * const prj_;
   Preprocessor * const parent_;
   SrcLine *lines_;
   unsigned num_lines_;
   unsigned max_lines_;
   void process_line(char *c);
   void finish();
};


// ===== yaar.cc ===================================================================================

bool ar_member_time(Ftime *time, const char *name, TsAlgo_t ts_algo);


// ===== yasrv.cc ==================================================================================

int server_main(int argc, char *argv[]);

#define DEFAULT_PORT "6789"


// ===== yauth.cc ==================================================================================

const char *make_auth_token(const char *cfg_dir);
struct passwd *auth_check(const char *cfg_dir, unsigned uid, const char *token);


// ===== yapoll.cc =================================================================================

// Abstrakte Basisklasse für poll()-bare Objekte.

struct PollObj {
    PollObj();
    virtual ~PollObj();
    void add_fd(int fd);
    void del_fd();
    void clear_events(int events);
    void set_events(int events);
    int fd() const;
    static bool poll(int timeout);
    virtual int handle_input(int fd, int events) = 0;
    virtual int handle_output(int fd, int events) { return 1; }
    unsigned const id_;
    int idx_;			// Index in «pollfd» oder -1
};


// ===== yacomm.cc =================================================================================

struct IoBuffer {
   struct Record;
   IoBuffer(PollObj &po);
   ~IoBuffer();
   int read(int fd);
   bool next();
   void append(char tag, const char *msg, ...);
   void append(char tag, size_t len, char const *data);
   int write(int fd);
   char tag_;
   size_t len_;
   char *data_;
private:
   char *rbuf_;		// Empfangspuffer
   size_t rlen_;	// Anzahl Bytes in «rbuf_»
   size_t rmax_;	// Puffergröße
   size_t rp_;		// Leseposition

   char *wbuf_;		// Sendepuffer
   size_t wlen_;        // Anzahl Bytes in «wbuf_»
   size_t wmax_;        // Puffergröße
   size_t wp_;          // 1. zu sendendes Zeichen

   PollObj &obj_;
   void commit(char tag, size_t len);
};



// ===== yasys.cc ==================================================================================

static const unsigned EXEC_COLLECT_OUTPUT = 1;
static const unsigned EXEC_MERGE_STDERR = 2;
static const unsigned EXEC_LOCAL = 4;
extern volatile bool got_sigchld;
typedef void (*yabu_sigh_t)(int sig);
const char *my_hostname();
const char *my_osname();
const char *my_osrelease();
const char *my_machine();
void sys_setup_sig_handler(yabu_sigh_t hup, yabu_sigh_t intr, yabu_sigh_t term);
void set_close_on_exec(int fd);
bool resolve(struct in_addr *a, const char *name);
int yabu_open(const char *fn, int flags);
int yabu_read(int fd, void *buf, size_t len);
int yabu_write(int fd, void const *buf, size_t len);
bool yabu_fork(int *pipe_fd, pid_t *pid, unsigned flags);
bool yabu_stat(const char *name, struct stat *sb);
void yabu_ftime(Ftime *ft, struct stat const *sb);
void yabu_cot(const char *fn);


// ===== yacfg.cc ==================================================================================

class CfgReader: public FileReader {
public:
   CfgReader(const char *fn, int prio)
      :FileReader(fn,""), prio_(prio), mode_(PREFS)  {}
   virtual ~CfgReader() {}
   virtual bool host(const char *name, struct sockaddr_in const *sa,
	 int max_jobs, int prio, const char *cfg) = 0;
private:
   int const prio_;
   enum { IGNORE, PREFS, OPTIONS, SERVERS } mode_;
   void process_line(char *c);
   bool do_server(char *lbuf);
   bool do_host(size_t argc, char **argv);
};


// ===== Allgemeine Templates ======================================================================

void array_alloc(void **t, size_t n, size_t size);
void array_realloc(void **t, size_t n, size_t size);
void array_insert(void **t, size_t *n, size_t size, size_t pos);

template <typename T>
void array_alloc(T *&t, size_t n) { array_alloc((void**) &t,n,sizeof(T)); }

template <typename T>
void array_realloc(T *&t, size_t n) { array_realloc((void**) &t,n,sizeof(T)); }

template <typename T>
void array_insert(T * &t, size_t &n, size_t pos) {array_insert((void**)&t,&n,sizeof(T),pos); }


////////////////////////////////////////////////////////////////////////////////////////////////////
// Sucht das Element «s» in dem (sortierten) Array «tab». Wie bsearch(), liefert aber in «pos» die
// Einfügeposition, wenn nicht gefunden. Benötigt Vergleichsfunktion mit folgender Signatur:
//    int compare(const char *s,T const* obj) mit strcmp()-Semantik
////////////////////////////////////////////////////////////////////////////////////////////////////

template <typename T>
bool my_bsearch(size_t *pos, T const *tab, size_t size, const char *s)
{
   size_t lo = 0;
   size_t hi = size;
   while (lo < hi) {
      size_t mid = (hi & lo) + (hi ^ lo) / 2;
      int cmp = compare(s, tab + mid);
      if (cmp == 0) {
         if (pos)
            *pos = mid;
         return true;
      }
      else if (cmp < 0)
         hi = mid;
      else
         lo = mid + 1;
   }
   if (pos)
      *pos = lo;
   return false;
}

#endif

// vim:shiftwidth=3:cindent
