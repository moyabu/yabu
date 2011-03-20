# Variablen in Transformationsregeln
#
# Kommt eine Variablenreferenzu auf der rechten Seite einer
# Transformationsregel vor, dann kann der Variablenname '*'-Platzhalter
# enthalten.

LD_a=-laaa
LD_b=-lbbb
LIBS=a b
all:
  echo "Xx:$(LIBS:*=$(LD_*))"

#STDOUT:Xx:-laaa -lbbb
