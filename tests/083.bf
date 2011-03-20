# Regelauswahl: Fehler, wenn keine eindeutige Regel existiert.
# Die beiden folgenden Regeln haben für "test-abc.o" die gleiche Priorität
# und führen deshalb zu einem Fehler.

test-%.o::
	echo "XxRegel 1"

%-abcd.o::
	echo "XxRegel 2"

all: test-abcd.o

#SHOULD_FAIL:L31
