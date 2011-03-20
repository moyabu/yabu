# Variablenersetzung: Rekursion im Variablennamen

SELECTOR=x
VALUE_x=Wertx

all::
  echo "Xx:$(VALUE_$(SELECTOR))"

#STDOUT:Xx:Wertx
