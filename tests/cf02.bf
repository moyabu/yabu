# Statische und zielspezifische Konfiguration
#
# Die mit !configure für ein Ziel bestimmte Konfiguration
# wird zur Startkonfiguration hinzugefügt. In diesem Beispiel
# ist die Startkonfiguration "+bbb", und für das Ziel all
# wird zusätzlich "+aaa" gesetzt.

!options
   aaa bbb

!configure [aaa] all

all::
  echo Xx:$(_CONFIGURATION)

#INVOKE:-c bbb
#STDOUT:Xx:+aaa+bbb
