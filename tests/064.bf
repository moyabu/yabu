# Variablen in Transformationsregeln
#
# Innerhalb einer Transformationsregel k�nnen beliebige Variablen benutzt werden.

DIR=/usr/local
FILES=a b c
all:
  echo "Xx:$(FILES:*=$(DIR)/*)"

#STDOUT:Xx:/usr/local/a /usr/local/b /usr/local/c
