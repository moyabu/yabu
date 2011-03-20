# Regelauswahl mit Konfigurationen
#
# Eine Regel, die nicht zur aktuellen Konfiguration paßt,
# geht nicht in die Auswahl ein.


# Definiere zwei Optionen die sich gegenseitig ausschließen
!options
   grp(a b)

# Wähle die Konfiguration "+a-b" aus
!configure x
   x: a

all:: [a] t-1


# Diese Regel wäre die beste, paßt aber nicht zur Konfiguration
t-1:: [b]
    echo "FEHLER"


# Diese Regel wird benutzt
%-1:: [a]
    echo "Xx:Ok"

# Diese Regel würde mit der vorigen kollidieren, wird aber ebenfalls ignoriert
%-1:: [b]
    echo "Xx:Nicht ok"


#STDOUT:Xx:Ok
