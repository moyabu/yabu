////////////////////////////////////////////////////////////////////////////////////////////////////
// I, the creator of this work, hereby release it into the public domain. This applies worldwide.
// In case this is not legally possible: I grant anyone the right to use this work for any purpose,
// without any conditions, unless such conditions are required by law.
////////////////////////////////////////////////////////////////////////////////////////////////////

#include "yabu.h"



////////////////////////////////////////////////////////////////////////////////////////////////////
// Konstruktor.
////////////////////////////////////////////////////////////////////////////////////////////////////


Rule::Rule(const SrcLine *sl)
    : srcline_(sl), config_(0), script_beg_(0), script_end_(0),
      adscript_beg_(0), adscript_end_(0), is_alias_(false), create_only_(false), next_(0)
{
}


////////////////////////////////////////////////////////////////////////////////////////////////////
// Gibt ein Skript aus stdout aus.
////////////////////////////////////////////////////////////////////////////////////////////////////

static void dump_script(const char *tag, SrcLine const *beg, SrcLine const *end)
{
   if (beg && end) {
      unsigned indent = calc_indent(beg->text);
      printf("  [%s]\n",tag);
      for (SrcLine const *l = beg; l < end; ++l) {
	 const char *c = l->text;
	 const char *ind = remove_indent(&c,indent);
	 printf("  |%s%s\n",ind,c);
      }
   }
}


////////////////////////////////////////////////////////////////////////////////////////////////////
// Alle Regeln ausgeben
////////////////////////////////////////////////////////////////////////////////////////////////////

void Rule::dump() const
{
   Str src;
   dump_source(src,srcline_);
   const char *s = src;
   if (*s == ' ') ++s;
   printf("%s:\n", s);
   printf("  %s%s: %s\n", Msg::targets(),is_alias_ ? " (alias)" : "", targets_);
   printf("  %s: %s\n", Msg::sources(),sources_);
   if (config_)
      printf("  %s: %s\n", Msg::options(),config_);
   dump_script("build",script_beg_,script_end_);
   dump_script("auto-depend",adscript_beg_,adscript_end_);
}


// vim:sw=3 cin fileencoding=utf-8
