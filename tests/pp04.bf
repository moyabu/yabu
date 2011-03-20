# Fehler L03 mit Schleifenvariable (PP-Variable mehrfach definiert)
# Versuch, die Schleifenvariablein der Schleife neu zu definieren

.foreach a 1 2 3
.a=1
.endforeach

all::

#SHOULD_FAIL:L03
