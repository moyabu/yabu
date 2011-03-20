# Regelauswahl und -priorität
# Unter den Regeln mit Skript whlt Yabu die mit der kleinsten Anzahl
# von %-Platzhaltern aus. Alle anderen Regeln (mit Skript) werden ignoriert.


all: target-x-y tarGET-x-y tARGET-x-y



targ%::
    echo "XxRegel1:$(0)"

tar%-%::
    echo "XxRegel2:$(0)"

t%-%-%::
    echo "XxRegel3:$(0)"


#STDOUT:XxRegel1:target-x-y
#STDOUT:XxRegel2:tarGET-x-y
#STDOUT:XxRegel3:tARGET-x-y

