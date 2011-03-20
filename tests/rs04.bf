# Regelauswahl: Verhinderung direkter Rekursion
#
# Eine Regel wird niemals für ihre eigenen Quellen in Betracht gezogen.


all:: a-1

# Wird für a-1, aber nicht für a-2 benutzt
a-1 a-2:: a-2
   echo "Xx:$(0),erste Regel"

# Wird für a-2 benutzt, keine Kollision mit der ersten Regel
a-2::
   echo "Xx:$(0),zweite Regel"


#STDOUT:Xx:a-2,zweite Regel
#STDOUT:Xx:a-1,erste Regel
