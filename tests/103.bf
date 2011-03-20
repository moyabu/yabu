# Spezielle Variable $(_CONFIGURATION)

!options
	opt1 opt2 opt3

a:: [opt1]
   echo "Xx:$(0):$(_CONFIGURATION)"

b:: [opt1-opt2]
   echo "Xx:$(0):$(_CONFIGURATION)"

c:: [opt1-opt2+opt3]
   echo "Xx:$(0):$(_CONFIGURATION)"

all:: a b c
   echo "Xx:$(0):$(_CONFIGURATION)"

#STDOUT:Xx:a:+opt1
#STDOUT:Xx:b:+opt1-opt2
#STDOUT:Xx:c:+opt1-opt2+opt3
#STDOUT:Xx:all:

