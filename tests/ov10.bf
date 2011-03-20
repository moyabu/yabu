# Automatisch definierte konfigurationsabhängige Variablen 

!options
   cfg1
   platform(linux solaris aix)
   cfg2

# Alle Variablen leer, wenn keine Konfiguration definiert ist.
# -----------------------------------------------------------
test1::
   echo "Xx:_CONFIGURATION=$(_CONFIGURATION)"
   echo "Xx:cfg1=$(_cfg1)"
   echo "Xx:cfg2=$(_cfg2)"
   echo "Xx:platform=$(_platform)"
   echo "Xx:platforms:$(_linux).$(_solaris).$(_aix)"
#STDOUT:Xx:_CONFIGURATION=
#STDOUT:Xx:cfg1=
#STDOUT:Xx:cfg2=
#STDOUT:Xx:platform=
#STDOUT:Xx:platforms:..


# Reihenfolge ist die aus options-Abschnitt, nicht die Reihenfolge
# aus der Konfigurationsauswahl.
# -----------------------------------------------------------
test2:: [+cfg2+cfg1]
   echo "Xx:_CONFIGURATION=$(_CONFIGURATION)"
   echo "Xx:cfg1=$(_cfg1)"
   echo "Xx:cfg2=$(_cfg2)"
#STDOUT:Xx:_CONFIGURATION=+cfg1+cfg2
#STDOUT:Xx:cfg1=+
#STDOUT:Xx:cfg2=+

# Ausgeschaltete Optionen liefern "-" als Wert.
# -----------------------------------------------------------
test3:: [-cfg1]
   echo "Xx:_CONFIGURATION=$(_CONFIGURATION)"
   echo "Xx:cfg1=$(_cfg1)"
#STDOUT:Xx:_CONFIGURATION=-cfg1
#STDOUT:Xx:cfg1=-

# Ist eine Option einer Gruppe eingeschaltet, dann wird nur diese
# in $(_CONFIGURATION) aufgeführt.
# -----------------------------------------------------------
test4:: [+linux-solaris-aix]
    echo "Xx:_CONFIGURATION=$(_CONFIGURATION)"
    echo "Xx:platform=$(_platform)/$(_linux).$(_solaris).$(_aix)"
#STDOUT:Xx:_CONFIGURATION=+linux
#STDOUT:Xx:platform=linux/+.-.-

# Ist keine Option einer Gruppe eingeschaltet, dann werden
# alle ausgeschalteten Optionen in $(_CONFIGURATION) aufgeführt.
# -----------------------------------------------------------
test5:: [-linux-aix]
    echo "Xx:_CONFIGURATION=$(_CONFIGURATION)"
    echo "Xx:platform=$(_platform)/$(_linux).$(_solaris).$(_aix)"
#STDOUT:Xx:_CONFIGURATION=-linux-aix
#STDOUT:Xx:platform=/-..-


all: test1 test2 test3 test4 test5

