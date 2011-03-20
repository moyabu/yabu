# ":?"-Regeln: Fehler, falls kein Skript vorhanden.
# In diesem Testfall wird versucht, eine ":?"-Regel ohne Skript zu definieren.
# Das ist unzulässig und muß zu einem Fehler führen.


all:: target2

target2 :? /etc/hosts

target2 :
   touch $(0)


#SHOULD_FAIL:S04

