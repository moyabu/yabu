# Variablenersetzung: Schleifenerkennung
# Eine Variable, die sich (direkt oder indirekt) selbst referenziert,
# liefert bei der Ersetzung einen Fehler L04.

A=$(B)
B=$(C)    
C=$(A)    

all:
  echo $(A)

#SHOULD_FAIL:L04
