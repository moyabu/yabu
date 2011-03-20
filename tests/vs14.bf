# Variablen: Undefinierter Teilstring *N im Muster.
# 
# Platzhalter können bereits im Muster referenziert werden,
# allerdings nur nach ihrer Definition. In diesem Testfall
# wird '*2' im Muster benutzt, aber es ist nur ein Platzhalter
# definiert.
#

A=a-a-c

all: $(A:*-*1-*2=*.*2)

#SHOULD_FAIL:L01
