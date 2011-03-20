# Regelauswahl: Mehrere Regeln für ein Ziel
#
# Wenn für ein Ziel mehrere Regeln mit Skript existieren,
# wird genau eine davon ausgewählt, und die übrigen werden
# ignoriert. Sie tragen insbesondere NICHT zur Liste der Quellen bei.
#
# Die Reihenfolge, in der die Quellen geprüft werden, ist wie folgt:
# - Zuerst alle Quellen aus Regeln ohne Skript (in der Reihenfolge des Buildfiles)
# - Dann die Quellen der ausgewählten Regel

# Benutze das Ziel "target.1" für diesen Testfall
all: target.1


# Dies ist die "beste" Regel, sie wird von Yabu ausgewählt:
target.1:: src1
   echo "Xx:Ausgewaehlte Regel"

# Diese Regel enthält einen Platzhalter und hat deshalb niedrigere
# Priorität. Sie wird komplett ignoriert, und auch die Quelle src2
# wird niemals geprüft.
target.%:: src2 src3 src4
   echo "Xx:Zweite Regel"

# Diese Regel hat kein Skript und trägt deshalb zur Liste der
# Quellen bei. Die Anzahl der Platzhalter ist dabei nicht relvant.
tar%.%:: src%1.%2


# Ausgabe der ausgewählten Quellen
src%%::
   echo "Xx:Source-$(0)"



#STDOUT:Xx:Source-srcget.1
#STDOUT:Xx:Source-src1
#STDOUT:Xx:Ausgewaehlte Regel
