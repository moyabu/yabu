# Variablen: Rekursive Referenzierung (2)
#
# Der Name einer Variablen kann wiederum Variablennamen enthalten. Im folgenden
# Beispiel finden nacheinander folgende Ersetzungen statt:
#   $(VAR$($(N)))
#   $(VAR$(ZERO))
#   $(VAR0)
#   NULL

VAR0=NULL
ZERO=0
N=ZERO

all:
  echo "Xx:$(VAR$($(N)))"

#STDOUT:Xx:NULL
