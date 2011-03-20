# Makro und Variable mit gleichem Namen
#
# Ein Makro kann den gleichen Namen wie eine Variable haben.
# Dann (und nur dann!) wird ".var =xxx" als Makroaufruf und nicht
# als Zuweisung behandelt.

.macro=123
.define macro a
echo::
  echo Xx$(.a)
.enddefine

# Makroaufruf
.macro =123


# Zuweisung
.var   =456
all: echo
   echo Xx$(.var)

#STDOUT:Xx=123
#STDOUT:Xx456

