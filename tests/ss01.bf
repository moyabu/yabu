# Die (früher erlaubte) Verwendung von '%' an Stelle von '%1'
# in Regeln mit mehreren Platzhaltern ist nicht mehr erlaubt.

all: test.1.2

test.%.%::
   echo "ok:%1"
   echo "ok:%2"
   echo "fehler:%"

#SHOULD_FAIL:S03

