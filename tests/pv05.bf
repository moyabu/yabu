# Phase1-Variablen: Verwendung von "?:" zum Entfernen von Wörtern.
#


# Eine Liste von Dateinamen
.files = _begin.1 aaa.1 _bbb.1 _abc.1 def.1 _end.1

# Die folgende Transformation entfernt alle Wörter, die mit "_" beginnen
.files2 = $(.files?:_**=) 

# Nochmal das gleiche, aber mit einem Plathalter rechts
.files3 = $(.files?:_*.1*=*2) 


all::
  echo "Xx:$(.files2):"
  echo "Xx:$(.files3):"

# Die entfernten Wörter haben keine Lücken hinterlassen:
#STDOUT:Xx:aaa.1 def.1:
#STDOUT:Xx:aaa.1 def.1:
