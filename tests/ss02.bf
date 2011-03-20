# Die (früher erlaubte) Verwendung von '*' an Stelle von '*1'
# in Transformationen Regeln mit mehreren Platzhaltern ist
# # nicht mehr erlaubt.

A=1.x 2.y 3.z
B=$(A:*.*=*.*2)

all:
  echo $(B)

#SHOULD_FAIL:S03

