# Verwendung von $(12)
# Set Version 1.10 ist $(n) nicht mehr auf einstellige Argument beschränkt

all:: t1 t2 t3 t4 t5 t6 t7 t8 t9 t10 t11 t12
  echo "Xx:$(12)"

t%::

#STDOUT:Xx:t12

