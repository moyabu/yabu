# Definition von Konfiguration in mehreren Schritten.
#
# Konfigurationen können schrittweise definiert werden, d.h. zu einer
# Konfiguration gibt es mehrere .configuration-Abschnitte.
#
# Das Beispiel zeigt zugleich, daß eine Optionsauswahl %-Platzhalter
# enthalten kann.


!options
	cfg1 cfg2

VAR1=
VAR2=

# Konfiguration 1, erster Teil
!configuration cfg1
    VAR1=cfg1-var1

# Konfiguration 2
!configuration cfg2
    VAR1=xxx
    VAR2=yyy

# Rest von Konfiguration 1
!configuration cfg1
    VAR2=cfg1-var2

test%:: [cfg%]
    echo "Xx: VAR1=$(VAR1)"
    echo "Xx: VAR2=$(VAR2)"

all: test1 test2


#STDOUT:Xx: VAR1=cfg1-var1
#STDOUT:Xx: VAR2=cfg1-var2
#STDOUT:Xx: VAR1=xxx
#STDOUT:Xx: VAR2=yyy
