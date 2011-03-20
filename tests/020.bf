# Verwendung von $(0)...$(9) und $(*)

# Dummy-Targets für die folgenden Tests
tgt%::

# Einfaches Beispiel mit allen Variablen
all:: tgt1 tgt2 tgt3 tgt4 tgt5 tgt6 tgt7 tgt8 tgt9 tgt10
    echo "Xx: $(0)-$(1)-$(2)-$(3)-$(4)-$(5)-$(6)-$(7)-$(8)-$(9)"
    echo "Xx: $(*)"
#STDOUT:Xx: all-tgt1-tgt2-tgt3-tgt4-tgt5-tgt6-tgt7-tgt8-tgt9
#STDOUT:Xx: tgt1 tgt2 tgt3 tgt4 tgt5 tgt6 tgt7 tgt8 tgt9 tgt10


