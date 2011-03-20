# Verhinderung direkter Rekursion.
# Bei der Auswertung der Quellen einer Regel ist diese Regel selbst
# "gesperrt": sie wird nicht zur Aktualisierung einer Quelle in Betracht gezogen.
#
# Im Testfall hat "all" die Quellen "aa", "bb" und "cc"
# Für jede dieser Quellen gäbe es -- ohne den Rekursionsschutz --
# zwei gleichberechtigte Regeln, nämlich "%" und "a% %b %c".
# Die erste Regel ist aber gesperrt, so daß die zweite Regel
# eindeutig ist.
#
# Bem.: das Target "aaa" würde den Fehler B04 erzeugen.


%:: aa bb cc
   echo "Xx: $(0)-$(1)-$(2)-$(3)"

%a %b %c::
   echo "Xx: $(0)"

#STDOUT:Xx: aa
#STDOUT:Xx: bb
#STDOUT:Xx: cc
#STDOUT:Xx: all-aa-bb-cc
