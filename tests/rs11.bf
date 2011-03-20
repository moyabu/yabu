# Leerzeichen innerhalb von Konfigurationen sind erlaubt
!options
  a b

!configure x
  x: a

all:: [ a +b ]
   echo Xx:Ok

#STDOUT:Xx:Ok
