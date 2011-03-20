# Mehrzeilige Shellscripte
# Ein Skript kann mit '{' ... '}' (jeweils allein in einer Zeile!)
# gruppiert werden. Alle Zeilen einer Gruppe werden als ein Kommando
# an die Shell übergeben

all:
  {
    print() {
      echo "Xx:$1"
    }
    print a
    print b
  }

#STDOUT:Xx:a
#STDOUT:Xx:b
