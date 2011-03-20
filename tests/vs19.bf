# Transformationsregeln: Fehler bei nicht passendem Wert
# Die Variable enthält einen Wert, der nicht auf das
# Transformationsmuster paßt --> FEHLER!

OBJS=A.c B.c C.cc D.c
all: $(OBJS:*.c=*.C)

#SHOULD_FAIL:L14
