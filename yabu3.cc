////////////////////////////////////////////////////////////////////////////////////////////////////
// I, the creator of this work, hereby release it into the public domain. This applies worldwide.
// In case this is not legally possible: I grant anyone the right to use this work for any purpose,
// without any conditions, unless such conditions are required by law.
////////////////////////////////////////////////////////////////////////////////////////////////////

// Auswertung des (vorverarbeiteten) Buildfiles.

#include "yabu.h"


////////////////////////////////////////////////////////////////////////////////////////////////////
// Durchläuft den -- bei der Initialisierung des Objektes festgelegten -- Bereich von Eingabezeilen
// und verarbeitet die darin enthaltenen Anweisungen (Regeln, Optionen und Konfigurationen).
////////////////////////////////////////////////////////////////////////////////////////////////////

void Project::parse_buildfile(SrcLine const *begin, SrcLine const *end)
{
   cur_ = begin;
   eoi_ = end;
   while (exit_code < 2 && cur_ < eoi_) {
      YabuContext ctx(cur_,0,0);
      const char *c = cur_->text;
      Assignment as;

      if (IS_SPACE(*c))
         YUERR(S01,syntax_error());
      else if (skip_str(&c, "!options "))
         do_options(c);
      else if (skip_str(&c, "!configuration "))
         do_configuration(c);
      else if (skip_str(&c, "!configure "))
         do_configure(c);
      else if (skip_str(&c, "!settings "))
         do_settings(c);
      else if (skip_str(&c, "!queue "))
         ++cur_;		// Wird nicht mehr benutzt
      else if (skip_str(&c, "!export "))
         do_export(c);
      else if (skip_str(&c, "!serialize "))
         do_serialize(c);
      else if (skip_str(&c, "!project "))
         do_project(c);
      else if (as.split(c)) {
         var_assign(vscope_, as.name, 0, as.cfg, as.mode, as.value, cur_);
         ++cur_;
      }
      else
         do_rule();             // Regel oder Vorlage
   }
}



////////////////////////////////////////////////////////////////////////////////////////////////////
// Verarbeitet einen !options-Abschnitt. Beim Aufruf ist «cur_» die Zeile, welche die !options-
// Anweisung enthält, und «*c» ist das erste Zeichen hinter "!options".
////////////////////////////////////////////////////////////////////////////////////////////////////

void Project::do_options(const char *c)
{
   // Kein weiterer Text hinter "!options" erlaubt
   if (skip_blank(&c) != 0)
      YUERR(S01,syntax_error());
   else {
      // Optionen (eingerückte Zeilen) auswerten
      for (++cur_; exit_code <= 1 && cur_ < eoi_ && IS_SPACE(*cur_->text); ++cur_)
	 var_parse_options(vscope_, cur_->text);
   }
}


////////////////////////////////////////////////////////////////////////////////////////////////////
// Liefert den Text zwischen '[' und ']' oder 0 falls kein '[' gefunden.
////////////////////////////////////////////////////////////////////////////////////////////////////

static const char *parse_cfg(const char **cp)
{
   const char *c = *cp;
   if (!skip_str(&c, " [ ")) return 0;
   const char *beg = c;
   while (*c != 0 && *c != ']') ++c;
   if (*c != ']') {
      YUERR(S02,nomatch("[","]"));
      *cp = c;
   } else
      *cp = c + 1;
   return str_freeze(beg,c - beg);
}


////////////////////////////////////////////////////////////////////////////////////////////////////
// Verarbeitet einen !configuration-Abschnitt. Beim Aufruf ist «cur_» die Zeile, welche die
// !configuration Anweisung enthält, und «*c» ist das erste Zeichen hinter "!configuration".
////////////////////////////////////////////////////////////////////////////////////////////////////

void Project::do_configuration(const char *c)
{
   // Der Rest der Zeile enthält Optionen für die folgenden Zuweisungen
   const char *cfg = parse_cfg(&c);
   if (cfg == 0) {
      if (*c != 0)
	 cfg = c;				// alte Syntax ohne [...]
      else
	 YUERR(S01,syntax_error());
   } else if (skip_blank(&c) != 0)
      YUERR(S01,syntax_error(c));

   // Und jetzt die eigentlichen Zuweisungen
   for (++cur_; exit_code <= 1 && cur_ < eoi_ && IS_SPACE(*cur_->text); ++cur_) {
      Assignment as;
      if (!as.split(cur_->text))
         YUERR(S01,syntax_error());
      else
         var_assign(vscope_, as.name, cfg, as.cfg, as.mode, as.value, cur_);
   }
}


CfgCommand::CfgCommand(SrcLine const *s, char const *sel, SrcLine const *beg, SrcLine const *end)
   : next_(0), src_(s), selector_(sel), beg_(beg), end_(end)
{
}


////////////////////////////////////////////////////////////////////////////////////////////////////
// Verarbeitet eine !configure-Anweisung. Die Auswertung erfolgt in Phase 2!
// Syntax 1 (mit Skript):  !configure
// Syntax 2 (mit Skript):  !configure selector
// Syntax 3 (ohne Skript): !configure [cfg] tgt tgt ...
////////////////////////////////////////////////////////////////////////////////////////////////////

void Project::do_configure(const char *c)
{
   const char * const cfg = parse_cfg(&c);
   if (cfg != 0) {				// 3. Syntax
      StringList patterns;
      const char *w;
      while ((w = next_word(&c)) != 0)
	 patterns.append(w);
      add_cfg_rule(cfg,patterns);
      ++cur_;
   } else {					// 1. oder 2. Syntax
      const char *sel = next_word(&c);
      if (next_word(&c) != 0) {
	 YUERR(S01,syntax_error());
	 return;
      }
      SrcLine const *src = cur_++;
      SrcLine const *b, *e;
      const char *type;
      get_script(&type,&b,&e);
   
      // Anweisung an Liste anhängen
      ac_tail_ = &(*ac_tail_ = new CfgCommand(src,sel,b,e))->next_;
   }
}


////////////////////////////////////////////////////////////////////////////////////////////////////
// Verarbeitet eine !settings-Anweisung.
////////////////////////////////////////////////////////////////////////////////////////////////////

void Project::do_settings(const char *c)
{
   // Kein weiterer Text hinter "!settings" erlaubt
   skip_blank(&c);
   if (*c != 0)
      YUERR(S01,syntax_error());

   // Alle folgenden eingerückten Zeilen als Einstellungen interpretieren.
  // In Unterprojekten ignorieren wir die EInstellungen.
   for (++cur_; cur_ < eoi_ && IS_SPACE(*cur_->text); ++cur_) {
      if (parent_ == 0)
         Setting::parse_line(cur_->text, Setting::BUILDFILE);
   }
}


////////////////////////////////////////////////////////////////////////////////////////////////////
// Verarbeitet eine !export-Anweisung.
////////////////////////////////////////////////////////////////////////////////////////////////////


static void export_vars(const char *c)
{
   const char *name;
   while ((name = next_word(&c)) != 0) {
      if (*name == '$')			// !export $NAME
	 job_export_env(name + 1);
      else {				// !export NAME
	 if (var_check_name(name))
	    job_export_var(name);
      }
   }
}


void Project::do_export(const char *c)
{
   // 1. Zeile
   export_vars(c);

   // Folgezeilen
   for (++cur_; exit_code <= 1 && cur_ < eoi_ && IS_SPACE(*cur_->text); ++cur_) {
      if (strchr(cur_->text,'=') != 0) {			// !export NAME=WERT
	 Assignment as;
	 if (!as.split(cur_->text) || as.cfg || as.mode != SET)
	    YUERR(S01,syntax_error());
	 else
	    job_export(as.name,as.value);
      } else							// !export [$]VAR ...
	 export_vars(cur_->text);
   }
}


////////////////////////////////////////////////////////////////////////////////////////////////////
// Verarbeitet das Skript ab Zeile «cur_». Nach erfolgreicher Ausführung sind «type», «bp» und «ep»
// gesetzt, und «cur_» zeigt auf die nächste Zeile nach dem Skript.
////////////////////////////////////////////////////////////////////////////////////////////////////

bool Project::get_script(const char **type, const SrcLine **bp, const SrcLine **ep)
{
   *type = "build";	// Default
   *bp = *ep = 0;
   for (; cur_ < eoi_ && IS_SPACE(*cur_->text); ++cur_) {
      const char *c = cur_->text;
      const char *beg = c;
      skip_blank(&beg);
      const char *end = beg + strlen(beg);
      if (*beg == '[' && IS_IDENT(beg[1]) && end[-1] == ']' && IS_IDENT(end[-2])) {
	 // Abschnittsmarkierung ([...])
         if (*bp == 0)
            *type = str_freeze(beg + 1, end - beg - 2);
         else
            break;
      } else if (*bp == 0)			// 1. Skriptzeile
	    *bp = cur_;
   }
   *ep = cur_;
   return *bp != 0 && *bp < *ep;
}



////////////////////////////////////////////////////////////////////////////////////////////////////
// Verarbeitet den linken Teil (Ziele) einer Regel im Buildfile. Setzt die Attribute «Targets» und
// «is_alias_» der Regel.  Nach der Rückkehr zeigt «*rpp» auf das erste unverarbeitete Zeichen
// hinter dem ':' bzw '::'.
// r: Aktuelle Regel.
// rpp: Lesezeiger.
////////////////////////////////////////////////////////////////////////////////////////////////////

static void parse_targets(Rule * r, const char **rpp)
{
   const char *c = *rpp;

   // Suche den ':', aber beachte Klammerebenen.
   // Der Doppelpunkt in "$(VAR:*=*.o)" ist zum Beispiel nicht relevant.
   int level = 0;
   skip_blank(&c);
   const char *begin = c;
   while (*c && (*c != ':' || level > 0)) {
      if (*c == '(')
         ++level;
      else if (*c == ')')
         --level;
      ++c;
   }
   if (*c != ':' || c == begin)
      YUERR(S01,syntax_error());        // Fehlendes ':' oder keine Ziele [T:bf04]

   r->targets_ = str_freeze(begin, c - begin);

   // Art der Regel: ":", "::" oder ":?"
   ++c;
   if (*c == ':') {
      r->is_alias_ = true;
      ++c;
   } else if (*c == '?') {
      r->create_only_ = true;
      ++c;
   }

   *rpp = c;
}


static void set_script(SrcLine const **bp, SrcLine const **ep, 
      SrcLine const *beg, SrcLine const *end, const char *tag)
{
   if (*bp)
      YUERR(L03,redefined('[',tag,0));	// [T:bf03]
   *bp = beg;
   *ep = end;
}


////////////////////////////////////////////////////////////////////////////////////////////////////
// Verarbeitet eine Regel.
////////////////////////////////////////////////////////////////////////////////////////////////////

void Project::do_rule()
{
   const char *c = cur_->text;

   Rule *r = new Rule(cur_);
   add_rule(r);
   parse_targets(r, &c);
   r->config_ = parse_cfg(&c);

   // Der Rest der Zeile sind die Quellen.
   skip_blank(&c);
   r->sources_ = str_freeze(c);
   ++cur_;

   // Skript.
   const char *tag;
   const SrcLine *beg, *end;
   while (get_script(&tag, &beg, &end)) {
      if (!strcmp(tag, "build"))
	 set_script(&r->script_beg_,&r->script_end_,beg,end,tag);
      else if (!strcmp(tag, "auto-depend"))
	 set_script(&r->adscript_beg_,&r->adscript_end_,beg,end,tag);
      else
         YUERR(S01,syntax_error(tag));		// [T:bf05]
   }

   if (r->create_only_ && r->script_beg_ == 0)
      YUERR(S04,missing_script());
}


////////////////////////////////////////////////////////////////////////////////////////////////////
// Verarbeitet eine !project-Anweisung
////////////////////////////////////////////////////////////////////////////////////////////////////

void Project::do_project(const char *c)
{
   // Der Rest der Zeile hat die Form <Pfad>/<Buildfile>
   skip_blank(&c);
   const char *fn = strrchr(c,'/');
   if (fn == 0 || *c == '/') {
      YUERR(L43,invalid_prj_root(c));
      return;
   }
   ++fn;
   const char *rroot = str_freeze(c,fn - c);

   if (!strncmp(rroot,"../",3) || strstr(rroot,"/../")) {
      YUERR(L43,invalid_prj_root(rroot));
      return;
   }

   add_prj(rroot,fn,cur_);
   ++cur_;
}


////////////////////////////////////////////////////////////////////////////////////////////////////
// Verarbeitet eine !serialize-Anweisung
////////////////////////////////////////////////////////////////////////////////////////////////////

void Project::do_serialize(const char *c)
{
   StringList tgts;
   const char *tag = 0;
   while (const char *w = next_word(&c)) {
      // Das erste Argument kann eine Gruppen-Id der Form <...> sein
      if (tgts.size() == 0 && tag == 0 && (*w == '<' && w[strlen(w)-1] == '>'))
	 tag = w;
      else
         tgts.append(w);
   }
   if (tgts.size() != 0)
      Serial::create(tag,tgts);
   ++cur_;
}


// vim:sw=3 cin fileencoding=utf-8
