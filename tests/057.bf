# Erweitern von Variablen mit "+="

A=

# tut nichts
A+=

# tut auch nichts
A+=""

# Zwei Zeichen anhängen. Anführungszeichen haben keine besondere Bedeutung.
A+=""""

# Beim Anhängen weiterer Fragmente jeweils ein Leerzeichen einfügen
A+=test

all::
  echo 'Xx<$(A)>'

#STDOUT:Xx<"" test>
