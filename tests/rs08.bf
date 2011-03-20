# Regelauswahl: Schleifenerkennung
# Dies ist eine Schleife vom Typ 2: Das Ziel hängt nicht von sich
# selbst ab, die Regeln produzieren jedoch eine unendliche Folge
# von Quellen

a%:: ba%
b%:: ab%

all:: a

#SHOULD_FAIL:L04
