# Prototyp-Regeln: %% und Variablensubstitution

OBJS=Aa Bb Cc
X=x

# Normale Regel (kein Prototyp): '%' ist ein normales Textzeichen.
# In der Transformationsregel können wiederum Variablen enthalten sein (hier $(X))
simple::
    echo "Xx: Prozentzeichen %"
    echo "Xx: Variablen: $(OBJS:*=$(X)(*))"
#STDOUT:Xx: Prozentzeichen %
#STDOUT:Xx: Variablen: x(Aa) x(Bb) x(Cc)

# Prototypregel: '%%' steht für '%' (ohne Platzhalterfunktion).
# Diese Regel enthält einen '%'-Platzhalter im Target, den wir in der
# Transformationsregel verwenden. Man beachte die Interpretation von '%%%'
# als '%%' (Prozentzeichen), gefolgt von %1. Die zweite Zeile zeigt, wie
# man die umgekehrte Interpretation -- %1 gefolgt von Prozentzeichen -- erzwingt.
complex-%::
    echo "Xx: Prozentzeichen %%"
    echo "Xx: $(0): $(OBJS:*=%%%:*)"
    echo "Xx: $(0): $(OBJS:*=%1%%:*)"
#STDOUT:Xx: Prozentzeichen %
#STDOUT:Xx: complex-XYZ: %XYZ:Aa %XYZ:Bb %XYZ:Cc
#STDOUT:Xx: complex-XYZ: XYZ%:Aa XYZ%:Bb XYZ%:Cc

all: simple complex-XYZ

