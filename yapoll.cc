////////////////////////////////////////////////////////////////////////////////////////////////////
// I, the creator of this work, hereby release it into the public domain. This applies worldwide.
// In case this is not legally possible: I grant anyone the right to use this work for any purpose,
// without any conditions, unless such conditions are required by law.
////////////////////////////////////////////////////////////////////////////////////////////////////

// Die Klasse PollObj - Eine C++-Schnittstelle für «poll()»

#include "yabu.h"
#include <unistd.h>
#include <errno.h>
#include <poll.h>


static unsigned last_id = 0;		// Fortlaufende Numerierung der Objekte
static size_t n_poll = 0;		// Anzahl Dateien
static size_t max_poll = 0;		// Größe der Puffer «pollfd» und «pollobj»
static struct pollfd *pollfd = 0;	// Aktive Dateien
static PollObj **pollobj = 0;		// Zugehörige Objekte


////////////////////////////////////////////////////////////////////////////////////////////////////
// Fügt einen Deskriptor in die Liste «pollfd[]» ein und setzt gleichzeitig in
// «pollobj[]» einen Zeiger auf das Objekt, welches den Deskriptor besitzt.
////////////////////////////////////////////////////////////////////////////////////////////////////

void PollObj::add_fd(int fd)
{
    YABU_ASSERT(fd >= 0);				// Ungültiger Deskriptor
    YABU_ASSERT(idx_ < 0 || pollfd[idx_].fd < 0);	// Mehrfaches add_fd()
    if (idx_ < 0) {
       if (n_poll >= max_poll) {			// Puffer bei Bedarf vergrößern
	  max_poll += 10;
	  array_realloc(pollfd,max_poll);
	  array_realloc(pollobj,max_poll);
       }
       idx_ = n_poll++;
    }
    pollfd[idx_].fd = fd;
    pollfd[idx_].events = POLLIN;
    pollobj[idx_] = this;
}


////////////////////////////////////////////////////////////////////////////////////////////////////
// Schließt den Deskriptor und entfernt die zugehörigen Einträge aus «pollfd[]» und «pollobj[]».
////////////////////////////////////////////////////////////////////////////////////////////////////

void PollObj::del_fd()
{
   if (idx_ >= 0) {
      YABU_ASSERT((size_t) idx_ < n_poll);
      close(pollfd[idx_].fd);
      pollfd[idx_].fd = -1;
      pollobj[idx_] = 0;
      idx_ = -1;
   }
}


////////////////////////////////////////////////////////////////////////////////////////////////////
// Unbenutzte Einträge in «pollfd[]» und «pollobj[]» entfernen.
////////////////////////////////////////////////////////////////////////////////////////////////////

static void purge()
{
   size_t i, k;
   for (i = k = 0; i < n_poll; ++i) {
      if (pollobj[i] != 0) {	// Eintrag ist noch aktiv
	 if (k < i) {
	    // Eintrag von Position «i» nach «k» verschieben
	    pollfd[k] = pollfd[i];
	    pollobj[k] = pollobj[i];
	    pollobj[k]->idx_ = k;
	 }
	 ++k;
      }
   }
   n_poll = k;
}


////////////////////////////////////////////////////////////////////////////////////////////////////
// Konstruktor
////////////////////////////////////////////////////////////////////////////////////////////////////

PollObj::PollObj()
    :id_(++last_id & 0xFFF), idx_(-1)
{
}


////////////////////////////////////////////////////////////////////////////////////////////////////
// Destruktor
////////////////////////////////////////////////////////////////////////////////////////////////////

PollObj::~PollObj()
{
   del_fd();
}


////////////////////////////////////////////////////////////////////////////////////////////////////
// Warte auf Datei-I/O.
// Return: false, wenn keine Dateien offen sind, sonst immer true
////////////////////////////////////////////////////////////////////////////////////////////////////

bool PollObj::poll(int timeout)
{
   purge();
   //if (exit_code > 1 || n_poll == 0) return false;
   if (n_poll == 0) return false;
   int rc =::poll(pollfd, n_poll, timeout);
   if (rc < 0 && errno != EINTR)
      YUFTL(G20,syscall_failed("poll",0));
   else if (rc > 0) {
      // «n_poll» kann sich innerhab der Scheife erhöhen, wenn ein Callback «add_fd()»
      // aufruft. Für die neuen hinzugekommenen Dateien ist «revents» aber noch ungültig.
      size_t const n = n_poll;
      for (size_t i = 0; i < n; ++i) {
	 int result = (pollfd[i].revents & (POLLIN | POLLOUT)) == 0
	           && (pollfd[i].revents & (POLLHUP | POLLERR)) != 0;
	 if (pollfd[i].revents & POLLIN)
	    result |= pollobj[i]->handle_input(pollfd[i].fd,pollfd[i].revents);
	 if ((pollfd[i].revents & POLLOUT) && pollfd[i].fd >= 0)
	    result |= pollobj[i]->handle_output(pollfd[i].fd,pollfd[i].revents);
	 if (result != 0 && pollobj[i])
	    pollobj[i]->del_fd();
      }
   }
   return true;
}



void PollObj::clear_events(int events)
{
    pollfd[idx_].events &= ~events;
}


void PollObj::set_events(int events)
{
    pollfd[idx_].events |= events;
}

int PollObj::fd() const
{
   return (idx_ >= 0) ? pollfd[idx_].fd : -1;
}



// vim:sw=3:cin:fileencoding=utf-8
