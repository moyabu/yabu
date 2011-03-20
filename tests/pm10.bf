# Makros: zwei formale Argumente gleichen Namens (Fehler)

.define error a b c a
.enddefine

all:
   echo "ok"

#SHOULD_FAIL:L03

