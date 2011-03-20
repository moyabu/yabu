# Verhalten von Zielgruppen im Fehlerfalle
#
# Wird ein Mitglied der Gruppe ausgelassen, dann versucht Yabu
# trotzdem, die übrigen Ziele in der Gruppe zu erreichen.
# Tritt dagegen ein Fehler auf, dann werden alle übrigen Ziele
# in der Gruppe ausgelassen.
#
# In diesem Testfall sind 5 Ziele (aa..ee) in einer Gruppe.
# aa wird ausgelassen, und beim Versuch bb zu erreichen tritt
# ein Fehler auf.


all:: aa bb cc dd ee
aa bb cc dd ee::
    echo "Xx:$(0)"
    false


# Erzwinge einen Fehler in der Quelle von "aa"
# Dadurch wird "aa" ausgelassen
aa:: xxx

xxx::
   echo "Xx:$(0)"
   false

#SHOULD_FAIL:Xx
#STDOUT:Xx:xxx
#STDOUT:Xx:bb

