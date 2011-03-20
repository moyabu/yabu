# Optionale Zuweisung überstimmt globale Zuweisungen.
#
# Ist einer Variablen in einer Konfiguration ein Wert zugewiesen,
# dann werden  -- in dieser Konfiguration -- alle globalen Zuweisungen
# (= und +=) unwirksam.
#

!options
	test

# VAR1 wird explizit gesetzt -> Optionale Zuweisung ist unwirksam
VAR1=WERT1
VAR1[test]=OPT
VAR1+=WERT2

all:: global cfg

global::
   echo "Xx:$(0):VAR1=$(VAR1)"

cfg:: [test]
   echo "Xx:$(0):VAR1=$(VAR1)"

#STDOUT:Xx:global:VAR1=WERT1 WERT2
#STDOUT:Xx:cfg:VAR1=OPT
