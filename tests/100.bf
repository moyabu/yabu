# Mehrzeilige Shellscripte
# Ein Skript kann mit '{' ... '}' (jeweils am Ende einer Zeile!)
# gruppiert werden. Alle Zeilen einer Gruppe werden als ein Kommando
# an die Shell übergeben

all:
    {
    TESTVAR=}123{
    echo Xx$TESTVAR
    TESTVAR={ABC}
    echo Xx$TESTVAR
    }

#STDOUT:Xx}123{
#STDOUT:Xx{ABC}
