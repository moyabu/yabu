# Präprozessor: Verschachtelte Variablen
#
# Variablenname, linke und rechte Seiter der Transformationsregel
# können Variablen enthalten.


.a=11 22 33
.b=a
.c=*

# Variablenname enthält eine Variable
.v1=$(.$(.b))

# Linke Seite der Transformation enthält eine Variable
.v2=$(.a:$(.c)=[*])

# Rechte Seite der Transformation enthält eine Variable
.v3=$(.a:*=$(.b)-*)


all::
  echo Xx:$(.v1)
  echo Xx:$(.v2)
  echo Xx:$(.v3)

#STDOUT:Xx:11 22 33
#STDOUT:Xx:[11] [22] [33]
#STDOUT:Xx:a-11 a-22 a-33
