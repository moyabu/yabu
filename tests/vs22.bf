# Transformationsregeln ohne rechte Seite.
# $(VARIABLE:LS) ist gleichbedeutend mit $(VARIABLE:LS=LS).

AAA=a1 b2 c3 aa4
all::
   echo "Xx:<$(AAA!:a*)>"


#STDOUT:Xx:<a1 aa4>
