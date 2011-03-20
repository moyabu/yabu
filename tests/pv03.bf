# Erlaubte Zeichen in Namen von Präprozessor-Variablen
#

.azAZöäß_=TEST

all:
   echo "Xx:$(.azAZöäß_)"

#STDOUT:Xx:TEST
