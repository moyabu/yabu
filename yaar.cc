////////////////////////////////////////////////////////////////////////////////////////////////////
// I, the creator of this work, hereby release it into the public domain. This applies worldwide.
// In case this is not legally possible: I grant anyone the right to use this work for any purpose,
// without any conditions, unless such conditions are required by law.
////////////////////////////////////////////////////////////////////////////////////////////////////

// Behandlung von Bibliotheken und Zielen der Form BIB.a(FILE)

#include "yabu.h"

#include <sys/types.h>
#include <sys/stat.h>
#include <errno.h>
#include <unistd.h>
#include <fcntl.h>
#include <string.h>


// ar-Header
static unsigned const HDR_NAME = 0;
static unsigned const HDR_NAME_LEN = 16;
static unsigned const HDR_TIME = 16;
static unsigned const HDR_SIZE = 46;



////////////////////////////////////////////////////////////////////////////////////////////////////
// Objekt innerhalb einer Bibliothek.
////////////////////////////////////////////////////////////////////////////////////////////////////

struct Member	
{
  const char *name_;             // Dateiname
  unsigned time_;                // Änderungszeit
  // Für my_bsearch():
  friend int compare(const char *s, const Member * a) { return strcmp (s, a->name_); }
};



////////////////////////////////////////////////////////////////////////////////////////////////////
// Inhaltsverzeichnis einer Bibliothek.
////////////////////////////////////////////////////////////////////////////////////////////////////

struct Archive
{
  const char *name_;		// Dateiname der Bibliothek.
  struct stat last_stat_;	// Status bei letzen Dateizugriff.
  size_t n_members_;		// Anzahl der Dateien.
  Member *members_;		// Enthaltene Dateien.
  char *long_names_;		// Puffer für lange Dateinamen
  unsigned long_names_size_;	// Puffergröße in Bytes - 1
  char *crc_buf_;		// Zur CRC-Berechnung (-A cksum)
  unsigned crc_buf_size_;
  friend int compare(const char *s, const Archive * a) { return strcmp(s, a->name_); }
  void cleanup();
  bool read_long_names (int fd, unsigned size);
  bool calc_cksum(unsigned long *crc, int fd, unsigned len);
  void add_member(const char *name, unsigned ftime);
  int next(int fd, TsAlgo_t ts_algo);
  void rescan(TsAlgo_t ts_algo);
};

static Archive *archives = 0;	// Alle Bibliotheken
static size_t n_archives = 0;	// Anzahl der Einträge in «archives»



////////////////////////////////////////////////////////////////////////////////////////////////////
// Öffnet eine Bibliothek und prüft das Vorhandensein des "Magic String".
// Return: Dateideskriptor oder -1 (E/A-Fehler oder kein ar-Header).
////////////////////////////////////////////////////////////////////////////////////////////////////

static int open_archive(const char *archive_name)
{
   int fd = yabu_open(archive_name, O_RDONLY);
   if (fd < 0)
      return -1;
   static const char AR_MAGIC[] = "!<arch>\n";
   char magic[sizeof(AR_MAGIC) - 1];
   if (   yabu_read(fd, magic, sizeof(magic)) != sizeof(magic)
       || memcmp(magic, AR_MAGIC, sizeof(magic))) {
      close(fd);
      return -1;
   }
   return fd;
}


////////////////////////////////////////////////////////////////////////////////////////////////////
// Einen (ar-)Dateiheader lesen.
// Liest den 60 Bytes großen Header an der aktuellen Position aus der Datei ||fd||.
// hdr: Header (60 Bytes).
// size: Dateigröße.
// return: -1: Fehler. 1: Dateiende. 0: Ok.
////////////////////////////////////////////////////////////////////////////////////////////////////

static int read_header(char *hdr, unsigned long *size, int fd)
{
  // Header lesen und (sehr simple) Konsistenzprüfung.
  int rc = yabu_read(fd, hdr, 60);
  if (rc == 0)
    return 1;                   // Dateiende
  if (rc != 60 || hdr[58] != '`' || hdr[59] != '\n')
    return -1;                  // Unvollständig, Lesefehler oder korrupter Header

  // Header mit NULL beenden, damit wir später die üblichen
  // Stringfunktionen ohne Gefahr benutzen können.
  hdr[58] = 0;

  // Dateigröße auslesen.
  if (sscanf (hdr + HDR_SIZE, "%lu", size) != 1)
    return -1;
  return 0;                     // Ok

}


////////////////////////////////////////////////////////////////////////////////////////////////////
// Datei zum Inhaltsverzeichnis hinzufügen.
// name: Dateiname.
// ftime: Änderungszeit.
////////////////////////////////////////////////////////////////////////////////////////////////////

void Archive::add_member(const char *name, unsigned ftime)
{
   Message(MSG_3,"Add member %u %s", ftime, name);
   size_t pos;
   if (!my_bsearch(&pos, members_, n_members_, name)) {
      array_insert(members_, n_members_, pos);
      members_[pos].name_ = name;
   }
   members_[pos].time_ = ftime;
}





////////////////////////////////////////////////////////////////////////////////////////////////////
// Speicher freigeben.
////////////////////////////////////////////////////////////////////////////////////////////////////

void Archive::cleanup()
{
   if (long_names_)
      free(long_names_);
   long_names_ = 0;
   long_names_size_ = 0;
   if (crc_buf_)
      free(crc_buf_);
   crc_buf_ = 0;
   crc_buf_size_ = 0;
}


////////////////////////////////////////////////////////////////////////////////////////////////////
// Lange Dateinamen lesen.
// Liest ||size|| Bytes aus der Datei und speichert sie -- mit einem abschließenden NUL -- 
// in ||long_names_||.
////////////////////////////////////////////////////////////////////////////////////////////////////

bool Archive::read_long_names(int fd, unsigned size)
{
   if (size % 2 == 1)
      ++size;                   // Länge eines Eintrags muß gerade sein.
   long_names_size_ = size;
   array_realloc(long_names_, size + 1);
   if (yabu_read(fd, long_names_, size) != (int) size)
      return false;
   long_names_[size] = 0;
   return true;
}

bool Archive::calc_cksum(unsigned long *cksum, int fd, unsigned len)
{
   if (len > crc_buf_size_) {
      array_realloc(crc_buf_, len);
      crc_buf_size_ = len;
   }
   if ((unsigned) yabu_read(fd, crc_buf_, len) != len)
      return false;
   *cksum = Crc(crc_buf_, len).final();
   return true;
}


////////////////////////////////////////////////////////////////////////////////////////////////////
// Liest das nächste Element in der Bibliothek und trägt es in das Inhaltsverzeichnis ein.
// Beim Aufruf erwartet die Funktion, daß der Lesezeiger auf einem ar-Header positioniert ist.
// Nach erfolgreicher Ausführung zeigt der Lesezeiger auf den Header des folgenden Elementes.
// fd: Dateideskiptor.
// ts_algo: Algorithmus zur Bestimmung der Änderungszeit.
////////////////////////////////////////////////////////////////////////////////////////////////////

int Archive::next(int fd, TsAlgo_t ts_algo)
{
   char hdr[60];
   unsigned long size;
   int rc = read_header(hdr, &size, fd);
   if (rc != 0)
      return rc;

   // Wenn der Dateiname mit "//" beginnt, ist es kein normales Objekt, sondern 
   // eine Liste von langen Dateinamen. Wir lesen und merken uns die Namen für später.
   if (hdr[0] == '/' && hdr[1] == '/')
      return read_long_names(fd, size) ? 0 : -1;

   // Wenn der Name mit "/ " beginnt, ist es die Symboltabelle. Die interessiert uns nicht.
   if (hdr[0] == '/' && hdr[1] == ' ') {
      lseek(fd, size % 2 ? size + 1 : size, SEEK_CUR);
      return 0;
   }

   // Es ist eine normale Datei. Beginnt der Name mit "/", dann steht dahinter der
   // Offset des Namens in long_names_.
   char *name = hdr;
   if (hdr[0] == '/') {
      unsigned off;
      if (sscanf(hdr + 1, "%u", &off) != 1 || off >= long_names_size_)
         return -1;
      name = long_names_ + off;
      // '/' markiert das Ende des Dateinamens.
      char *c = name;
      while (*c != '/' && *c != 0)
         ++c;
      *c = 0;
   }
   else {
      // Es ist ein kurzer Name. Entferne Leerzeichen aus dem Dateinamen.
      char *c = hdr;
      while (*c != '/' && *c != 0 && c < hdr + HDR_NAME_LEN)
         ++c;
      if (*c != '/')
         while (c > hdr && c[-1] == ' ')
            --c;
      *c = 0;
   }

   // Änderungszeit ermitteln. Je nach benutztem Algorithmus nehmen wir die
   // Zeitangabe aus dem ar-Header bzw. die Prüfsumme über den Dateiinhalt.
   unsigned long ftime;
   switch (ts_algo) {
   case TSA_DEFAULT:
   case TSA_MTIME:
   case TSA_MTIME_ID:
      if (sscanf(hdr + HDR_TIME, "%lu", &ftime) != 1)
         return -1;
      // Dateiinhalt überspringen. Beachte, daß bei ungerader Dateilänge
      // ein Füllbyte eingeschoben ist.
      lseek(fd, size % 2 ? size + 1 : size, SEEK_CUR);
      break;
   case TSA_CKSUM:
      if (!calc_cksum(&ftime, fd, size))
         return -1;
      if (size % 2)
         lseek(fd, 1, SEEK_CUR);        // Füllbyte überspringen
      break;
   }

   add_member(str_freeze(name), ftime);
   return 0;
}


// Inhaltsverzeichnis neu erstellen

void Archive::rescan(TsAlgo_t ts_algo)
{
   Message(MSG_3,"Archiv %s wird gelesen", name_);
   if (members_)
      free(members_);
   members_ = 0;
   n_members_ = 0;
   int fd = open_archive(name_);
   if (fd < 0)
      return;
   int rc;
   while ((rc = next(fd, ts_algo)) == 0);
   if (rc < 0)
      Message(MSG_W,"%s",Msg::archive_corrupted(name_));
   close(fd);
   cleanup();
}

////////////////////////////////////////////////////////////////////////////////////////////////////
// Bibliothek öffnen und Inhaltsverzeichnis allokieren.
////////////////////////////////////////////////////////////////////////////////////////////////////
 
static Archive *find_archive(const char *archive_name, TsAlgo_t ts_algo)
{
   struct stat sb;
   if (!yabu_stat(archive_name, &sb)) {
      MSG(MSG_1,Msg::archive_not_found(archive_name, errno));
      return 0;
   }
   size_t pos;
   if (!my_bsearch(&pos, archives, n_archives, archive_name)) {
      array_insert(archives, n_archives, pos);
      memset(archives + pos, 0, sizeof(archives[pos]));
      archives[pos].name_ = archive_name;
   }
   Archive *a = archives + pos;
   if (sb.st_mtime != a->last_stat_.st_mtime
       || sb.st_size != a->last_stat_.st_size || sb.st_ino != a->last_stat_.st_ino) {
      a->rescan(ts_algo);
      a->last_stat_ = sb;
   }
   return a;
}


////////////////////////////////////////////////////////////////////////////////////////////////////
// Änderungszeit einer Datei in einer Bibliothek ermitteln.
// mtime (o): Änderungszeit.
// name: Name des Zieles, BIB.a(NAME.o).
// ts_algo: Algorithmus zur Bestimmung der Änderungszeit.
// return: True: Ok. False: Ziel hat nicht das erwartete Format oder sonstiger Fehler.
////////////////////////////////////////////////////////////////////////////////////////////////////

bool ar_member_time(Ftime *mtime, const char *name, TsAlgo_t ts_algo)
{
   // Namen zerlegen.
   const char *c = strchr(name,'(');
   if (c == 0 || c == name)
      return false;
   const char *mstart = c + 1;
   if ((c = strchr(mstart,')')) == 0 || c[1] != 0)
      return false;
   const char *archive_name = str_freeze(name, mstart - name - 1);
   const char *member_name = str_freeze(mstart, c - mstart);

   Archive *a = find_archive(archive_name, ts_algo);
   if (a == 0)
      *mtime = 0;               // Nicht gefunden
   else {
      size_t pos;
      if (!my_bsearch(&pos, a->members_, a->n_members_, member_name))
         *mtime = 0;            // Nicht gefunden
      else
         *mtime = a->members_[pos].time_;
   }
   return true;
}
