# Defaultwerte für Makroargumente: Verwendung von '()', '{}' und '[]'

.define myrule arg1(][}{) arg2[)(}{] arg3{)(][}
all::
   echo 'Xx:$(.arg1)/$(.arg2)/$(.arg3)'
.enddefine

.myrule

#STDOUT:Xx:][}{/)(}{/)(][

