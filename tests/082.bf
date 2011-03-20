# Regelauswahl: Priorität
# Mehrere Regeln mit Skript für ein Target aber mit nicht-maximaler Priorität
# sind erlaubt und erzeugen keinen Fehler.

all: sample.o

# Prio 6
sample.o::
    echo "XxRegel 1"

# Prio 5
%.o::
    echo "XxRegel 1"

# Prio 5
sampl%.o::
    echo "XxRegel 1"


#STDOUT:XxRegel 1
