# Variablen: Auswertung von Optionen als Variablen
#
# Eine Auswahloption X(x1 x2 x3) definiert zugleich die Variablen _X, _x1, _x2, _x3.
# _X hat den Wert _x1, _x2, _x3 (wenn eine Option ausgewählt ist) und ist ansonsten undefiniert.
# _x1 hat den Wert _x1 (wenn X=x1 ausgewählt ist) und andernfalls "".

!options
	opt(aaa bbb ccc) other

test-%:: [%]
  echo "Xx:$(0):aaa=$(_aaa)"
  echo "Xx:$(0):bbb=$(_bbb)"
  echo "Xx:$(0):ccc=$(_ccc)"
  echo "Xx:$(0):opt=$(_opt)"


all: test-aaa test-bbb test-ccc
  echo "Xx:ohne:aaa=$(_aaa)"
  echo "Xx:ohne:bbb=$(_bbb)"
  echo "Xx:ohne:ccc=$(_ccc)"

#STDOUT:Xx:test-aaa:aaa=+
#STDOUT:Xx:test-aaa:bbb=-
#STDOUT:Xx:test-aaa:ccc=-
#STDOUT:Xx:test-aaa:opt=aaa
#STDOUT:Xx:test-bbb:aaa=-
#STDOUT:Xx:test-bbb:bbb=+
#STDOUT:Xx:test-bbb:ccc=-
#STDOUT:Xx:test-bbb:opt=bbb
#STDOUT:Xx:test-ccc:aaa=-
#STDOUT:Xx:test-ccc:bbb=-
#STDOUT:Xx:test-ccc:ccc=+
#STDOUT:Xx:test-ccc:opt=ccc
#STDOUT:Xx:ohne:aaa=
#STDOUT:Xx:ohne:bbb=
#STDOUT:Xx:ohne:ccc=
