# Makroaufruf mit fehlenden Argumenten (Test2)

.define myrule arg1(def1) arg2 arg3(def3)
all::
.enddefine

.myrule 111
#SHOULD_FAIL:L01

