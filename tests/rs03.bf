# Regelauswahl: Mehrere Regeln für ein Ziel
#
# Yabu bevorzugt Regeln mit vielen übereinstimmenden Zeichen,
# im Zweifelsfalle Regeln mit weniger '%'-Platzhaltern.


# Die zweite Regel hat mehr übereinstimmende Zeichen
%.o::
  echo "Xx:Verlierer1"
gui%x%.o::
  echo "Xx:Gewinner1"

all:: guixxx.o


# Gleiche Anzahl übereistimmender Zeichen, aber die
# erste Regel hat weniger '%'-Platzhalter
x%.o::
	echo "Xx:Gewinner2"
%_%.o::
	echo "Xx:Verlierer2"

all:: x_eins.o



#STDOUT:Xx:Gewinner1
#STDOUT:Xx:Gewinner2
