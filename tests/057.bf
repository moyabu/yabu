# Erweitern von Variablen mit "+="

A=

# tut nichts
A+=

# tut auch nichts
A+=""

# Zwei Zeichen anh�ngen. Anf�hrungszeichen haben keine besondere Bedeutung.
A+=""""

# Beim Anh�ngen weiterer Fragmente jeweils ein Leerzeichen einf�gen
A+=test

all::
  echo 'Xx<$(A)>'

#STDOUT:Xx<"" test>
