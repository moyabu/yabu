# Regelauswahl: Priorit�t
# Mehrere Regeln mit Skript f�r ein Target aber mit nicht-maximaler Priorit�t
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
