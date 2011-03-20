# Regelauswahl: Mehrdeutige Regeln
# Für jedes Target darf höchstens eine Regel mit Regel mit Skript
# und höchster Priorität existieren. Sonst: Fehler.

%.o: %.c
	echo "Regel 1"

%.o: %.cc
	echo "Regel 2"

all: test.o


#SHOULD_FAIL:L31
