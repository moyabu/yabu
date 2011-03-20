# Präprozessor $(.glob) mit Filter.
# 

# $(.glob:PATTERN) liefert alle Dateien, die zu PATTERN passen
# $(.glob:PATTERN=) liefert nichts

all:
    echo 'Xx:$(.glob:yabu.h)'
    echo 'Xx:$(.glob:yabu.h=)'

#STDOUT:Xx:yabu.h
#STDOUT:Xx:
