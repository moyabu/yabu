# Skript-Environment: Export von Yabu-Variablen
# Verschaltelte Variablen werden vollständig ersetzt
#
!options
   klein gross

!export VAR2

VAR1=eins
VAR1[gross]=EINS
VAR2=$(VAR1)/zwei


all:: echo.klein echo.gross

echo.%:: [%]
  echo "Xx:$VAR2"

#STDOUT:Xx:eins/zwei
#STDOUT:Xx:EINS/zwei

