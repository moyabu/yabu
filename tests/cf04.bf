# !configure: Syntaxfehler

!options
    aaa

# Fehler S01 hier:
!configure x y
    a: aaa

all::

#SHOULD_FAIL:S01
