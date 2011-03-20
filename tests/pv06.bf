# Präprozessor: Verschachtelte Variablennamen
# 

# Name der Variablen enthält eine Variable
.name=var1
.var1=val1
.result1=$(.$(.name))

# Ersetzungeregel enthält Variablen
.var2=aaa.bbb
.pat2=aaa.*
.repl2=AAA.*
.result2=$(.var2:$(.pat2)=$(.repl2))

all::
  echo "Xx:result1=$(.result1)"
  echo "Xx:result2=$(.result2)"

#STDOUT:Xx:result1=val1
#STDOUT:Xx:result2=AAA.bbb
