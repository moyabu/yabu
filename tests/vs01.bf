# Variablenersetzung: Rekursive Variablenreferenzen
# - Reihenfolge der Variablendefinition ist nicht entscheidend
# - Ein Variablenwert kann Variablenreferenzen enthalten;
#   diese werde rekursiv ersetzt.
# - Eine Transformationsregel wird erst nach der vollständigen 
#   Ersetzung von Variablen durchgeführt.

C=$(B:*=c*)
B=$(A) b1 b2
A=a1 a2 a3

all::
  echo "Xx:$(C)"

#STDOUT:Xx:ca1 ca2 ca3 cb1 cb2
