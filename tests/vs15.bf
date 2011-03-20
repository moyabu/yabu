# Transformationsregeln: Auflösung von Mehrdeutigkeiten
# Ein *-Platzhalter wird duch einen möglichst langen Teilstring ersetzt.
# Weiter links stehende Platzhalter haben dabei höhere Priorität

INPUT=aaa-bbb-ccc-ddd-eee-fff-ggg

all:
	echo "Xx:$(INPUT:*-*-*=*1/*2/*3)"

#STDOUT:Xx:aaa-bbb-ccc-ddd-eee/fff/ggg

