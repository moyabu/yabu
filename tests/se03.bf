# !export Syntaxvarianten

VAR1=var1
VAR2=var2

!export VAR1
   VAR2=var2neu
   $YABU_EXPORT_TEST

all::
   echo "Xx:VAR1=$VAR1"
   echo "Xx:VAR2=$VAR2"
   echo "Xx:YABU_EXPORT_TEST=$YABU_EXPORT_TEST"

#STDOUT:Xx:VAR1=var1
#STDOUT:Xx:VAR2=var2neu
#STDOUT:Xx:YABU_EXPORT_TEST=yabu

