# Transformationsregeln: Verwendung von **
# Der Platzhalter '**' steht f�r einen beliebigen Teilstring, der
# auch '.' und '/' enthalten kann.

INPUT=A.B.C/DEF/Z

all:
  echo 'Xx:$(INPUT:A**Z=<*>)'
#STDOUT:Xx:<.B.C/DEF/>

