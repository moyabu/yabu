# ":?"-Regeln: Fehler, falls das ziel existiert
#
# In diesem Testfall wird eine ":?"-Regel auf ein bereits existierendes
# Ziel angewendet. Das muß zu einem Fehler führen.

all:: test-131.tgt
  rm -f test-131.tgt test-131.src


test-131.tgt :? test-131.src
  cp $(1) $(0)

test-131.src : !ALWAYS
  touch -t 200001011213 test-131.tgt
  touch test-131.src

  
#SHOULD_FAIL:G32
