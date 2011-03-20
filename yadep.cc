////////////////////////////////////////////////////////////////////////////////////////////////////
// I, the creator of this work, hereby release it into the public domain. This applies worldwide.
// In case this is not legally possible: I grant anyone the right to use this work for any purpose,
// without any conditions, unless such conditions are required by law.
////////////////////////////////////////////////////////////////////////////////////////////////////

#include "yabu.h"


////////////////////////////////////////////////////////////////////////////////////////////////////
// Erzeugt eine Abhängigkeitsrelation «tgt» <--- «src», falls diese noch nicht existiert.
//
// Jedes «Dependency»-Objekt ist in zwei Listen enthalten. Die erste Liste beginnt bei 
// «tgt_->srcs_» und ist über «next_src_» verkettet; sie enthält alle Quellen von «tgt_».
// Die zweite Liste beginnt mit «src_->targets_» und ist über «next_tgt_» verkettet; sie
// enthält alle Ziele, die von «src_» abhängen.
////////////////////////////////////////////////////////////////////////////////////////////////////

Dependency *Dependency::create(Target *tgt, Target *src, Rule const *rule)
{
   // Wenn die Relation bereits existiert brauchen wir nichts zu tun. Eine explizite Relation
   // («rule_» != 0) ersetzt aber ggf. eine automatisch erzeugte Relation.
   for (Dependency **dp = &tgt->srcs_; *dp; dp = &(*dp)->next_src_) {
      if ((*dp)->src_ == src) {
	 Dependency * const d = *dp;
	 if (rule != 0 && d->rule_ == 0)
	    d->rule_ = rule;
	 d->deleted_ = false;
         return d;
      }
   }

   // Neue Relation erzeugen.
   Dependency *d = new Dependency(tgt, src, rule);
   if (d == 0)
       YUFTL(G20,syscall_failed("new",0));
   return d;
}


////////////////////////////////////////////////////////////////////////////////////////////////////
// Der Konstruktor trägt das Objekt in beide Listen («src->tgts_» und «tgt->srcs_») ein -- siehe
// «create()».
//
// Explizite Abhängigkeiten (rule != 0) fügen wir vor den automatischen Abhängigkeiten (rule_ == 0)
// ein. Dadurch wird «Target::is_outdated()» etwas effizienter -- wenn man annimmt, daß sich die
// expliziten Quellen häufiger ändern. «is_outdated()» untersucht die Quellen nur so weit bis
// eine auftritt, die neuer als das Ziel ist.
////////////////////////////////////////////////////////////////////////////////////////////////////

Dependency::Dependency(Target *tgt, Target *src, Rule const *rule)
   : tgt_(tgt), src_(src), rule_(rule), deleted_(false), next_tgt_(0), next_src_(0)
{
   YABU_ASSERT(tgt != 0);
   YABU_ASSERT(src != 0);

   Dependency **dp;
   for (dp = &tgt->srcs_; *dp && (rule == 0 || (*dp)->rule_ != 0); dp = &(*dp)->next_src_);
   next_src_ = *dp;
   *dp = this;

   for (dp = &src->tgts_; *dp && (rule == 0 || (*dp)->rule_ != 0); dp = &(*dp)->next_tgt_);
   next_tgt_ = *dp;
   *dp = this;
}


// Setzt das «deleted_»-Flag

void Dependency::destroy(Dependency *dep)
{
   dep->deleted_ = true;
}


// vim:sw=3:cin:fileencoding=utf-8
