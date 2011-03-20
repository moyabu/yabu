# !configure (switch-Moduls): die erste passende Konfiguration gilt
#
# Passen mehrere Muster in einer Konfigurations-Auswahlliste
# auf den Selektor, dann wählt Yabu die erste passende Konfiguration.

!options
	opt1 opt2


!configure abc
	%c: opt1
	abc: opt2

all::
	echo "Xx:$(_opt1).$(_opt2)"

#STDOUT:Xx:+.
