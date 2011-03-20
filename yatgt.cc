////////////////////////////////////////////////////////////////////////////////////////////////////
// I, the creator of this work, hereby release it into the public domain. This applies worldwide.
// In case this is not legally possible: I grant anyone the right to use this work for any purpose,
// without any conditions, unless such conditions are required by law.
////////////////////////////////////////////////////////////////////////////////////////////////////

#include "yabu.h"

#include <errno.h>
#include <fcntl.h>
#include <stdarg.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/time.h>
#include <sys/mman.h>
#include <sys/resource.h>
#include <sys/wait.h>
#include <time.h>
#include <unistd.h>
#include <unistd.h>
#include <utime.h>



Target *Target::sel_head = 0;			// Für Phase 2 ausgewählte Ziele
Target **Target::sel_tail = &sel_head;


////////////////////////////////////////////////////////////////////////////////////////////////////
// Konstruktor
////////////////////////////////////////////////////////////////////////////////////////////////////

Target::Target(Project *prj, const char *name)
   : prj_(prj), name_(str_freeze(name)), build_cfg_(0), status_(IGNORED),
     req_by_(0), older_than_(0), srcs_(0), tgts_(0), build_rule_(0),
     is_alias_(*name == '!'),	// hier NICHT den Fall "all" behandeln!
     rule_id_(0), rule_id_new_(0),
     is_regular_file_(false), sel_next_(0), sel_prevp_(0), group_(0), next_in_group_(0)
{
}


////////////////////////////////////////////////////////////////////////////////////////////////////
// Status in Text umwandeln.
////////////////////////////////////////////////////////////////////////////////////////////////////

const char *Target::status_str(Target::Status st)
{
   switch (st) {
      case IGNORED: return "IGNORED";
      case SELECTING: return "SELECTING";
      case SELECTED: return "SELECTED";
      case BUILDING: return "BUILDING";
      case BUILT: return "BUILT";
      case FAILED: return "FAILED";
   }
   return "???";
}

////////////////////////////////////////////////////////////////////////////////////////////////////
// Ausgabe aller Ziele (zur Fehlersuche)
////////////////////////////////////////////////////////////////////////////////////////////////////

void Target::dump() const
{
   const char * const cfg = build_cfg_ ? build_cfg_ : "";
   printf("%s [%s] (%s)",name_,cfg,status_str(status_));
   if (group_ && group_->tailp_ != &group_->head_->next_in_group_)
      printf("  {%s}",group_->id_);
   if (is_alias_) fputs(" alias",stdout);
   putc('\n',stdout);
   if (req_by_)
      printf("  ---> %s\n",req_by_->tgt_->name_);
   if (older_than_)
      printf("  <--- %s\n",older_than_->name_);
}


////////////////////////////////////////////////////////////////////////////////////////////////////
// Berechnet die Prüfsumme über «len» Bytes der Datei «fn» (beginnend am Dateianfang).
// return: Prüfsumme oder 0 bei Fehler.
////////////////////////////////////////////////////////////////////////////////////////////////////

static unsigned checksum(const char *fn, size_t len)
{
   unsigned cksum = 0;
   len &= 0x7FFFFFFF;
   int fd = yabu_open(fn, O_RDONLY);
   if (fd >= 0) {
      void *map = mmap(0, len, PROT_READ, MAP_SHARED, fd, 0);
      if (map) {
	 cksum = Crc(map, len).final();
	 Message(MSG_3, "CRC(%s)=%u", fn, cksum);
	 munmap((char *) map, len);
      }
      close(fd);
   }
   return cksum;
}


////////////////////////////////////////////////////////////////////////////////////////////////////
// Löscht alle automatischen Quellen (also die, bei denen «rule_» gleich 0 ist)
////////////////////////////////////////////////////////////////////////////////////////////////////

void Target::delete_auto_sources()
{
   for (Dependency * d = srcs_; d; d = d->next_src_) {
      if (d->rule_ == 0)
         Dependency::destroy(d);
   }
}


////////////////////////////////////////////////////////////////////////////////////////////////////
// Bestimmt, ob das Ziel veraltet ist.
////////////////////////////////////////////////////////////////////////////////////////////////////

static bool less(Ftime const &tt, Target *src, Ftime const &last_srcs_time)
{
   return tt < src->time_;
}

static bool not_equal(Ftime const &tt, Target *src, Ftime const &last_srcs_time)
{
   return src->time_ != last_srcs_time;
}

bool Target::is_outdated(TsAlgo_t tsa)
{
   // Änderungszeit ermitteln, falls noch nicht geschehen
   if (time_ == 0)
      get_file_time(tsa);
   if (is_alias_ || time_ < Target::T0) {
      MSG(MSG_1,Msg::target_missing(name_));
      return true;					// Nicht vorhanden oder Alias
   }

   // Vergleichskriterium für die Zeitstempel
   bool (*ood)(Ftime const &, Target *, Ftime const &) 
      = (tsa == TSA_DEFAULT || tsa == TSA_MTIME) ? less : not_equal;

   // Quellen überprüfen
   for (Dependency const *d = srcs_; d; d = d->next_src_) {
      // Wurde eine Auto-Quelle gelöscht, dann wissen wir nicht, ob das Ziel tatsächlich
      // nicht mehr von der Quelle abhängt, oder ob nur die Quelldatei verschwunden ist.
      // Deshalb betrachten wir das Ziel als veraltet und erzwingen damit die Ausführung
      // der Build-Regel.
      if (d->deleted_)
	 return true;
      YABU_ASSERT(d->src_->time_ != 0);
      if (ood(time_,d->src_,d->last_src_time_)) {
	 if (older_than_ == 0)
	    older_than_ = d->src_;
	 MSG(MSG_1, Msg::is_outdated(name_, d->src_->name_));
         return true;
      }
   }

   // Falls sich die Build-Regel geändert hat, gilt das Ziel ebenfalls als veraltet
   if (build_rule_ && rule_id_new_ != rule_id_ && rule_id_ != 0) {
      MSG(MSG_1,Msg::rule_changed(name_));
      return true;
   }

   return false;
}





////////////////////////////////////////////////////////////////////////////////////////////////////
// Beginn der Auswahl (Status SELECTING).
// req_by: Ziel, welches «this» als Quelle benötigt.
////////////////////////////////////////////////////////////////////////////////////////////////////

void Target::begin_select(Dependency *req_by)
{
   if (!is_selected()) {		// Mehrfachauswahl ignorieren
      Message(MSG_3,"%s: %s --> %s",name_,status_str(status_), status_str(SELECTING));
      YABU_ASSERT(status_ == IGNORED);
      YABU_ASSERT(*sel_tail == 0);
      status_ = SELECTING;
      req_by_ = req_by;
      sel_next_ = 0;
      sel_prevp_ = sel_tail;
      *sel_tail = this;
      sel_tail = &sel_next_;
   }
}



////////////////////////////////////////////////////////////////////////////////////////////////////
// Ende der Auswahl. Während der Auswahl der Quellen kann das Ziel selbst unerreichbar geworden
// sein. Andernfalls ist die Auswahl erfolgreich.
////////////////////////////////////////////////////////////////////////////////////////////////////

bool Target::end_select()
{
   if (status_ == FAILED)
      return false;
   YABU_ASSERT(status_ == SELECTING);
   Message(MSG_3,"%s: %s --> %s",name_,status_str(status_), status_str(SELECTED));
   status_ = SELECTED;
   return true;
}

////////////////////////////////////////////////////////////////////////////////////////////////////
// Hebt die Auswahl wieder auf (z.B., nachdem das Ziel erreicht wurde)
////////////////////////////////////////////////////////////////////////////////////////////////////

void Target::deselect(Status status)
{
   Message(MSG_3,"%s: %s --> %s",name_,status_str(status_), status_str(status));
   status_ = status;
   if (sel_prevp_ != 0) {
      *sel_prevp_ = sel_next_;
      if (sel_next_)
	 sel_next_->sel_prevp_ = sel_prevp_;
      else
	 sel_tail = sel_prevp_;
      sel_prevp_ = 0;

      // Aus der Gruppe entfernen
      if (group_) {
	 Target **pp;
	 for (pp = &group_->head_; *pp != 0 && *pp != this; pp = &(*pp)->next_in_group_);
	 if (*pp && (*pp = next_in_group_) == 0)
	    group_->tailp_ = pp;
      }
   }
}


////////////////////////////////////////////////////////////////////////////////////////////////////
// Zielstatistik ausgeben
////////////////////////////////////////////////////////////////////////////////////////////////////

unsigned Target::total_built = 0;
unsigned Target::total_failed = 0;
unsigned Target::total_utd = 0;
unsigned Target::total_cancelled = 0;

void Target::statistics()
{
   if (exit_code > 0)
      MSG(MSG_W,Msg::target_stats_failed(total_built,total_failed, total_cancelled));
   else {
      MSG(MSG_ALWAYS | MSG_0,Msg::target_stats(total_built+total_utd,total_utd,total_built,
	       time(0) - yabu_start_time));
   }
}


////////////////////////////////////////////////////////////////////////////////////////////////////
// Speichert die Änderungszeit (je nach Algorithmus) in «time_». Existiert die Datei nicht,
// wird «time_» gleich 0 gesetzt. Ist das Ziel ein Verzeichnis, wird «time_» gleich T0 gesetzt.
// return: Neuer Wert von «time_».
////////////////////////////////////////////////////////////////////////////////////////////////////

Ftime Target::get_file_time(TsAlgo_t tsa)
{
   if (is_alias_)
      time_ = time(0);
   else {
      const char *name = name_;
      Str full_name;
      if (*prj_->aroot_ && *name_ != '/') {		// Dateinamen sind relativ zum Projektverzeichnis
	 full_name = prj_->aroot_;
	 full_name.append(name_);
	 name = full_name;
      }
      struct stat sb;
      if (ar_member_time(&time_, name, tsa))
	 is_regular_file_ = true;
      else if (!yabu_stat(name,&sb))
	 time_ = 0;
      else {
	 is_regular_file_ = S_ISREG(sb.st_mode);
	 if (is_regular_file_) {
	    switch (tsa) {
	       case TSA_DEFAULT:
	       case TSA_MTIME:
	       case TSA_MTIME_ID:
		  yabu_ftime(&time_,&sb);
		  break;
	       case TSA_CKSUM:
		  time_ = checksum(name,sb.st_size);
		  break;
	    }
	 } else if (S_ISDIR(sb.st_mode))
	    time_ = T0;
	 else
	    time_ = 0;
      }
   }
   return time_;
}




////////////////////////////////////////////////////////////////////////////////////////////////////
// Liefert true, wenn das Ziel keine expliziten Quellen hat.
////////////////////////////////////////////////////////////////////////////////////////////////////

bool Target::is_leaf()
{
   for (Dependency const *d = srcs_; d; d = d->next_src_) {
      if (!d->deleted_ && d->rule_ != 0 && d->rule_ != &YABU_INTERNAL_RULE)
	 return false;
   }
   return true;
}



////////////////////////////////////////////////////////////////////////////////////////////////////
// Status auf BUILDING setzen
////////////////////////////////////////////////////////////////////////////////////////////////////

void Target::set_building()
{
   YABU_ASSERT(status_ == SELECTED);
   status_ = BUILDING;
   Message(MSG_3,"%s: %s --> %s",name_,status_str(status_), status_str(SELECTING));
   if (group_) group_->lock(this);
}






////////////////////////////////////////////////////////////////////////////////////////////////////
// Konstruktor
////////////////////////////////////////////////////////////////////////////////////////////////////

Serial *Serial::head = 0;		// Liste aller Gruppen
Serial **Serial::tail = &head;		// Listenende

static const char *next_grp_name()
{
   static unsigned n = 0;
   char tmp[40];
   snprintf(tmp,sizeof(tmp),"!serialize-%u",++n);
   return str_freeze(tmp);
}

Serial::Serial(const char *id)
   : TargetGroup(id), next_(0)
{
   fail_all_ = false;
   *tail = this;
   tail = &next_;
}


////////////////////////////////////////////////////////////////////////////////////////////////////
// Prüft, ob «tgt» in die Gruppe paßt
////////////////////////////////////////////////////////////////////////////////////////////////////

bool Serial::match(const char *tgt) const
{
   StringList dummy;
   for (size_t i = 0; i < tgts_.size(); ++i) {
      if (dummy.match('%', tgts_[i],tgt))
	 return true;
   }
   return false;
}


////////////////////////////////////////////////////////////////////////////////////////////////////
// Findet die erste passende Gruppe für ein Ziel
////////////////////////////////////////////////////////////////////////////////////////////////////

Serial *Serial::find(const char *name)
{
   for (Serial *s = head; s; s = s->next_) {
      if (s->match(name))
	 return s;
   }
   return 0;
}


////////////////////////////////////////////////////////////////////////////////////////////////////
// Erzeugt eine neue Gruppe oder fügt »tgts» zu der bestehenden Gruppe «id» hinzu
////////////////////////////////////////////////////////////////////////////////////////////////////

void Serial::create(const char *id, StringList &tgts)
{
   if (id == 0) {
      Serial *s = new Serial(next_grp_name());
      s->tgts_.swap(tgts);
   } else {
      Serial *s;
      for (s = head; s && s->id_ != id; s = s->next_);
      if (s == 0)
	 s = new Serial(id);
      for (size_t i = 0; i < tgts.size(); ++i)
	 s->tgts_.append(tgts[i]);
   }
}


////////////////////////////////////////////////////////////////////////////////////////////////////
// Fügt das Ziel falls nötig einer Gruppe hinzu.
////////////////////////////////////////////////////////////////////////////////////////////////////

void Target::set_group(Rule *r, StringList const &args)
{
   if (group_) return;
   YABU_ASSERT(is_selected());

   TargetGroup *g = Serial::find(name_);	// Mit !serialize definierten Gruppen haben Vorrang

   // Implizite Gruppe (nur bei Regeln mit mehreren Zielen oder Platzhaltern)
   if (!g && (args.size() > 0 || strcmp(r->targets_,name_))) {
      Str key;
      key.printf("%lx",(unsigned long)r);
      for (size_t i = 0; i < args.size(); ++i)
	 key.printfa("+%s",args[i]);
      static size_t n_groups = 0;
      static TargetGroup **groups = 0;
      size_t pos;
      if (!my_bsearch(&pos,groups,n_groups,key)) {
	 array_insert(groups,n_groups,pos);
	 g = groups[pos] = new TargetGroup(str_freeze(key));
      }
      g = groups[pos];
   }

   // Ziel zur Gruppe hinzufügen
   if (g) {
      group_ = g;
      YABU_ASSERT(next_in_group_ == 0);
      YABU_ASSERT(*group_->tailp_ == 0);
      *group_->tailp_ = this;
      group_->tailp_ = &next_in_group_;
      next_in_group_ = 0;
   }
}


////////////////////////////////////////////////////////////////////////////////////////////////////
// Konstruktor (löscht «tgts»!)
////////////////////////////////////////////////////////////////////////////////////////////////////

ConfigureRule::ConfigureRule(ConfigureRule ***tail, const char *cfg, StringList &tgts)
   : cfg_(cfg), next_(0)
{
   **tail = this;
   *tail = &next_;
   tgts_.swap(tgts);
}

////////////////////////////////////////////////////////////////////////////////////////////////////
// Gibt die Konfiguration für das Ziel «tgt» zurück (oder 0, falls keine definiert).
////////////////////////////////////////////////////////////////////////////////////////////////////

const char *ConfigureRule::get_cfg(const ConfigureRule *head, const char *tgt)
{
   StringList dummy;
   for (const ConfigureRule *r = head; r; r = r->next_) {
      for (size_t i = 0; i < r->tgts_.size(); ++i) {
	 if (dummy.match('%',r->tgts_[i],tgt))
	    return r->cfg_;
      }
   }
   return 0;
}



// vim:shiftwidth=3:cindent
