# Phase1-Variablen: sofortige Ersetzung.
#
# Im Gegensatz zu Phase2-Variablen werden Phase1-Variablen sofort ersetzt.
# Eine Variable muﬂ deshalb vor ihrer ersten Verwendung definiert werden.


all::
  echo Xx$(.var)

.var=NAME

#SHOULD_FAIL:L01
