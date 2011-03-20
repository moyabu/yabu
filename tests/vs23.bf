# Explizite Angabe von '*' als Platzhalterzeichen ist nicht erlaubt
# 
# '*' ist der Standard-Platzhalter. Es ist nicht zulässig, '*' explizit
# als Platzhalter anzugeben. Das '*' wird vielmehr als Teil des Variablennamens
# interpretiert. Grund: konsistente Behandlung von $(*) und "normalen" Variablen.

VAR=TEST

all:: 
  echo "Xx:$(VAR*:*=*)"

t%::

#SHOULD_FAIL:L01

