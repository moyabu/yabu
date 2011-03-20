# Fehler: $(3) nicht definiert

tgt%::

# Verwendung einer undefinierten Variablen
all: tgt1 tgt2
    echo "$(3)"

#SHOULD_FAIL:L01
