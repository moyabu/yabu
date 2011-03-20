# Phase1-Variablen.
#
# Einfacher Testfall für eine Variable in Phase1.

.var=NAME

all::
  echo Xx$(.var)

#STDOUT:XxNAME
