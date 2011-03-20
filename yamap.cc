////////////////////////////////////////////////////////////////////////////////////////////////////
// I, the creator of this work, hereby release it into the public domain. This applies worldwide.
// In case this is not legally possible: I grant anyone the right to use this work for any purpose,
// without any conditions, unless such conditions are required by law.
////////////////////////////////////////////////////////////////////////////////////////////////////

// Die Klasse «StringMap» ist eine sortierte Liste von Zeichenfolgen der Form NAME=WERT.

#include "yabu.h"

#include <errno.h>
#include <unistd.h>


////////////////////////////////////////////////////////////////////////////////////////////////////
// Konstruktor
////////////////////////////////////////////////////////////////////////////////////////////////////

StringMap::StringMap()
   :n_(1), buf_(0)
{
   array_alloc(buf_,1);
}


////////////////////////////////////////////////////////////////////////////////////////////////////
// Copy-Konstruktor
////////////////////////////////////////////////////////////////////////////////////////////////////

StringMap::StringMap(const StringMap &src)
   :n_(src.n_)
{
   array_alloc(buf_,n_);
   for (size_t i = 0; i < src.n_ - 1; ++i)
      buf_[i] = strdup(src.buf_[i]);
   buf_[n_ - 1] = 0;
}


////////////////////////////////////////////////////////////////////////////////////////////////////
// Destruktor
////////////////////////////////////////////////////////////////////////////////////////////////////

StringMap::~StringMap()
{
   clear();
}

void StringMap::swap(StringMap &x)
{
   size_t n = n_; n_ = x.n_; x.n_ = n;
   char **buf = buf_; buf_ = x.buf_; x.buf_ = buf;
}


////////////////////////////////////////////////////////////////////////////////////////////////////
// Löscht alle Variablen und gibt alle internen Puffer frei.
////////////////////////////////////////////////////////////////////////////////////////////////////

void StringMap::clear()
{
   for (size_t i = 0; i + 1 < n_; ++i)
      free(buf_[i]);
   free(buf_);
   n_ = 0;
   buf_ = 0;
}


////////////////////////////////////////////////////////////////////////////////////////////////////
// Wie «strcmp(a,b,a_len)», aber «b» wird nur bis zum ersten '=' berücksichtigt.
////////////////////////////////////////////////////////////////////////////////////////////////////

static int cmpnames(const char *a, size_t a_len, const char *b)
{
   while (a_len > 0 && *b != 0 && *b != '=') {
      if (*a > *b) return 1;
      if (*a < *b) return -1;
      ++a;
      ++b;
      --a_len;
   }
   if (*b != 0 && *b != '=') return 1;
   if (a_len > 0) return -1;
   return 0;
}


////////////////////////////////////////////////////////////////////////////////////////////////////
// Binäre Suche
////////////////////////////////////////////////////////////////////////////////////////////////////

static bool bs(size_t *pos, char **tab, size_t n, const char *key, size_t key_len)
{
   size_t lo = 0;
   int rc = 1;
   if (n != 0) {
      size_t hi = n - 1;
      while (lo < hi && rc != 0) {
	 size_t mid = (hi + lo) / 2;
	 rc = cmpnames(key,key_len,tab[mid]);
	 if (rc == 0)
	    hi = lo = mid;
	 else if (rc > 0)
	    lo = mid + 1;
	 else
	    hi = mid;
      }
   }
   if (pos) *pos = lo;
   return rc == 0;
}


////////////////////////////////////////////////////////////////////////////////////////////////////
// Variable hinzufügen bzw. Wert ändern. Der Name «key» muß nicht mit NUL abgeschlossen sein.
// Ist «val» gleich 0, dann wird der entsprechende Eintrag, falls vorhanden, gelöscht.
// return: True: Wert wurde neu eingefügt oder gelöscht. False: vorhandener Wert wurde
//    überschrieben, oder (beim löschen) der Schlüssel war nicht vorhanden
////////////////////////////////////////////////////////////////////////////////////////////////////

bool StringMap::set(const char *key, size_t key_len, const char *val)
{
   size_t pos;
   bool found = bs(&pos,buf_,n_,key,key_len);
   if (val == 0) {	// Wert löschen
      if (found) {
	 free(buf_[pos]);
	 memmove(buf_ + pos,buf_ + pos + 1,(n_ - pos - 1) * sizeof(*buf_));
	 --n_;
      }
      return found;
   } 

   if (!found)
      array_insert(buf_,n_,pos);
   const size_t val_len = strlen(val);
   array_realloc(buf_[pos],key_len + 1 + val_len + 1);
   memcpy(buf_[pos],key,key_len);
   buf_[pos][key_len] = '=';
   memcpy(buf_[pos] + key_len + 1,val,val_len + 1);
   return !found;
}


////////////////////////////////////////////////////////////////////////////////////////////////////
// Wert zum Namen «key» oder 0, falls nicht definiert.
////////////////////////////////////////////////////////////////////////////////////////////////////

const char *StringMap::operator[] (const char *key) const
{
   size_t const key_len = strlen(key);
   size_t pos;
   return bs(&pos,buf_,n_,key,key_len) ? buf_[pos] + key_len + 1 : 0;
}


////////////////////////////////////////////////////////////////////////////////////////////////////
// Prüft ob ein Eintrag mit dem Namen «key» vorhanden ist.
////////////////////////////////////////////////////////////////////////////////////////////////////

bool StringMap::contains(const char *key) const
{
   return bs(0,buf_,n_,key,strlen(key));
}


////////////////////////////////////////////////////////////////////////////////////////////////////
// Eintrag hinzufügen bzw. Wert ändern.
// return: True: neuer Wert wurde eingefügt. False: vorhandener Wert wurde überschrieben.
////////////////////////////////////////////////////////////////////////////////////////////////////

bool StringMap::set(const char *key, const char *val)
{
   if (key == 0 || *key == 0) return false;
   return set(key,strlen(key),val);
}


////////////////////////////////////////////////////////////////////////////////////////////////////
// Eintrag hinzufügen bzw. Wert ändern. «key_val» hat die Form KEY=VALUE.
// return: True: neuer Wert wurde eingefügt. False: vorhandener Wert wurde überschrieben.
////////////////////////////////////////////////////////////////////////////////////////////////////

bool StringMap::set(const char *key_val)
{
   const char *equ = strchr(key_val,'=');
   if (equ == 0 || equ == key_val) return false;
   return set(key_val,equ - key_val,equ+1);
}



char **StringMap::env() const
{
   return buf_;
}


// vim:sw=3:cin:fileencoding=utf-8
