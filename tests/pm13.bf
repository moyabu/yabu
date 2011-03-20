# Defaultwerte für Makroargumente

.define myrule arg1(def1) arg2 arg3(def3)
all::
   echo Xx:$(.arg1)/$(.arg2)/$(.arg3)
.enddefine

.myrule
    arg2=22

#STDOUT:Xx:def1/22/def3

