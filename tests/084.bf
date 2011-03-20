# Regelauswahl: Variablen im Ziel werden ersetzt
# Enthält die linke Seite einer Regel eine Variablenreferenz, dann wird diese
# ersetzt bevor die Regel auf ihre Tauglichkeit für ein bestimmtes Ziel
# untersucht wird.

all: $(TARGETS)

$(TARGETS)::
    echo "Xx:$(0)"

TARGETS=aa bb cc

#STDOUT:Xx:aa
#STDOUT:Xx:bb
#STDOUT:Xx:cc
