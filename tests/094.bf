# '%' in Variablennamen.
# Der Platzhalter % kann Bestandteil eines Variablennamens sein.
# % wird vor den normalen Variablen ersetzt.

CFLAGS_debug=-g
CFLAGS_release=-O
%/%.o::
   echo "Xx:Build $(0) $(CFLAGS_%1)"

all: debug/aaa.o release/bbb.o

#STDOUT:Xx:Build debug/aaa.o -g
#STDOUT:Xx:Build release/bbb.o -O
