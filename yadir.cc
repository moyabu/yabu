////////////////////////////////////////////////////////////////////////////////////////////////////
// I, the creator of this work, hereby release it into the public domain. This applies worldwide.
// In case this is not legally possible: I grant anyone the right to use this work for any purpose,
// without any conditions, unless such conditions are required by law.
////////////////////////////////////////////////////////////////////////////////////////////////////

#include "yabu.h"

#include <errno.h>
#include <sys/stat.h>


////////////////////////////////////////////////////////////////////////////////////////////////////
// Erstellt das Verzeichnis, welches die Datei «path» enthält. Als Verzeichnis zählt der Teil von
// «path» bis zum letzten '/'. Beispiel:
//   * create_parents("aa/bb/cc")  erzeugt "aa" und "aa/bb"
//   * create_parents("aa/bb/cc/") erzeugt "aa", "aa/bb" und "aa/bb/cc"
// «DirMaker» merkt sich die bereits erstellten Verzeichnisse und ignoriert diese fortan.
////////////////////////////////////////////////////////////////////////////////////////////////////

bool DirMaker::create_parents(const char *path, bool silent)
{
   if (dirs_.contains(path)) return true;	// Hatten wir schon
   for (const char *d = strchr(path, '/'); d; d = strchr(d + 1, '/')) {
      if (d == path)
         continue;
      const char *dir = str_freeze(path, d - path);
      if (dirs_.contains(dir)) continue;
      if (mkdir(dir, 0755) != 0 && errno != EEXIST) {
	 if (!silent) YUERR(G20,syscall_failed("mkdir",dir));
	 return false;
      }
      dirs_.set(dir,"");
   }
   return true;
}
