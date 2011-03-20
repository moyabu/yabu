# Benannte Makroargumente
#
# Mischung mit normalen Argumenten.
# Reihenfolge der benannten Argumente ist nicht relevant.

.define macro a b c d
all:
  echo Xx$(.a)
  echo Xx$(.b)
  echo Xx$(.c)
  echo Xx$(.d)
.enddefine

.macro 1 2
  d=4
  c=3

#STDOUT:Xx1
#STDOUT:Xx2
#STDOUT:Xx3
#STDOUT:Xx4
