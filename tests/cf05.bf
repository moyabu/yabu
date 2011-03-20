# Mehrere !configure-Anweisungen

!options
    aaa bbb

!configure x
    x: aaa

!configure x
    x: bbb

all::
    echo "Xx:$(_CONFIGURATION):"

#STDOUT:Xx:+aaa+bbb:
