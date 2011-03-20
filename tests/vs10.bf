# Variablen: Rekursive Referenzierung (1)
#
# Der Inhalt einer Variablen kann Variablen enthalten.
# Die Auswertung des Inhalts erfolgt erst, wenn die Variable benutzt wird.

TEST=Test:$(A)
A=A-$(B)
B=B-$(C)
C=C

all:
  echo "Xx:$(TEST)"

#STDOUT:Xx:Test:A-B-C
