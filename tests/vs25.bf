# Verwendung von "?:" zum Entfernen von Wörtern.
#


# Eine Liste von Dateinamen
FILES = _begin.1 aaa.1 _bbb.1 _abc.1 def.1 _end.1

# Die folgende Transformation entfernt alle Wörter, die mit "_" beginnen
FILES2 = $(FILES?:_**=) 

# Nochmal das gleiche, aber mit einem Platzhalter auf der rechten Seite
# (der den Wert "" annimmt)
FILES3 = $(FILES?:_*.1*=*2) 


all::
  echo "Xx:$(FILES2):"
  echo "Xx:$(FILES3):"

# Die entfernten Wörter haben keine Lücken hinterlassen:
#STDOUT:Xx:aaa.1 def.1:
#STDOUT:Xx:aaa.1 def.1:
