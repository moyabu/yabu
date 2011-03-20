# Rekursive Makros
#
# Ein Makro, das sich selbst aufruft, verursacht einen Fehler L04


.define a
.b
.enddefine

.define b
.c
.enddefine

.define c
.a
.enddefine

.a

#SHOULD_FAIL:L04

