# Makros: Makroargumente mehrfach definiert (2. Variante)
#
# Ein Makroargument darf beim Aufruf nicht mehrfach übergeben werden.
# Bei diesem Testfall wird das Argument einmal in der Aufrufzeile und
# einmal als benanntes Argument übergeben.

.define macro arg1 arg2 arg3
.enddefine

.macro 1
  arg2=2
  arg3=3
  arg1=1

#SHOULD_FAIL:L03
