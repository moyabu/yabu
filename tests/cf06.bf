# Fehler: !configure-Syntax

!options
    aaa

!configure x
    x: aaa
    error

all::

#SHOULD_FAIL:S01
