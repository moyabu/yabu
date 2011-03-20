# Transformationsregeln: Verwendung von ** auf der rechten Seite

INPUT=A B C

# Die Zeichenfolge '**' auf der rechten Seite einer Transformation
# zählt als einfaches '*' ohne Platzhalterfunktion.
eins::
  echo 'Xx:$(INPUT:*=**)'
#STDOUT:Xx:* * *

# '***' gilt als '**', gefolgt von einem '*'.
zwei::
  echo 'Xx:$(INPUT:*=***)'
#STDOUT:Xx:*A *B *C


# Um '***' als '*', gefolgt von '**' zu interpretieren,
# schreibt man '*1'.
drei::
  echo 'Xx:$(INPUT:*=*1**)'
#STDOUT:Xx:A* B* C*


all: eins zwei drei
