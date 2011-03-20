# Fehler: SIGSEGV beim 2. Versuch eine existierende Datei zu erreichen
#
all: cleanup

cleanup:: a b
  rm -f a b xxx

a b: xxx
   touch $(0)

xxx:: create-xxx

create-xxx::
   touch xxx


