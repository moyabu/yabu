# Makroaufruf mit fehlenden Argumenten

.define proc a b c
.enddefine

.proc x y

all::

#SHOULD_FAIL:L01
