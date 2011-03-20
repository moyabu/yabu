# Konfigurationen gelten nur für die jeweilige Regel
# 
# In diesem Beispiel werden für das Ziel "target" zwei Regeln benutzt,
# eine ohne Skript und eine mit Skript. Die beiden Regel wählen verschiedene
# Konfigurationen, die aber nur für die jeweilige Regel gelten.
# Insbesondere gilt die ausgewählte Konfiguration NICHT beim Erreichen der
# aufgeführten Quellen!

!options
	cfg0 cfg1 cfg2

# [cfg0] vererbt sich nicht auf target
all:: [cfg0] target

# [cfg1] vererbt sich nicht auf source1
target:: [cfg1] source1

# [cfg2] vererbt sich nicht auf source2, gilt aber im Skript
target:: [cfg2] source2
    echo "Xx:$(0):$(_CONFIGURATION)"

# Die Konfiguration ist hier immer []!
source%::
    echo "Xx:$(0):$(_CONFIGURATION)"


#STDOUT:Xx:source1:
#STDOUT:Xx:source2:
#STDOUT:Xx:target:+cfg2

