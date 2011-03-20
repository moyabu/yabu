# for-Anweisung: Schleifenvariable ist lokal

# Definiere die Variable «a»
.a=99

# Die Schleifenvariable ist unabhängig davon
.foreach a 1 2
all:: echo-$(.a)
.endforeach

# Hier gilt wieder .a=99
all:: echo-$(.a)

echo-1 echo-2 echo-99::
	echo "Xx:$(0)"

#STDOUT:Xx:echo-1
#STDOUT:Xx:echo-2
#STDOUT:Xx:echo-99
