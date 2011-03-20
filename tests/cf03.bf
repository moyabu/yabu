# Verwendung von !configure mit Skript

!options
    aaa

!configure
    echo aaa

all::
    echo Xx:$(_CONFIGURATION)

#STDOUT:Xx:+aaa
