////////////////////////////////////////////////////////////////////////////////////////////////////
// I, the creator of this work, hereby release it into the public domain. This applies worldwide.
// In case this is not legally possible: I grant anyone the right to use this work for any purpose,
// without any conditions, unless such conditions are required by law.
////////////////////////////////////////////////////////////////////////////////////////////////////

// Funktionen zum Lesen und Schreiben von Buildfile.state


#include "yabu.h"

#include <unistd.h>
#include <time.h>
#include <string.h>


static const char FILE_HEADER[] = "yabu " YABU_VERSION;
static FILE *file = 0;
static const char *file_name;
static bool entry_open;
static char *line = 0;
static size_t line_len = 0;
static size_t line_capacity = 0;
static const unsigned char SEPARATOR = 9;	// Trennzeichen


////////////////////////////////////////////////////////////////////////////////////////////////////
// String in Wörter zerlegen und an Liste anhängen.
////////////////////////////////////////////////////////////////////////////////////////////////////


static void split(StringList &sl, char *c)
{
   while (c) {
      char *beg = c;
      if ((c = strchr(c,SEPARATOR)) != 0) *c++ = 0;
      unescape(beg);
      sl.append(beg);
   }
}

static void close_file()
{
   if (file) {
      fclose(file);
      file = 0;
   }
}

static bool open_file(const char *name, const char *mode)
{
   close_file();
   file_name = name;
   file = fopen(file_name, mode);
   return file != 0;
}


////////////////////////////////////////////////////////////////////////////////////////////////////
// Nächste Zeile nach «line» lesen.
// Return: True: Ok. False: Fehler oder Dateiende.
////////////////////////////////////////////////////////////////////////////////////////////////////

static bool next_line()
{
   if (file == 0)
      return false;
   line_len = 0;
   while (true) {
      if (line_len + 80 > line_capacity)
	 array_realloc(line,line_capacity += 100);
      if (fgets(line + line_len,line_capacity - line_len,file) == 0)
	 break;
      char *end = line + line_len  + strlen(line + line_len);
      if (end > line + line_len && end[-1] == '\n') {
	 end[-1] = 0;
	 break;
      }
      line_len = end - line;
   }
    const bool finish = feof(file) || ferror(file);
    if (finish)
       close_file();
    return !finish;
}


static void end()
{
    if (file && entry_open) {
	entry_open = false;
	fputc('\n',file);
    }
}


void StateFileWriter::begin(const char *c)
{
    if (file) {
       end();
       entry_open = true;
       fputs(c,file);
    }
}


void StateFileWriter::append(const char *s)
{
   if (file) {
      Str b;
      escape(b,s ? s : "");
      fprintf(file,"%c%s",SEPARATOR,(const char*) b);
   }
}

void StateFileWriter::append(unsigned val)
{
   if (file) {
      char tmp[20];
      snprintf(tmp,sizeof(tmp),"%x",val);
      append(tmp);
   }
}

bool StateFileReader::decode(unsigned &val, const char *s)
{
   val = 0;
   return sscanf(s,"%x",&val) == 1;
}

void StateFileWriter::append(Ftime const &val)
{
   if (file) {
      char tmp[30];
      snprintf(tmp,sizeof(tmp),"%x.%x",val.s_,val.ns_);
      append(tmp);
   }
}

bool StateFileReader::decode(Ftime &t, const char *s)
{
    return sscanf(s,"%x.%x",&t.s_,&t.ns_) == 2;
}




StateFileReader::StateFileReader(const char *state_file)
   :invalid_(false)
{
   // Datei öffnen
   if (!open_file(state_file,"r"))
      return;
   MSG(MSG_1,Msg::reading_file(file_name,0,0));

   // Header lesen und prüfen
   if (!next_line() || strcmp(line,FILE_HEADER)) {
      invalid_ = true;
      close_file();
   }
}


StateFileReader::~StateFileReader()
{
   close_file();
   if (invalid_) {
      Message(MSG_W,Msg::discarding_state_file(file_name));
      unlink(file_name);
   }
}


bool StateFileReader::next()
{
   argv_.clear();
   if (!next_line()) return false;
   split(argv_,line);
   return true;
}


StateFileWriter::StateFileWriter(const char *state_file)
{
   if (!open_file(state_file,"w"))
      return;
   fprintf(file,"%s\n",FILE_HEADER);
}


StateFileWriter::~StateFileWriter()
{
   end();
   close_file();
}


// vim:sw=3 cin fileencoding=utf-8

