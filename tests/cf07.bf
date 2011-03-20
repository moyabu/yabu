# Fehler bei widersprüchlichen !configure-Anweisungen

!options
    aaa bbb

!configure x
    x: aaa+bbb

!configure x
    x: aaa-bbb

all::
    echo "Xx:$(_CONFIGURATION):"

#SHOULD_FAIL:L24
