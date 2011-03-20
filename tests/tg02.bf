# Zielgruppen mit zus‰tzlichen Abh‰ngigkeiten
# Normalerweise werden die Ziele in einer Gruppe in der Reihenfolge
# abgearbeitet, in der sie auftreten. Durch zus‰tziche Abh‰ngigkeiten
# zwischen den Mitgliedern der Gruppe kann sich die Reihenfolge aber
# ‰ndern.


all:: aa bb cc dd ee

# Bewirkt daﬂ alle 5 Ziele in einer Gruppe sind
aa bb cc dd ee::
    echo "Xx:$(0)"

# Folgenden Regeln ‰ndern die Reihenfolge in "cc bb aa dd ee"
aa:: bb
bb:: cc
 
#STDOUT:Xx:cc
#STDOUT:Xx:bb
#STDOUT:Xx:aa
#STDOUT:Xx:dd
#STDOUT:Xx:ee
