# Makros: Falsche Anzahl von Argumenten.
#
# Ein Makro, welches ohne Argumente definert ist, kann nicht
# mit Argumenten aufgerufen werden.

.define macro
.enddefine

.macro arg

#SHOULD_FAIL:L09
