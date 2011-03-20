# Verwendung von "" in Variablendefinitionen
#

A = "a b c"
A += "  "x"  "
A ?= "y "

all::
  echo 'Xx:$(A):'

#STDOUT:Xx:a b c   "x"   y :
