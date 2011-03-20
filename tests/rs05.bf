# Regelauswahl: Verhinderung direkter Rekursion
#
# Eine Regel wird niemals für ihre eigenen Quellen in Betracht gezogen.


all:: b-1

# Diese Regel wird für b-2 nicht benutzt --> Fehler L33
b-%:: b-2
   echo "Xx:$(0),erste Regel"


#SHOULD_FAIL:L33
