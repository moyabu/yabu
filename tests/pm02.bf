# Makros: Variable Anzahl von Argumenten mit "arg..."
# Ist das letzte formale Argument als NAME... deklariert, dann
# nimmt es beim Aufruf des Makros alle restlichen Argument auf.
# Letztes Makroargument

.define macro a b...
all::
  echo Xx$(.a)
  echo Xx$(.b)
.enddefine

.macro 1 2 3 4 5

#STDOUT:Xx1
#STDOUT:Xx2 3 4 5
