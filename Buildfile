##### Configuration ############################################################

!options
  os(other SunOS)

!configure $(_SYSTEM)
   SunOS: SunOS
   %: other

# Name of your C++ compiler
CC=g++ 
#CC=c++
#CC=CC
#CC=/usr/sfw/bin/g++
#CC=/opt/SUNWspro/bin/CC

# Compile options
CFLAGS=-Wall -Werror -g -pedantic
#CFLAGS=-Wall -Werror -g -pg
#CFLAGS=-Wall -Werror -g --coverage
#CFLAGS=-Wall -Werror -O6

# Link options
LFLAGS=
#LFLAGS=-pg
#LFLAGS=--coverage
LFLAGS[SunOS] += -lsocket -lnsl


##### End Of Configuration #####################################################

!settings
    auto_mkdir=no
    echo_after_error=no

all: yabu yabusrv tests

clean:: clean-tests !CLEAN_STATE
    rm -f yabu
    rm -f *.o core *.gcda *.gcno *.cc.gcov
    rm -f yabu.1 yabusrv.1 Buildfile.export

# ------------------------------------------------------------------------------
# Yabu
# ------------------------------------------------------------------------------

SRCS=yaprj.cc yaar.cc yauth.cc yabu.cc yacomm.cc yadir.cc yapp.cc yabu3.cc \
     yacfg.cc yadep.cc yajob.cc yamap.cc yamsg.cc yapoll.cc yapref.cc yaread.cc \
     yarule.cc yastate.cc yasrv.cc yastr.cc yasys.cc yatgt.cc yavar.cc

yabu: $(SRCS:*.cc=*.o)
    rm -f $(0)
    $(CC) $(LFLAGS) -o $(0) $(*)
    
yabusrv: yabu
    rm -f $(0)
    ln $(1) $(0)

%.o: %.cc yabu.h
    $(CC) $(CFLAGS) -o $(0) -c $(1)

# ------------------------------------------------------------------------------
# Tests
# ------------------------------------------------------------------------------

TESTS=$(.glob:tests/*.bf=*)

tests:: $(TESTS:*=tests/*.done)

clean-tests::
  rm -f tests/*.output.actual tests/*.output.expected tests/*.stdout tests/*.stderr
  rm -f tests/*.done tests/*.bf.state

tests/%.done: tests/%.bf yabu
 {
    Abort()
    {
        echo "ERROR IN TEST %: $*"
        exit 1
    }
    LC_ALL=C
    export LC_ALL
    sed -n -e '/^#STDOUT:/s/^#STDOUT://p' $(1) >tests/%.output.expected
    SHOULD_FAIL=`sed -n -e '/^#SHOULD_FAIL:/s/^#SHOULD_FAIL://p' $(1)`
    INVOKE=`sed -n -e '/^#INVOKE:/s/^#INVOKE://p' $(1)`
    PARALLEL=`sed -n -e '/^#PARALLEL:/s/^#PARALLEL://p' $(1)`
    YABU_EXPORT_TEST=yabu
    export YABU_EXPORT_TEST
    if [ -n "$PARALLEL" ]; then
        echo "!servers" >tests/yabu.cfg
        echo "host `hostname` max=$PARALLEL" >>tests/yabu.cfg
        ./yabu -g tests -s -f $(1) $INVOKE >tests/%.stdout 2>tests/%.stderr
        RC=$?
    else
        ./yabu -s -f $(1) $INVOKE >tests/%.stdout 2>tests/%.stderr
        RC=$?
    fi
    grep 'Xx' tests/%.stdout >tests/%.output.actual

    if [ -n "$SHOULD_FAIL" ] ; then
        [ $RC -ne 0 ] || Abort "Yabu succeeded (but should have failed)"
        grep $SHOULD_FAIL tests/%.stdout >/dev/null || Abort "Message $SHOULD_FAIL not found"
    else 
        [ $RC -eq 0 ] || Abort "Yabu failed unexpectedly"
    fi
    cmp -s tests/%.output.actual tests/%.output.expected || Abort "Unexpected output"
    rm -f tests/%.stdout tests/%.stderr tests/%.output.actual tests/%.output.expected
    touch $(0)
 }


