////////////////////////////////////////////////////////////////////////////////////////////////////
// I, the creator of this work, hereby release it into the public domain. This applies worldwide.
// In case this is not legally possible: I grant anyone the right to use this work for any purpose,
// without any conditions, unless such conditions are required by law.
////////////////////////////////////////////////////////////////////////////////////////////////////

// Implementierung der Klasse «Project»

#include "yabu.h"

#include <time.h>
#include <utime.h>
#include <unistd.h>



// Pseudo-Regel für intern generierte Abbhängigkeiten.

static const SrcLine INTERNAL_SRC = {"[yabu]",0};
const Rule YABU_INTERNAL_RULE(&INTERNAL_SRC);


static unsigned count_dirs(const char *path)
{
   unsigned n = 0;
   for (; *path; ++path)
      if (*path == '/') ++n;
   return n;
}

////////////////////////////////////////////////////////////////////////////////////////////////////
// Hilfsklasse zur Variablenersetzung in einem Skript
////////////////////////////////////////////////////////////////////////////////////////////////////

class ScriptExpander: public CfgFreeze
{
public:
   ScriptExpander(Target *tgt, VarScope *vscope)
      : CfgFreeze(vscope), vscope_(vscope), tgt_(tgt), lp_(0)
   {
      var_cfg_change(vscope,tgt->build_cfg_);
   }
   bool expand(Str &buf, SrcLine const *beg, SrcLine const *end);
private:
   VarScope * const vscope_;
   Target const * const tgt_;
   SrcLine const *lp_;		// Aktuelle Zeile
};


////////////////////////////////////////////////////////////////////////////////////////////////////
// Fügt die Skriptzeilen zu einem String zusammen und ersetzt Variablen, '%' und $(n).
////////////////////////////////////////////////////////////////////////////////////////////////////

bool ScriptExpander::expand(Str &buf, SrcLine const *beg, SrcLine const *end)
{
   // Einrückung der ersten Zeile berechnen
   unsigned indent = end > beg ? calc_indent(beg->text) : 0;

   unsigned last_line = 0;
   for (lp_ = beg; lp_ < end; ++lp_) {
      YabuContext ctx(lp_,0,0);

      // Leerzeilen oder Kommentarzeilen aus dem Buildfile ein eine Leerzeile übersetzen
      if (last_line != 0 && lp_->line != last_line + 1)
	 buf.append("\n",1);
      last_line = lp_->line;

      // Einrückung entfernen
      const char *c = lp_->text;
      buf.append(remove_indent(&c,indent));

      // Variablen ersetzen und Zeile anhängen
      expand_vars(vscope_, buf, c, &tgt_->build_args_, '%', &tgt_->build_files_);
      if (buf.last() != '\n')
	 buf.append("\n",1);
   }
   return true;
}


////////////////////////////////////////////////////////////////////////////////////////////////////
// Konstruktor
////////////////////////////////////////////////////////////////////////////////////////////////////

Project::Project(Project *parent, const char *aroot, const char *rroot,
      const char *buildfile, const SrcLine *src)
   : aroot_(aroot), rroot_(rroot), depth_(count_dirs(rroot)),
     buildfile_(buildfile),
     state_file_(str_freeze(Str(aroot).append(buildfile).append(".state"))),
     src_(src),
     parent_(parent),
     ac_(0), ac_tail_(&ac_),
     next_(0),
     prjs_head_(0), prjs_tail_(&prjs_head_),
     vscope_(var_create_scope()),
     rules_head_(0), rules_tail_(&rules_head_),
     cfg_rules_head_(0), cfg_rules_tail_(&cfg_rules_head_),
     all_tgts_(0), n_tgts_(0),
     ts_algo_(global_ts_algo), discard_build_times_(false),
     state_(NEW),
     eoi_(0), cur_(0)
{
}


////////////////////////////////////////////////////////////////////////////////////////////////////
// Regel hinzufügen
////////////////////////////////////////////////////////////////////////////////////////////////////

void Project::add_rule(Rule *r)
{
   YABU_ASSERT(r->next_ == 0);
   *rules_tail_ = r;
   rules_tail_ = &r->next_;
}


////////////////////////////////////////////////////////////////////////////////////////////////////
// Konfigurationsregel hinzufügen
////////////////////////////////////////////////////////////////////////////////////////////////////

void Project::add_cfg_rule(const char *cfg, StringList &tgts)
{
   new ConfigureRule(&cfg_rules_tail_,cfg,tgts);
}


////////////////////////////////////////////////////////////////////////////////////////////////////
// Unterprojekt hinzufügen
////////////////////////////////////////////////////////////////////////////////////////////////////

void Project::add_prj(const char *rroot, const char *buildfile, const SrcLine *src)
{
   // Prüfen, ob der Pfad mit keinem der vorhandenen Unterprojekt kollidiert.
   size_t const rroot_len = strlen(rroot);
   for (Project *p = prjs_head_; p; p = p->next_) {
      size_t l = strlen(p->rroot_);
      if (l > rroot_len) l = rroot_len;
      if (memcmp(p->rroot_, rroot, l) == 0) {
	 YUERR(L42,conflicting_prj_root(rroot,p));
	 return;
      }
   }

   // Neues Projekt erzeugen
   const char * const aroot = str_freeze(Str(aroot_).append(rroot));
   *prjs_tail_ = new Project(this, aroot, rroot, buildfile,src);
   prjs_tail_ = &(*prjs_tail_)->next_;
}


////////////////////////////////////////////////////////////////////////////////////////////////////
// Bewertung eines Ziel-Musters.
////////////////////////////////////////////////////////////////////////////////////////////////////

static int pattern_priority(const char *pattern)
{
   // [T:rs03]
   int n = 0;
   for (const char *c = pattern; *c; ++c) {
      if (*c == '%')
         --n;
      else
         n += 50;
   }
   return n;
}


////////////////////////////////////////////////////////////////////////////////////////////////////
// Zerlegt die rechte Seite einer Regel in einzelne Quellen. Variablen und $(0) werden vorher
// ersetzt.
// Beim Aufruf muß «files» genau einen Wert enthalten, nämlich den Wert von $(0).
// Nach der Rückkehr ist «files[0]» unverändert, und die Quellen stehen ab «files[1]».
////////////////////////////////////////////////////////////////////////////////////////////////////

void Project::split_srcs(StringList &files, const char *srcs, StringList const &args)
{
   Str exp;
   expand_vars(vscope_, exp, srcs, &args, '%', &files);
   for (char *c = exp.data(); skip_blank(&c) != 0;) {
      char *beg = c;

      while (*c != 0 && *c != '(' && !IS_SPACE(*c))
         ++c;
      if (*c != '(')	// Einfacher Name
	 files.append(str_freeze(beg, c - beg));
      else {		// Bibliothek: lib.a(obj1.o obj2.o ...)
         unsigned const nlen = c - beg;
         char *opar = ++c;
         while (true) {
            while (IS_SPACE(*c))
               ++c;
            if (*c == ')')
               break;
            if (*c == 0) {
               YUERR(S02,nomatch("(",")"));
	       break;
	    }
            char *mbeg = c;
            while (*c != 0 && !IS_SPACE(*c) && *c != ')')
               ++c;
            unsigned const mlen = c - mbeg;
            memmove(opar, mbeg, mlen);
            char const save = opar[mlen];
            opar[mlen] = ')';
            files.append(str_freeze(beg, nlen + mlen + 2));
            opar[mlen] = save;
         }
         ++c;                   // ')'
      }
   }
}


////////////////////////////////////////////////////////////////////////////////////////////////////
// Prüft, ob die Regel für das Ziel «target» in der aktuellen Konfiguration anwendbar ist.
// Falls ja, gibt die Funktion true zurück und setzt die beiden Argumente wie folgt:
// - «args»: Werte für %1, %2, ...
// - «srcs»: Werte für $(0), $(1), ...
////////////////////////////////////////////////////////////////////////////////////////////////////

bool Project::match_rule(Rule *r, int *score, const char **cfg, StringList &args, StringList &srcs,
      const Target *tgt)
{
   YabuContext ctx(r->srcline_,tgt,0);
   char const * const target = tgt->name_;

   // Prüfe, ob das Ziel paßt.
   Str tb;
   expand_vars(vscope_, tb,r->targets_,0,0,0);
   char *ptr = tb.data();
   while (const char *target_pattern = str_chop(&ptr)) {
      bool const is_ok = (*target == '!') ?
	   !strcmp(target_pattern, target) : args.match('%', target_pattern, target);
      if (is_ok) {
	 srcs.clear();
	 srcs.append(target);		// $(0)

         // Optionen prüfen und ggf. anwenden
	 CfgFreeze os(vscope_);    	   	// statische Konfiguration sichern
         if (r->config_) {
            Str buf;
            expand_vars(vscope_,buf, r->config_, &args, '%',&srcs);
            const char *rule_cfg = buf.data();
            if (!var_try_cfg_change(vscope_,rule_cfg)) {
               // Die Regel ist nicht für diese Konfiguration --> ignorieren.
	       MSG(MSG_3,Msg::rule_unusable(r->srcline_,rule_cfg));
               continue;
            }
         }
	 *cfg = var_current_cfg(vscope_);

         // $(*) setzen
         split_srcs(srcs, r->sources_, args);

         // Bewertung der Regel (Priorität)
         *score = pattern_priority(target_pattern);
	 Message(MSG_3,"Rule @%s(%d) score %d",r->srcline_->file,r->srcline_->line,*score);
         return true;
      }
   }
   return false;                // Regel hat kein passendes Ziel
}


////////////////////////////////////////////////////////////////////////////////////////////////////
// Wählt die Regel für «t» aus und ermittelt alle Quellen (nichtrekursiv).
////////////////////////////////////////////////////////////////////////////////////////////////////

void Project::select_rule(Target *t)
{
   Dependency * const req_by = t->req_by_;
   YABU_ASSERT(t->prj_ == this);

   // Schritt 0: Übergeordnete Verzeichnisse anlegen, falls sie nicht existieren
   if (auto_mkdir) {
      static DirMaker dm;
      dm.create_parents(Str(rroot_).append(t->name_),false);
   }

   // !configure-Anweisung anwenden (falls eine existiert)
   CfgFreeze saved_cfg(vscope_);		// statische Konfiguration sichern
   if (const char *precfg = ConfigureRule::get_cfg(cfg_rules_head_, t->name_))
	var_cfg_change(vscope_,precfg);

   // Schritt 1: Auswahl der passenden Regel und aller Quellen
   bool found = false;				// Mindestens eine Regel gefunden
   Rule *rule2 = 0;				// Die zweite Regel mit maximaler Priorität
   int max_score = -1000000;
   for (Rule *r = rules_head_; exit_code <= 1 && r; r = r->next_) {
      if (req_by && r == req_by->rule_)		// Direkte Rekursion verhindern [T:rs04,rs05]
         continue;
      StringList args;				// %n
      StringList files;        			// $n
      int score;
      const char *cfg = 0;
      if (!match_rule(r,&score, &cfg, args, files, t))
         continue;				// Regel paßt nicht
      found = true;
      if (r->script_beg_ == 0) {       		// Regel ohne Skript: alle Quellen auswählen
	 Message(MSG_2,Msg::using_rule(r->srcline_,t->name_));
         add_sources(t,files,r);
      } else if (score > max_score) {   	// Neuer Spitzenreiter
	 t->build_files_.swap(files);
	 t->build_args_.swap(args);
	 t->build_rule_ = r;
	 t->build_cfg_ = cfg;
	 rule2 = 0;
	 max_score = score;
      }
      else if (score == max_score && !rule2)	// Regel ist nicht eindeutig
	 rule2 = r;
   }

   if (t->build_rule_) {			// Regel mit Skript gefunden
      if (rule2)				// nicht eindeutig? [T:rs12,rs13]
	 YUERR(L31,ambiguous_rules(t->name_,t->build_rule_->srcline_,rule2->srcline_));
      else {
	 Message(MSG_2,Msg::using_rule(t->build_rule_->srcline_,t->name_));
	 prepare_build(t);
      }
   } else if (found)
      t->is_alias_ = true;	// Nur Regeln ohne Skript: Ziel als Alias behandeln
}


////////////////////////////////////////////////////////////////////////////////////////////////////
// Wählt ein Ziel einschließlich aller Quellen (rekursiv) aus. Ggf. wird das Ziel erzeugt.
// Das Ziel gehört unter Umständen einem anderen Target. Deshalb ist select_tgt(const char *) eine
// normale und select_tgt(Target *) eine statische Methode.
////////////////////////////////////////////////////////////////////////////////////////////////////

void Project::select_tgt(const char *tgt, Dependency *req_by)
{
   select_tgt(get_tgt(tgt,true),req_by);
}


void Project::select_tgt(Target *t, Dependency *req_by)
{
   Project * const prj = t->prj_;

   YabuContext ctx(0,t,0);
   if (t->status_ == Target::SELECTING) {
      YUERR(L04,circular_dependency(0,t->name_));
   } else if (t->status_ == Target::IGNORED) {		// Ziel wurde noch nicht ausgewählt

      if (exit_code >= 2 || prj->state_ != OK) {
	 // Im Fehlerfalle keine neuen Ziele mehr auswählen
	 cancel_tgt(t,MSG_1);
      } else {
	 static int depth = 0;
	 if (++depth > 50)
	    YUERR(L04,circular_dependency(0,t->name_));
	 // Alle Ziele setzen implizit !INIT voraus.
	 Target *init = prj->get_tgt("!INIT",true);
	 if (t != init)
	    Dependency::create(t,init,&YABU_INTERNAL_RULE);

	 // Regel auswählen und Quellen ermitteln -- in dieser Reihenfolge wegen set_group()!
	 t->begin_select(req_by);
	 prj->select_rule(t);
	 if (   t->build_rule_ == 0 && req_by != 0 && req_by->rule_ == 0
	       && t->time_ == 0 && t->get_file_time(prj->ts_algo_) == 0) {
	    // Auto-Quelle ohne passende Regel löschen.
	    t->deselect(Target::IGNORED);
	    Dependency::destroy(req_by);
	 } else {
	    // Alle Quellen auswählen.
	    for (Dependency *d = t->srcs_; d; d = d->next_src_) {
	       if (!d->deleted_)
		  select_tgt(d->src_,d);
	    }

	    // Wenn die Auswahl erfolgreich war, versuchen wir das Ziel zu erreichen.
	    if (t->end_select())
	       try_build(t);

	    --depth;
	    job_process_queue(false);
	 }
      }
   }
}


////////////////////////////////////////////////////////////////////////////////////////////////////
// Vergleicht den Namen eines «Target»s mit einem gegebenen String. Wird von «yabu_bsearch()»
// benötigt, um in der Tabelle aller «Target»s ein Objekt mit gegebenem Namen zu finden.
////////////////////////////////////////////////////////////////////////////////////////////////////

int compare(const char *name, Target const *const *t)
{
   return strcmp(name, (*t)->name_);
}


////////////////////////////////////////////////////////////////////////////////////////////////////
// «Target»-Objekt suchen oder erzeugen.
////////////////////////////////////////////////////////////////////////////////////////////////////

Target *Project::get_tgt(const char *name, bool may_create)
{
   init();

   // Ggf. an Oberprojekt delegieren.
   const char *n = name;
   for (int i = depth_; i > 0 && !strncmp(n,"../",3); --i) {
      n += 3;
      if (i == 1)
	 return parent_->get_tgt(n,may_create);
   }

   // Ggf. an Unterprojekt delegieren.
   for (Project *p = prjs_head_; p; p = p->next_) {
      const size_t rroot_len = strlen(p->rroot_);
      if (!strncmp(name,p->rroot_,rroot_len)) {
	 return p->get_tgt(name + rroot_len,may_create);
      }
   }

   // Das Ziel ist für uns.
   size_t pos;
   if (!my_bsearch(&pos, all_tgts_, n_tgts_, name)) {
      if (!may_create)
	 return 0;
      array_insert(all_tgts_, n_tgts_, pos);
      all_tgts_[pos] = new Target(this, name);
   }
   return all_tgts_[pos];
}



////////////////////////////////////////////////////////////////////////////////////////////////////
// Löscht die aus Buildfile.state gelesenen Build-Zeiten.
////////////////////////////////////////////////////////////////////////////////////////////////////

void Project::clear_all_build_times()
{
   for (size_t i = 0; i < n_tgts_; ++i) {
      for (Dependency *d = all_tgts_[i]->srcs_; d; d = d->next_src_)
	 d->last_src_time_ = 0;
   }
}




void Project::dump_rules()
{
   Message(MSG_0,"----- %s (%s) -----",Msg::rules(),aroot_);
   for (Rule const *r = rules_head_; r; r = r->next_)
      r->dump();
}

void Project::dump_tgts()
{
   Message(MSG_0,"----- %s (%s) -----",Msg::targets(),aroot_);
   for (size_t i = 0; i < n_tgts_; ++i)
      all_tgts_[i]->dump();
   for (Project *p = prjs_head_; p; p = p->next_)
      p->dump_tgts();
}


////////////////////////////////////////////////////////////////////////////////////////////////////
// Skript erzeugen und zur Ausführung freigeben.
// args: Werte für die %-Ersetzung.
// files: Werte für Ersetzung von $(0),...$(9), $(*)
////////////////////////////////////////////////////////////////////////////////////////////////////

void Project::exec(Target *t)
{
   if (exit_code > 1) {		// Keine neuen Jobs erzeugen
      cancel_tgt(t,MSG_1);
      return;
   }
   t->set_building();

   // Build-Konfiguration setzen, damit Variablen korrekt exportiert werden.
   CfgFreeze os(vscope_);
   var_cfg_change(vscope_,t->build_cfg_);
   job_create(this,t,'b',t->build_script_,EXEC_MERGE_STDERR);
}


////////////////////////////////////////////////////////////////////////////////////////////////////
// Skript wurde gestartet oder beendet.
////////////////////////////////////////////////////////////////////////////////////////////////////

static Str *local_obuf;
static const char *local_title;

void Project::job_notify(Target *t, char tag, notify_event_t event, const char *host, Str *output)
{
   if (tag == 'b') {	// Build-Skript
      YABU_ASSERT(t->prj_ == this);
      switch (event) {
	 case NOTIFY_STARTED:
	    MSG(MSG_0,Msg::building(t,host,0));
	    fflush(stdout);
	    break;
	 case NOTIFY_CANCELLED:
	    cancel_tgt(t,MSG_1);
	    break;
	 case NOTIFY_OK:
	    if (t->is_alias_ || no_exec) {
	       t->time_ = time(0);
	       set_done(t,&Target::total_built);
	    } else if (t->get_file_time(ts_algo_) == 0) {  // Fehler, wenn nicht erzeugt [T:032]
	       YUWRN(G31,not_built(t->name_));
	       fail_tgt(t);
	    } else {					   // Ok!
	       set_done(t,&Target::total_built);
	    }

	    // Nach Fehler im Build-Skript das Autodepend-Skript nicht ausführen, denn 
	    // das Ziel ist dann ohnehin veraltet, und zweitens würde das Autodepend-Skript
	    // wahrscheinlich auch einen Fehler verursachen.
	    if (   use_auto_depend && use_state_file && t->status_ != Target::FAILED
		  && !t->ad_script_.empty()) {
	       job_create(this, t, 'a', t->ad_script_,EXEC_COLLECT_OUTPUT);
	    }
	    break;
	 case NOTIFY_FAILED:
	    YUWRN(G30,script_failed(t->name_,0));
	    fail_tgt(t);
	    if (!t->is_alias_ && ts_algo_ == TSA_MTIME) {
	       // Wenn das Ziel existiert, ist es möglicherweise korrupt. Setze die Änderungszeit
	       // auf einen sehr kleinen Wert, so daß es beim nächsten Durchlauf als veraltet gewertet
	       // und neu erzeugt wird. BUG: funktioniert nur mit ts_algo_ = TSA_MTIME.
	       struct utimbuf ut;
	       ut.actime = time(0);
	       ut.modtime = Target::T0;
	       if (utime(t->name_, &ut) == 0)
		  MSG(MSG_1, Msg::reset_mtime_after_error(t->name_));
	    }
	    break;
      }
   } else if (tag == 'a') {	// [auto-depend]
      switch (event) {
	 case NOTIFY_STARTED:
	    MSG(MSG_1,Msg::building(t,host," [auto-depend]"));
	    break;
	 case NOTIFY_CANCELLED:
	    break;
	 case NOTIFY_OK:
	    if (output) {				// Ausgabe auswerten
	       char *rp = output->data();
	       MessageBlock(MSG_3,"auto-depend>",rp);
	       const char *w;
	       t->delete_auto_sources();
	       while ((w = str_chop(&rp)) != 0) {
		  if (*w && strcmp(w, "\\") && w[strlen(w) - 1] != ':') {
		     Target *s = get_tgt(w,true);
		     s->get_file_time(ts_algo_);
		     Dependency *d = Dependency::create(t, s, 0);
		     d->last_src_time_ = s->time_;
		  }
	       }
	    }
	    break;
	 case NOTIFY_FAILED:
	    YUWRN(G30,script_failed(t->name_," [auto-depend]"));
	    break;
      }
   } else if (tag == 'l') {	// exec_local()
      if (event == NOTIFY_STARTED)
	 return;
      if (event != NOTIFY_OK)
	 YUERR(G30,script_failed(local_title,0));
      else
	 *local_obuf = *output;
   }

}


////////////////////////////////////////////////////////////////////////////////////////////////////
// Skript lokal ausführen und Ausgabe in «output» speichern. «title» ist nur für Fehlermeldungen.
////////////////////////////////////////////////////////////////////////////////////////////////////

void Project::exec_local(Str &output, Str const &script, const char *title)
{
   local_title = title;
   local_obuf = &output;
   job_create(this,0,'l',script,EXEC_LOCAL | EXEC_COLLECT_OUTPUT);
   while (!job_queue_empty())
      job_process_queue(true);
}


////////////////////////////////////////////////////////////////////////////////////////////////////
// Alle Quellen (beginnend mit «srcs[1]») hinzufügen. Hat ein Element von «srcs» die Form ",",
// dann wird es nicht als Quelle sondern als Trennzeichen interpretiert: alle Ziele rechts vom ","
// werden implizit abhängig von allen Zielen links vom ",".
// Nach der Rückkehr sind alle "," aus «srcs» entfernt.
////////////////////////////////////////////////////////////////////////////////////////////////////

// Alt: Target::add_sources()

void Project::add_sources(Target*t, StringList &srcs, Rule const *r)
{
   size_t grp_begin = 0;	// Position der 1. Quelle links vom ","
   size_t grp_end = 0;		// Position des ","
   for (size_t n = 1; n < srcs.size(); ++n) {
      if (!strcmp(srcs[n],",")) {
	 grp_begin = (grp_end == 0) ? 1 : grp_end + 1;
	 grp_end = n;
      } else {
	 Target *s = get_tgt(srcs[n],true);
         Dependency::create(t, s, r);
	 for (size_t k = grp_begin; k < grp_end; ++k)
	    Dependency::create(s, get_tgt(srcs[k],true),r);
      }
   }

   // "," entfernen
   for (size_t n = 1; n < srcs.size(); ) {
      if (!strcmp(srcs[n],","))
	 srcs.remove(n);
      else
	 ++n;
   }
}


////////////////////////////////////////////////////////////////////////////////////////////////////
// Wird aufgerufen, wenn ein Ziel erreicht wurde.
// «utd»: Ziel war bereits aktuell.
////////////////////////////////////////////////////////////////////////////////////////////////////

void Project::set_done(Target *t, unsigned *counter)
{
   t->rule_id_ = t->rule_id_new_;
   YABU_ASSERT(t->is_selected());
   t->deselect(Target::BUILT);
   if ((!t->is_alias_ || t->build_rule_ || t->req_by_ == 0) && counter) ++*counter;

   // Build-Zeiten der Quellen merken
   for (Dependency *d = t->srcs_; d; d = d->next_src_) {
      if (!d->deleted_)
         d->last_src_time_ = d->src_->time_;
   }

   unlock_group(t);

   // Prüfe, ob von «this» abhängige Ziele bereit geworden sind.
   for (Dependency *d = t->tgts_; d; d = d->next_tgt_) {
      if (!d->deleted_)
         try_build(d->tgt_);
   }
}


////////////////////////////////////////////////////////////////////////////////////////////////////
// Vorbereitung nach Auswahl der Regel.
////////////////////////////////////////////////////////////////////////////////////////////////////

void Project::prepare_build(Target *t)
{
   t->is_alias_ |= t->build_rule_->is_alias_ || !strcmp(t->name_,"all");
   add_sources(t,t->build_files_,t->build_rule_);
   t->set_group(t->build_rule_,t->build_args_);

   // Variablen und '%' in den Skripten ersetzen.
   Rule const * const r = t->build_rule_;
   YABU_ASSERT(r && r->script_beg_ != 0);
   ScriptExpander sex(t,vscope_);
   sex.expand(t->build_script_,r->script_beg_,r->script_end_);
   if (use_auto_depend && r->adscript_beg_)
      sex.expand(t->ad_script_,r->adscript_beg_,r->adscript_end_);

   // Signatur der Regel berechnen (zum späteren Vergleich, ob sich die Regel verändert hat).
   // Deshalb müssen wir bereits hier Variablen im Skript ersetzen -- auch wenn das Ziel aktuell
   // ist das Skript gar nicht ausgeführt wird.
   Crc crc(t->build_script_,t->build_script_.len());
   for (size_t i = 1; i < t->build_files_.size(); ++i)
      crc.feed(t->build_files_[i]);
   t->rule_id_new_ = crc.final();

}


////////////////////////////////////////////////////////////////////////////////////////////////////
// Prüft, ob «this» ein spezielles Ziel (wie zum Beispiel !CLEAN_STATE) ist
// und führt gegebenenfalls die erforderliche Aktion aus.
// return: True: es war ein spezielles Ziel. False: «target» ist kein spezielles Ziel.
////////////////////////////////////////////////////////////////////////////////////////////////////

bool Project::build_special(Target *t)
{
   if (!strcmp(t->name_, "!CLEAN_STATE")) {
      t->time_ = time(0);
      state_file_delete();
      clear_all_build_times();
      return true;
   }
   else if (!strcmp(t->name_, "!ALWAYS")) {
      // Das Folgende bewirkt, daß alle von !ALWAYS abhängigen Ziele jedesmal
      // aktualisiert werden, egal welchen Algorithmus wir benutzen.
      // Wir setzen dazu time_ auf einen Wert größer als alle vorkomenden echten
      // Zeiten und ungleich last_build_time_.
      t->time_.s_ = 0x7FFFFFFF;
      t->time_.ns_ = time(0);
      //t->last_build_time_ = 0x7FFFFFFE;
      return true;
   }
   else if (!strcmp(t->name_, "!INIT")) {
      // Auch ohne passende Regel als erreicht betrachten
      t->time_ = Target::T0;
      //t->last_build_time_ = Target::T0;
      return true;
   }
   return false;
}


////////////////////////////////////////////////////////////////////////////////////////////////////
// Prüft, ob alle Vorbedingungen erfüllt sind, und führt dann den letzten Schritt aus, um das Ziel
// zu erreichen.
////////////////////////////////////////////////////////////////////////////////////////////////////

void Project::try_build(Target *t)
{
   Project *prj = t->prj_;
   
   // Ist das Ziel bereits erreicht oder in Arbeit?
   if (exit_code > 1 || t->status_ != Target::SELECTED)
      return;
   YABU_ASSERT(t->is_selected());

   // Ist die Gruppe durch Ziel X gesperrt, dann: falls X unerreichbar ist, wird auch «this»
   // unerreichbar ("fail-all"-Gruppe), ansonsten abwarten.
   if (t->group_ && t->group_->locked_by_ && t->group_->locked_by_ != t) {
      if (t->group_->locked_by_->status_ == Target::FAILED)
	 cancel_tgt(t,MSG_1);
      return;
   }

   // Sind alle Quellen schon erreicht?
   for (Dependency *s = t->srcs_; s; s = s->next_src_) {
      if (!s->deleted_ && s->src_->status_ != Target::BUILT)
	 return;
   }

   // Alle Bedingungen erfüllt
   YabuContext ctx(0,t,0);

   if (prj->build_special(t))
      set_done(t,0);
   else if (t->build_rule_) {        			// Skript vorhanden?
      if (t->is_alias_ || t->is_outdated(prj->ts_algo_) || no_exec > 1) {
							// Alias oder veraltet
	 t->delete_auto_sources();
	 if (t->build_rule_->create_only_ && t->time_ != 0) {
	    YUERR(G32,target_exists(t->name_));
	    fail_tgt(t);
	 } else
            prj->exec(t);
      } else {						// Ziel ist aktuell
         if (t->is_leaf())
            MSG(MSG_3,Msg::leaf_exists(t->name_));
         else
            MSG(MSG_2,Msg::target_is_up_to_date(t->name_));
	 set_done(t,&Target::total_utd);
      }
   } else if (t->is_alias_) {
      t->time_ = time(0);				// Es ist ein Alias: auch ohne Skript ok
      set_done(t,0);
   } else if (   t->req_by_ == 0			// Vom Benutzer vorgegeben ...
	      || t->is_outdated(prj->ts_algo_)) {	// ... oder veraltet: Fehler
      YUERR(L33,no_rule(t->name_));
      fail_tgt(t); 
   } else if (t->is_leaf() && !t->is_regular_file_) {
      YUERR(L05,nonregular_leaf(t->name_));		// Keine normale Datei: Fehler [T:rs01]
      fail_tgt(t);
   } else {
      if (t->is_leaf()) {
	 MSG(MSG_3,Msg::leaf_exists(t->name_));		// Existiert, keine Quellen: okay
	 set_done(t,0);
      } else {
	 MSG(MSG_1,Msg::already_built(t->name_));	// Noch aktuell: okay
	 set_done(t,&Target::total_utd);
      }
   }
}


////////////////////////////////////////////////////////////////////////////////////////////////////
// Falls weitere Gruppenmitglieder bereit sind, das erste aktivieren
////////////////////////////////////////////////////////////////////////////////////////////////////

void Project::unlock_group(Target *t)
{
   if (t->group_ && t->group_->locked_by_ == t) {
      t->group_->unlock();
      for (Target *s = t->group_->head_; t->group_->locked_by_ == 0 && s; s = s->next_in_group_)
	 try_build(s);
   }
}


////////////////////////////////////////////////////////////////////////////////////////////////////
// Ziel als "nicht erreicht" markieren.
////////////////////////////////////////////////////////////////////////////////////////////////////

void Project::fail_tgt(Target *t)
{
   YABU_ASSERT(t->status_ == Target::BUILDING || t->status_ == Target::SELECTED);
   ++Target::total_failed;
   t->deselect(Target::FAILED);

   // Alle von «t» abhängigen Ziele sind ebenfalls unerreichbar.
   for (Dependency *d = t->tgts_; d; d = d->next_tgt_) {
      if (!d->deleted_)
	 cancel_tgt(d->tgt_,MSG_1);
   }

   // Gehört das Ziel zu einer "fail-all"-Gruppe, werden die übrigen Mitglieder 
   // ebenfalls unerreichbar. Außerdem bleibt die Gruppe gesperrt, so daß auch
   // zukünftige Mitglieder unerreichbar werden (siehe «try_build()»).
   if (t->group_) {
      if (t->group_->fail_all_) {
	 for (Target *s = t->group_->head_; s; s = s->next_in_group_) {
	    if (s != t && s->time_ == 0)
	       cancel_tgt(s,MSG_1);
	 }
      } else
	 unlock_group(t);
   }
}


////////////////////////////////////////////////////////////////////////////////////////////////////
// Ziel als "ausgelassen" markieren
////////////////////////////////////////////////////////////////////////////////////////////////////

void Project::cancel_tgt(Target *t, unsigned flags)
{
   if (t->status_ == Target::FAILED || t->status_ == Target::IGNORED) return;
   YABU_ASSERT(t->status_ != Target::BUILT);
   MSG(flags,Msg::target_cancelled(t->name_));
   ++Target::total_cancelled;
   t->deselect(Target::FAILED);

   // Alle abhängigen Ziele ebenfalls als ausgelassen markieren.
   for (Dependency *d = t->tgts_; d; d = d->next_tgt_) {
      if (!d->deleted_)
	 cancel_tgt(d->tgt_,flags);
   }

   unlock_group(t);
}



void Project::cancel_all(unsigned msg_level)
{
   while (Target *t = Target::sel_head)
      t->prj_->cancel_tgt(t,msg_level);
}



////////////////////////////////////////////////////////////////////////////////////////////////////
// Autokonfigurations-Skript ausführen (switch-Modus).
// Vergleich «ac_selector» mit den in «ac_script» enthaltenen Mustern und liefert die
// erste passende Konfiguration zurück.
// return: Ergebnis (eine gültige Konfiguration)
////////////////////////////////////////////////////////////////////////////////////////////////////

const char *Project::exec_ac_switch(CfgCommand const *cmd)
{
   Str selbuf;
   expand_vars(vscope_,selbuf, cmd->selector_, 0, 0, 0);

   const char *result = 0;
   for (SrcLine const *l = cmd->beg_; l < cmd->end_; ++l) {
      const char *c = l->text;
      skip_blank(&c);
      const char *key = next_str(&c,":");
      if (!skip_str(&c,": ") || c == 0 || *c == 0 || key == 0 || *key == 0)
	 YUERR(S01,syntax_error());
      else if (result == 0) {
	 StringList args;
	 if (args.match('%',key,selbuf)) {
	    Str cfgbuf;
	    expand_vars(vscope_,cfgbuf,c, &args, '%', 0);
	    result = str_freeze(cfgbuf);
	    Message(MSG_3, "!configure: %s ~= %s  --> %s", (const char*)selbuf, key, result);
	 }
      }
   }
   if (result == 0)
      YUERR(L01,undefined(0,selbuf));
   return result;
}


const char *Project::exec_ac_script(CfgCommand const *cmd)
{
   Str script;
   for (SrcLine const *l = cmd->beg_; l < cmd->end_; ++l) {
      expand_vars(vscope_,script, l->text, 0, 0, 0);
      if (exit_code >= 2)
	 return 0;
      script.append("\n");
   }

   Str output;
   exec_local(output,script,"!configure");
   if (exit_code >= 2)
      return 0;
   Str cfg;
   char *c = output.data();
   const char *w;
   while (exit_code < 2 && (w = str_chop(&c)) != 0) {
      Message(MSG_3,"Auto-configure: %s", w);
      // Plausibilitätskontrolle des Wortes. Eigentlich redundant, da SetOption dies
      // ebenfalls prüft. Ermöglicht aber eine benutzerfreundlichere Fehlermeldung..
      const char *w1 = w;
      if (*w1 == '+' || *w1 == '-')
	 ++w1;
      if (*w == '_')
	 ++w1;
      if (!IS_IDENT(*w1))
	 YUERR(S01,syntax_error(w));
      else if (*w == '+' || *w == '-')
	 cfg.append(w);
      else
	 cfg.append("+").append(w);
   }
   return str_freeze(cfg);
}



////////////////////////////////////////////////////////////////////////////////////////////////////
// Hier (vor dem Beginn von Phase 2) wird die Startkonfiguration festgelegt.
// - Kommandozeile (-c)
// - Buildfile (!configure)
// Ist eine Option an beiden Stellen definiert, hat die Kommandozeile höhere Priorität.
////////////////////////////////////////////////////////////////////////////////////////////////////

void Project::set_static_options()
{
   if (user_options)
      var_cfg_change(vscope_,user_options);

   for (CfgCommand const *cmd = ac_; cmd; cmd = cmd->next_) {
      YabuContext ctx(cmd->src_,0,0);
      const char *opts = 0;
      if (cmd->selector_ && *cmd->selector_)
         opts = exec_ac_switch(cmd);
      else
         opts = exec_ac_script(cmd);
      if (opts && *opts)
         var_cfg_change(vscope_, opts);
   }
}


////////////////////////////////////////////////////////////////////////////////////////////////////
// Name des Vergleichsalgorithmus
////////////////////////////////////////////////////////////////////////////////////////////////////

static const char *tsa_name(TsAlgo_t a)
{
   switch (a)
   {
      case TSA_DEFAULT:
      case TSA_MTIME: return "mt";
      case TSA_MTIME_ID: return "mtid";
      case TSA_CKSUM: return "cksum";
   }
   return "???";
}


////////////////////////////////////////////////////////////////////////////////////////////////////
// Buildfile lesen und für Phase 2 vorbereiten.
////////////////////////////////////////////////////////////////////////////////////////////////////

bool Project::init()
{
   if (state_ == OK)
      return true;
   if (state_ != NEW)
      return false;

   state_ = INVALID;

   // Buildfile lesen (im Projektverzeichnis, damit .include, $(.glob) etc. wie erwartet arbeiten).
   if (*aroot_ != 0 && chdir(aroot_) != 0) {
      YUERR(G20,syscall_failed("chdir",aroot_));
      return false;
   } else {
      BuildfileReader(this,buildfile_,0).read(false);
      if (*aroot_ != 0 && chdir(yabu_wd) != 0)
	 YUFTL(G20,syscall_failed("chdir",yabu_wd));
   }
   if (exit_code > 1)
      return false;
   if (dump_req('c')) var_dump_options(vscope_,aroot_);
   if (dump_req('v')) var_dump(vscope_,aroot_);
   if (dump_req('r')) dump_rules();

   if (use_state_file)
      state_file_read();
   if (dump_req('T')) dump_tgts();

   // TS-Algorithmus setzen
   if (ts_algo_ == TSA_DEFAULT)
      ts_algo_ = TSA_MTIME;      // Default, falls noch nicht gesetzt
   if (ts_algo_ != TSA_MTIME && !use_state_file)
      YUERR(G44,need_state_file(tsa_name(ts_algo_)));

   // Den gleichen TS-Algorithmus für alle Unterprojekte setzen. Damit überstimmen wir
   // ggf. die Algorithmenauswahl in den Statusdateien der Unterprojekte. 
   for (Project *p = prjs_head_; p; p = p->next_)
      p->ts_algo_ = ts_algo_;

   if (discard_build_times_)
      clear_all_build_times();

   set_static_options();

   state_ = OK;
   return true;
}


////////////////////////////////////////////////////////////////////////////////////////////////////
// Eine Zeile aus der Statusdatei auswerten
////////////////////////////////////////////////////////////////////////////////////////////////////

bool Project::state_file_read(const StringList &args)
{
   if (!strcmp(args[0],"target")) {
      // target <target> <cfg> <rule_id> [<src> <mtime>]...
      if (args.size() >= 5) {
	 Target *t = get_tgt(args[1],true);
	 t->build_cfg_ = str_freeze(args[2]);
	 StateFileReader::decode(t->rule_id_,args[3]);
	 for (unsigned i = 4; i + 1 < args.size(); i += 2) {
	    Dependency *d = Dependency::create(t,get_tgt(args[i],true),0);
	    StateFileReader::decode(d->last_src_time_,args[i+1]);
	 }
      }
   } else if (!strcmp(args[0],"default_targets")) {
      // nicht mehr benutzt
   } else if (!strcmp(args[0],"tsa")) {
      int val;
      if (   args.size() >= 2 && str2int(&val,args[1])
	  && (val == TSA_MTIME || val == TSA_MTIME_ID || val == TSA_CKSUM)) {
	 if (ts_algo_ == TSA_DEFAULT)
	    ts_algo_ = (TsAlgo_t) val;
	 else if ((int) ts_algo_ != val)
	    discard_build_times_ = true;	// Neuer Algorithmus, Build-Zeiten verwerfen
      }
   }

   return true;
}


////////////////////////////////////////////////////////////////////////////////////////////////////
// Statusdatei lesen
////////////////////////////////////////////////////////////////////////////////////////////////////

void Project::state_file_read()
{
   StateFileReader r(state_file_);
   while (r.next())
      state_file_read(r.argv_);
}


////////////////////////////////////////////////////////////////////////////////////////////////////
// Statusdatei für das Projekt und alle Unterprojekte schreiben
////////////////////////////////////////////////////////////////////////////////////////////////////

void Project::state_file_write()
{
   if (state_ == OK && state_file_ != 0) {
      StateFileWriter sf(state_file_);
      sf.begin("tsa");
      sf.append((unsigned)ts_algo_);
      for (unsigned i = 0; i < n_tgts_; ++i) {
	 Target *t = all_tgts_[i];
	 if (t->is_alias_) continue;
	 if (t->is_leaf()) continue;
	 
	    //if (   t->status_ == Target::BUILT
	    //    || (t->time_ >= Target::T0))
	    //   t->last_build_time_ = t->time_;
	    //if (t->last_build_time_ >= Target::T0) {
	       sf.begin("target");
	       sf.append(t->name_);
	       sf.append(t->build_cfg_);
	       //sf.append(t->last_build_time_);
	       //sf.append(t->srcs_id_);
	       sf.append(t->rule_id_);
	       for (Dependency const *d = t->srcs_; d; d = d->next_src_) {
		  if (d->deleted_ || *d->src_->name_ == '!') continue;
		  sf.append(d->src_->name_);
		  sf.append(d->last_src_time_);
	       }
	    //}
	
      }
   }

   for (Project *p = prjs_head_; p; p = p->next_)
      p->state_file_write();
}

void Project::state_file_delete()
{
   unlink(state_file_);
   state_file_ = 0;
}


// vim:sw=3 cin fileencoding=utf-8
