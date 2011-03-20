# Dynamische Variablen im linken Teil einer Regel
#
# Variablen auf der linken Seite einer Regel werden gem‰ﬂ der Startkonfiguration
# und (ggf.) der per !configure gew‰hlten, zielspezifischen Konfiguration ersetzt.
# Die Konfigurationsauswahl in der Regel selbst gilt dabei nicht!

!options
     abc xyz static

PREFIX=
PREFIX[static+abc]+=ABC_
PREFIX[xyz]+=XYZ_

# Die Startkonfiguration ist [static]
!configure x
    x: static

# Unser Testziel:
all: ABC_123.test

# Hiermit wird [abc] f¸r das Testziel ausgew‰hlt, zusammen also [abc+static]
!configure [abc] %.test

# Deshalb ist hier PREFIX=ABC_, so daﬂ die Regel paﬂt.
$(PREFIX)%.test:: [xyz]
	echo "Xx:$(_CONFIGURATION):%"

#STDOUT:Xx:+abc+xyz+static:123
