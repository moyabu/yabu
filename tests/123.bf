# Letztes Argument einer Vorlage
# Das letzte formale Argumten nimmt alle restlichen Werte 
# aus dem Aufruf der Vorlage auf.

.define myrule arg0 arg1...
all::
    echo "Xx: $(.arg0)"
    echo "Xx: $(.arg1)"
.enddefine

.myrule 000 111 222 333 444


#STDOUT:Xx: 000
#STDOUT:Xx: 111 222 333 444
