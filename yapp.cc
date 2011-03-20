////////////////////////////////////////////////////////////////////////////////////////////////////
// I, the creator of this work, hereby release it into the public domain. This applies worldwide.
// In case this is not legally possible: I grant anyone the right to use this work for any purpose,
// without any conditions, unless such conditions are required by law.
////////////////////////////////////////////////////////////////////////////////////////////////////

// Präprozessor (PP-Variablen, .define, .foreach, .include)


#include "yabu.h"
#include <sys/types.h>
#include <sys/stat.h>
#include <dirent.h>


////////////////////////////////////////////////////////////////////////////////////////////////////
// Gültigkeitsprüfung für Variablen- und Makronamen
////////////////////////////////////////////////////////////////////////////////////////////////////

static bool check_name(const char *name)
{
   if (name == 0)
      name = "(NULL)";	          	// Fehler: NULL
   else if (var_is_valid_name(name)) {
      static const char *RESERVED_WORDS[] = {
	 "once", "endonce",
	 "define", "enddefine",
	 "foreach", "endforeach",
	 "include",
	 "include_source",
	 0
      };
      const char *const *p = RESERVED_WORDS;
      while (*p && strcmp(name, *p))
	 ++p;
      if (*p == 0)
	 return true;             // Okay!
   }

   // Fehler!
   YUERR(S01,syntax_error(name));
   return false;
}



////////////////////////////////////////////////////////////////////////////////////////////////////
// Ein Makro
////////////////////////////////////////////////////////////////////////////////////////////////////

class Macro
{
public:
   Macro(Macro **list, const char *name, const SrcLine * sl);
   static Macro *find(Macro *list, const char *name);
   SrcLine const *begin_;
   SrcLine const *end_;
   StringList args_;			// Formale Argumente
   StringMap defaults_;			// Standardwerte für Argumente
   bool allow_var_args_;          	// Letztes Argument hat "..."-Suffix
   const char *const name_;
   const SrcLine *const src_;
   bool in_use_;
private:
   Macro * const next_;
};

static Macro *global_macros = 0;	// Alle nicht mit local: definerten Makros



////////////////////////////////////////////////////////////////////////////////////////////////////
// Konstruktor. «name» ohne führenden '.', «src» ist der Beginn der Makrodefinition.
////////////////////////////////////////////////////////////////////////////////////////////////////

Macro::Macro(Macro **list, const char *name, const SrcLine * sl)
    : begin_(0), end_(0), allow_var_args_(false), name_(name), src_(sl),
      in_use_(false), next_(*list)
{
   *list = this;
}


////////////////////////////////////////////////////////////////////////////////////////////////////
// Sucht das Makro mit dem Namen «name». Falls nicht gefunden, return 0, aber kein Fehler.
////////////////////////////////////////////////////////////////////////////////////////////////////

Macro *Macro::find(Macro *list, const char *name)
{
   for (Macro * t = list; t; t = t->next_) {
      if (!strcmp(t->name_, name))
         return t;
   }
   return 0;
}


////////////////////////////////////////////////////////////////////////////////////////////////////
// Konstruktor.
// «begin»...«end» ist der Eingabebereich, «parent» der übergeordnete Präprozessor (bei
// verschachtelten Strukturen).
////////////////////////////////////////////////////////////////////////////////////////////////////

Preprocessor::Preprocessor(Project *prj, SrcLine const *begin, SrcLine const *end,
      Preprocessor *parent, bool first_time)
    :prj_(prj), eoi_(end), parent_(parent), first_time_(first_time),
     cur_(begin), sob_(begin), local_macros_(0), obuf_(0), obuf_size_(0), obuf_capacity_(0),
     flushing_(false)
     //, foreach_var_(0), foreach_val_(0)
{
   // Aufrufende Zeile bestimmen (zur späteren Verwendung in «dump_source()»).
   // Beachte: wir müssen hier eine Kopie von «parent_->sob_» machen, weil
   // ein und dieselbe Quellzeile in verschiedenen Kontexten auftreten kann
   // (zum Beispiel bei Makros).
   while (parent && parent->sob_ == 0)		// siehe «do_foreach()»
      parent = parent->parent_;
   if (parent && parent->sob_) {
      array_alloc(caller_,1);
      *caller_ = *parent->sob_;
      caller_->up = parent->caller_;
      //caller_ = parent->sob_;
   }
   else
      caller_ = 0;
}


////////////////////////////////////////////////////////////////////////////////////////////////////
// Destruktor.
////////////////////////////////////////////////////////////////////////////////////////////////////

Preprocessor::~Preprocessor()
{
}


////////////////////////////////////////////////////////////////////////////////////////////////////
// Skript auswerten.
// Beim Aufruf muß «cur_» auf die erste (potentielle) Skriptzeile zeigen. Nach der Rückkehr
// zeigt «cur_» auf die Zeile umittelbar nach dem Skript (oder EOI).
////////////////////////////////////////////////////////////////////////////////////////////////////

bool Preprocessor::get_script(Str &script)
{
   for (unsigned indent = 0; cur_ < eoi_ && IS_SPACE(cur_->text[0]); ++cur_) {
      const char *c = cur_->text;
      if (indent == 0) indent = calc_indent(c);;
      script.append(remove_indent(&c,indent));
      expand_vars(script,c);
      script.append("\n",1);
   }
   return exit_code < 2;
}


////////////////////////////////////////////////////////////////////////////////////////////////////
// Zählt die Verschachtelungstiefe und bricht Yabu ab, wenn das Maximum überschritten wird.
////////////////////////////////////////////////////////////////////////////////////////////////////

struct MaxIncludeDepth {
   static unsigned const MAX_INCLUDE_DEPTH = 25;
   static unsigned depth;
   MaxIncludeDepth() {
      if (++depth >= MAX_INCLUDE_DEPTH)
	 YUERR(L40,too_many_nested_includes());
   }
   ~MaxIncludeDepth() { --depth; }
};
unsigned MaxIncludeDepth::depth = 0;


////////////////////////////////////////////////////////////////////////////////////////////////////
// Führt eine .include_output-Anweisung aus
////////////////////////////////////////////////////////////////////////////////////////////////////

void Preprocessor::do_include_output()
{
   MaxIncludeDepth guard;

   // Skript ausführen
   flush();
   SrcLine sl;
   sl.file = "STDOUT";
   sl.line = 1;
   sl.up = cur_++;
   Str script;
   if (!get_script(script)) return;
   Str output;
   prj_->exec_local(output,script,".include_output");

   // Ausgabe als Buildfile verarbeiten
   for (char *c = output.data(); exit_code < 2 && c && *c != 0; ) {
      sl.text = c;
      char *tp = c;
      if ((c = strchr(c,'\n')) != 0) *c++ = 0;
      for (tp += strlen(tp); tp > sl.text && IS_SPACE(tp[-1]); --tp);
      *tp = 0;
      if (*sl.text && (*sl.text != '#'))
         emit(&sl,sl.text);
      ++sl.line;
   }
}


////////////////////////////////////////////////////////////////////////////////////////////////////
// Führt eine .include-Anweisung aus.
// c: Rest der Zeile nach ".include".
////////////////////////////////////////////////////////////////////////////////////////////////////

void Preprocessor::do_include(const char *c)
{
   MaxIncludeDepth guard;
   skip_blank(&c);
   Str fn;
   const char *x = 0;
   if (*c == '<' && (x = strrchr(++c,'>')) != 0 && x[1] == 0) {		// Form 1: .include <xyz>
      if (*yabu_cfg_dir == 0)
	 YUERR(L01,undefined(0,"YABU_CFG_DIR"));
      fn.append(yabu_cfg_dir).append("/include/").append(c,x - c);
   } else if (*c != '/') {						// Form 2: .include xyz
      if ((x = strrchr(cur_->file, '/')) != 0)
	 fn.append(cur_->file, x + 1 - cur_->file);		// Name relativ zur Mutterdatei
      fn.append(c);
   } else								// Form 3: .include /xyz
      fn = c;

   // Regenerierungsskript (optional)
   ++cur_;
   Str fetch_script;
   vars_.set("_",fn);
   if (!get_script(fetch_script)) return;
   vars_.set("_",0);

   // Falls nötig und möglich, das Regenerierungsskript ausführen
   struct stat sb;
   if (!fetch_script.empty() && !yabu_stat(fn,&sb)) {
      Str output;
      prj_->exec_local(output,fetch_script,".include");
   }

   // Datei verarbeiten
   if (exit_code < 2)
      BuildfileReader(prj_,fn,this).read(false);
}


////////////////////////////////////////////////////////////////////////////////////////////////////
// Liefert den Wert der angegebenen Variable aus dem aktuellen oder einem übergeordneten Kontext.
// name: Name der Variable ohne den führenden '.'
// return: Wert der Variable oder 0, falls nicht gefunden
////////////////////////////////////////////////////////////////////////////////////////////////////

const char *Preprocessor::find_var(const char *name) const 
{
   if (vars_.contains(name))
      return vars_[name];
   if (parent_)
      return parent_->find_var(name);

   // Systemvariablen: $(_TIMESTAMP) ist in Phase 1 als $(.TIMESTAMP) referenzierbar.
   char name2[50];
   snprintf(name2,sizeof(name2),"_%s",name);
   Str val;
   var_get_global(val,name2, "");
   return val.len() > 0 ? str_freeze(val) : 0;
}



////////////////////////////////////////////////////////////////////////////////////////////////////
// $(.glob) auswerten. Liest das Verzeichnis «dir», filtert alle zum Muster «pat» passenden Namen
// heraus, transformiert sie gemäß «repl» und hängt das Ergebnis an «buf».
// Nicht passende Dateinamen werden stillschweigend ignoriert.
////////////////////////////////////////////////////////////////////////////////////////////////////

static void eval_glob(Str &buf, const char *dirname, DIR * dir, const char *pat, const char *repl)
{
   struct dirent *de;
   StringList list;
   while ((de = readdir(dir)) != 0) {
      const char *name = de->d_name;
      if (!strcmp(name, ".") || !strcmp(name, ".."))
         continue;
      StringList sl;
      Str tmp;
      if (sl.match('*', pat, name)) {
         if (repl == 0)
            tmp.append(dirname).append(name);		// «pat» dient nur als Filter
         else
            expand_substrings(tmp, repl, &sl, '*');	// Transformation anwenden
         list.insert(str_freeze(tmp));
      }
   }
   for (unsigned i = 0; i < list.size(); ++i) {
      if (i > 0)
         buf.append(" ",1);
      buf.append(list[i]);
   }
}


////////////////////////////////////////////////////////////////////////////////////////////////////
// $(.glob) auswerten. Dateinamen werden an «buf» angehängt. «repl» ist der Ausdruck rechts vom '='
// oder 0, wenn «pat» nur als Filter wirkt.
////////////////////////////////////////////////////////////////////////////////////////////////////

static void eval_glob(Str & buf, const char *pat, const char *repl)
{
   const char *c = strrchr(pat, '/');
   const char *dir = c ? str_freeze(pat, c - pat) : ".";
   const char *dirname = c ? str_freeze(pat, c - pat + 1) : "";
   const char *fpat = c ? c + 1 : pat;
   DIR *d = opendir(dir);
   if (d == 0)
      YUFTL(G20,syscall_failed("opendir",dir));
   else {
      eval_glob(buf, dirname, d, fpat, repl);
      closedir(d);
   }
}

static void str_copy(char *d, char *de, const char *s)
{
   while (*s && d + 1 < de)
      *d++ = *s++;
   if (d < de)
      *d = 0;
}

static const int FLAG_RECURSE = 0x01;	// Unterverzeichnisse durchsuchen

////////////////////////////////////////////////////////////////////////////////////////////////////
// Ein Verzeichnis durchsuchen
////////////////////////////////////////////////////////////////////////////////////////////////////

static void scan_dir(StringList &result, char * const path, char * const end,
      bool recurse, char kind)
{
   char *fn = path + strlen(path);
   if (fn + 10 >= end) return;
   DIR *d = opendir(path);
   if (d == 0) {
      YUFTL(G20,syscall_failed("opendir",path));
      return;
   }
   *fn++ = '/';
   StringList subdirs;
   while (struct dirent *de = readdir(d)) {
      if (!strcmp(de->d_name,".") || !strcmp(de->d_name,".."))
	 continue;
      str_copy(fn,end,de->d_name);
      struct stat sb;
      if (lstat(path,&sb) == 0) {
	 if ((S_ISREG(sb.st_mode) && kind == 'f') || (S_ISDIR(sb.st_mode) && kind == 'd'))
	    result.insert(str_freeze(path));
	 if (S_ISDIR(sb.st_mode) && recurse)
	    scan_dir(result,path,end,true, kind);
      }
   }
   closedir(d);
}


////////////////////////////////////////////////////////////////////////////////////////////////////
// $(.find): ein Verzeichnisargument verarbeiten.
////////////////////////////////////////////////////////////////////////////////////////////////////

static void scan_dir(StringList &buf, char const *dir, char kind)
{
   char path[1024];
   char * const end = path + sizeof(path);
   bool const ignore_missing = *dir == '?';
   if (ignore_missing) ++dir;
   str_copy(path,end,dir);
   char *fn = strrchr(path,'/');
   bool const recurse = fn != 0 && fn[1] == 0;
   if (recurse)
      *fn = 0;
   else
      fn = path + strlen(path);
   struct stat sb;
   if (ignore_missing && stat(path,&sb) != 0)
      return;
   scan_dir(buf, path, end, recurse, kind);
}


////////////////////////////////////////////////////////////////////////////////////////////////////
// $(.find): eine Regel anwenden
////////////////////////////////////////////////////////////////////////////////////////////////////

static void apply_rule(StringList &result, StringList const &files,
      const char *pat, const char *repl)
{
   bool const exclude = *pat == '!';
   if (exclude) ++pat;

   for (size_t i = 0; i < files.size(); ++i) {
      const char *path = files[i];
      StringList sl;
      if (sl.match('*',pat,path)) {
	 Str tfn;
	 if (repl == 0)
	    tfn = path;
	 else
	    expand_substrings(tfn,repl,&sl,'*');
	 unsigned pos;
	 bool const found = result.find(&pos,tfn);
	 if (exclude && found)
	    result.remove(pos);
	 else if (!exclude && !found)
	    result.insert(str_freeze(tfn));
      }
   }
}


////////////////////////////////////////////////////////////////////////////////////////////////////
// $(.find): Zerlegen der Argument- bzw. Regellisten
////////////////////////////////////////////////////////////////////////////////////////////////////

static const char *next_arg(const char **rp)
{
   const char *c = *rp;
   if (skip_blank(&c) == ')') {
      *rp = c + 1;
      return 0;
   }
   const char *beg = c;
   while (*c != ')' && !IS_SPACE(*c)) ++c;
   beg = str_freeze(beg,c - beg);
   *rp = c;
   return beg;
}


static const char *first_arg(const char **rp)
{
   const char *c = *rp;
   if (*c != '(') {
      YUERR(S01,syntax_error(c));
      return 0;
   }
   if (strchr(c,')') == 0) {
      YUERR(S02,nomatch("(",")"));
      return 0;
   }
   *rp = c + 1;
   return next_arg(rp);
}


////////////////////////////////////////////////////////////////////////////////////////////////////
// $(.find) und $(.dirfind) auswerten
////////////////////////////////////////////////////////////////////////////////////////////////////

static void eval_find(Str &buf, char const *spec, char kind)
{
   StringList files;
   for (const char *w = first_arg(&spec); w; w = next_arg(&spec))
      scan_dir(files,w,kind);

   if (exit_code < 2 && *spec != 0) {
       StringList result;
       for (const char *w = first_arg(&spec); w; w = next_arg(&spec)) {
	  const char *eq = strchr(w,'=');
	  if (eq)
	     apply_rule(result,files,str_freeze(w,eq-w),eq + 1);
	  else
	     apply_rule(result,files,w,0);
       }
       files.swap(result);
   }
   if (*spec != 0) {
      YUERR(S01,syntax_error(spec));
      return;
   }

   for (size_t i = 0; i < files.size(); ++i) {
      if (i > 0) buf.append(" ");
      buf.append(files[i]);
   }
}


////////////////////////////////////////////////////////////////////////////////////////////////////
// PP-Variablen in «c» ersetzen, Ergebnis an «buf» anhängen
////////////////////////////////////////////////////////////////////////////////////////////////////

bool Preprocessor::expand_vars(Str &buf, const char *c)
{
   while (exit_code < 2 && *c != 0) {
      if (const char *next_ref = strstr(c,"$(.")) {
	 if (next_ref > c) {
	    buf.append(c,next_ref - c);			// Text vor dem "$(."
	    c = next_ref;
	 }
	 VariableName vn;
	 if (!vn.split(&c))
	    return false;
	 if (vn.ph != '*') {
	    char tmp[2] = {vn.ph,0};
	    YUFTL(S01,syntax_error(tmp));
	 }

	 // Variablen im Namen und in der Ersetzunsregel rekursiv ersetzen
	 expand_vars(&vn.name);
	 expand_vars(&vn.pat);
	 expand_vars(&vn.repl);

	 // Variablenwert ermitteln
	 Str result;
	 if (!strcmp(vn.name, ".glob")) {
	    eval_glob(result, vn.pat, vn.repl);
	    vn.pat = 0;		// Spezialfall, Transformation wird anders behandelt
	 } else if (!strncmp(vn.name, ".find(",6))
	    eval_find(result, vn.name + 5, 'f');
	 else if (!strncmp(vn.name, ".dirfind(",9))
	    eval_find(result, vn.name + 8, 'd');
	 else {
	    const char *val = find_var(vn.name + 1);	// '.' ignorieren
	    if (!val) 
	       return YUFTL(L01,undefined('.',vn.short_name + 1)), false;
	    result = val;
	 }

	 // Ergebnis (ggf. mit Transformation) an «buf» anhängen
	 Str erepl, epat;
	 if (vn.pat  && expand_vars(epat ,vn.pat )) vn.pat  = epat;
	 if (vn.repl && expand_vars(erepl,vn.repl)) vn.repl = erepl;
	 if (vn.pat == 0)			// Ohne Transformation
	    buf.append(result);
	 else {					// Mit Transformation
	    bool first = true;
	    char *val = result.data();
	    while (const char *w = str_chop(&val)) {
	       const char *r = vn.repl;
	       StringList substr;
	       if (!substr.match(vn.ph, vn.pat, w)) {
		  switch (vn.tr_mode) {
		     case ':': YUFTL(L14,no_match(vn.short_name, w, vn.pat)); break;
		     case '!': continue;	// '!': unterdrücken
		     default: r = 0; break;	// '?': unverändert durchlassen
		  }
	       }
	       
	       Str tmp;
	       if (r) {
		  expand_substrings(tmp, r, &substr, vn.ph);
		  if (*(w = tmp) == 0) continue;		// Leeres Wort
	       }
	       buf.append(" ",first ? 0 : 1).append(w);
	       first = false;
	    }
	 }
      } else {
	 buf.append(c); 
	 break;
      }
   }
   return true;
}


////////////////////////////////////////////////////////////////////////////////////////////////////
// PP-Variablen in «cp» ersetzen.
////////////////////////////////////////////////////////////////////////////////////////////////////

void Preprocessor::expand_vars(const char **cp)
{
   if (cp && *cp) {
      Str buf;
      if (expand_vars(buf,*cp))
	 *cp = str_freeze(buf);
   }
}


////////////////////////////////////////////////////////////////////////////////////////////////////
// Ende eines Blocks (.define .. .enddefine o.ä) suchen. Beim Aufruf ist cur_ die erste Zeile
// n a c h  dem .define. Nach Rückkehr ist cur_ das zugehörige .endif. Verschachtelte Blöcke
// werden berücksichtigt.
// begin: Blockanfang.
// bcmd: Marker für Blockanfang.
// ecmd:  Marker für Blockende
////////////////////////////////////////////////////////////////////////////////////////////////////

void Preprocessor::find_block_end(const SrcLine *begin, const char *bcmd, const char *ecmd)
{
   YabuContext ctx(begin,0,0);
   int level = 1;
   while (cur_ < eoi_) {
      const char *c = cur_->text;
      if (skip_str(&c, bcmd) && !IS_IDENT(*c))
         ++level;
      else if (skip_str(&c, ecmd) && !IS_IDENT(*c)) {
         if (--level == 0) {
            cur_= cur_;
	    return;
	 }
      }
      ++cur_;
   }
   YUERR(S02,nomatch(bcmd,ecmd));
}


////////////////////////////////////////////////////////////////////////////////////////////////////
// Schleife (.foreach) ausführen.
// c: Rest der Zeile nach ".foreach".
////////////////////////////////////////////////////////////////////////////////////////////////////

void Preprocessor::do_foreach(const char *c)
{
   flush();

   const char *varname = next_word(&c);		// Schleifenvariable
   if (!check_name(varname)) return;

   // Rest der Zeile in Wörter zerlegen
   StringList values;
   const char *w;
   while ((w = next_word(&c)) != 0)
      values.append(w);

   // Ende der .foreach-Schleife suchen
   SrcLine const *begin = ++cur_;
   find_block_end(cur_ - 1, ".foreach", ".endforeach");

   // Schleife für jeden Wert einmal ausführen
   for (unsigned i = 0; i < values.size(); ++i) {
      Preprocessor pp(prj_, begin, cur_, this, first_time_);
      pp.vars_.set(varname, values[i]);
      pp.execute();
   }

   ++cur_;                       // .endforeach
}


////////////////////////////////////////////////////////////////////////////////////////////////////
// Makrodefinition (.define) ausführen.
// c: Rest der Zeile nach ".define".
////////////////////////////////////////////////////////////////////////////////////////////////////

void Preprocessor::do_define(const char *c)
{
   const char *tname = next_word(&c);		// Makroname
   Macro **list = &global_macros;
   if (tname != 0 && !strncmp(tname,"local:",6)) {
      list = &local_macros_;
      tname += 6;
   }
   if (!check_name(tname))
      return;
   Macro *m = Macro::find(*list, tname);
   if (m != 0) {
      YUERR(L03,redefined('m',tname,m->src_));
      return;
   }
   Macro *t = new Macro(list, tname, cur_);

   // Formale Argumente
   while (const char *name = next_name(&c)) {
      static const char PCHAR[] = {'(','[','{',0,')',']','}',0};
      if (const char *d = strchr(PCHAR,*c ? *c : '-')) {	// Standardwert
	 d += 4;						// schließende Klammer
	 const char *beg = ++c;
	 while (*c != 0 && *c != *d) ++c;
	 if (*c != *d)
	    YUERR(S01,syntax_error(name));
	 else
	    t->defaults_.set(name,str_freeze(beg,c++ - beg));
      } else if (!strcmp(c,"...")) {		// "..." nur am letzen Argument erlaubt
	 t->allow_var_args_ = true;
	 c += 3;
      }

      if (t->args_.find(0,name))
	 YUERR(L03,redefined(0,name,0));	// [T:pm10]
      else
	 t->args_.append(name);
   }
   if (*c != 0) {
      YUERR(S01,syntax_error(c));
      return;
   }

   // Makroinhalt
   t->begin_ = ++cur_;
   find_block_end(cur_ - 1, ".define", ".enddefine");
   t->end_ = cur_++;
}



bool Preprocessor::do_once()
{
   SrcLine const *begin = ++cur_;
   find_block_end(cur_ - 1, ".once", ".endonce");
   if (exit_code < 2 && first_time_) {
      Preprocessor pp(prj_, begin, cur_, this, true);
      pp.execute();
   }
   ++cur_;                       // .endonce
   return true;
}




////////////////////////////////////////////////////////////////////////////////////////////////////
// Prüft, ob die aktuelle Zeile («cur_») eine Variablenzuweisung der Form ".NAME=WERT" enthält
// und führt, falls ja, die Zuweisung aus.
// Der Returnwert ist «true», wenn es eine Zuweiung war.
////////////////////////////////////////////////////////////////////////////////////////////////////

bool Preprocessor::do_assignment(const char *c)
{
   ++c;						// '.'
   const char *name = next_name(&c);		// Variablenname
   if (name == 0 || *name == 0) return false;
   if (!skip_str(&c," = ")) return false;

   // Zuweisung ausführen
   if (check_name(name)) {
      if (vars_.contains(name))
	 YUERR(L03,redefined('.',name,0));
      else 
	 vars_.set(name,c);
   }
   ++cur_;
   return true;
}


////////////////////////////////////////////////////////////////////////////////////////////////////
// Makro ausführen. 
// c: Inhalt der aktuellen Zeile.
// return: True: Makro ausgeführt. False: kein Makro in aktueller Zeile.
////////////////////////////////////////////////////////////////////////////////////////////////////

bool Preprocessor::do_macro(const char *c)
{
   ++c;
   const char *tname = next_word(&c);		// Makroname ohne '.'
   if (tname == 0)
      return false;
   Macro *t = Macro::find(local_macros_,tname);
   if (t == 0)
      t = Macro::find(global_macros,tname);
   if (t == 0)
      return false;				// Makro existiert nicht
   if (t->in_use_) {
      YUERR(L04,circular_dependency('m',t->name_));
      return true;
   }
   StringMap args;

   // Argumente in der ersten Zeile auswerten
   for (unsigned i = 0; skip_blank(&c) != 0 && i < t->args_.size(); ++i) {
      if (i == t->args_.size() - 1 && t->allow_var_args_) {
         args.set(t->args_[i], c);	// Alle restlichen Argumente aufnehmen
         while (*c != 0)
            ++c;
      }
      else
         args.set(t->args_[i], next_word(&c));
   }
   if (skip_blank(&c) != 0) {
      YUERR(L09,too_many_args(tname));
      return true;
   }

   // Benannte Argumente hinzufügen
   while (exit_code <= 1 && cur_ + 1 < eoi_ && IS_SPACE(cur_[1].text[0])) {
      ++cur_;
      Assignment as;
      if (!as.split(cur_->text) || as.mode != SET || as.cfg != 0)
         YUERR(S01,syntax_error());
      else if (args.contains(as.name))
         YUERR(L03,redefined(0,as.name,0));
      else if (!t->args_.find(0, as.name))
         YUERR(L02,unknown_arg(as.name,t->name_));
      else {
	 Str buf;
	 if (expand_vars(buf,as.value))
	    args.set(as.name,buf);
      }
   }
   if (exit_code > 1) return true;

   // Prüfe, ob alle Argumente vollständig sind
   for (unsigned i = 0; i < t->args_.size(); ++i) {
      if (!args.contains(t->args_[i])) {
	 if (const char *defval = t->defaults_[t->args_[i]])
	    args.set(t->args_[i],defval);
	 else if (i == t->args_.size() - 1 && t->allow_var_args_)
            args.set(t->args_[i], "");
         else
            YUERR(L01,undefined('m',t->args_[i]));	// [T:pm11]
      }
   }
   if (exit_code > 1) return true;

   // Makro ausführen
   t->in_use_ = true;
   Preprocessor p(prj_, t->begin_, t->end_, this, first_time_);
   p.vars_.swap(args);
   p.execute();
   t->in_use_ = false;
   ++cur_;
   return true;
}


////////////////////////////////////////////////////////////////////////////////////////////////////
// Gepufferte Zeilen ausgeben.
// Übergibt die bislang gepufferten Ausgabezeilen an den Regelsatzerzeuger
// und löscht den Puffer.
////////////////////////////////////////////////////////////////////////////////////////////////////

void Preprocessor::flush()
{
   if (obuf_size_ > 0) {
      flushing_ = true;
      prj_->parse_buildfile(obuf_, obuf_ + obuf_size_);
      flushing_ = false;
      obuf_size_ = 0;
      obuf_ = 0;                // Nicht freigeben!
      obuf_capacity_ = 0;
   }
}


////////////////////////////////////////////////////////////////////////////////////////////////////
// Eine Zeile an den Ausgabepuffer anhängen. «src» ist die Quellzeile, aus welcher die Ausgabezeile
// entstanden ist (zwecks Ausgabe des Kontextes bei Fehlern).
// Weitergabe der gepufferten zeilen an Phase 2 erfolgt in «Flush()».
////////////////////////////////////////////////////////////////////////////////////////////////////

void Preprocessor::emit(SrcLine const *src, const char *txt)
{
   if (obuf_size_ >= obuf_capacity_)
      array_realloc(obuf_, obuf_capacity_ += 100);
   SrcLine *s = obuf_ + obuf_size_++;
   s->file = src->file;
   s->line = src->line;
   s->text = str_freeze(txt);
   s->up = caller_;
}


////////////////////////////////////////////////////////////////////////////////////////////////////
// Vorverarbeitung eines Zeilenbereiches.
// Die Funktion durchläuft den -- bei der Initialisierung des Objektes festgelegten -- 
// Bereich von Eingabezeilen, führt alle darin enthaltenen Phase1-Anweisungen aus
// und übergibt das Ergebnis abschnittweise an das Projekt zur Auswertung.
////////////////////////////////////////////////////////////////////////////////////////////////////

void Preprocessor::execute()
{
   while (exit_code < 2 && cur_ < eoi_) {
      YabuContext ctx(cur_,0,0);
      sob_ = cur_;
      Str buf;
      if (!expand_vars(buf, cur_->text)) break;
      const char *c = buf;
      if (*c == '.') {           // Ist es eine Präprozessor-Anweisung?
	 flush();
	 if (skip_str(&c, ".include_output "))
	    do_include_output();
	 else if (skip_str(&c, ".include "))
	    do_include(c);
	 else if (skip_str(&c, ".foreach "))
	    do_foreach(c);
	 else if (skip_str(&c, ".define "))
	    do_define(c);
	 else if (skip_str(&c, ".once"))
	    do_once();
	 else if (do_macro(c))  // Makro  v o r  Zuweisung auswerten! [T:pm06]
	    ;
	 else if (do_assignment(c))
	    ;
	 else {
	    YUERR(L01,undefined(0,next_word(&c)));
	    break;
	 }
      } else			// Keine Präprozessor-Anweisung
	 emit(cur_++, c);
   }
   if (exit_code < 2) 
      flush();
}

// vim:sw=3:cin:fileencoding=utf-8

