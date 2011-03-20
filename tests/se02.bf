# Skript-Environment: Export von Environment-Variablen
#
# Während der Testauführung ist YABU_EXPORT_TEST=yabu.

YABU_EXPORT_TEST=Dieser Wert ist egal

# Dies exportiert die Variable aus dem Environment,
# n i c h t  die oben definiert Yabu-Variable!
!export $YABU_EXPORT_TEST

all::
  echo "Xx:$YABU_EXPORT_TEST"

#STDOUT:Xx:yabu

