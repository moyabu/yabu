# Variablen: Auswertung von Optionen als Variablen
#
# Eine boolsche Option xyz definiert zugleich eine Variable mit gleichem Namen.
# Ihr Wert ist "+", "-" oder ""

!options
	beispiel


aktiv:: [beispiel]
	echo "Xx:$(0):$(_beispiel)"

inaktiv:: [-beispiel]
	echo "Xx:$(0):$(_beispiel)"

undef::
	echo "Xx:$(0):$(_beispiel)"

all: aktiv inaktiv undef

#STDOUT:Xx:aktiv:+
#STDOUT:Xx:inaktiv:-
#STDOUT:Xx:undef:

