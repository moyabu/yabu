# Präprozessor: Verschachtelte Variablennamen in $(.glob)


# (.glob) mit variablem Argument
.pattern=Buildfile
Y=$(.glob:$(.pattern))


# Noch einmal, diesmal in einem Makro
.define macro arg
X=$(.glob:$(.arg))
.enddefine
.macro Buildfile

all::
  echo "Xx:X=$(X)"
  echo "Xx:Y=$(Y)"

#STDOUT:Xx:X=Buildfile
#STDOUT:Xx:Y=Buildfile
