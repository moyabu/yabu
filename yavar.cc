////////////////////////////////////////////////////////////////////////////////////////////////////
// I, the creator of this work, hereby release it into the public domain. This applies worldwide.
// In case this is not legally possible: I grant anyone the right to use this work for any purpose,
// without any conditions, unless such conditions are required by law.
////////////////////////////////////////////////////////////////////////////////////////////////////

// Optionen und (Phase-2-)Variablen

#include "yabu.h"

#include <string.h>
#include <unistd.h>
#include <time.h>

static unsigned NOTFOUND = 0xFFFFFFFF;	// Spezieller Wert für "nicht gefunden".

static unsigned char ON = '+';
static unsigned char OFF = '-';
static unsigned char UNDEF = '?';



////////////////////////////////////////////////////////////////////////////////////////////////////
// Die Klasse «Option» repräsentiert eine Option.
////////////////////////////////////////////////////////////////////////////////////////////////////

struct Option
{
   const char *group_;		// Name der Gruppe oder 0
   const char *name_;		// Name der Option
};


////////////////////////////////////////////////////////////////////////////////////////////////////
// Bedinge Wertzuweisung.
// Diese Struktur enthält eine optionale Wertzuweisung für eine Variable.
// Neben den beiden Attributen -- Konfiguration und Wert -- besitzt die Struktur ein "next_"-Feld,
// mit dem sich mehere optionale Wertzuweisungen in einer verketteten Liste organisieren lassen.
// Die Struktur «Variable» enthält drei solcher Listen von «OptionalValue»-Objekten,
// und zwar je eine für Zuweisungen vom Typ "=", "+=" und "?=".
////////////////////////////////////////////////////////////////////////////////////////////////////

struct OptionalValue
{
   unsigned char *cfg_;		// Konfiguration, für die der Wert gilt.
   const char *value_;		// Wert der Variablen für diese Konfiguration
   const SrcLine * const src_;	// Stelle der Definition im Buildfile
   OptionalValue *next_;
   OptionalValue(const char *value, const SrcLine *src)
      :cfg_(0), value_(value), src_(src), next_(0) { }
   void append(OptionalValue ** head) {
      while (*head)
	 head = &(*head)->next_;
      *head = this;
   }
};


////////////////////////////////////////////////////////////////////////////////////////////////////
// Diese Klasse repräsentiert eine (Phase-2-)Variable
////////////////////////////////////////////////////////////////////////////////////////////////////

struct Variable
{
  const char *name_;		// Name der Variablen.
  const SrcLine *src_;		// Stelle der Definition
  char *value_;			// Wert nach Ausführung aller unbedingten Zuweisungen.
  bool locked_;			// Zur Schleifenerkennung. Siehe «var_get()» und «VarLock»
  OptionalValue *Set;		// Optionale Zuweisungen. Reihenfolge wie im Buildfile, siehe «Set».
  OptionalValue *Append;	// Optionale Zuweisungen (+=). Siehe «Set».
  OptionalValue *Merge;		// Optionale Zuweisungen (?=). Siehe «Set».
};






class VarScope {
public:
   VarScope();
   Variable *vars_;		// Liste aller Variablen.
   size_t n_vars_;      	// Anzahl der Variablen.
   Option *opts_;		// Alle Optionen
   size_t n_opts_;		// Anzahl der Optionen
   unsigned char *ccfg_;	// Aktuelle Konfiguration
   bool opts_finalized_;	// Keine weiteren Optionen mehr definierbar.

   bool group_exists(const char *grp);
   unsigned find_opt(const char *opt);
   void create_option(const char *group, const char *name, bool no_check = false);
   bool next_option(unsigned char *val, unsigned *opt, const char **rp);
   bool parse_cfg1(unsigned char *optab, const char *cfg, bool silent);
   bool parse_cfg(unsigned char *&optab, const char *cfg1, const char *cfg2, bool silent);
   bool compat(unsigned char const *from, unsigned char const *to) const;
   bool is_subset_of(unsigned char const *a, unsigned char const *b) const;
   const char *current_cfg();

   Variable *find_var(const char *name);
   void create_var(const char *name, const char *value, const SrcLine *sl);
   void var_assign(const char *name, VarMode_t mode, const char *value, const SrcLine *sl);
   bool var_get(Str &val, const char *name, const char *defval);
   void dump_opt_values(OptionalValue const *ov, const char *mode);
};

static VarScope global_scope;


VarScope::VarScope()
   :vars_(0), n_vars_(0),
    opts_(0), n_opts_(0), ccfg_(0), opts_finalized_(false)
{
}


VarScope *var_create_scope()
{
   VarScope *scope = new VarScope;
   // Globale Optionen erzeugen
   scope->create_option(0,"_local",true);
   for (size_t i = 0; i < global_scope.n_opts_; ++i)
      scope->create_option(global_scope.opts_[i].group_,global_scope.opts_[i].name_);
   return scope;
}


////////////////////////////////////////////////////////////////////////////////////////////////////
// Gültigkeitsprüfung für Variablen- und Optionsnamen.
////////////////////////////////////////////////////////////////////////////////////////////////////

bool var_is_valid_name(const char *name)
{
   const char *c = name;
   if (!IS_DIGIT(*c))
   {
      for (++c; IS_IDENT(*c); ++c)
	 ;
   }
   return *c == 0 && c != name;
}


////////////////////////////////////////////////////////////////////////////////////////////////////
// Fehler auslösen, wenn der Name ungültig ist.
////////////////////////////////////////////////////////////////////////////////////////////////////

bool var_check_name(const char *name)
{
   if (var_is_valid_name(name))
      return true;
   YUERR(S01,syntax_error(name));
   return false;
}


////////////////////////////////////////////////////////////////////////////////////////////////////
// Prüfen ob die Optionsgruppe «grp» existiert.
////////////////////////////////////////////////////////////////////////////////////////////////////

bool VarScope::group_exists(const char *grp)
{
   for (unsigned i = 0; i < n_opts_; ++i) {
      if (opts_[i].group_ && !strcmp(opts_[i].group_, grp))
         return true;
   }
   return false;
}


////////////////////////////////////////////////////////////////////////////////////////////////////
// Option mit dem Namen «opt» suchen. Return ist der Index in «opts» oder «NOTFOUND».
////////////////////////////////////////////////////////////////////////////////////////////////////

unsigned VarScope::find_opt(const char *opt)
{
   for (unsigned i = 0; i < n_opts_; ++i) {
      if (!strcmp(opts_[i].name_, opt))
         return i;
   }
   return NOTFOUND;
}


////////////////////////////////////////////////////////////////////////////////////////////////////
// Sortierkriterium für die Variablenliste «vars». Benötigt von «my_bsearch()».
////////////////////////////////////////////////////////////////////////////////////////////////////

int compare(const char *n, const Variable * const v)
{
   if (*n == '_' && *v->name_ != '_') return -1;	// Systemvariablen vorne einsortieren
   if (*n != '_' && *v->name_ == '_') return 1;
   return strcmp(n,v->name_);
}


////////////////////////////////////////////////////////////////////////////////////////////////////
// Suche eine Variable mit gegebenem Namen.
////////////////////////////////////////////////////////////////////////////////////////////////////

Variable *VarScope::find_var(const char *name)
{
   size_t pos;
   return my_bsearch(&pos,vars_,n_vars_,name) ? vars_ + pos : 0;
}


////////////////////////////////////////////////////////////////////////////////////////////////////
// Erzeugt eine neue Option mit dem Namen «name». Ist «group» gleich 0, handelt es sich um eine
// boolesche Option, andernfalls um eine Option in der angegebenen Gruppe.
////////////////////////////////////////////////////////////////////////////////////////////////////

void VarScope::create_option(const char *group, const char *name, bool no_check)
{
   Variable *v = 0;

   if (opts_finalized_)
      YUERR(L12,options_finalized());
   else if (find_opt(name) != NOTFOUND || group_exists(name))
      YUERR(L03,redefined(0,name,0));
   else if ((v = find_var(name)) != 0)
      YUERR(L03,redefined('$',name,v->src_));
   else if (group && find_opt(group) != NOTFOUND)
      YUERR(L03,redefined(0,group,0));
   else if (group && (v = find_var(group)) != 0)
      YUERR(L03,redefined('$',group,v->src_));
   else if (no_check || ((group == 0 || var_check_name(group)) && var_check_name(name))) {
      // Position der neuen Option in «opts» bestimmen: einfache Optionen hinten
      // anhängen. Auswahloptionen hinten an die jeweilige Gruppe anhängen.
      unsigned pos = n_opts_;
      if (group) {
	 while (pos > 0 && opts_[pos - 1].group_ != group)
	    --pos;
	 if (pos == 0)
	    pos = n_opts_;      // Neue Gruppe, hinten anhängen
      }

      // Eintrag in »opts» einfügen
      array_insert(opts_, n_opts_, pos);
      --n_opts_;
      array_insert(ccfg_, n_opts_, pos);
      opts_[pos].name_ = name;
      opts_[pos].group_ = group;
      ccfg_[pos] = UNDEF;
   }
}


////////////////////////////////////////////////////////////////////////////////////////////////////
// Wertet eine Zeile mit Optionsdefinitionen aus.
////////////////////////////////////////////////////////////////////////////////////////////////////

bool var_parse_global_options(const char *c)
{
   return var_parse_options(&global_scope,c);
}

bool var_parse_options(VarScope *scope, const char *c)
{
   while (exit_code <= 1) {
      skip_blank(&c);
      if (*c == 0)			// Ende der Zeile
	 return true;
      const char *w1 = next_name(&c);
      if (w1 == 0) YUERR(S01,syntax_error());
      if (skip_str(&c," ( ")) {		// Optionsgruppe
	 while (!skip_str(&c," )")) {
	    const char *w2 = next_name(&c);
	    if (w2 == 0) YUERR(S01,syntax_error());
	    scope->create_option(w1, w2);
	 }
      }
      else				// Boolesche Option
	 scope->create_option(0, w1);   
   }
   return false;
}


////////////////////////////////////////////////////////////////////////////////////////////////////
// Extrahiert die nächste Option aus einem Konfigurationsstring. Nach erfolgreicher Ausführung
// enthält «opt» die Nummer der Option und «val» den Wert (ON oder OFF). Der Lesezeiger «*rp»
// zeigt nach Rückkehr auf das erste unverarbeitete Zeichen.
// return: True: Option extrahiert. False: Fehler oder Ende der Konfiguration erreicht.
////////////////////////////////////////////////////////////////////////////////////////////////////

bool VarScope::next_option(unsigned char *val, unsigned *opt, const char **rp)
{
   const char *c = *rp;
   if (skip_blank(&c) != 0) {
      *val = (*c == '-') ? OFF : ON;
      if (*c == '+' || *c == '-') ++c;
      const char *name = next_name(&c);
      if (name == 0) {
	 YUERR(S01,syntax_error(*rp));
	 return false;
      } else {
	 if ((*opt = find_opt(name)) == NOTFOUND)
	    YUFTL(L01,undefined('-',name));
	 else {
	    *rp = c;
	    return true;
	 }
      }
   }
   return false;
}


////////////////////////////////////////////////////////////////////////////////////////////////////
// Zerlegt «cfg» in einzelne Optionswerte und trägt diese in «optab» ein. Widersprüchliche Werte
// für eine Option führen zu einem Fehler. Beim Aufruf muß «optab» initialisiert sein und kann
// bereits Werte ungleich UNDEFINED enthalten).
////////////////////////////////////////////////////////////////////////////////////////////////////

bool VarScope::parse_cfg1(unsigned char *optab, const char *cfg, bool silent)
{
   unsigned char val;
   unsigned opt;
   const char *c = cfg;
   while (next_option(&val, &opt, &c)) {

      // Fehler, wenn eine Option gleichzeitig mit '+' und '-' aufgeführt ist
      if (optab[opt] != UNDEF && optab[opt] != val) {
	 const char *confl = 0;
	 if (val == ON && opts_[opt].group_ != 0) {
	    for (unsigned i = 0; i < n_opts_; ++i) {
	       if (opts_[i].group_ == opts_[opt].group_ && optab[i] == ON) {
		  confl = opts_[i].name_;
		  break;
	       }
	    }
	 }
	 if (!silent) YUERR(L24,conflicting_options(opts_[opt].name_,confl));
	 return false;
      }

      // Optionswert setzen (bei Auswahloption, alle anderen Werte auf OFF)
      optab[opt] = val;
      if (opts_[opt].group_ != 0 && val == ON) {
	 for (unsigned k = 0; k < n_opts_; ++k) {
	    if (k != opt && opts_[k].group_ == opts_[opt].group_)
	       optab[k] = OFF;
	 }
      }
   }

   // Wenn hier noch Zeichen übrig sind, war «c» keine gültige Konfiguration.
   return c == 0 || skip_blank(&c) == 0;
}


////////////////////////////////////////////////////////////////////////////////////////////////////
// Wandelt einen Konfigurationsstring in ein Array von Optionswerten um. Die Reihenfolge der
// Optionen ist durch «opts» vorgegeben, also gleich der Reihenfolge, in der die Optionen definiert
// wurden.
// «optab» muß beim Aufruf entweder 0 oder ein mit malloc() allokierte Puffer sein.
////////////////////////////////////////////////////////////////////////////////////////////////////

bool VarScope::parse_cfg(unsigned char *&optab, const char *cfg1, const char *cfg2, bool silent)
{
   array_realloc(optab,n_opts_);
   memset(optab, UNDEF, n_opts_);
   if (cfg1 && !parse_cfg1(optab,cfg1,silent)) return false;
   if (cfg2 && !parse_cfg1(optab,cfg2,silent)) return false;
   return true;
}



////////////////////////////////////////////////////////////////////////////////////////////////////
// Prüft, ob das Argument eine gültige Konfiguration ist
////////////////////////////////////////////////////////////////////////////////////////////////////

bool var_cfg_is_valid(VarScope *scope, const char *cfg)
{
   if (scope == 0) scope = &global_scope;
   static unsigned char *t = 0;
   return scope->parse_cfg(t,cfg,0,false);
}


////////////////////////////////////////////////////////////////////////////////////////////////////
// Kompatibilität zweier Konfigurationen
////////////////////////////////////////////////////////////////////////////////////////////////////

bool VarScope::compat(unsigned char const *from, unsigned char const *to) const
{
   for (unsigned i = 0; i < n_opts_; ++i) {
      if (from[i] == ON && to[i] == OFF)
	 return false;
      if (from[i] == OFF && to[i] == ON)
	 return false;
   }
   return true;
}


////////////////////////////////////////////////////////////////////////////////////////////////////
// Return: -1: nicht enthalten. Sonst: Anzahl der notwendigen Änderungen
////////////////////////////////////////////////////////////////////////////////////////////////////

bool var_cfg_compatible(VarScope *scope, const char *from, const char *to)
{
   static unsigned char *t = 0;
   if (!scope->parse_cfg(t,to,0,false)) return -1;
   static unsigned char *f = 0;
   if (!scope->parse_cfg(f,from,0,false)) return -1;
   return scope->compat(f,t);
}


////////////////////////////////////////////////////////////////////////////////////////////////////
// Aktiviert eine Konfiguration «cfg», falls sie zur aktuellen Konfiguration kompatibel ist. Falls
// nicht, bleibt die aktuelle Konfiguration unverändert.
////////////////////////////////////////////////////////////////////////////////////////////////////

bool var_try_cfg_change(VarScope *scope, const char *cfg)
{
   if (cfg && *cfg) {
      static unsigned char *tmp = 0;
      if (!scope->parse_cfg(tmp,cfg,0,true)) return false;
      if (!scope->compat(tmp,scope->ccfg_)) return false;
      scope->parse_cfg1(scope->ccfg_,cfg,false);
   }
   return true;
}



////////////////////////////////////////////////////////////////////////////////////////////////////
// Wendet «cfg» auf die aktive Konfiguration an. In «cfg» nicht aufgeführte Optionen behalten ihren
// aktuellen Wert.
////////////////////////////////////////////////////////////////////////////////////////////////////

void var_cfg_change(VarScope *scope, const char *cfg)
{
   if (cfg && *cfg) {
      scope->opts_finalized_ = true;
      scope->parse_cfg1(scope->ccfg_,cfg,false);
   }
}


////////////////////////////////////////////////////////////////////////////////////////////////////
// Liefert die aktive Konfiguration in normalisierter Form, d.h.
// - Die Reihenfolge der Optionen entspricht der Definitionsreihenfolge
// - Eingeschaltete Optionen werden immer mit '+' aufgeführt, auch wenn dadurch ein "+" am Anfang
//   des Strings entsteht
// - Ausgeschaltete Optionen in einer Gruppe werden nur dann aufgeführt, wenn kein Mitglied der
//   Gruppe eingeschaltet ist.
// - Das Ergebnis enthält keine Leerzeichen.
////////////////////////////////////////////////////////////////////////////////////////////////////

const char *var_current_cfg(VarScope *scope)
{
   return scope->current_cfg();
}

const char *VarScope::current_cfg()
{
   opts_finalized_ = true;            // Ab jetzt keine neuen Optionen oder Werte mehr zulassen
   Str buf;
   for (unsigned i = 0; i < n_opts_; ++i) {
      if (ccfg_[i] == ON) {
	 buf.append("+");
	 buf.append(opts_[i].name_);
      }
      else if (ccfg_[i] == OFF) {
	 // Ausgeschaltete Optionen innerhalb einer Gruppe nur dann anzeigen,
	 // wenn kein Element der Gruppe eingeschaltet ist.
	 bool show = true;
	 if (opts_[i].group_ != 0) {
	    for (unsigned k = 0; show && k < n_opts_; ++k) {
	       if (opts_[k].group_ == opts_[i].group_ && ccfg_[k] == ON)
		  show = false;
	    }
	 }
	 if (show) {
	    buf.append("-");
	    buf.append(opts_[i].name_);
	 }
      }
   }
   return str_freeze(buf);
}


////////////////////////////////////////////////////////////////////////////////////////////////////
// Gibt alle Optionen zu Debugzwecken aus.
////////////////////////////////////////////////////////////////////////////////////////////////////

void var_dump_options(VarScope *scope, const char *prj_root)
{
   Message(MSG_0,"----- %s (%s) -----",Msg::options(),prj_root);
   for (unsigned i = 0; i < scope->n_opts_; ++i) {
      if (scope->opts_[i].group_)
	 printf("%s:%s\n", scope->opts_[i].group_, scope->opts_[i].name_);
      else
	 printf("%s\n", scope->opts_[i].name_);
   }
}


////////////////////////////////////////////////////////////////////////////////////////////////////
// Der Konstruktor speichert den aktuellen Zustand aller Optionen.
////////////////////////////////////////////////////////////////////////////////////////////////////

CfgFreeze::CfgFreeze(VarScope *scope)
   :cfg_(0), scope_(scope)
{
   scope->opts_finalized_ = true;            // Ab jetzt keine neuen Optionen oder Werte mehr zulassen
   array_realloc(cfg_, scope->n_opts_);
   for (unsigned i = 0; i < scope->n_opts_; ++i)
      cfg_[i] = scope->ccfg_[i];
}


////////////////////////////////////////////////////////////////////////////////////////////////////
// Der Destruktor stellt den Zustand aller Optionen wieder so her, wie er beim Aufruf des
// Konstruktor gesichert wurde.
////////////////////////////////////////////////////////////////////////////////////////////////////

CfgFreeze::~CfgFreeze()
{
   for (unsigned i = 0; i < scope_->n_opts_; ++i)
      scope_->ccfg_[i] = cfg_[i];
   free(cfg_);
}


////////////////////////////////////////////////////////////////////////////////////////////////////
// String anhängen und ggf. ein " " einfügen.
////////////////////////////////////////////////////////////////////////////////////////////////////

static void append_val(char **buf, const char *val)
{
   size_t olen = *buf ? strlen(*buf) : 0;
   size_t const vlen = strlen(val);
   array_realloc(*buf,olen + (olen ? 1 : 0) + vlen + 1);
   if (olen) (*buf)[olen++] = ' ';
   memcpy(*buf + olen,val,vlen + 1);
}


////////////////////////////////////////////////////////////////////////////////////////////////////
// Variable erzeugen.
////////////////////////////////////////////////////////////////////////////////////////////////////

void VarScope::create_var(const char *name, const char *value, const SrcLine *sl)
{
   size_t pos;
   if (my_bsearch(&pos,vars_,n_vars_,name)) {
      YUERR(L03,redefined('$',name,vars_[pos].src_));
   }
   else if (find_opt(name) != NOTFOUND || group_exists(name))
      YUERR(L03,redefined(0,name,0));
   else {
      array_insert(vars_,n_vars_,pos);
      Variable &v = vars_[pos];
      v.name_ = name;
      v.src_ = sl;
      v.value_ = 0;
      append_val(&v.value_,value);
   }
}


////////////////////////////////////////////////////////////////////////////////////////////////////
// Liefert «true», wenn der String «s» den String «t» als Wort enthält,
// d.h. links und rechts des Teilstrings steht entweder ein Leerzeichen oder Tab,
// oder der Anfang bzw. Ende des Strings.
// s: Text, in dem der Teilstring gesucht werden soll.
// w: Teilstring.
////////////////////////////////////////////////////////////////////////////////////////////////////

static bool contains_word(const char *s, const char *w)
{
   const char *p = strstr(s, w);
   if (p != 0 && (p == s || IS_SPACE( p[-1]))) {
      const char *e = p + strlen(w);
      if (*e == 0 || IS_SPACE( *e))
	 return true;
   }
   return false;
}


////////////////////////////////////////////////////////////////////////////////////////////////////
// Definiert eine Systemvariable und exportierte sie in das Skript-Environment
////////////////////////////////////////////////////////////////////////////////////////////////////

static void export_system_variable(const char *name, const char *value)
{
    char tmp[100];
    snprintf(tmp,sizeof(tmp),"YABU%s",name);
    job_export(tmp,value);
    global_scope.var_assign(name,SET,str_freeze(value),0);
}


////////////////////////////////////////////////////////////////////////////////////////////////////
// Definiert die Systemvariablen (_HOSTNAME, _SYSTEM, ...)
////////////////////////////////////////////////////////////////////////////////////////////////////

void var_set_system_variables()
{
   char buf[500];

   global_scope.var_assign("_HOSTNAME",SET,my_hostname(),0);
   global_scope.var_assign("_SYSTEM",  SET,my_osname(),0);
   global_scope.var_assign("_RELEASE", SET,my_osrelease(),0);
   global_scope.var_assign("_MACHINE", SET,my_machine(),0);
   global_scope.var_assign("_LOCAL_CFG", SET,job_local_cfg(),0);

   // _PID: Prozeß-Id
   snprintf(buf, sizeof(buf), "%u", (unsigned) getpid());
   export_system_variable("_PID", buf);

   // _TIMESTAMP: aktuelle Zeit (JJJJ-MM-TT-HH-MM-SS)
   time_t now = time(0);
   struct tm *lt = gmtime(&now);
   snprintf(buf, sizeof(buf), "%4.4d-%2.2d-%2.2d-%2.2d-%2.2d-%2.2d",
            lt->tm_year + 1900, lt->tm_mon + 1, lt->tm_mday, lt->tm_hour, lt->tm_min, lt->tm_sec);
   export_system_variable("_TIMESTAMP", buf);

   // _CWD: aktuelles Verzeichnis
   export_system_variable("_CWD",yabu_wd);
}


////////////////////////////////////////////////////////////////////////////////////////////////////
// Weist der Variablen «name» den Wert «value» zu. «mode» ist die Art der Zuweisung (=, += oder ?=).
////////////////////////////////////////////////////////////////////////////////////////////////////

void VarScope::var_assign(const char *name, VarMode_t mode, const char *value, const SrcLine *sl)
{
   Variable *v = find_var(name);
   if (mode == SET) {
      if (v)
	 YUERR(L03,redefined('$',name,v->src_));
      else
	 create_var(name, value, sl);
   } else if (v == 0)
      YUFTL(L01,undefined('$', name));
   else if (mode == APPEND || !contains_word(v->value_, value))
      append_val(&v->value_,value);
}


////////////////////////////////////////////////////////////////////////////////////////////////////
// Führt eine bedingte Wertzuweisung aus. Die Konfiguration wird durch Aneinanderhängen von
// «options1» und «options2» gebildet. Sind beide leer, dann führt die Funktion eine unbedingte
// Zuweisung aus. «mode» ist die Art der Zuweisung (=, += oder ?=).
////////////////////////////////////////////////////////////////////////////////////////////////////

void var_assign(VarScope *scope, const char *name, const char *options1, const char *options2,
       VarMode_t mode, const char *value, const SrcLine *sl)
{
   if (*name == '_' && scope != &global_scope) {
      YUERR(L13,assignment_to_system_variable(name));	// [T:052]
   } else if (!var_check_name(name))
      ;
   else if ((options1 == 0 || *options1 == 0) && (options2 == 0 || *options2 == 0))
      scope->var_assign(name, mode, value,sl);
   else {
      // Optionale Zuweisung: Variable muß bereits definiert sein (auch wenn mode == SET)
      Variable *v = scope->find_var(name);
      if (v == 0)
	 YUFTL(L01,undefined('$',name));
      else {
	 scope->opts_finalized_ = true;
	 OptionalValue *ov = new OptionalValue(value, sl);
	 scope->parse_cfg(ov->cfg_, options1, options2,false);
	 switch (mode) {
	    case SET:
	       ov->append(&v->Set);
	       break;
	    case APPEND:
	       ov->append(&v->Append);
	       break;
	    case MERGE:
	       ov->append(&v->Merge);
	       break;
	 }
      }
   }
}


////////////////////////////////////////////////////////////////////////////////////////////////////
// True, wenn «a» in «b» enthalten ist
////////////////////////////////////////////////////////////////////////////////////////////////////

bool VarScope::is_subset_of(unsigned char const *a, unsigned char const *b) const
{
   for (unsigned i = 0; i < n_opts_; ++i) {
      if (a[i] != UNDEF && b[i] != a[i])
	 return false;
   }
   return true;
}


////////////////////////////////////////////////////////////////////////////////////////////////////
// Liefert den Wert einer Variablen in der aktiven Konfiguration. «defval» ist ein Defaultwert
// für den Fall, daß keine Variable mit dem namen «name» existiert. Ist «defval» gleich 0, dann
// führt ein unbekannter Name zu einem Fehler.
////////////////////////////////////////////////////////////////////////////////////////////////////

bool var_get(VarScope *scope, Str &val, const char *name, const char *defval)
{
   return scope->var_get(val,name,defval);
}

bool var_get_global(Str &val, const char *name, const char *defval)
{
   return global_scope.var_get(val,name,defval);
}

bool VarScope::var_get(Str &val, const char *name, const char *defval)
{
   // Optionen und Optionsgruppen werden auf $(_opt) abgebildet
   if (*name == '_') {
      for (unsigned i = 0; i < n_opts_; ++i) {
	 // $(_opt) liefert  "+", "-" oder "" [T:054]
	 if (!strcmp(opts_[i].name_, name + 1)) {
	    val = (ccfg_[i] == ON) ? "+" : ((ccfg_[i] == OFF) ? "-" : "");
	    return true;
	 }
	 // $(_grp) liefert den Namen der aktiven Option oder "" [T:055]
	 if (opts_[i].group_ != 0 && !strcmp(opts_[i].group_, name + 1)) {
	    unsigned k = i;
	    while (k < n_opts_ && (opts_[k].group_ != opts_[i].group_ || ccfg_[k] != ON))
	       ++k;
	    val = (k < n_opts_) ? opts_[k].name_ : (defval ? defval : "");
	    return true;
	 }
      }
   }

   // Spezialfall: $(_CONFIGURATION) = aktuelle Konfiguration
   if (!strcmp(name, "_CONFIGURATION")) {
      val = var_current_cfg(this);
      return true;
   }

   // Sonstige Systemvariablen sind global
   if (*name == '_' && this != &global_scope)
      return global_scope.var_get(val,name,defval);

   // Es ist eine normale Variable
   Variable *v = find_var(name);
   if (v == 0) {
      if (defval != 0)
	 val = defval;
      else
	 YUFTL(L01,undefined('$',name));
      return defval != 0;
   }
   if (v->locked_) {
      YUERR(L04,circular_dependency('$',name));
      return false;
   }

   // Suche bedingte =-Zuweisungen:
   // - Gibt es genau eine, nehmen wie diese (und verwerfen den Standardwert).
   // - Gibt es keine, nehmen wir den globalen Wert.
   // - Gibt es mehr als eine: Fehler V35.
   const char *val1 = v->value_;  // Globaler Wert
   OptionalValue *ov0 = 0;
   for (OptionalValue *ov = v->Set; ov; ov = ov->next_) {
      if (is_subset_of(ov->cfg_,ccfg_)) {
	 if (ov0) {
	    YUERR(L35,conflicting_values(v->name_,var_current_cfg(this),ov0->src_,ov->src_));
	    return false;
	 } else {
	    ov0 = ov;
	    val1 = ov->value_;
	 }
      }
   }
   val = val1;

   // Alle bedingten "+="-Zuweisungen anwenden
   for (OptionalValue * ov = v->Append; ov; ov = ov->next_) {
      if (is_subset_of(ov->cfg_,ccfg_)) {
	 if (val.len() > 0) val.append(" ");
	 val.append(ov->value_);
      }
   }

   // Alle bedingten "?="-Zuweisungen anwenden
   for (OptionalValue * ov = v->Merge; ov; ov = ov->next_) {
      if (is_subset_of(ov->cfg_,ccfg_) && !contains_word(val, ov->value_)) {
	 if (val.len() > 0) val.append(" ");
	 val.append(ov->value_);
      }
   }

   return true;
}


////////////////////////////////////////////////////////////////////////////////////////////////////
// Bedingte Wertzuweisungen ausgeben (Debug)
////////////////////////////////////////////////////////////////////////////////////////////////////

void VarScope::dump_opt_values(OptionalValue const *ov, const char *mode)
{
   for (; ov != 0; ov = ov->next_) {
      printf("  [");
      for (unsigned o = 0; o < n_opts_; ++o) {
	 if (ov->cfg_[o] != UNDEF)
	    printf("%c%s",ov->cfg_[o] == ON ? '+' : '-',opts_[o].name_);
      }
      printf("]%s%s @%s\n", mode, ov->value_,
	    Msg::line(ov->src_->file,ov->src_->line));
   }
}


////////////////////////////////////////////////////////////////////////////////////////////////////
// Alle Variablen ausgeben (Debug)
////////////////////////////////////////////////////////////////////////////////////////////////////

void var_dump(VarScope *scope, const char *prj_root)
{
   Message(MSG_0,"----- %s (%s) -----",Msg::variables(),prj_root);
   for (unsigned i = 0; i < scope->n_vars_; ++i) {
      printf("%s=%s\n", scope->vars_[i].name_, (const char*) scope->vars_[i].value_);
      scope->dump_opt_values(scope->vars_[i].Set, "=");
      scope->dump_opt_values(scope->vars_[i].Append, "+=");
      scope->dump_opt_values(scope->vars_[i].Merge, "?=");
   }
}

////////////////////////////////////////////////////////////////////////////////////////////////////
// Der Konstruktor markiert die angegebene Variable als "in Benutzung".
// Ist die Variable bereits in Benutzung, tritt ein Fehler auf.
////////////////////////////////////////////////////////////////////////////////////////////////////

VarLock::VarLock(VarScope *scope, const char *name)
   :var_(scope->find_var(name))
{
   if (var_) {
      if (var_->locked_)
         YUERR(L04,circular_dependency('$',name));
      var_->locked_ = true;
   }
}


////////////////////////////////////////////////////////////////////////////////////////////////////
// Destruktor
////////////////////////////////////////////////////////////////////////////////////////////////////

VarLock::~VarLock()
{
   release();
}


////////////////////////////////////////////////////////////////////////////////////////////////////
// Sperre aufheben
////////////////////////////////////////////////////////////////////////////////////////////////////

void VarLock::release()
{
   if (var_) {
      var_->locked_ = false;
      var_ = 0;
   }
}



////////////////////////////////////////////////////////////////////////////////////////////////////
// Wert einer Variablen: $(NAME), $(n) oder $(*)
////////////////////////////////////////////////////////////////////////////////////////////////////

static bool get_value(VarScope *scope, Str &val, const char *name, const StringList *files)
{
   if (IS_DIGIT(*name)) {
      int n;
      if (!str2int(&n,name) || n < 0) {
	 YUERR(S08,bad_int_value(name));
	 return false;
      } else if (files == 0 || (size_t) n >= files->size()) {
	 YUERR(L01,undefined('$',name));          // [T:021]
	 return false;
      } else
	 val = (*files)[n];
   } else if (!strcmp(name,"*")) {
      if (files == 0) {
	 YUERR(L01,undefined('$',name));
	 return false;
      } else {
	 for (unsigned i = 1; i < files->size(); ++i) {
	    if (i > 1)
	       val.append(" ",1);
	    val.append((*files)[i]);
	 }
      }
   } else if (!var_get(scope,val,name))
      return false;

   return true;
}




////////////////////////////////////////////////////////////////////////////////////////////////////
// Kopiert «s» nach «buf» und ersetzt dabei Variablen (einschließlich $(0),$(1),...) und
// Platzhalter. Vorhandener Text in «buf» bleibt erhalten; der Inhalt von «s» wird angehängt.
//
// Pro Aufruf kann nur ein Typ von Platzhalter ersetzt werden; «args_tag» ist das
// Platzhalterzeichen, und «args» enthält die einzusetzenden Werte. «files» enthält die
// einzusetzenden Dateinamen für $(0) usw.
//
// Ist «args» gleich 0 sein, dann unterbleibt die Platzhalter-Ersetzung. Etwaige in «s»
// auftretendes Platzhalterzeichen werden unverändert nach «buf» übernommen.
//
// «files» = 0 wird wie eine leere Liste behandelt.
////////////////////////////////////////////////////////////////////////////////////////////////////

void expand_vars(VarScope *scope, Str &buf, const char *s, const StringList *args, char args_tag,
      const StringList *files)
{
   if (s == 0 || *s == 0)
      return;
   // Platzhalter ersetzen, aber nur wenn das Muster welche enthält, ansonsten wie
   // normalen Text behandeln (insbesondere n i c h t  '%%'-->'%' umwandeln). [T:091]
   Str tmp;
   const char *c = s;
   if (args && args->size() > 0) {
      expand_substrings(tmp, s, args, args_tag);
      c = tmp.data();
   }

   // Variablen ersetzen
   while (exit_code <= 1 && *c != 0) {
      if (*c == '$' && c[1] == '(') {
	 VariableName vn;
	 if (!vn.split(&c))
	    break;
         Str vname;
         expand_vars(scope, vname, vn.name, 0, 0, files);	// Variablennamen rekursiv ersetzen
	 Str val;
	 if (!get_value(scope,val,vname,files))
	    break;
	 VarLock vl(scope,vname);
         if (vn.pat == 0)				// Ohne Transformation
            expand_vars(scope, buf, val, 0, 0, files);
         else {						// Mit  Transformation
            Str eval;
            expand_vars(scope, eval, val, 0, 0, files);	// Variablenwert rekursiv ersetzen

	    // Die Transformationsregel darf die Variable verwenden [T:vs08]
	    vl.release();
	    Str epat;
	    if (vn.pat) {
	       expand_vars(scope, epat,vn.pat,0,0,files);
	       vn.pat  = epat;
	    }

            const char *w;
            bool first = true;
            for (char *vp = eval.data(); exit_code <= 1 && (w = str_chop(&vp)) != 0;) {
	       const char *r = vn.repl;
               StringList substr;
               if (!substr.match(vn.ph, vn.pat, w)) {
		  switch (vn.tr_mode) {
		     case ':': YUFTL(L14,no_match(vn.short_name, w, vn.pat)); break;	// [T:vs19]
		     case '!': continue;	// '!': unterdrücken [T:vs04]
		     default: r = 0; break;	// '?': unverändert durchlassen [T:vs04]
		  }
               }
	       Str tmp;
	       if (r) {						// [T:vs22]
		  expand_vars(scope, tmp, r, &substr, vn.ph, files);
		  if (*(w = tmp) == 0) continue;                // Leeres Wort
	       }
	       buf.append(" ",first ? 0 : 1).append(w);
	       first = false;
            }
         }
      } else {						// Normalen Text kopieren
	 const char *b = c++;
	 while (*c != '$' && *c != 0) ++c;
         buf.append(b,c-b);
      }
   }
}


// vim:sw=3:cin:fileencoding=utf-8
