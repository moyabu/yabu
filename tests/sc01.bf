# Mehrzeilige Shellscripte mit '|'

all:
  |for x in 1 2; do
  |  echo "Xx:$x"
  |done

#STDOUT:Xx:1
#STDOUT:Xx:2
