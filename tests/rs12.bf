# Regelauswahl: Mehrdeutige Regeln
# F�r jedes Target darf h�chstens eine Regel mit Regel mit Skript
# und h�chster Priorit�t existieren. Sonst: Fehler.

%.o: %.c
	echo "Regel 1"

%.o: %.cc
	echo "Regel 2"

all: test.o


#SHOULD_FAIL:L31
