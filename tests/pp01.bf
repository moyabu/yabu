# for-Anweisung: Schleifenvariable ist lokal

.foreach a 1 2
all:: echo-$(.a)
.endforeach

# Zweite Schleife mit gleicher Variablen

.foreach a 3 4
all:: echo-$(.a)
.endforeach

echo-%::
	echo "Xx:%"

#STDOUT:Xx:1
#STDOUT:Xx:2
#STDOUT:Xx:3
#STDOUT:Xx:4
