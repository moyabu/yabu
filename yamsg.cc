////////////////////////////////////////////////////////////////////////////////////////////////////
// I, the creator of this work, hereby release it into the public domain. This applies worldwide.
// In case this is not legally possible: I grant anyone the right to use this work for any purpose,
// without any conditions, unless such conditions are required by law.
////////////////////////////////////////////////////////////////////////////////////////////////////

// Ausgabe von Meldungen, Internationalisierung.

#include "yabu.h"
#include <limits.h>
#include <string.h>
#include <errno.h>
#include <signal.h>
#include <stdarg.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <locale.h>


class HighlightSetting: public StringSetting {
public:
   HighlightSetting(const char *name);
   static void get(const char **beg, const char **end, unsigned flags);
private:
   const char *beg_;
   const char *end_;
   void set_impl(const char *value);
};


IntegerSetting max_warnings("max_warnings",-1,INT_MAX,-1);// Anzahl erlaubter Warnungen (-k)
IntegerSetting verbosity("verbosity",-3,10,0);		// Message-Level (-v,-q)

static StringSetting yabu_locale("locale", "");		// Sprache, überstimmt LC_MESSAGES
static HighlightSetting highlight_e("highlight_e");	// Hervorhebung von Fehlern
static HighlightSetting highlight_w("highlight_w");	// Hervorhebung von Warnungen
static HighlightSetting highlight_0("highlight_0");	// Hervorhebung Ebene 0
static HighlightSetting highlight_s("highlight_s");	// Hervorhebung Gruppe s

static bool use_highlight = false;			// stdout ist ein Terminal
static unsigned language = 0;				// Gewählte Sprache.
static const unsigned yl_de = 0x1965;			// Deutsch
static const unsigned yl_en = 0x192e;			// Englisch
static const unsigned yl_es = 0x1933;			// Spanisch
static const unsigned yl_fr = 0x19f2;			// Französisch
static const unsigned yl_it = 0x1a34;			// Italienisch

static bool utf8_locale = false;			// Ausgabe in UTF-8



static SrcLine const *ysrc = 0;
static Target const *ytgt = 0;
static char const *ymsg = 0;

YabuContext::YabuContext(SrcLine const *src, Target const *tgt, char const *msg)
   : src_(ysrc), tgt_(ytgt), msg_(ymsg)
{
   ysrc = src;
   ytgt = tgt;
   ymsg = msg;
}

YabuContext::~YabuContext()
{
   ysrc = src_;
   ytgt = tgt_;
   ymsg = msg_;
}




////////////////////////////////////////////////////////////////////////////////////////////////////
// Konstruktor
////////////////////////////////////////////////////////////////////////////////////////////////////

HighlightSetting::HighlightSetting(const char *name)
   : StringSetting(name,""), beg_(""), end_("")
{
}


////////////////////////////////////////////////////////////////////////////////////////////////////
// Wert setzen und, wenn er die Form "BEGIN|END" hat, in die beiden Teile zerlegen.
////////////////////////////////////////////////////////////////////////////////////////////////////

void HighlightSetting::set_impl(const char *value)
{
   StringSetting::set_impl(value);
   const char *sep = strchr(*this,'|');
   if (sep != 0) {
      beg_ = str_freeze((const char *) *this,sep - (const char *) *this);
      end_ = str_freeze(sep + 1);
   } else
      beg_ = end_ = "";
}


////////////////////////////////////////////////////////////////////////////////////////////////////
// Hervorgebung für eine gegebene Meldung bestimmen. Nach Rückkehr zeigen «beg» und «end» auf die
// Steuersequenzen zum Ein- bzw. Ausschalten der Hervorhebung.
////////////////////////////////////////////////////////////////////////////////////////////////////

void HighlightSetting::get(const char **beg, const char **end, unsigned flags)
{
   HighlightSetting *which = 0;
   if (use_highlight) {
      unsigned const level = MSG_LEVEL(flags);
      if (level <= MSG_E)
	 which = &highlight_e;
      else if (level == MSG_W)
	 which = (*highlight_w.beg_ != 0) ? &highlight_w : &highlight_e;
      else if (level == MSG_0)
	 which = (flags & MSG_SCRIPT) ? &highlight_s : &highlight_0;
   }
   if (which) {
      *beg = which->beg_;
      *end = which->end_;
   } else
      *beg = *end = "";
}


////////////////////////////////////////////////////////////////////////////////////////////////////
// Standard-Ausgabefunktion (Ausgabe auf stdout).
////////////////////////////////////////////////////////////////////////////////////////////////////

void std_message_handler(unsigned flags, const char *txt)
{
   fputs(txt,stdout);
}

static void (*msg_handler)(unsigned flags, const char *txt) = std_message_handler;

////////////////////////////////////////////////////////////////////////////////////////////////////
// Ausgabefunktion für Meldungen setzen
////////////////////////////////////////////////////////////////////////////////////////////////////

void set_message_handler(void (*h)(unsigned flags, const char *txt))
{
   msg_handler = h;
}



////////////////////////////////////////////////////////////////////////////////////////////////////
// Konvertiert einen UTF8-String in den lokalen Zeichensatz (zur Zeit nur ISO-8859-15). Nicht
// konvertierbare Zeichen werden durch '?' ersetzt.
////////////////////////////////////////////////////////////////////////////////////////////////////

static void utf8_to_local(char *buf)
{
   const unsigned char *rp = (const unsigned char *) buf;
   unsigned char *wp = (unsigned char *) buf;
   while (*rp) {
      if ((*rp & 0x80) == 0)
         *wp++ = *rp++;			// ASCII-Zeichen unverändert lassen
      else {
	 // Multibyte-Sequenzen auswerten
         unsigned n = *rp++;
	 int len = 0;
	 if ((n & 0xE0) == 0xC0) { n &= 0x1f; len = 1; }
	 else if ((n & 0xF0) == 0xE0) { n &= 0x0f; len = 2; }
	 else if ((n & 0xF8) == 0xF0) { n &= 0x07; len = 3; }
	 else if ((n & 0xFC) == 0xF8) { n &= 0x03; len = 4; }
	 else if ((n & 0xFE) == 0xFC) { n &= 0x01; len = 5; }
	 for (; len > 0 && *rp; --len)
	    n = (n << 6) | (*rp++ & 0x3F);
	 // Viele Zeichen in 8859-15 haben den gleichen Wert im UCS. Wir behandeln
	 // deshalb hier nur Zeichen, die in 8859-15 nicht darstellbar sind.
	 if (n > 0x100 || n < 0x20) {
	    switch (n) {
               case 0x0107: n = 'c';  break;   // LATIN SMALL LETTER C WITH ACUTE
               case 0x0131: n = 'i';  break;   // LATIN SMALL LETTER DOTLESS I
               case 0x0152: n = 0xBC; break;   // LATIN CAPITAL LIGATURE OE
               case 0x0153: n = 0xBD; break;   // LATIN SMALL LIGATURE OE
               case 0x0160: n = 0xA6; break;   // LATIN CAPITAL LETTER S WITH CARON
               case 0x0161: n = 0xA8; break;   // LATIN SMALL LETTER S WITH CARON
               case 0x0178: n = 0xBE; break;   // LATIN CAPITAL LETTER Y WITH DIAERESIS
               case 0x017c: n = 'z';  break;   // LATIN SMALL LETTER Z WITH DOT ABOVE
               case 0x017d: n = 0xB4; break;   // LATIN CAPITAL LETTER Z WITH CARON
               case 0x017e: n = 0xB8; break;   // LATIN SMALL LETTER Z WITH CARON
               case 0x1ec3: n = 0xEA; break;   // LATIN SMALL E WITH CIRC. AND HOOK ABOVE -> ê
               case 0x1ed1: n = 0xD5; break;   // LATIN SMALL O WITH CIRC. AND ACUTE -> õ
               case 0x1ed9: n = 0xF4; break;   // LATIN SMALL O WITH CIRC. AND DOT BELOW -> ô
               case 0x1edb: n = 0xF3; break;   // LATIN SMALL O WITH HORN AND ACUTE -> ó
               case 0x20ac: n = 0xA4; break;   // EURO SIGN
	       default: n = '?';
            }
	 }
         *wp++ = n;
      }
   }
   *wp = 0;
}


////////////////////////////////////////////////////////////////////////////////////////////////////
// Arbeitet wie «sprintf()», aber das Ergebnis wird als Yabu-String (siehe «str_freeze()»)
// zurückgegeben.
////////////////////////////////////////////////////////////////////////////////////////////////////

const char *Msg::Format(const char *fmt, ...)
{
   char tmp[5000];
   va_list args;
   va_start(args, fmt);
   vsnprintf(tmp, sizeof(tmp), fmt, args);
   va_end(args);
   return str_freeze(tmp);
}



static void trace_back(Str &buf)
{
   //if (ytgt == 0 && ymsg == 0 && yasrc == 0)
      //return;
   if (ymsg) {
      buf.printfa("  * %s", ymsg);
      dump_source(buf,ysrc);
      buf.printfa("\n");
   }
   else if (ysrc) {
      buf.printfa("  * |%s|", ysrc->text);
      dump_source(buf,ysrc);
      buf.printfa("\n");
   }
   
   if (Target const *tgt = ytgt) {
      Rule *r = tgt->build_rule_;
      buf.printfa("  * %s", Msg::building(tgt,0,0));
      if (r != 0)
	 dump_source(buf,r->srcline_);
      buf.printfa("\n");
      if (tgt->older_than_)
	 buf.printfa("  * %s\n", Msg::is_outdated(tgt->name_,tgt->older_than_->name_));

      for (Target const *t = tgt; t; ) {
	 Dependency const *d = t->req_by_;
	 if (d != 0) {
	    buf.printfa("  * %s",Msg::depends_on(d->tgt_->name_,t->name_,d->rule_ != 0));
	    if (d->rule_ && d->rule_->srcline_)
	       dump_source(buf,d->rule_->srcline_);
	    buf.append("\n");
	    t = d->tgt_;
	 } else {
	    buf.printfa("  * %s\n", Msg::selecting(t->name_));
	    t = 0;
	 }
      }
   }
}



////////////////////////////////////////////////////////////////////////////////////////////////////
// Meldung ausgeben, falls mit dem aktuellen Wert von «verbosity» verträglich. Der Returnwert ist
// «true», wenn die Meldung ausgegeben wurde.
////////////////////////////////////////////////////////////////////////////////////////////////////

static bool MessageV(unsigned flags, const char *tag, const char *msg, va_list args)
{
   if (!MSG_OK(flags))
      return false;
   Str buf;
   const char *hl_beg, *hl_end;
   HighlightSetting::get(&hl_beg,&hl_end,flags);
   buf.append(hl_beg);					// Hervorhebung ein

   // Bei Warnungen und Fehlern: Präfix ausgeben und Exit-Code setzen
   int exc = (int) MSG_0 - MSG_LEVEL(flags);
   static int n_warnings = 0;
   if (exc == 1 && max_warnings >= 0 && ++n_warnings > max_warnings)
      exc = 2;						// Warnung als Fehler behandeln
   if (exc > 0) {
      if (exit_code < exc)
	 exit_code = exc;
      buf.append("*** ");
      if (tag != 0) {
	 if (tag) buf.append(tag);
	 if (ysrc)
	    buf.printfa(" [%s]",Msg::line(ysrc->file,ysrc->line));
         buf.append(": ");
      }
   }

   // Text
   buf.vprintfa(msg, args);
   buf.append(hl_end);
   buf.append("\n");

   // Fehlerkontext
   if (exc > 0 && verbosity > 0)
      trace_back(buf);

   // Ausgabe
   if (!utf8_locale)
      utf8_to_local(buf.data());
   msg_handler(flags,buf);

   if (exit_code > 2) 		// MSG_F -> sofortiger Abbruch
      exit(3);
   return true;
}


////////////////////////////////////////////////////////////////////////////////////////////////////
// Wie MessageV(), aber Argumente wie bei printf()
////////////////////////////////////////////////////////////////////////////////////////////////////

bool Message(unsigned flags, const char *msg, ...)
{
    va_list args;
    va_start(args, msg);
    return MessageV(flags, 0, msg, args);
    va_end(args);
}


////////////////////////////////////////////////////////////////////////////////////////////////////
// Einen Text zeilenweise ausgeben, dabei jeder Zeile den Text «prefix» voranstellen.
// Im Interschied zu ||Message()|| findet keine printf-Formatierung statt.
////////////////////////////////////////////////////////////////////////////////////////////////////

void MessageBlock(unsigned flags, const char *prefix, const char *text)
{
   if (!MSG_OK(flags))
      return;
   while (text != 0 && *text != 0) {
      const char *eol = strchr(text, '\n');
      if (eol != 0)
         Message(flags, "%s%.*s", prefix, (int) (eol - text), text);
      else
         Message(flags, "%s%s", prefix, text);
      text = eol ? eol + 1 : 0;
   }
}


////////////////////////////////////////////////////////////////////////////////////////////////////
// Zeilenangabe in der Form @DATEI(Zeile)@DATEI(Zeile)...
////////////////////////////////////////////////////////////////////////////////////////////////////

void dump_source(Str &buf, SrcLine const *s)
{
   const char *f = 0;
   int n = 5;
   for (; s && --n > 0; s = s->up) {
      if (f == 0) buf.append(" @");
      buf.printfa("%s(%u)",s->file != f ? s->file : "", s->line);
      f = s->file;
      if (verbosity < 2) break;
   }
}


////////////////////////////////////////////////////////////////////////////////////////////////////
// Sprache und Zeichensatz festlegen
////////////////////////////////////////////////////////////////////////////////////////////////////

void set_language()
{
   // System-locale gemäß «yabu_locale» setzen.
   const char *lang = setlocale(LC_MESSAGES,yabu_locale);
   // Falls das System die locale «yabu_locale» nicht unterstüzt, verwenden wir sie trotzdem.
   if (lang == 0) lang = yabu_locale;
   // Letzter Ausweng: Environment auswerten.
   if (*lang == 0)  {
      lang = getenv("LC_ALL");
      if (lang == 0) lang = getenv("LC_MESSAGES");
      if (lang == 0) lang = getenv("LANG");
   }
   if (lang == 0) lang = "";

   utf8_locale = (strstr(lang, "UTF") || strstr(lang, "utf"));
   language = 0;
   for (const char *lc = lang; lc && *lc && *lc != '_' && *lc != '.'; ++lc)
      language = (language << 6) ^ *lc;
   use_highlight = isatty(fileno(stdout));
}


////////////////////////////////////////////////////////////////////////////////////////////////////
// Gibt eine Fehlermeldung aus.
////////////////////////////////////////////////////////////////////////////////////////////////////

static bool yabu_error2(unsigned flags, const char *tag, const char *msg, ...)
{
    va_list args;
    va_start(args, msg);
    return MessageV(flags, tag, msg, args);
    va_end(args);
}

void yabu_error(const char *id, unsigned flags, const char *msg)
{
   yabu_error2(flags, id, "%s", msg);
}



#define M(dflt,xlat) \
   switch (language) { xlat } return Format dflt;
#define M_(l,x) case yl_##l: return Format x; break;


////////////////////////////////////////////////////////////////////////////////////////////////////
// Gxx - Allgemeine und externe Fehler
////////////////////////////////////////////////////////////////////////////////////////////////////

const char *Msg::G03_assertion_failed(const char *fn, int line, const char *txt)
{
   M(   ("INTERNAL ERROR: %s(%d): %s", fn,line,txt),
   M_(de,("INTERNER FEHLER: %s(%d): %s", fn,line,txt))
   M_(es,("ERROR INTERNO: %s(%d): %s", fn,line,txt))
   M_(fr,("ERREUR INTERNE: %s(%d): %s", fn,line,txt))
   M_(it,("ERRORE INTERNO: %s(%d): %s", fn,line,txt))
    )
}


const char *Msg::G10_cannot_open(const char *name)
{
   const char *errmsg = strerror(errno);
   M(   ("Cannot open %s: %s", name, errmsg),
   M_(de,("Kann %s nicht öffnen: %s", name, errmsg))
   M_(es,("No se puede abrir %s: %s", name, errmsg))
   M_(fr,("Ne peut ouvrir %s : %s", name, errmsg))
   M_(it,("Impossibile aprire %s: %s", name, errmsg))
    )
}

const char *Msg::G20_syscall_failed(const char *name, const char *arg)
{
   if (arg == 0) arg = "";
   const char *errmsg = strerror(errno);
   M(   ("System call %s(%s) failed, %s", name, arg, errmsg),
   M_(de,("%s(%s) fehlgeschlagen: %s", name, arg, errmsg))
   M_(es,("Falló la llamada al sistema %s(%s), %s", name, arg, errmsg))
   M_(fr,("L'appel système %s(%s) a échoué, %s", name, arg, errmsg))
   M_(it,("La chiamata di sistema %s(%s) è fallita, %s", name, arg, errmsg))
    )
}


const char *Msg::G30_script_failed(const char *target, const char *script_type)
{
   if (script_type == 0) script_type = "";
   M(   ("Script%s for %s failed", script_type, target),
   M_(de,("Fehler im Skript%s für %s", script_type, target))
   M_(es,("Falló el guión%s para %s ", script_type, target))
   M_(fr,("Script%s pour %s en échec", script_type, target))
   M_(it,("Lo script%s per %s è fallita", script_type, target))
    )
}


const char *Msg::G31_not_built(const char *target)
{
   M(   ("%s was not generated", target),
   M_(de,("%s wurde nicht erzeugt",target))
   M_(es,("%s no se ha creado", target))
   M_(fr,("%s n'a pas été créé", target))
   M_(it,("%s non è stato creato", target))
    )
}


const char *Msg::G32_target_exists(const char *target)
{
   M(   ("Cannot overwrite existing target %s", target),
   M_(de,("%s existiert und darf nicht überschrieben werden",target))
   M_(fr,("Le cible existant %s ne peut pas être écrasé",target))
    )
}




const char *Msg::G41_unknown_option(char opt)
{
   M(   ("Unknown option '-%c'", opt),
   M_(de,("Unbekannte Option '-%c'", opt))
   M_(es,("Opción desconocida '-%c'", opt))
   M_(fr,("Option inconnue «%c»", opt))
   M_(it,("Opzione '-%c' non riconosciuta", opt))
   )
}


const char *Msg::G44_need_state_file(const char *algo)
{
   M(    ("Algorithm '%s' not available because the state file is disabled",algo),
   M_(fr,("L'algorithme '%s' n'est pas disponible parce que le fichier d'état est désactivé",algo))
   M_(de,("Der Algorithmus '%s' ist nicht verfügbar, da die Statusdatei deaktiviert ist",algo))
   M_(es,("El algoritmo '%s' no está disponible porque el fichero de estado está desactivado",algo))
   M_(it,("L'algoritmo '%s' non è disponible perché Buildfile.state non è attivato",algo))
   )
}




////////////////////////////////////////////////////////////////////////////////////////////////////
// Lxx - Logische Fehler (alles was kein externer oder Syntaxfehler ist)
////////////////////////////////////////////////////////////////////////////////////////////////////

static void pre_post(const char **pre, const char **post, char kind)
{
   *pre = ""; *post = "";
   switch (kind) {
      case '$': *pre = "$("; *post = ")"; break;
      case '.': *pre = "$(."; *post = ")"; break;
      case '[': *pre = "["; *post = "]"; break;
      case 'm': *pre = "."; break;
      case '%': *pre = "%"; break;
      case '*': *pre = "*"; break;
      case '@': *pre = "@"; break;
      case '&': *pre = "&"; break;
      case '~': *pre = "~"; break;
      case '+': *pre = "+"; break;
      case '#': *pre = "#"; break;
      case '-': *pre = " option '"; *post = "'"; break;
   }
}


const char *Msg::L01_undefined(char kind, const char *name)
{
   char const *pre, *post;
   pre_post(&pre,&post,kind);

   M( ("%s%s%s undefined", pre,name,post),
   M_(de,("%s%s%s ist nicht definiert", pre,name,post))
   M_(es,("%s%s%s no ha sido definido", pre,name,post))
   M_(fr,("Non défini: %s%s%s", pre,name,post))
   M_(it,("%s%s%s sconosiuto", pre,name,post))
   )
}


const char *Msg::L02_unknown_arg(const char *arg, const char *macro)
{
   M(   (".%s: unknown argument '%s'",macro,arg),
   M_(de,("Unbekanntes Argument '%s' für .%s", arg,macro))
   M_(es,(".%s: argumento desconocido: '%s'", macro,arg))
   M_(fr,(".%s: argument inconnu: '%s'", macro,arg))
   M_(it,(".%s: argomento sconosciuto: '%s'", macro,arg))
    )
}

const char *Msg::L03_redefined(char kind, const char *name, const SrcLine *src)
{
   char const *pre, *post;
   pre_post(&pre,&post,kind);
   const char *prev = src ? Msg::line(src->file,src->line) : "";
   const char *sep = *prev ? " @" : "";

   M(   ("%s%s%s already defined%s%s", pre,name,post,sep,prev),
   M_(de,("%s%s%s ist bereits definiert%s%s", pre,name,post,*prev ? " in " : "",prev))
   M_(es,("%s%s%s ya esta definido%s%s", pre,name,post,sep,prev))
   M_(fr,("%s%s%s est déjà défini%s%s", pre,name,post,sep,prev))
   M_(it,("%s%s%s ridefinito%s%s", pre,name,post,sep,prev))
    )
}


// L04: Variable oder Ziel hängt von sich selbst ab

const char *Msg::L04_circular_dependency(char kind, const char *name)
{
   char const *pre, *post;
   pre_post(&pre,&post,kind);

   M(    ("'%s%s%s' depends on itself", pre,name,post),
   M_(de,("'%s%s%s' hängt von sich selbst ab", pre,name,post))
   M_(es,("Dependencia circular de '%s%s%s'", pre,name,post))
   M_(fr,("Dépendance circulaire pour '%s%s%s'", pre,name,post))
   M_(it,("'%s%s%s' dipende di se stesso", pre,name,post))
    )
}


// L05: Ziel hat keine Regel und uns keine normale Datei

const char *Msg::L05_nonregular_leaf(const char *target)
{
   M(   ("%s is not a regular file", target),
   M_(de,("%s ist keine reguläre Datei", target))
   M_(es,("%s no es un fichero regular", target))
   M_(fr,("%s n'est pas un fichier régulier", target))
   M_(it,("%s non è un file regolare", target))
    )
}


// L09: Zu viele Argumente in Makroaufruf

const char *Msg::L09_too_many_args(const char *name)
{
   M(    ("Macro '%s' called with too many arguments", name),
   M_(de,("Zu viele Argumente für '%s'", name ))
   M_(es,("Número incorrecto de argumentos para '%s'", name ))
   M_(fr,("Trop d'arguments pour '%s:'", name ))
   M_(it,("'%s:' numero di argomenti errato", name))
    )
}




// L11: mehr als 9 '%' oder '*'-Platzhalter

const char *Msg::L11_too_many_placeholders(char tag)
{
   M(   ("Too many %c's in pattern", tag),
   M_(de,("Zu viele '%c' in Muster", tag))
   M_(es,("Demasiados %cs en el patrón", tag))
   M_(fr,("Trop de %cs dans le schéma", tag))
   M_(it,("Troppi '%c' nel modello", tag))
    )
}


// L12: !options nicht mehr möglich

const char *Msg::L12_options_finalized()
{
   M( ("'!options' is not allowed at this point in file"),
   M_(de,("An dieser Stelle können keine Optionen mehr definiert werden"))
   M_(es,("No se pueden definir opciones en este punto"))
   M_(fr,("Impossible de créer des options à ce point"))
   M_(it,("Non si possono definire opzioni a questo punto"))
    )
}



const char *Msg::L13_assignment_to_system_variable(const char *name)
{
   M(    ("Assignment to system variable %s not allowed",name),
   M_(fr,("Interdiction de modifier la variable interne %s",name))
   M_(es,("No se permite cambiar la variable de sistema %s",name))
   M_(de,("Systemvariable %s darf nicht geändert werden",name))
   M_(it,("Non è permesso cambiare il variabile di sistema %s",name))
    )
}


// L14: Wert paßt nicht zur linken Seite der Transformation

const char *Msg::L14_no_match(const char *vname, const char *val, const char *pat)
{
   M(   ("$(%s): value '%s' does not match pattern '%s'", vname, val, pat),
   M_(de,("$(%s): der Wert '%s' paßt nicht zum Muster '%s'", vname, val, pat))
   M_(es,("$(%s): el valor '%s' no coincide con el patrón '%s'", vname, val, pat))
   M_(fr,("$(%s): la valeur «%s» ne correspond pas au modèle «%s»", vname, val, pat))
   M_(it,("$(%s): il valore '%s' non corrisponde al modello '%s'", vname, val, pat))
    )
}


// L24: Widersprüchliche Optionen in einer Konfiguration

const char *Msg::L24_conflicting_options(const char *name, const char *name2)
{
   if (name2) {
      M(   ("Cannot set option '%s' because '%s' is already set", name, name2),
      M_(de,("Option '+%s' ist unzulässig, da '%s' schon gesetzt ist", name, name2))
      M_(fr,("Impossible d'activer l'option «%s» parce que «%s» est déjà active",name,name2))
      M_(es,("Opción '%s' no es posible porque '%s' ya está activa",name,name2))
      M_(it,("L'opzione '%s' non può essere attivata, perché '%s' è già attivo",name,name2))
       )
   }
   else {
      M(   ("Conflicting values for option %s", name),
      M_(fr,("Valeurs contradictoires de l'option «%s»", name))
      M_(de,("Widersprüchliche Werte für '%s'", name))
      M_(es,("Valores contradictorios para la opción «%s»", name))
      M_(it,("Valori dell'opzione '%s' in conflitto", name))
       )
   }
}


// L31: Mehrdeutige Regeln für ein Ziel

const char *Msg::L31_ambiguous_rules(const char *target, const SrcLine *s1, const SrcLine *s2)
{
   const char *p1 = s1 ? Msg::line(s1->file,s1->line) : "???";
   const char *p2 = s2 ? Msg::line(s2->file,s2->line) : "???";

   M(   ("Ambiguous rules for %s at %s and %s", target, p1, p2),
   M_(de,("Mehrdeutige Regeln für %s bei %s und %s", target, p1, p2))
   M_(es,("Reglas ambiguas para «%s» en %s y %s", target, p1, p2))
   M_(fr,("Règles ambiguës pour «%s» à %s et %s", target, p1, p2))
   M_(it,("Regole ambigue per '%s' a %s e %s", target, p1, p2))
    )
}


// L33: Keine Regel gefunden

const char *Msg::L33_no_rule(const char *target)
{
   M(   ("No rule for '%s' found (or rule not applicable)", target),
   M_(de,("Keine passende Regel für '%s'", target))
   M_(es,("No hay regla para '%s' (o regla no se puede utilizar)", target))
   M_(fr,("Règle non trouvée pour «%s» (ou règle non utilisable)", target))
   M_(it,("Nessuna regola trovata per '%s' (o regola inutilizzabile)", target))
    )
}


// L35: Widersprüchliche Zuweisungen

const char *Msg::L35_conflicting_values(const char *var, const char *cfg,
      const SrcLine *s1, const SrcLine *s2)
{
   const char *p1 = s1 ? Msg::line(s1->file,s1->line) : "???";
   const char *p2 = s2 ? Msg::line(s2->file,s2->line) : "???";
   M(   ("Conflicting values for $(%s) [%s] at %s and %s",var,cfg,p1,p2),
   M_(de,("Widersprüchliche Werte für $(%s) [%s] in %s und %s",var,cfg,p1,p2))
   M_(fr,("Valeurs conflictuelles pour $(%s) [%s] à %s et %s",var,cfg,p1,p2))
   M_(es,("Valores en conflicto para $(%s) [%s] en %s et %s",var,cfg,p1,p2))
   M_(it,("Valori per $(%s) [%s] in conflitto: %s e %s",var,cfg,p1,p2))
    )
}



const char *Msg::L40_too_many_nested_includes()
{
   M(   ("Too many nested include files"),
   M_(de,("Zu viele verschachtelte Include-Dateien"))
   M_(fr,(".include imbriqué trop profondément"))
   M_(es,(".include anidado con demasiada profundidad"))
    )
}

const char *Msg::L42_conflicting_prj_root(const char *root, const Project *prj)
{
   const char *src = Msg::line(prj->src_);
   M(   ("%s collides with !project @%s",root,src),
   M_(de,("Verzeichnis '%s' kollidiert mit bestehendem Projekt @%s",root,src))
    )
}

const char *Msg::L43_invalid_prj_root(const char *root)
{
   M(    ("Invalid project root '%s'",root),
   M_(de,("Ungültiges Projektverzeichnis '%s'",root))
    )
}
   

const char *Msg::archive_not_found(const char *ar_name, int err)
{
   M(   ("Archive %s not found (errno=%d)", ar_name, err),
   M_(de,("Bibliothek %s nicht gefunden (errno=%d)", ar_name, err))
   M_(es,("Archivo %s no se encuentra (errno=%d)", ar_name, err))
   M_(fr,("L'archive «%s» n'existe pas (errno=%d)", ar_name, err))
   M_(it,("Archivio '%s' non trovato (errno=%d)", ar_name, err))
   )
}


const char *Msg::building(const Target *tgt, const char *host, const char *script_type)
{
#if 1
   Str tmp(tgt->prj_->aroot_);
   tmp.append(tgt->name_);
   if (script_type)
      tmp.append(script_type);
   if (host)
       tmp.append(" @").append(host);
   const char * const t = tmp;
#else
   const char * const t = host;
#endif

#if 1
   M(    ("Building %s", t),
   M_(de,("Erzeuge %s", t))
   M_(es,("Creando %s", t))
   M_(fr,("Fabriquer %s", t))
   M_(it,("Rifacendo %s", t))
   )
#endif
}

const char *Msg::selecting(const char *target)
{
   M(   ("Selected target: %s", target),
   M_(de,("Ausgewähltes Ziel: %s", target))
   M_(es,("Se considera el objetivo %s", target))
   M_(fr,("Étude du cible %s", target))
   M_(it,("Target scelto: %s", target))
   )
}


const char *Msg::using_rule(const SrcLine *sl, const char *target)
{
   M(   ("Using rule @%s(%d) for %s", sl->file,sl->line, target),
   M_(de,("Ausgewählte Regel für %s: @%s(%d)", target,sl->file,sl->line))
   M_(es,("Usando la regla @%s(%d) para %s", sl->file,sl->line,target))
   M_(fr,("Utilisation de la règle @%s(%d) pour %s", sl->file,sl->line,target))
   M_(it,("Usando la regola @%s(%d) per %s", sl->file,sl->line, target))
   )
}



const char *Msg::depends_on(const char *target, const char *source, bool reg)
{
   const char * const post = reg ? "" : " (auto-depend)";
   M(   ("%s requires %s%s", target, source, post),
   M_(de,("%s setzt %s voraus%s", target, source,post)) 
   M_(es,("%s depende de %s%s", target, source,post))
   M_(fr,("%s dépend de %s%s", target, source,post))
   M_(it,("%s è richiesto di %s%s", source, target,post))
   )
}

const char *Msg::target_stats(unsigned total, unsigned utd, unsigned ok, unsigned ttime)
{
   M(   ("%u target%s: %u up to date, %u remade in %us", total, total == 1 ? "" : "s", utd, ok, ttime),
   M_(de,("%u Ziel%s: %u unverändert, %u erneuert in %us", total, total == 1 ? "" : "e", utd, ok, ttime))
   M_(es,("%u objetivo%s: %u inalterados, %u reconstruidos en %us", total, total == 1 ? "" : "s", utd, ok, ttime))	
   M_(fr,("%u cible%s : %u à jour, %u refabriquées en %us", total, total == 1 ? "" : "s", utd, ok, ttime))
   M_(it,("%u obiettivi: %u già aggiornati, %u rifatti (%us)", total, utd, ok, ttime))
   )
}

const char *Msg::target_is_up_to_date(const char *target)
{
   M(   ("%s is up to date", target),
   M_(de,("%s ist aktuell", target))
   M_(es,("%s está actualizado", target))
   M_(fr,("%s est à jour", target))
   )
}


const char *Msg::target_stats_failed(unsigned ok, unsigned failed, unsigned skipped)
{
   M(   ("NOT SUCCESSFUL: %u rebuilt, %u failed, %u skipped", ok, failed, skipped),
   M_(de,("NICHT ERFOLGREICH: %u erneuert, %u fehlerhaft, %u ausgelassen", ok, failed, skipped))
   M_(es,("FALLO: %u reconstruidos, %u fallidos, %u ignorados", ok, failed, skipped))		
   M_(fr,("ECHEC : %u refabriquées, %u echouées, %u escamotées", ok, failed, skipped))
   M_(it,("FALLITO: %u riusciti, %u falliti, %u omessi", ok, failed, skipped))
   )
}


const char *Msg::reset_mtime_after_error(const char *target)
{
   M(   ("%s may be damaged, reset mtime", target),
   M_(de,("%s könnte beschädigt sein, Änderungszeit zurückgesetzt", target))
   M_(es,("El fichero %s podría estar corrupto; reajustando la fecha de modificación", target))
   M_(fr,("%s pourrait être endommagé; ré-initialisation du temps de modification", target))
   )
}


const char *Msg::leaf_exists(const char *target)
{
   M(   ("%s exists (no sources)", target),
   M_(de,("%s existiert und hat keine Quellen", target))
   M_(fr,("%s existe et n'a pas de sources", target))
   M_(es,("%s existe y no tiene fuentes", target))
   )
}



const char *Msg::reading_file(const char *name, unsigned line, unsigned line2)
{
   char tmp[200];
   if (line2 > line) {
      snprintf(tmp,sizeof(tmp),"%s(%u..%u)", name,line,line2);
      name = tmp;
   } else if (line > 0) {
      snprintf(tmp,sizeof(tmp),"%s(%u)", name,line);
      name = tmp;
   }

   M(   ("Processing file: %s", name),
   M_(de,("Verarbeite Datei: %s", name))
   M_(es,("Procesando el fichero %s", name))
   M_(fr,("Traitement du fichier %s", name))
   M_(it,("Lettura di %s", name))
   )
}

const char *Msg::line(const char *fn, unsigned line)
{
   if (!strcmp(fn, "Buildfile")) {
      // Kurzform (nur Zeilennummer)
      M(   ("line %u", line),
      M_(de,("Zeile %u", line))
      M_(es,("línea %u", line))
      M_(fr,("ligne %u", line))
      M_(it,("riga %u", line))
      )
   }
   else {
      // Lange Form, aber Dateinamen auf 20 Zeichen beschränken
      size_t len = strlen(fn);
      return len <= 20 ? Format("%s(%u)", fn, line) : Format("...%s(%u)", fn + len - 20, line);
   }
}

const char *Msg::line(const SrcLine *src)
{
   return src ? line(src->file,src->line) : "-";
}


const char *Msg::already_built(const char *name)
{
   M(   ("%s already built", name),
   M_(de,("%s ist bereits aktualisiert", name))
   M_(fr,("%s a déjà été fabriqué", name))
   M_(es,("El objetivo %s ya está construido",name))
   )
}

const char *Msg::usage()
{
   M(  ("Syntax: yabu [-c <Cfg>] [-f <File>] [-g <CfgDir>] [-y <Algo>] \\\n"
        "             [-S <Shell>] [-D <Dump>] [-aAceEjJkKmMnpPqrRsSvVy] [<Target>]...\n"
        "\n"
        "    -a/A   Execute/don't execute auto dependency scripts\n"
        "    -c     Use specified configuration\n"
        "    -D     Dump internal data: c=options, p=settings,\n"
        "           r=rules, v=variables, T=targets, t=targets after phase 2, A=all\n"
        "    -e/E   Echo/don't echo commands\n"
        "    -f     Use <File> instead of Buildfile\n"
        "    -g     Global configuration directory\n"
        "    -j/J   Don't use/use Yabu server(s)\n"
        "    -k/K   Abort/continue on non-fatal error\n"
        "    -m/M   Create/don't create missing directories\n"
        "    -n     Dry run, don't execute commands. Implies -e\n"
        "    -p/P   Execute commands sequentially/concurrently\n"
        "    -q     Quiet\n"
        "    -r/R   Ignore/use $HOME/.yaburc\n"
        "    -s     Don't use Buildfile.state\n"
        "    -S     Use <Shell> instead of /bin/sh to execute scripts\n"
        "    -v     Verbose (-vv, -vvv.. to increase verbosity)\n"
        "    -V     Show version\n"
        "    -y     Select timestamp algorithm:\n"
        "              mt ........ modification time (default)\n"
        "              mtid ...... use mt, but treat like checksum\n"
        "              cksum ..... CRC checksum of file contents\n"),
   M_(de,("Syntax: yabu [-c <Cfg>] [-f <File>] [-g <CfgDir>] [-y <Algo>] \\\n"
        "             [-S <Shell>] [-D <Auswahl>] [-aAceEjJkKmMnpPqrRsSvVy] [<Ziel>]...\n"
        "\n"
        "    -a/A   Autodepend-Skripte nicht ausführen/ausführen\n"
        "    -c     Benutze die Konfiguration <Cfg>\n"
        "    -D     Interne Datenstrukturen ausgeben: c=Optionen, p=Einstellungen,\n"
        "           r=Regeln, v=Variablen, T=Ziele, t=Ziele nach Phase 2, A=alles\n"
        "    -e/E   Ausgeführte Kommandos ausgeben/nicht ausgeben\n"
        "    -f     Verwende <File> statt Buildfile\n"
        "    -g     Globales Konfigurationsverzeichnis\n"
        "    -j/J   Yabu-Server nicht verwenden/verwenden\n"
        "    -k/K   Bei Fehler abbrechen/weitermachen\n"
        "    -m/M   Wenn Verzeichnis fehlt: erstellen/abbrechen\n"
        "    -n     Probelauf, Kommandos nicht ausführen (impliziert -e)\n"
        "    -q     Weniger Meldungen ausgeben\n"
        "    -p/P   Skripte nacheinander/parallel ausführen\n"
        "    -r/R   $HOME/.yaburc ignorieren/verwenden\n"
        "    -s     Statusdatei Buildfile.state nicht verwenden\n"
        "    -S     Andere Shell benutzen (Default: /bin/sh)\n"
        "    -v     Mehr Meldungen ausgeben (noch mehr mit -vv, -vvv, ...)\n"
        "    -V     Versionsnummer anzeigen\n"
        "    -y     Algorithmus zur Erkennung veralteter Dateien:\n"
        "              mt ........ Änderungszeit (Standard)\n"
        "              mtid ...... Änderungszeit wie Prüfsumme behandeln \n"
        "              cksum ..... Prüfsumme (CRC)\n"))
   M_(fr,("Syntaxe: yabu [-c <Cfg>] [-f <Fichier>] [-g <CfgDir>] [-y <Algo>] \\\n"
        "             [-S <Shell>] [-aAceEjJkKmMnpPqrRsSvVy] [<Cible>]...\n"
        "\n"
        "    -a/A   Exécuter/ne pas exécuter les scripts «[auto-depend]»\n"
        "    -c     Utiliser configuration specifiée\n"
        "    -D     Montrer données internes: c=options, p=réglages,\n"
        "           r=règles, v=variables, t=objectifs, T=objectifs après la phase 2, A=tout\n"
        "    -e/E   Faire/ne pas faire l'écho des commandes\n"
        "    -g     Répertoire globale de configuration\n"
        "    -j/J   Ne pas utiliser/utiliser serveur(s) Yabu\n"
        "    -f     Lire <Fichier> à la place de Buildfile\n"
        "    -k/K   En cas d'erreur: avorter/continuer\n"
        "    -m/M   Créer/ne pas créer répertoires parents manquants\n"
        "    -n     N'exécuter aucune des commandes, seulement les afficher\n"
        "    -p/P   Exécuter les commandes une à une/simultanément\n"
        "    -q     Afficher moins de messages\n"
        "    -r/R   Ne pas lire/lire $HOME/.yaburc\n"
        "    -s     Ne pas utiliser le fichier d'état Buildfile.state\n"
        "    -S     Utiliser <Shell> au lieu de /bin/sh pour exécuter les scripts\n"
        "    -v     Afficher plus de messages (-vv, -vvv... )\n"
        "    -V     Afficher le numéro de version\n"
        "    -y     Choisir l'algorithme pour dépister les fichiers à refabriquer\n"
        "              mt ........ temps de modification (défaut)\n"
        "              mtid ...... traiter le temps de modification comme somme de contrôle\n"
        "              cksum ..... somme de contrôle CRC\n"))

   M_(es,("Sintaxis: yabu [-c <Cfg>] [-f <File>] [-g <CfgDir>] [-y <Algo>] \\\n"
        "             [-S <Shell>] [-D <Dump>] [-aAceEjJkKmMnpPqrRsSvVy] [<Target>]...\n"
        "\n"
        "    -a/A   Ejecutar/no ejecutar los guiónes «[auto-depend]»\n"
        "    -c     Utilizar la configuración especificada\n"
        "    -D     Indicar los datos internos: c=opciónes, p=arreglos, r=reglas,\n"
        "           v=variables, T=objectivos, t=objectivos después de la fase 2, A=todos\n"  
        "    -e/E   Indicar el pedido ejecutado/ no indicarle\n"
        "    -f     Leer <File> en lugar de Buildfile\n"
        "    -g     Directorio global de configuración\n"
        "    -j/J   No utilizar/utilizar yabusrv\n"
        "    -k/K   En el caso de error: cancelar/seguir\n"
        "    -m/M   Cuando un directorio falta: crear/no crear\n"
        "    -n     No ejecutar el pedido, solamente mostrar\n"
        "    -p/P   Ejecutar los pedidos uno por uno/juntos\n"        
        "    -q     Mostrar menos mesajes\n"
        "    -r/R   No leer/leer $HOME/.yaburc\n"
        "    -s     No utilizar el fichero de estado Buildfile.state\n"
        "    -S     Utilizar <Shell> en lugar de /bin/sh para executar los guiónes\n"
        "    -v     Mostrar más mesajes (-vv, -vvv... )\n"
        "    -V     Mostrar el número de versión\n"
        "    -y     Eligir el algoritmo para encontrar los ficheros antiguos\n"
        "              mt ........ tiempo de modificación (valor por defecto)\n"
        "              mtid ...... tratar el tiempo de modificación como suma de control\n"
        "              cksum ..... suma de control CRC\n"))
   )
}

const char *Msg::srv_usage()
{
   M(   ("Syntax: yabusrv [-g <CfgDir>] [-S <Shell>] [-qvV]\n"
        "\n"
        "    -g     Global configuration directory\n"
        "    -q     Quiet\n"
        "    -S     Use <Shell> instead of /bin/sh to execute scripts\n"
        "    -v     Verbose (-vv, -vvv.. to increase verbosity)\n"
        "    -V     Show version\n"),
   M_(de,("Syntax: yabusrv [-g <CfgDir>] [-S <Shell>] [-qvV]\n"
        "\n"
        "    -g     Globales Konfigurationsverzeichnis\n"
        "    -q     Weniger Meldungen ausgeben\n"
        "    -S     Andere Shell benutzen (Default: /bin/sh)\n"
        "    -v     Mehr Meldungen ausgeben (noch mehr mit -vv, -vvv, ...)\n"
        "    -V     Versionsnummer anzeigen\n"))
   M_(fr,("Syntaxe: yabu [-g <CfgDir>] [-S <Shell>] [-qvV]\n"
        "\n"
        "    -g     Répertoire globale de configuration\n"
        "    -q     Afficher moins de messages\n"
        "    -S     Utiliser <Shell> au lieu de /bin/sh pour exécuter les scripts\n"
        "    -v     Afficher plus de messages (-vv, -vvv... )\n"
        "    -V     Afficher le numéro de version\n"))
   M_(es,("Sintaxis: yabu [-g <CfgDir>] [-S <Shell>] [-qvV]\n"
        "\n"
        "    -g     Directorio global de configuración\n"
        "    -q     Mostrar menos mesajes\n"
        "    -S     Utilizar <Shell> en lugar de /bin/sh para executar los guiónes\n"
        "    -v     Mostrar más mesajes (-vv, -vvv... )\n"
        "    -V     Mostrar el número de versión\n"))
   )
}

const char *Msg::script_terminated_on_signal(int sig)
{
   const char *signame = 0;
   switch (sig) {
      case SIGINT: signame = "Interrupt"; break;
      case SIGILL: signame = "Illegal instruction"; break;
      case SIGKILL: signame = "Killed"; break;
      case SIGSEGV: signame = "Segmentation fault"; break;
      case SIGABRT: signame = "Aborted"; break;
      case SIGALRM: signame = "Alarm clock"; break;
      case SIGFPE: signame = "Floating point exception"; break;
      case SIGPIPE: signame = "Broken pipe"; break;
#ifdef SIGBUS
      case SIGBUS: signame = "Bus error"; break;
#endif
   }

   M(   (signame ? "Signal %d (%s)" : "Signal %d", sig, signame),
   M_(es,(signame ? "Señal %d (%s)" : "Señal %d", sig, signame))
   M_(it,(signame ? "Segnale %d (%s)" : "Segnale %d", sig, signame))
   )
}


const char *Msg::is_outdated(const char *target, const char *source)
{
   M(   ("%s is outdated with respect to %s", target, source),
   M_(de,("%s ist nicht mehr aktuell, da %s geändert wurde", target, source))
   M_(es,("La fuente %s es más reciente que el objetivo %s", source, target))
   M_(fr,("%s est plus récente que la cible %s", source, target))
   M_(it,("%s è più vecchio di %s", target, source))
   )
}


const char *Msg::target_missing(const char *target)
{
   M(   ("%s is missing and must be built", target),
   M_(de,("%s fehlt und muß erzeugt werden", target))
   M_(es,("%s falta y se debe reconstruir", target))
   M_(fr,("%s n'existe pas et doit être fabriqué", target))
   M_(it,("%s non esista e deve essere rifatto", target))
   )
}


const char *Msg::discarding_state_file(const char *fn)
{
   M(   ("Ignoring invalid state file %s", fn),
   M_(de,("%s ist ungültig und wird verworfen",fn))
   M_(es,("Fichero de estado %s es inválido y no se toma en cuenta", fn))
   M_(fr,("Fichier d'état %s invalide sera ignoré", fn))
   )
}

//const char *Msg::change_cfg(const char *cfg)
//{
//   M(   ("Using configuration [%s]", cfg),
//   M_(de,("Neue Konfiguration: [%s]",cfg))
//   M_(es,("Nueva configuración: [%s]", cfg))
//   M_(fr,("Changement de configuration à [%s]", cfg))
//   M_(it,("Configurazione cambiata a [%s]", cfg))
//   )
//}


const char *Msg::rule_changed(const char *tgt)
{
   M(    ("Rule for %s has changed", tgt),
   M_(de,("Die Regel für %s wurde verändert",tgt))
   M_(es,("La regla para %s fue alterada", tgt))
   M_(fr,("La règle pour %s a été modifiée", tgt))
   M_(it,("La regola per %s è stata modificata", tgt))
   )
}


const char *Msg::rules()
{
   M(   ("Rules"),
   M_(de,("Regeln"))
   M_(es,("Reglas"))
   M_(fr,("Règles"))
   M_(it,("Regole"))
   )
}

const char *Msg::variables()
{
   M(   ("Variables"),
   M_(de,("Variablen"))
   M_(es,("Variables"))
   M_(fr,("Variables"))
   M_(it,("Variabili"))
   )
}

const char *Msg::targets()
{
   M(   ("Targets"),
   M_(de,("Ziele"))
   M_(es,("Objetivos"))
   M_(fr,("Cibles"))
   M_(it,("Obiettivi"))
   )
}

const char *Msg::sources()
{
   M(   ("Sources"),
   M_(de,("Quellen"))
   M_(es,("Fuentes"))
   M_(fr,("Sources"))
   M_(it,("Fonti"))
   )
}

const char *Msg::options()
{
   M(   ("Options"),
   M_(de,("Optionen"))
   M_(es,("Opciónes"))
   M_(fr,("Options"))
   M_(it,("Opzioni"))
   )
}


const char *Msg::cannot_read(const char *fn, int err)
{
   const char *msg = strerror(err);

   M(   ("Cannot read %s: %s",fn,msg),
   M_(fr,("Ne peut lire %s: %s",fn,msg))
   M_(de,("Kann %s nicht lesen: %s",fn,msg))
   M_(es,("No se puede leer %s: %s",fn,msg))
   M_(it,("Non posso leggere %s: %s",fn,msg))
   )
}

	 
const char *Msg::no_queue(Target const *t)
{
   const char *tn = t ? t->name_ : "??";
   const char *cfg = t ? t->build_cfg_ : "??";

   M(   ("Cannot build %s: no server for [%s] available",tn,cfg),
   M_(de,("%s unerreichbar, kein Server für [%s] definiert",tn,cfg))
   M_(fr,("%s ne peut pas être refabriqué, configuration [%s] n'est pas supportée",tn,cfg))
   M_(es,("%s no se puede reconstruir, configuración [%s] no es disponibile",tn,cfg))
   M_(it,("Non posso ricostruire %s, nessun server per [%s] è disponibile",tn,cfg))
   )
}


const char *Msg::login_failed(const char *host)
{
   M(   ("Login on %s failed",host),
   M_(de,("Login auf %s gescheitert",host))
   M_(es,("Login para %s no funciona",host))
   M_(fr,("Login en %s ne fonctionne pas",host))
   M_(it,("%s: login non riuscito",host))
   )
}

const char *Msg::server_unavailable(const char *host, int err)
{
   const char *s = err > 0 ? strerror(err) : 0;
   M(    ("Server %s unavailable (%s)",host,s ? s : "connection closed"),
   M_(de,("Server %s nicht verfügbar (%s)",host,s ? s : "Verbindung beendet"))
   M_(es,("Server %s no es disponibile (%s)",host,s ? s : "conexión acabada"))
   M_(fr,("Serveur %s non disponible (%s)",host,s ? s : "connexion terminée"))
   M_(it,("Server %s non è disponibile (%s)",host,s ? s : "connessione chiusa"))
   )
}

const char *Msg::userinit_failed(const char *name, const char *f, int arg, int err)
{
   const char *s = strerror(err);
   M(   ("Error switching to user %s, %s(%d): %s",name,f,arg,s),
   M_(de,("Wechsel zu Benutzer %s gescheitert: %s(%d): %s",name,f,arg,s))
   M_(es,("No se puede convertirse en %s: %s(%d): %s",name,f,arg,s))
   M_(fr,("Ne peut devenir %s: %s(%d): %s",name,f,arg,s))
   M_(it,("Non posso diventare %s: %s(%d): %s",name,f,arg,s))
   )
}

const char *Msg::waiting_for_jobs(unsigned n)
{
   M(    ("Still waiting for %u jobs to finish",n),
   M_(de,("Es %s noch %u Job%s, warte...",n == 1 ? "läuft" : "laufen",n,n == 1 ? "" : "s"))
   M_(fr,("En attendant la fin de %u commande(s)...",n))
   )
}


const char *Msg::wrong_token(const char *name)
{
   M(   ("Invalid password for %s",name),
   M_(de,("Falsches Kennwort für %s",name))
   M_(es,("Contraseña incorrecta para %s",name))
   M_(it,("Password sbagliata per %s",name))
   M_(fr,("Mauvais mot de passe pour %s",name))
   )
}

const char *Msg::bad_file_attributes(const char *fn)
{
   M(   ("%s: bad owner, size, or permissions",fn),
   M_(de,("%s hat falsche Größe, Besitzer oder Zugriffsrechte",fn))
   //M_(es,("%s no tiene el tamaño correcto ni el utilizador correcto o no tiene derechos",fn))
   //M_(fr,("%s n'a pas la dimension correcte ni l'utilisateur correct, ni les droits",fn))
   )
}


const char *Msg::cwd_failed(const char *dir, int err)
{
   const char *s = strerror(err);
   M(   ("Cannot change working directory to %s, %s",dir,s),
   M_(de,("Verzeichniswechsel nach %s gescheitert: %s",dir,s))
   M_(es,("No se puede cambiar al directorio %s, %s",dir,s))
   M_(fr,("Ne peut changer le répertoire courant à %s, %s",dir,s))
   M_(it,("Impossibile cambiare la directory a %s, %s",dir,s))
   )
}


const char *Msg::login_result(const char *name, bool ok)
{
   M(   ("User %s %s",name,ok ? "accepted" : "rejected"),
   M_(de,("Benutzer %s %s",name,ok ? "akzeptiert" : "abgelehnt"))
   M_(es,("Utilizador %s %s",name,ok ? "acceptado" : "rechazado"))
   M_(fr,("Utilisateur %s %s",name,ok ? "accepté" : "refusé"))
   M_(it,("Utente %s %s",name,ok ? "accettato" : "rifiutato"))
   )
}


const char *Msg::addr_unusable(const char *addr,int err)
{
   const char *s = strerror(err);
   M(   ("Address %s unusable, %s",addr,s),
   M_(de,("Adresse %s nicht benutzbar: %s",addr,s))
   M_(es,("Dirección %s no está disponible: %s",addr,s))
   M_(fr,("L'adresse %s n'est pas disponible: %s",addr,s))
   M_(it,("L'indirizzo %s non è utilizzabile: %s",addr,s))
   )
}



const char *Msg::server_running(const char *ver, const char *addr)
{
   M(   ("Yabu server %s listening on %s",ver,addr),
   M_(de,("Yabu-Server %s gestartet auf Adresse %s",ver,addr))
   M_(fr,("Démarrage du serveur Yabu %s écoutant sur %s",ver,addr))
   M_(it,("Server Yabu %s iniziato, ascoltando su %s",ver,addr))
   M_(es,("El servidor Yabu %s está escuchando en %s",ver,addr))
   )
}


const char *Msg::my_hostname_not_found(const char *cf,const char *hn)
{
   M(   ("My host name (%s) not found in %s",hn,cf),
   M_(de,("Kein passender Eintrag (host %s) in %s",hn,cf))
   M_(es,("Nombre de la máquina (%s) no se encuentra en %s",hn,cf))
   M_(fr,("Nom d'hôte (%s) non repéré dans %s",hn,cf))
   M_(it,("Nome dell'host (%s) non trovato in %s",hn,cf))
   )
}


const char *Msg::no_dir(const char *dir)
{
   M(   ("%s not found or not a directory",dir),
   M_(de,("%s existiert nicht oder ist kein Verzeichnis",dir))
   M_(es,("%s no es un directorio",dir))
   M_(fr,("%s n'existe pas ou n'est pas un répertoire",dir))
   M_(it,("%s non esiste o non è un directory",dir))
   )
}


const char *Msg::unexpected_args()
{
   M(   ("Arguments not allowed (use -? for help)"),
   M_(de,("Unzulässige Argumente (Hilfe mit -?)"))
   M_(es,("Los argumentos no estan permitidos (utilizar -?)"))
   M_(fr,("Arguments ne sont pas permis (utiliser -?)"))
   M_(it,("Non sono permessi argomenti (aiuta: -?)"))
   )
}


const char *Msg::archive_corrupted(const char *name)
{
   M(   ("Archive %s is corrupt", name),
   M_(de,("Die Bibliothek %s ist beschädigt", name))
   M_(es,("El archivo %s es defectuoso", name))
   M_(fr,("L'archive %s est corrompue", name))
   M_(it,("L'archivio %s è corrotto", name))
   )
}


const char *Msg::target_cancelled(const char *name)
{
   M(   ("Target %s skipped", name),
   M_(de,("Ziel %s wird ausgelassen", name))
   M_(es,("Se omite el objetivo %s", name))
   M_(fr,("La cible %s est escamotée", name))
   M_(it,("L'obiettivo %s è omesso", name))
   )
}


const char *Msg::rule_unusable(const SrcLine *s, const char *cfg)
{
   const char *f = s ? s->file : "";
   const unsigned l = s ? s->line : 0;

   M(   ("Not using rule @%s(%d): configuration [%s] is unreachable", f,l,cfg),
   M_(de,("Regel @%s(%d) nicht anwendbar: [%s] nicht möglich", f,l,cfg))
   M_(es,("La regla @%s(%d) no se puede utilizar, configuración [%s] no es posible",f,l,cfg))
   M_(fr,("Règle @%s(%d) n'est pas utilisable car la configuration [%s] n'est pas accessible", f,l,cfg))
   M_(it,("Regola @%s(%d) ignorato, configurazione [%s] non è possibile",f,l,cfg))
   )
}
   
const char *Msg::G42_missing_opt_arg(char opt)
{
   M(   ("Missing argument after '-%c'", opt),
   M_(de,("Die Option '-%c' benötigt ein Argument", opt))
   M_(es,("Se necesita un argumento después la opción '-%c'", opt))
   M_(fr,("Il manque l'argument après l'option «-%c»", opt))	
   M_(it,("L'opzione '-%c' richiede un argomento", opt))
    )
}

const char *Msg::G43_bad_ts_algo(char const *arg)
{
   M(   ("Bad setting '-y %s' (use 'mt', 'mtid', or 'cksum')", arg),
   M_(fr,("Option invalide «-y %s» (utiliser «mt», «mtid» ou «cksum»)", arg))
   M_(de,("Ungültiges Argument in '-y %s' (erlaubt sind: mt, mtid, cksum)", arg))
   M_(es,("Valor inválido '-y %s' (utilizar 'mt', 'mtid' o 'cksum')", arg))
   M_(it,("Argomento non valido in '-y %s' (usa 'mt', 'mtid' o 'cksum')", arg))
   )
}
  
////////////////////////////////////////////////////////////////////////////////////////////////////
// Sxx - Syntaxfehler
////////////////////////////////////////////////////////////////////////////////////////////////////

const char *Msg::S01_syntax_error(const char *what)
{
   Str ext;
   if (what) ext.append(": «").append(what).append("»");
   M(    ("Syntax error%s",(const char*)ext),
   M_(de,("Syntaxfehler%s",(const char*)ext))
   M_(es,("Error de sintaxis%s",(const char*)ext))
   M_(fr,("Erreur de syntaxe%s",(const char*)ext))
   M_(it,("Errore di sintassi%s",(const char*)ext))
   )
}


const char *Msg::S02_nomatch(const char *bcmd, const char *ecmd)
{
   M(   ("No matching '%s' for '%s'", ecmd, bcmd),
   M_(de,("Fehlendes '%s' zu '%s'", ecmd, bcmd))
   M_(fr,("«%s» non terminé, «%s» manquant", bcmd, ecmd))
   M_(es,("No se terminó un '%s', falta un '%s'", bcmd, ecmd))
   M_(it,("Trovato '%s' senza il '%s' corrispondente", bcmd, ecmd))
    )
}


const char *Msg::S03_missing_digit(char ch)
{
   M(   ("Missing digit after '%c' (did you mean '%c1'?)", ch,ch),
   M_(de,("Fehlende Ziffer nach '%c' (meinten Sie '%c1'?)", ch,ch))
   M_(es,("Falta el dígito después del '%c' (¿no debería ser '%c1'?)", ch,ch))
   M_(fr,("Chiffre manquant après '%c' (vous vouliez peut-être utiliser '%c1'?)", ch,ch))
   M_(it,("Manca la cifra dopo '%c' (forse intendevi '%c1'?)", ch,ch))
   )
}


const char *Msg::S04_missing_script()
{
   M(   ("':?' rule without script not allowed"),
   M_(de,("Eine ':?'-Regel ohne Skript ist nicht erlaubt"))
   M_(fr,("Règle de type ':?' doit avoir un script"))
   )
}


const char *Msg::S07_bad_bool_value(const char *s)
{
   M(   ("Invalid boolean value '%s'", s),
   M_(de,("Unzulässiger boolescher Wert '%s'", s))
   M_(es,("Valor booleano inválido '%s'", s))
   M_(fr,("Valeur booléen invalide : «%s»", s))
   M_(it,("Valore logico '%s' non valido", s))
    );
}


const char *Msg::S08_bad_int_value(const char *s)
{
   M(    ("Invalid integer '%s'", s),
   M_(de,("Ungültige Zahl: '%s'", s))
   M_(es,("Número inválido '%s'", s))
   M_(fr,("Nombre invalide : «%s»", s))
   M_(it,("Numero intero '%s' non vaido", s))
    );
}

const char *Msg::S09_int_out_of_range(int v, int min, int max)
{
   M(   ("Integer (%d) out of range [%d..%d]", v,min,max),
   M_(de,("Wert (%d) nicht im erlaubten Bereich [%d..%d]", v,min,max))
   M_(es,("Valor (%d) no está entre %d y %d", v,min,max))
   M_(fr,("Valeur (%d) hors limite [%d..%d]", v,min,max))
    );
}

// vim:sw=3:cin:fileencoding=utf-8
