# Verwendung von !ALWAYS

.test1=test1-$(.TIMESTAMP)
.test2=test2-$(.TIMESTAMP)

all:: $(.test1)
   rm -f $(.test1) $(.test2)

# Regel wird wegen !ALWAYS ausgeführt, obwohl test1 neuer als test2 ist (siehe folgende Regel)
$(.test1): $(.test2) !ALWAYS
   echo "Xx:Ok"

# Erzeuge test1 mit späterem Zeitstempel als test2
$(.test2):
   touch $(.test2)
   touch $(.test1)

#STDOUT:Xx:Ok
