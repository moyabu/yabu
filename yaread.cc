////////////////////////////////////////////////////////////////////////////////////////////////////
// I, the creator of this work, hereby release it into the public domain. This applies worldwide.
// In case this is not legally possible: I grant anyone the right to use this work for any purpose, without any conditions,
// unless such conditions are required by law.
////////////////////////////////////////////////////////////////////////////////////////////////////

// Einlesen der Datei(en)

#include "yabu.h"

#include <string.h>
#include <sys/types.h>
#include <sys/stat.h>




////////////////////////////////////////////////////////////////////////////////////////////////////
// Der Konstruktor speichert unter anderem den Dateinamen zwecks späterer Verwendung.
////////////////////////////////////////////////////////////////////////////////////////////////////

FileReader::FileReader (const char *file_name, const char *root)
   : file_(0), first_time_(true), lbuf_(0), lbsize_ (0)
{
   src_.file = str_freeze(Str(root).append(file_name));
   src_.line = 0;
   file_name_ = src_.file + strlen(root);
}


////////////////////////////////////////////////////////////////////////////////////////////////////
// Destruktor
////////////////////////////////////////////////////////////////////////////////////////////////////

FileReader::~FileReader ()
{
  if (lbuf_)
    free(lbuf_);
}


////////////////////////////////////////////////////////////////////////////////////////////////////
// Nächste Zeile holen und vorverarbeiten: Zusammensetzen von Fortsetzungszeilen, Abschneiden von
// Leerzeichen und Tabs am Zeilenende, Zusammensetzen von Fortsetzungszeilen.
// Nach erfolgreicher Ausführung enthält ||line_|| die Nummer der letzen gelesenen Zeile.
////////////////////////////////////////////////////////////////////////////////////////////////////

bool FileReader::next_line()
{
   static size_t const MAX_LINE_LIZE = 5000;
   unsigned llen = 0;
   bool ok = false;
   while (true) {
      if (llen + MAX_LINE_LIZE > lbsize_)
         array_realloc(lbuf_, lbsize_ = llen + MAX_LINE_LIZE);
      char *c = lbuf_ + llen;
      if (fgets(c, lbsize_ - llen, file_) == 0)
         break;                 // Dateiende erreicht
      ok = true;
      ++src_.line;

      // Leerzeichen am Ende abschneiden
      char *eol = c + strlen(c);
      while (eol > c && (eol[-1] == '\n' || eol[-1] == ' ' || eol[-1] == '\t'))
         --eol;
      llen = eol - lbuf_;

      // Fortsetzungszeilen beachten
      if (eol > c && eol[-1] == '\\')
         --llen;
      else
         break;
   }
   lbuf_[llen] = 0;
   return ok;
}



////////////////////////////////////////////////////////////////////////////////////////////////////
// Kommentarfilter. Default-Kriterium ist, daß die Zeile mit '#' beginnt.
// c: Zeile.
// return: True: Es ist ein Kommentar. False: Es ist kein Kommentar.
////////////////////////////////////////////////////////////////////////////////////////////////////

bool FileReader::is_comment(const char *c) const
{
   return (*c == 0 || *c == '#');
}



////////////////////////////////////////////////////////////////////////////////////////////////////
// Datei verarbeiten und an Präprozessor übergeben.
// welches die weitere Verarbeitung übernimmt.
////////////////////////////////////////////////////////////////////////////////////////////////////

void FileReader::read(bool ignore_missing)
{
   if ((file_ = fopen(file_name_, "r")) == 0) {
      if (!ignore_missing)
          YUERR(G10,cannot_open(file_name_));
      return;
   }

   struct stat sb;
   yabu_stat(src_.file,&sb);
   char file_id[100];
   snprintf(file_id,sizeof(file_id),"i%xs%xu%x",(int)sb.st_ino,(int)sb.st_size,(int)sb.st_uid);
   static StringMap file_ids;
   first_time_ = file_ids.set(str_freeze(file_id),"");

   MSG(MSG_1,Msg::reading_file(src_.file,0,0));
   src_.line = 0;
   while (next_line()) {
      YabuContext ctx(&src_,0,0);
      if (*lbuf_ && !is_comment(lbuf_))
         process_line(lbuf_);
   }
   fclose(file_);
   file_ = 0;
   finish();
}


void BuildfileReader::finish()
{
   Preprocessor(prj_, lines_, lines_ + num_lines_, parent_, first_time_).execute();
}


////////////////////////////////////////////////////////////////////////////////////////////////////
// Konstruktor.
////////////////////////////////////////////////////////////////////////////////////////////////////

BuildfileReader::BuildfileReader(Project *prj, const char *file_name, Preprocessor *parent)
   :FileReader(str_freeze(file_name),prj->aroot_), prj_(prj), parent_(parent),
    lines_ (0), num_lines_ (0), max_lines_ (0)
{
}


////////////////////////////////////////////////////////////////////////////////////////////////////
// Destruktor.
////////////////////////////////////////////////////////////////////////////////////////////////////

BuildfileReader::~BuildfileReader ()
{
}


////////////////////////////////////////////////////////////////////////////////////////////////////
// Eine Zeile verarbeiten.
// Hängt die Zeile an den Ausgabepuffer an.
// Die weitere Verarbeitung erfolgt in [[Preprocessor]].
// c: Zeile.
////////////////////////////////////////////////////////////////////////////////////////////////////

void BuildfileReader::process_line(char *c)
{
   // Puffer in 100er-Schritten vergrößern
   if (num_lines_ >= max_lines_)
      array_realloc(lines_, max_lines_ += 100);

   // Zeile anhängen
   SrcLine *l = lines_ + num_lines_++;
   l->file = src_.file;
   l->line = src_.line;
   l->text = str_freeze(c);
}



// vim:shiftwidth=3:cindent
