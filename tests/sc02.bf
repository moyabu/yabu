# '|'-Syntax: mehrteilige Skripte

# Das Skript in der folgenden Regel besteht aus zwei Teilen, die
# separat ausgeführt werden. Die Definition von PREFIX aus dem
# ersten Teil ist im zweiten Teil nicht mehr gültig.

all:
  |PREFIX=z
  |for x in 1 2; do
  |  echo "Xx:$PREFIX$x"
  |done
  
  |for x in a b; do
  |  echo "Xx:$PREFIX$x"
  |done


#STDOUT:Xx:z1
#STDOUT:Xx:z2
#STDOUT:Xx:a
#STDOUT:Xx:b
