////////////////////////////////////////////////////////////////////////////////////////////////////
// I, the creator of this work, hereby release it into the public domain. This applies worldwide.
// In case this is not legally possible: I grant anyone the right to use this work for any purpose,
// without any conditions, unless such conditions are required by law.
////////////////////////////////////////////////////////////////////////////////////////////////////

// yastr.cc - Hilfsfunktionen zur Stringverarbeitung

#include "yabu.h"

#include <stdlib.h>
#include <string.h>



#define LESS    ((const char *)1)
#define GREATER ((const char *)2)


const char DDIG[10][2] = {"0","1","2","3","4","5","6","7","8","9"};

////////////////////////////////////////////////////////////////////////////////////////////////////
// Prüft, ob «a» als Endstück in «b» enthalten ist. Falls ja, ist der Returnwert der Zeiger auf
// den mit «b» übereinstimmenden Teil von «a». Andernfalls gibt der Returnwert an, ob «a» "größer"
// oder "kleiner" als «b» ist, wenn man die Strings rückwärts vergleicht.
// Weder «a» noch «b» müssen mit NUL abgeschlossen sein.
////////////////////////////////////////////////////////////////////////////////////////////////////

static const char *compare(const char *a, unsigned alen, const char *b, unsigned blen)
{
   const char *ap = a + alen - 1;
   const char *bp = b + blen - 1;
   for (; ap >= a && bp >= b; --ap, --bp) {
      if (*ap > *bp) return LESS;
      if (*ap < *bp) return GREATER;
   }
   if (ap < a)                  // b enthält a als Teilstring
      return bp + 1;
   return GREATER;              // a enthält b als echten Teilstring
}


////////////////////////////////////////////////////////////////////////////////////////////////////
// Liefert eine (mit NUL abgeschlossene) Kopie des Strings, die garantiert bis zum Programmende
// unverändert bleibt.
////////////////////////////////////////////////////////////////////////////////////////////////////

const char *str_freeze(const char *s, unsigned len)
{
   // Tabelle aller Strings. Ein Eintrag besteht aus Länge + String.
   // Sortierung gemäß compare().
   static unsigned **tab = 0;
   static size_t count = 0;

   // String suchen.
   unsigned lo = 0;
   unsigned hi = count;
   while (lo < hi) {
      unsigned mid = (hi & lo) + (hi ^ lo) / 2;
      const char *c = compare(s, len, (char const *) (tab[mid] + 1), *tab[mid]);
      if (c == LESS) hi = mid;
      else if (c == GREATER) lo = mid + 1;
      else return c;              // Bereits bekannt
   }

   // Nicht gefunden: neuen String an Position <lo> einfügen.
   array_insert(tab,count,lo);
   if ((tab[lo] = (unsigned *) malloc(sizeof(unsigned) + len + 1)) == 0)
      YUFTL(G20,syscall_failed("malloc",0));
   *tab[lo] = len;
   char *buf = (char *) (tab[lo] + 1);
   memcpy(buf, s, len);
   buf[len] = 0;
   return buf;

}

////////////////////////////////////////////////////////////////////////////////////////////////////
// Variante von «str_freeze()» für C-Strings.
////////////////////////////////////////////////////////////////////////////////////////////////////

const char *str_freeze(char const *s)
{
   return str_freeze(s, strlen(s));
}


////////////////////////////////////////////////////////////////////////////////////////////////////
// Leerzeichen und Tabs überspringen. Returnwert ist das erste nicht-Leerzeichen.
////////////////////////////////////////////////////////////////////////////////////////////////////

char skip_blank(const char **rp)
{
    skip_str(rp," ");
    return **rp;
}

////////////////////////////////////////////////////////////////////////////////////////////////////
// Gibt das nächste Wort ab «*rp» zurück und inkrementiert den Lesezeiger «*rp». Führende
// Leerzeichen werden abgeschnitten, und das Wort endet mit einem der Trennzeichen in «sep».
////////////////////////////////////////////////////////////////////////////////////////////////////

const char *next_str(const char **rp, const char *sep)
{
    const char *r = *rp;
    skip_blank(&r);
    const char *beg = r;
    while (*r != 0 && strchr(sep,*r) == 0)
	++r;
    *rp = r;
    while (r > beg && IS_SPACE(r[-1])) --r;
    return r > beg ? str_freeze(beg,r-beg) : 0;
}

unsigned char yabu_cclass[256] = {
   //     NUL                                      BS   HT   LF   VT   FF   CR
   /*00*/ 0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00, 0x00,0x20,0x20,0x00,0x00,0x20,0x00,0x00,
   /*10*/ 0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00, 0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,
   //     SPC
   /*20*/ 0x20,0x00,0x00,0x00,0x00,0x00,0x00,0x00, 0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,
       // 0    1    2    3    4    5    6    7    8    9
   /*30*/ 0xD0,0xD1,0xD2,0xD3,0xD4,0xD5,0xD6,0xD7, 0xD8,0xD9,0x00,0x00,0x00,0x00,0x00,0x00,
       // @    A    B    C    D    E    F
   /*40*/ 0x80,0x9A,0x9B,0x9C,0x9D,0x9E,0x9F,0x80, 0x80,0x80,0x80,0x80,0x80,0x80,0x80,0x80,
       // P                                                       [    \    ]    ^    _
   /*50*/ 0x80,0x80,0x80,0x80,0x80,0x80,0x80,0x80, 0x80,0x80,0x80,0x00,0x00,0x00,0x00,0x80,
   //     `    a                        f
   /*60*/ 0x00,0x9A,0x9B,0x9C,0x9D,0x9E,0x9F,0x80, 0x80,0x80,0x80,0x80,0x80,0x80,0x80,0x80,
   //                                                             {    |    }    ~    DEL
   /*70*/ 0x80,0x80,0x80,0x80,0x80,0x80,0x80,0x80, 0x80,0x80,0x80,0x00,0x00,0x00,0x00,0x00,
   /*80*/ 0x80,0x80,0x80,0x80,0x80,0x80,0x80,0x80, 0x80,0x80,0x80,0x80,0x80,0x80,0x80,0x80,
   /*90*/ 0x80,0x80,0x80,0x80,0x80,0x80,0x80,0x80, 0x80,0x80,0x80,0x80,0x80,0x80,0x80,0x80,
   /*a0*/ 0x80,0x80,0x80,0x80,0x80,0x80,0x80,0x80, 0x80,0x80,0x80,0x80,0x80,0x80,0x80,0x80,
   /*b0*/ 0x80,0x80,0x80,0x80,0x80,0x80,0x80,0x80, 0x80,0x80,0x80,0x80,0x80,0x80,0x80,0x80,
   /*c0*/ 0x80,0x80,0x80,0x80,0x80,0x80,0x80,0x80, 0x80,0x80,0x80,0x80,0x80,0x80,0x80,0x80,
   /*d0*/ 0x80,0x80,0x80,0x80,0x80,0x80,0x80,0x80, 0x80,0x80,0x80,0x80,0x80,0x80,0x80,0x80,
   /*e0*/ 0x80,0x80,0x80,0x80,0x80,0x80,0x80,0x80, 0x80,0x80,0x80,0x80,0x80,0x80,0x80,0x80,
   /*f0*/ 0x80,0x80,0x80,0x80,0x80,0x80,0x80,0x80, 0x80,0x80,0x80,0x80,0x80,0x80,0x80,0x80
};


////////////////////////////////////////////////////////////////////////////////////////////////////
// Überspringt den nächsten Variablennamen. Returnwert ist das erste Zeichen des Namens oder 0,
// «*rp» zeigt nach Rückkehr auf das letzte Zeichen + 1 des Namens
////////////////////////////////////////////////////////////////////////////////////////////////////

static const char *skip_name(const char **rp)
{
   char const *c = *rp;
   skip_blank(&c);
   char const *begin = c;
   while (IS_IDENT(*c)) ++c;
   if (c == begin) return 0;
   *rp = c;
   return begin;
}


////////////////////////////////////////////////////////////////////////////////////////////////////
// Text vergleichen. Ein SPC in «str» steht für eine beliebige (auch leere) Kombination von 
// Leerzeichen im Sinne von «IS_SPACE()». Alle anderen Zeichen müssen genauso in im Text vorkommen.
// Paßt der Text nicht, bleibt «*rp» unverändert.
////////////////////////////////////////////////////////////////////////////////////////////////////

bool skip_str(const char **rp, const char *str)
{
    const char *r = *rp;
    for (; *str; ++str) {
	if (*str == ' ')
	    while (IS_SPACE(*r)) ++r;
	else if (*r++ != *str)
	    return false;
    }
    *rp = r;
    return true;
}



////////////////////////////////////////////////////////////////////////////////////////////////////
// Gibt das nächste Wort ab Position *rp zurück. Führende Leerzeichen werden abgeschnitten.
// rp: Lesezeiger, zeigt nach Rückkehr auf das nächste unverarbeitete Zeichen.
// return: Nächstes Wort oder 0, wenn das Ende des Strings erreicht ist.
////////////////////////////////////////////////////////////////////////////////////////////////////

const char *next_word(const char **rp)
{
    return next_str(rp," \t\n\r");
}


////////////////////////////////////////////////////////////////////////////////////////////////////
// Wie «next_word()», aber das "Wort" besteht nur aus Ziffern, Buchstaben und '_'.
////////////////////////////////////////////////////////////////////////////////////////////////////

const char *next_name(const char **rp)
{
   char const *begin = skip_name(rp);
   return begin ? str_freeze(begin,*rp - begin) : 0;
}


////////////////////////////////////////////////////////////////////////////////////////////////////
// Wie «next_word()», arbeitet aber destruktiv.
////////////////////////////////////////////////////////////////////////////////////////////////////

char *str_chop(char **rp)
{
    char *r = *rp;
    while (IS_SPACE(*r)) ++r;
    if (*r == 0) return 0;
    char *beg = r;
    while (*r != 0 && (*r != 0 && !IS_SPACE(*r)))
	++r;
    if (*r != 0)
       *r++ = 0;
    *rp = r;
    return beg;
}


////////////////////////////////////////////////////////////////////////////////////////////////////
// Interpretiert eine Dezimalzahl und liefert den Wert. Führende Leerzeichen werden überlesen.
////////////////////////////////////////////////////////////////////////////////////////////////////

bool next_int(int *val, const char **rp)
{
   const char *r = *rp;
   skip_blank(&r);
   if (!IS_DIGIT(*r) && (*r != '-' || !IS_DIGIT(r[1])))
      return false;
   *val = atoi(r);
   if (*r == '-') ++r;
   while (IS_DIGIT( *r)) ++r;
   *rp = r;
   return true;
}

bool next_int(int *val, char **rp)
{
   return next_int(val,(const char **)rp);
}


////////////////////////////////////////////////////////////////////////////////////////////////////
// Wie «next_int()», prüft aber, ob nach der Zahl weitere (Zeichen stehen)
////////////////////////////////////////////////////////////////////////////////////////////////////

bool str2int(int *val, const char *s)
{
    return next_int(val,&s) && *s == 0;
}


////////////////////////////////////////////////////////////////////////////////////////////////////
// Berechnet die Spaltenposition des ersten nicht-Leerzeichens. TABs werden mit einer Weite von
// 8 berücksichtigt. Der Returnwert 0 entsprucht der ersten Spalte.
////////////////////////////////////////////////////////////////////////////////////////////////////

unsigned calc_indent(const char *c)
{
   unsigned n = 0;
   for (; (*c == ' ' || *c == '\t'); ++c)
      n = (*c == ' ') ? n + 1 : (n / 8 + 1) * 8;
   return n;
}


////////////////////////////////////////////////////////////////////////////////////////////////////
// Entfernt eine Einrückung um «indent» Zeichen.
////////////////////////////////////////////////////////////////////////////////////////////////////

const char *remove_indent(const char **cp, unsigned indent)
{
   const char *c = *cp;
   unsigned n = 0;
   for (; n < indent && (*c == ' ' || *c == '\t'); ++c)
      n = (*c == ' ') ? n + 1 : (n / 8 + 1) * 8;
   *cp = c;
   static const char SPACE[9] = "        ";
   return n > indent ? SPACE + 8 - (n - indent) : "";
}


////////////////////////////////////////////////////////////////////////////////////////////////////
// Zerlegt eine Variablenreferenz der Form $(...) in die einzelnen Teile:
//    «name»: Variablenname
//    «short_name»: Teil des Namens bis zum ersten '('
//    «tr_mode»: Ersetzungsmodus ('!', '?' oder 0)
//    «ph»: Platzhalterzeichen (*,@,&,^,~,+,#)
//    «pat»: linke Seite der Ersetzungsregel
//    «repl»: rechte Seite der Ersetzungsregel.
// Fehlende optionale Teile sind nach erfolgreicher Ausführung gleich 0.  Beim Aufruf muß «*rp»
// auf das "$(" zeigen. Nach der Rückkehr zeigt «*rp» auf das Zeichen hinter dem abschließenden
// ')'.
////////////////////////////////////////////////////////////////////////////////////////////////////

bool VariableName::split(const char **rp)
{
   const char *c = *rp + 2;     // "$(" überspringen
   const char *bon = c;         // Anfang des Namens
   const char *colon = 0;
   const char *eq = 0;

   // Trennzeichen ':' und '=' suchen, dabei Klammeerebenen beachten.
   short_name = 0;
   for (unsigned level = 0; *c != 0 && (*c != ')' || level > 0); ++c) {
      if (*c == '(') {
	 if (short_name == 0)
	    short_name = str_freeze(bon,c - bon);
         ++level;
      } else if (*c == ')')
         --level;
      else if (level == 0) {
	 if (colon == 0 && *c == ':') colon = c;
	 else if (*c == '=' && eq == 0 && colon != 0) eq = c;
      }
   }
   if (*c != ')')
      return YUERR(S02,nomatch("(",")")), false;	// [T:vs13]

   // Platzhalter und Ersetzungsmodus (beide optional)
   const char *eon = colon ? colon : c;			// Ende des Namens
   tr_mode = 0;
   ph = 0;
   // Hier nicht '*' zulassen! [T:vs023,vs024]
   while (colon && eon > bon && strchr("!?@&^~+#",eon[-1])) {
      --eon;
      char * const what = (*eon == '!' || *eon == '?') ? &tr_mode : &ph;
      if (*what != 0)
	 return YUERR(S01,syntax_error(str_freeze(bon,colon-bon))), false;
      else
	 *what = *eon;
   }
   if (tr_mode == 0) tr_mode = ':';
   if (ph == 0) ph = '*';

   // Name
   name = str_freeze(bon,eon - bon);
   if (short_name == 0) short_name = name;
   if (*name == 0 || !strcmp(name,".")) {
      YUERR(S01,syntax_error(name));					// [T:vs12]
      return false;
   }

   // 2. Teil (rechts vom '=') auswerten
   if (colon) {
      ++colon;
      if (eq == 0) {                 			// z.B. $(.glob:*.cc)
          pat = str_freeze(colon,c-colon);
	  repl = 0;
      } else {
	 pat = str_freeze(colon, eq - colon);		// Linke Seite
	 repl = str_freeze(eq + 1,c - eq - 1);		// Rechte Seite
      }
   }
   else 						// Keine Transformationsregel
      pat = repl = 0;

   *rp = c + 1;                 // ")" überspringen
   return true;
}





// Konstruktor.
StringList::StringList()
   :size_(0), data_(0)
{
}


// Liste löschen.
void StringList::clear()
{
   size_ = 0;
   if (data_) {
      free(data_);
      data_ = 0;
   }
}

// Destruktor.
StringList::~StringList()
{
   clear();
}


////////////////////////////////////////////////////////////////////////////////////////////////////
// Inhalt mit einer anderen «StringList» austauschen.
////////////////////////////////////////////////////////////////////////////////////////////////////

void StringList::swap(StringList & l)
{
   unsigned size = size_;
   size_ = l.size_;
   l.size_ = size;
   char const **data = data_;
   data_ = l.data_;
   l.data_ = data;
}


////////////////////////////////////////////////////////////////////////////////////////////////////
// «s» am Ende anhängen
////////////////////////////////////////////////////////////////////////////////////////////////////

void StringList::append(const char *s)
{
   array_realloc(data_, ++size_);
   data_[size_ - 1] = s;
}


////////////////////////////////////////////////////////////////////////////////////////////////////
// «s» so einfügen, daß die Liste sortiert bleibt. Erlaubt Duplikate.
////////////////////////////////////////////////////////////////////////////////////////////////////

void StringList::insert(const char *s)
{
   unsigned pos;
   for (pos = 0; pos < size_ && strcmp(s, data_[pos]) >= 0; ++pos);
   array_insert(data_, size_, pos);
   data_[pos] = s;
}


////////////////////////////////////////////////////////////////////////////////////////////////////
// Eine Element löschen
////////////////////////////////////////////////////////////////////////////////////////////////////

void StringList::remove(unsigned pos)
{
   if (pos < size_) {
      memmove(data_ + pos,data_ + pos + 1,(size_ - pos - 1) * sizeof(*data_));
      --size_;
   }
}


////////////////////////////////////////////////////////////////////////////////////////////////////
// Liefert das «n»-te Element
////////////////////////////////////////////////////////////////////////////////////////////////////

const char *StringList::operator[] (unsigned n) const
{
   YABU_ASSERT(n < size_);
   return data_[n];
}


////////////////////////////////////////////////////////////////////////////////////////////////////
// Liefert in «pos» den Index des ersten mit «s» übereinstimmenden Elements. Returnwert ist
// «false», falls nicht gefunden. «pos» darf 0 sein, wenn der Aufrufer den Index nicht wissen will.
////////////////////////////////////////////////////////////////////////////////////////////////////

bool StringList::find(unsigned *pos, const char *val) const
{
   for (unsigned i = 0; i < size_; ++i) {
      if (!strcmp(data_[i], val)) {
         if (pos)
            *pos = i;
         return true;
      }
   }
   return false;
}


////////////////////////////////////////////////////////////////////////////////////////////////////
// Dies ist die Kernfunktion für alle Platzhalter-Operationen (Ersetzung von '%', '*' usw.). Sie
// vergleicht den String «t» mit dem Muster «p», welches bis zu 9 Platzhalter («tag») enthalten
// kann.  Wenn der Text zum Muster paßt, gibt die Funktion «true» zurück,und füllt die übrigen
// Argumente:
// - «anum» muß beim Aufruf gleich 0 sein (!) und enthält nach Rückkehr die Anzahl der Platzhalter
// - «a[i]» zeigt auf den Beginn des Teilstrings in «t», den zum i-ten Platzhalter gehört
// - «alen[i]» ist die Länge dieses Teilstrings
//
// Wird ein Platzhalterzeichen von einer Ziffer gefolgt (zum Beispiel '%1'), dann steht es nicht
// für einen weiteren Platzhalter, sondern für den Teilstring, der dem i-ten Platzhalter zuvor
// zugeordnet wurde. Das Muster wird dabei von links nach rechts verarbeitet, und Platzhalter
// können erst referenziert werden, wenn sie einem Teilstring zugeordnet sind. Beispiel:
// "%/%/%2" ist erlaubt, "%/%2/%" aber nicht.
////////////////////////////////////////////////////////////////////////////////////////////////////

static bool str_match(const char *a[9], unsigned alen[9], unsigned &anum,
                      char tag, const char *p, const char *t)
{
   while (*p != 0) {
      if (*p == tag) {
	 if (p[1] == tag && p[2] == tag) {		// '%%%' = '%' ohne Platzhalterfunktion
	    if (*t != tag) return false;
	    ++t;
	    p += 3;
	 } else if (!IS_DIGIT(p[1])) {			// '%'
            if (anum >= 9) {
               YUERR(L11,too_many_placeholders(tag));
	       return false;
	    }
            unsigned n = anum++;
            a[n] = t;
            const char *end = t;
            if (p[1] == tag) {			// %%: so lang wie möglich
               ++p;
               while (*end != 0)
                  ++end;
            } else {				// %: maximal bis zum nächsten '.' oder '/'
               while (*end != 0 && *end != '.' && *end != '/')
                  ++end;
            }
            alen[n] = end - t;
            while (!str_match(a, alen, anum, tag, p + 1, t + alen[n])) {
               if (alen[n] == 0) {
                  --anum;
                  return false;
               }
               --alen[n];
            }
            return true;
         } else {					// '%N'
            unsigned n = (unsigned) (p[1] - '1') % 9;
            if (n >= anum)
               YUFTL(L01,undefined(tag,DDIG[n+1]));	// [T:vs14]
	    else {
	       const char *p2 = a[n];
	       for (unsigned i = alen[n]; i > 0; --i) {
		  if (*p2++ != *t++)
		     return false;
	       }
	    }
	    p += 2;
	 }
      }
      else {
         if (*t != *p) return false;
         ++p;
         ++t;
      }
   }
   return *t == 0;
}


////////////////////////////////////////////////////////////////////////////////////////////////////
// Wie «str_match()», liefert aber die Teilstrings in einer «StringList» zurück.
// «tag»: Platzhalterzeichen.
// «p»: Muster.
// «t»: Text.
// «return»: True: Text paßt zum Muster. False: Text paßt nicht zum Muster.
////////////////////////////////////////////////////////////////////////////////////////////////////

bool StringList::match(char tag, const char *p, const char *t)
{
   const char *a[9];
   unsigned alen[9];
   unsigned anum = 0;
   if (!str_match(a, alen, anum, tag, p, t))
      return false;
   clear();
   for (unsigned n = 0; n < anum; ++n)
      append(str_freeze(a[n], alen[n]));
   return true;
}


////////////////////////////////////////////////////////////////////////////////////////////////////
// Ersetzt Platzhalter in «s» durch die Werte in «args» und hängt das Ergebnis an «buf» an.
// «tag» ist das PLatzhalterzeichen (z.B. '%'). Returnwert: Anzahl der Substitutionen.
////////////////////////////////////////////////////////////////////////////////////////////////////

void expand_substrings(Str &buf, const char *s, const StringList *args, char args_tag)
{
   while (*s != 0) {
      if (*s == args_tag) {
	 if (*++s == args_tag)
	    buf.append(s++,1);					// '%%' -> '%'
	 else {
	    unsigned n = 0;
	    if (IS_DIGIT(*s))
	       n = (unsigned) (*s++ - '0') % 10;
	    else if (args->size() > 1) {			// '%' nur in einfachen Fällen
	       YUERR(S03,missing_digit(args_tag));		// [T:ss01,ss02]
	       return;
	    }
	    if (n > 0) --n;					// '%0' ist aquivalent zu '%1'
	    if (n >= args->size())
	       YUFTL(L01,undefined(args_tag,DDIG[n+1]));	// [T:059]
	    else
	       buf.append((*args)[n]);
	 }
      } else {
	 const char * const b = s++;
	 while (*s != 0 && *s != args_tag)
	    ++s;
	 buf.append(b, s - b);
      }
   }
}


////////////////////////////////////////////////////////////////////////////////////////////////////
// Prüft ob «buf» eine syntaktisch korrekte Variablenzuweisung der Form
//   NAME[KONFIG]=WERT
//   NAME[KONFIG]+=WERT
//   NAME[KONFIG]?=WERT
// enthält. Falls ja, zerlegt die Funktion die Anweisung in ihre vier Komponenten und setzt die
// entsprechenden Membervariablen: «name», «cfg», «mode» und «value».
// «cfg» ist nach erfolgreicher Ausführung gleich 0 oder ein nicht-leerer String.
// «name» und «value» sind nach erfolgreicher Ausführung niemals gleich 0.
////////////////////////////////////////////////////////////////////////////////////////////////////

bool Assignment::split(const char *buf)
{
   const char *c = buf;

   // Variablenname
   const char *name0 = skip_name(&c);
   if (name0 == 0) return false;
   const char *name1 = c;

   // Optionen (kein blank zwischen Name und '[' erlaubt)
   const char *opts0 = c;
   const char *opts1 = c;
   if (*c == '[') {
      opts0 = ++c;
      while (*c != ']' && *c != 0)
         ++c;
      if (*c != ']')
         return false;
      opts1 = c++;
   }

   // Operator (=, += oder ?=)
   skip_blank(&c);
   if (*c == '+' && c[1] == '=') {
      mode = APPEND;
      c += 2;
   }
   else if (*c == '?' && c[1] == '=') {
      mode = MERGE;
      c += 2;
   }
   else if (*c == '=') {
      mode = SET;
      ++c;
   }
   else
      return false;

   name = str_freeze(name0, name1 - name0);
   cfg = opts1 > opts0 ? str_freeze(opts0, opts1 - opts0) : 0;

   // Rechte Seite
   skip_blank(&c);
   const char *vbeg = c;
   const char *vend = c + strlen(c);
   if (*c == '"') {
      ++vbeg;
      if (vend - 1 < vbeg || vend[-1] != '"')
         YUERR(S01,syntax_error(0));
      else
	 --vend;
   }

   value = str_freeze(vbeg,vend-vbeg);

   return true;
}

////////////////////////////////////////////////////////////////////////////////////////////////////
// Berechnet eine Prüfsumme eines Datenblocks.
// data: Daten.
// len: Datenlänge.
// return: Prüfsumme.
////////////////////////////////////////////////////////////////////////////////////////////////////

static unsigned long const TAB[] = {
    0x00000000, 0x04c11db7, 0x09823b6e, 0x0d4326d9, 0x130476dc, 0x17c56b6b, 0x1a864db2, 0x1e475005,
    0x2608edb8, 0x22c9f00f, 0x2f8ad6d6, 0x2b4bcb61, 0x350c9b64, 0x31cd86d3, 0x3c8ea00a, 0x384fbdbd,
    0x4c11db70, 0x48d0c6c7, 0x4593e01e, 0x4152fda9, 0x5f15adac, 0x5bd4b01b, 0x569796c2, 0x52568b75,
    0x6a1936c8, 0x6ed82b7f, 0x639b0da6, 0x675a1011, 0x791d4014, 0x7ddc5da3, 0x709f7b7a, 0x745e66cd,
    0x9823b6e0, 0x9ce2ab57, 0x91a18d8e, 0x95609039, 0x8b27c03c, 0x8fe6dd8b, 0x82a5fb52, 0x8664e6e5,
    0xbe2b5b58, 0xbaea46ef, 0xb7a96036, 0xb3687d81, 0xad2f2d84, 0xa9ee3033, 0xa4ad16ea, 0xa06c0b5d,
    0xd4326d90, 0xd0f37027, 0xddb056fe, 0xd9714b49, 0xc7361b4c, 0xc3f706fb, 0xceb42022, 0xca753d95,
    0xf23a8028, 0xf6fb9d9f, 0xfbb8bb46, 0xff79a6f1, 0xe13ef6f4, 0xe5ffeb43, 0xe8bccd9a, 0xec7dd02d,
    0x34867077, 0x30476dc0, 0x3d044b19, 0x39c556ae, 0x278206ab, 0x23431b1c, 0x2e003dc5, 0x2ac12072,
    0x128e9dcf, 0x164f8078, 0x1b0ca6a1, 0x1fcdbb16, 0x018aeb13, 0x054bf6a4, 0x0808d07d, 0x0cc9cdca,
    0x7897ab07, 0x7c56b6b0, 0x71159069, 0x75d48dde, 0x6b93dddb, 0x6f52c06c, 0x6211e6b5, 0x66d0fb02,
    0x5e9f46bf, 0x5a5e5b08, 0x571d7dd1, 0x53dc6066, 0x4d9b3063, 0x495a2dd4, 0x44190b0d, 0x40d816ba,
    0xaca5c697, 0xa864db20, 0xa527fdf9, 0xa1e6e04e, 0xbfa1b04b, 0xbb60adfc, 0xb6238b25, 0xb2e29692,
    0x8aad2b2f, 0x8e6c3698, 0x832f1041, 0x87ee0df6, 0x99a95df3, 0x9d684044, 0x902b669d, 0x94ea7b2a,
    0xe0b41de7, 0xe4750050, 0xe9362689, 0xedf73b3e, 0xf3b06b3b, 0xf771768c, 0xfa325055, 0xfef34de2,
    0xc6bcf05f, 0xc27dede8, 0xcf3ecb31, 0xcbffd686, 0xd5b88683, 0xd1799b34, 0xdc3abded, 0xd8fba05a,
    0x690ce0ee, 0x6dcdfd59, 0x608edb80, 0x644fc637, 0x7a089632, 0x7ec98b85, 0x738aad5c, 0x774bb0eb,
    0x4f040d56, 0x4bc510e1, 0x46863638, 0x42472b8f, 0x5c007b8a, 0x58c1663d, 0x558240e4, 0x51435d53,
    0x251d3b9e, 0x21dc2629, 0x2c9f00f0, 0x285e1d47, 0x36194d42, 0x32d850f5, 0x3f9b762c, 0x3b5a6b9b,
    0x0315d626, 0x07d4cb91, 0x0a97ed48, 0x0e56f0ff, 0x1011a0fa, 0x14d0bd4d, 0x19939b94, 0x1d528623,
    0xf12f560e, 0xf5ee4bb9, 0xf8ad6d60, 0xfc6c70d7, 0xe22b20d2, 0xe6ea3d65, 0xeba91bbc, 0xef68060b,
    0xd727bbb6, 0xd3e6a601, 0xdea580d8, 0xda649d6f, 0xc423cd6a, 0xc0e2d0dd, 0xcda1f604, 0xc960ebb3,
    0xbd3e8d7e, 0xb9ff90c9, 0xb4bcb610, 0xb07daba7, 0xae3afba2, 0xaafbe615, 0xa7b8c0cc, 0xa379dd7b,
    0x9b3660c6, 0x9ff77d71, 0x92b45ba8, 0x9675461f, 0x8832161a, 0x8cf30bad, 0x81b02d74, 0x857130c3,
    0x5d8a9099, 0x594b8d2e, 0x5408abf7, 0x50c9b640, 0x4e8ee645, 0x4a4ffbf2, 0x470cdd2b, 0x43cdc09c,
    0x7b827d21, 0x7f436096, 0x7200464f, 0x76c15bf8, 0x68860bfd, 0x6c47164a, 0x61043093, 0x65c52d24,
    0x119b4be9, 0x155a565e, 0x18197087, 0x1cd86d30, 0x029f3d35, 0x065e2082, 0x0b1d065b, 0x0fdc1bec,
    0x3793a651, 0x3352bbe6, 0x3e119d3f, 0x3ad08088, 0x2497d08d, 0x2056cd3a, 0x2d15ebe3, 0x29d4f654,
    0xc5a92679, 0xc1683bce, 0xcc2b1d17, 0xc8ea00a0, 0xd6ad50a5, 0xd26c4d12, 0xdf2f6bcb, 0xdbee767c,
    0xe3a1cbc1, 0xe760d676, 0xea23f0af, 0xeee2ed18, 0xf0a5bd1d, 0xf464a0aa, 0xf9278673, 0xfde69bc4,
    0x89b8fd09, 0x8d79e0be, 0x803ac667, 0x84fbdbd0, 0x9abc8bd5, 0x9e7d9662, 0x933eb0bb, 0x97ffad0c,
    0xafb010b1, 0xab710d06, 0xa6322bdf, 0xa2f33668, 0xbcb4666d, 0xb8757bda, 0xb5365d03, 0xb1f740b4
  };

Crc::Crc(void const *data, size_t len)
   :x_(0), len_(0)
{
   if (data) feed(data,len);
}

Crc &Crc::feed(const void *data, size_t len)
{
  unsigned char const *const end = (unsigned char const *) data + len;
  for (unsigned char const *p = (unsigned char const *)data; p < end; ++p)
    x_ = (((x_ << 8) ^ TAB[(x_ >> 24) ^ *p])) & 0xFFFFFFFF;
  len_ += len;
  return *this;
}


unsigned Crc::final()
{
  while (len_ > 0) {
      x_ = ((x_ << 8) ^ TAB[((x_ >> 24) ^ len_) & 0xFF]) & 0xFFFFFFFF;
      len_ >>= 8;
  }
  return ~x_ & 0xFFFFFFFF;
}

Crc &Crc::feed(const char *s)
{
   if (s)
      while (IS_SPACE(*s)) ++s;
   return feed(s,s ? strlen(s) : 0);
}




static inline unsigned char digit(char c)
{
   return yabu_cclass[(unsigned char)c] & 0x0F;
}

static inline bool is_oct(char c)
{
   unsigned char cc = yabu_cclass[(unsigned char)c];
   return (cc & 0x40) && (cc & 0x0F) < 8;
}


////////////////////////////////////////////////////////////////////////////////////////////////////
// Ersetzt in einem String die Escape-Sequenzen \r, \n, \t, \f, \b, \a, \e (ESC), \", \\  sowie
// sowie \xHH und \OOO durch das betreffende Zeichen. Überschreibt den Originalstring.
////////////////////////////////////////////////////////////////////////////////////////////////////

void unescape(char *str)
{
   char const *s = str;
   char *d = str;

   while (*s != 0) {
      char ch = *s++;
      if (ch == '\\' && *s != 0) {					// '\' gefunden
         if (s[0] == 'x' && IS_XDIGIT(s[1]) && IS_XDIGIT(s[2])) {	// \xHH
            ch = (digit(s[1]) << 4) + digit(s[2]);
            s += 3;
         }
         else if (is_oct(s[0]) && is_oct(s[1]) && is_oct(s[2])) {	// \OOO
            ch = (digit(s[0]) << 6) + (digit(s[1]) << 3) + digit(s[2]);
            s += 3;
         }
         else {								// Sonstige Steuersequenzen
            switch (*s) {
               case 'e': ch = '\033'; break;
               case 'f': ch = '\f'; break;
               case 't': ch = '\t'; break;
               case 'r': ch = '\r'; break;
               case 'n': ch = '\n'; break;
               case 'a': ch = '\a'; break;
               case 'b': ch = '\b'; break;
               case '\\': ch = '\\'; break;
               case '"': ch = '"'; break;
               default: --s;          					// Keine Steuersequenz
            }
            ++s;
         }
      }
      *d++ = ch;
   }

   *d = 0;
}


////////////////////////////////////////////////////////////////////////////////////////////////////
// Gegenstück zu «unescape()». Ersetzt alle Steuerzeichen durch Escape-Sequenzen.
////////////////////////////////////////////////////////////////////////////////////////////////////

void escape(Str &out, const char *src)
{
   if (src == 0)
      src = "";
   out.clear();
   out.reserve(strlen(src));
   for (unsigned char const *s = (unsigned char const *) src; *s; ) {
      const unsigned char *beg = s;
      while (*s > 32 && *s != 0x7f) ++s;
      if (s > beg) out.append((const char*)beg,s - beg);
      while (*s != 0 && (*s < 32 || *s == 0x7f))
	 out.printfa("\\x%02x",*s++);
   }
}


// =================================================================================================

#define REP(s) ((size_t *)(s) - 3)
#define LEN(s) (((size_t *)(s))[-1])
#define CAP(s) (((size_t *)(s))[-2])
#define REF(s) (((size_t *)(s))[-3])
#define TXT(h) ((char*)(h) + 3 * sizeof(size_t))

static size_t const EMPTY[4] = {0,0,0,0};

static inline void safe_strcpy(char *d, const char *s, size_t len)
{
   if (len > 0) {
      if (s) memcpy(d,s,len);
      d[len] = 0;
   }
}


static void delref(char *s)
{
    size_t * const r = REP(s);
    if (r[0] != 0 && --r[0] == 0)
       free(r);
}

static void addref(char *s)
{
    size_t * const r = REP(s);
    if (r[0] > 0)
	++r[0];
}

static char *mkrep(const char *s, size_t len, size_t cap)
{
    YABU_ASSERT(len <= cap);
    if (len == 0 && cap == 0) return TXT(EMPTY);
    size_t *rep = (size_t *) malloc(cap + 1 + 3 * sizeof(size_t));
    rep[0] = 1;
    rep[1] = cap;
    rep[2] = len;
    char *txt = TXT(rep);
    safe_strcpy(txt,s,len);
    return txt;
}

static char *mkrep(const char *s, size_t len)
{
    return mkrep(s,len,len);
}

static char *mkrep(const char *s)
{
    if (s == 0 || *s == 0) return TXT(EMPTY);
    return mkrep(s,strlen(s));
}


////////////////////////////////////////////////////////////////////////////////////////////////////
// Default-Konstruktor
////////////////////////////////////////////////////////////////////////////////////////////////////

Str::Str()
    :s_(TXT(EMPTY))
{
}


////////////////////////////////////////////////////////////////////////////////////////////////////
// Konstruktor aus C-String
////////////////////////////////////////////////////////////////////////////////////////////////////

Str::Str(const char *src)
    : s_(mkrep(src))
{
}


////////////////////////////////////////////////////////////////////////////////////////////////////
// Konstruktor aus char-Array ohne abschließendes NUL
////////////////////////////////////////////////////////////////////////////////////////////////////

Str::Str(const char *src, size_t len)
    : s_(mkrep(src,len))
{
}


////////////////////////////////////////////////////////////////////////////////////////////////////
// Copy-Konstruktor
////////////////////////////////////////////////////////////////////////////////////////////////////

Str::Str(Str const &src)
    : s_(src.s_)
{
    addref(s_);
}


////////////////////////////////////////////////////////////////////////////////////////////////////
// Destruktor
////////////////////////////////////////////////////////////////////////////////////////////////////

Str::~Str()
{
    delref(s_);
}


////////////////////////////////////////////////////////////////////////////////////////////////////
// Werte zweiter Strings austauschen
////////////////////////////////////////////////////////////////////////////////////////////////////

void Str::swap(Str &other)
{
    char *my_rep = s_;
    s_ = other.s_;
    other.s_ = my_rep;
}


////////////////////////////////////////////////////////////////////////////////////////////////////
// Inhalt löschen
////////////////////////////////////////////////////////////////////////////////////////////////////

void Str::clear()
{
   if (REF(s_) == 1) {
      LEN(s_) = 0;	// Puffer behalten, wird eventuell gleich wieder gebraucht.
      *s_ = 0;
   } else {
      delref(s_);
      s_ = TXT(EMPTY);
   }
}


////////////////////////////////////////////////////////////////////////////////////////////////////
// Zuweisungsoperator
////////////////////////////////////////////////////////////////////////////////////////////////////

Str &Str::operator=(Str const &rhs)
{
    // Folgendes funktioniert auch, wenn «rhs» gleich «*this» ist
    addref(rhs.s_);
    delref(s_);
    s_ = rhs.s_;
    return *this;
}


////////////////////////////////////////////////////////////////////////////////////////////////////
// Stellt sicher, daß folgende Bedingungen erfüllt sind:
// - REF == 1
// - CAP >= «cap»
// - CAP >= LEN
////////////////////////////////////////////////////////////////////////////////////////////////////

static void excl(char *&s, unsigned cap)
{
    if (REF(s) != 1) {				// Kopie erzeugen?
	if (cap < LEN(s)) cap = LEN(s);
	char * const old_s = s;
	s = mkrep(s,LEN(s),cap);
	delref(old_s);
    }
    else if (cap > CAP(s)) {			// Vergrößern?
	size_t *rep = REP(s);
	rep = (size_t *) realloc(rep,3 * sizeof(size_t) + cap + 1);
	rep[1] = cap;
	s = TXT(rep);
    }
}


////////////////////////////////////////////////////////////////////////////////////////////////////
// Schafft Platz für mindestens «min_avail» weitere Zeichen. LEN bleibt unverändert.
////////////////////////////////////////////////////////////////////////////////////////////////////

void Str::reserve(size_t min_avail)
{
    excl(s_,LEN(s_) + min_avail);
}


////////////////////////////////////////////////////////////////////////////////////////////////////
// Vergrößert die Länge ohne Reallokierung.
////////////////////////////////////////////////////////////////////////////////////////////////////

void Str::extend(size_t n)
{
   YABU_ASSERT(LEN(s_) + n <= CAP(s_));
   s_[LEN(s_) += n] = 0;
}


////////////////////////////////////////////////////////////////////////////////////////////////////
// Macht den Inhalt modifizierbar (erzeugt also ggf. eine Kopie)
////////////////////////////////////////////////////////////////////////////////////////////////////

char *Str::data()
{
    excl(s_,0);
    return s_;
}


////////////////////////////////////////////////////////////////////////////////////////////////////
// Hängt einen Text an.
////////////////////////////////////////////////////////////////////////////////////////////////////

Str &Str::append(const char *src, size_t len)
{
    size_t const my_len = LEN(s_);
    if (src >= s_ && src <= s_ + my_len) {
	// Wenn die Quelle Teil des Ziels ist, müssen wir beachten, daß
	// «excl()» möglicherweise den Puffer an eine andere Position verschiebt.
	size_t const n = src - s_;
	excl(s_,my_len + len);
	src = s_ + n;
    }
    else
	excl(s_,my_len + len);

    if (s_ != TXT(EMPTY)) {
       safe_strcpy(s_ + my_len,src,len);
       LEN(s_) = my_len + len;
    }
    return *this;
}

Str &Str::append(const char *src)
{
    return append(src,strlen(src));
}

Str &Str::append(Str const &src)
{
    return append(src.s_,LEN(src.s_));
}


////////////////////////////////////////////////////////////////////////////////////////////////////
// Wie «::vsprintf()», aber ohne feste Puffergröße. Returnwert ist «*this».
////////////////////////////////////////////////////////////////////////////////////////////////////

Str &Str::vprintf(const char *fmt, va_list args)
{
    va_list saved_args;
    va_copy(saved_args,args);
    reserve(strlen(fmt) + 32);
    int n = vsnprintf(s_,capacity() + 1,fmt,args);
    if (n < 0)
	return *this;
    if ((size_t) n > capacity()) {
	reserve(n - LEN(s_));
        vsnprintf(s_,n + 1,fmt,saved_args);
    }
    LEN(s_) = n;
    va_end(saved_args);
    return *this;
}


////////////////////////////////////////////////////////////////////////////////////////////////////
// Wie «vprintf()», aber der formatierte Text wird angehängt.
////////////////////////////////////////////////////////////////////////////////////////////////////

Str &Str::vprintfa(const char *fmt, va_list args)
{
    va_list saved_args;
    va_copy(saved_args,args);
    reserve(strlen(fmt) + 32);
    int n = vsnprintf(s_ + len(),avail() + 1,fmt,args);
    if (n < 0)
	return *this;
    if ((size_t) n > avail()) {
	reserve(n);
        vsnprintf(s_ + len(),n + 1,fmt,saved_args);
    }
    size_t const new_len = LEN(s_) + n;
    LEN(s_) = new_len;
    s_[new_len] = 0;
    va_end(saved_args);
    return *this;
}


////////////////////////////////////////////////////////////////////////////////////////////////////
// Wie «::sprintf()», aber ohne feste Puffergröße. Returnwert ist «*this».
////////////////////////////////////////////////////////////////////////////////////////////////////

Str &Str::printf(const char *fmt, ...)
{
    va_list al;
    va_start(al,fmt);
    return vprintf(fmt,al);
    va_end(al);
}


////////////////////////////////////////////////////////////////////////////////////////////////////
// Wie «printf()», aber der formatierte Text wird angehängt.
////////////////////////////////////////////////////////////////////////////////////////////////////

Str &Str::printfa(const char *fmt, ...)
{
    va_list al;
    va_start(al,fmt);
    return vprintfa(fmt,al);
    va_end(al);
}


////////////////////////////////////////////////////////////////////////////////////////////////////
// Allokiert Speicher für «n» Objekte des Type «T»
////////////////////////////////////////////////////////////////////////////////////////////////////

void array_alloc(void **t, size_t n, size_t size)
{
   if ((*t = malloc(n * size)) == 0 && n > 0)
      YUFTL(G20,syscall_failed("malloc",0));
   memset(*t,0,n * size);
}


////////////////////////////////////////////////////////////////////////////////////////////////////
// Ändert die Größe eines Arrays auf «n» Objekte des Typs «T». Ist beim Aufruf «*t» gleich 0,
// dann verhält sich die Funktion wie «array_alloc».
////////////////////////////////////////////////////////////////////////////////////////////////////

void array_realloc(void **t, size_t n, size_t size)
{
   if ((*t = realloc(*t,n * size)) == 0 && n > 0)
      YUFTL(G20,syscall_failed("realloc",0));
}


////////////////////////////////////////////////////////////////////////////////////////////////////
// Fügt ein Element on Position «pos» ein, füllt es mit 0 und erhöht «size».
////////////////////////////////////////////////////////////////////////////////////////////////////

void array_insert(void **t, size_t *n, size_t size, size_t pos)
{
   YABU_ASSERT(pos <= *n);
   char *c = (char*) (*t = realloc(*t, (*n + 1) * size));
   if (c == 0)
      YUFTL(G20,syscall_failed("realloc",0));
   else {
      memmove(c + (pos + 1) * size, c + pos*size, (*n - pos) * size);
      memset(c + pos*size, 0, size);
      ++*n;
   }
}


// vim:sw=3:cin:fileencoding=utf-8
