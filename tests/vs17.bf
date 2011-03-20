# Kollision zwischen Variablen- und Optionsnamen
#
# Option und gleichnamige Variable

test=abb

!options
    test


all::

#SHOULD_FAIL:L03

