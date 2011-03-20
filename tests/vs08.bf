# Rekursion in Ersetzungsregeln
# In einer Variablenreferenz mit Ersetzungsregel -- $(VAR:LINKS=RECHTS) --
# darf sowohl LINKS als auch RECHTS Referenzen auf $(VAR) enthalten.
#
A=abc
B=$(A:$(A)=$(A:abc=xyz))

all:
    echo "Xx:$(B)"

#STDOUT:Xx:xyz
