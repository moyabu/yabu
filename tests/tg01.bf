# Gruppierung von Zielen
# Innerhalb einer Gruppe wird das Build-Skript nur einmal ausgeführt.

SRC=test.$(_TIMESTAMP)

all:: t1 t2 t3 t4 t5
   cat script.log
   rm -f script.log t1 t2 t3 t4 t5 $(SRC)

# Definierten Anfangszustand herstellen
$(SRC):
   rm -f script.log t1 t2 t3 t4 t5
   touch $(0)

# Das Skript darf nur einmal ausgeführt werden!
t1 t2 t3 t4 t5: $(SRC)
   touch t1 t2 t3 t4 t5
   echo "Xx:Ziel=$(0)" >>script.log

#STDOUT:Xx:Ziel=t1
