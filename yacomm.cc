////////////////////////////////////////////////////////////////////////////////////////////////////
// I, the creator of this work, hereby release it into the public domain. This applies worldwide.
// In case this is not legally possible: I grant anyone the right to use this work for any purpose,
// without any conditions, unless such conditions are required by law.
////////////////////////////////////////////////////////////////////////////////////////////////////

// Hilfsklassen für die Kommunikation zwischen yabu und yabusrv

#include "yabu.h"

#include <unistd.h>
#include <poll.h>


////////////////////////////////////////////////////////////////////////////////////////////////////
// Länge (24-Bit) hardwareunabhängig kodieren/dekodieren
////////////////////////////////////////////////////////////////////////////////////////////////////

static size_t get3(void const *c)
{
    unsigned char const *p = (unsigned char const *)c;
    return ((unsigned) p[0] << 16) | ((unsigned) p[1] << 8) | (unsigned) p[2];
}

static void put3(void *c, size_t val)
{
   YABU_ASSERT(val < 0x1000000);
   unsigned char *p = (unsigned char *)c;
   p[0] = val >> 16;
   p[1] = val >> 8;
   p[2] = val;
}


////////////////////////////////////////////////////////////////////////////////////////////////////
// Konstruktor
////////////////////////////////////////////////////////////////////////////////////////////////////

IoBuffer::IoBuffer(PollObj &obj)
    :rbuf_(0), rlen_(0), rmax_(0), rp_(0),
     wbuf_(0), wlen_(0), wmax_(0), wp_(0),
     obj_(obj)
{
}


////////////////////////////////////////////////////////////////////////////////////////////////////
// Destruktor
////////////////////////////////////////////////////////////////////////////////////////////////////

IoBuffer::~IoBuffer()
{
   if (rbuf_) free(rbuf_);
   if (wbuf_) free(wbuf_);
}


////////////////////////////////////////////////////////////////////////////////////////////////////
// Puffer auf mindestens «req» Bytes vergrößern
////////////////////////////////////////////////////////////////////////////////////////////////////

static void reserve(char *&buf, size_t &buf_size, size_t req)
{
   if (req > buf_size) {
      buf_size += buf_size / 2 + req;
      array_realloc(buf,buf_size);
   }
}

////////////////////////////////////////////////////////////////////////////////////////////////////
// Daten empfangen und an den Puffer anhängen. Returnwert wie bei «read()».
////////////////////////////////////////////////////////////////////////////////////////////////////

int IoBuffer::read(int fd)
{
   reserve(rbuf_,rmax_,rlen_ + 200);
   int rc =::read(fd, rbuf_ + rlen_, rmax_ - rlen_);
   if (rc > 0)
      rlen_ += rc;
   return rc;
}


////////////////////////////////////////////////////////////////////////////////////////////////////
// Nächsten Datensatz abrufen. Return ist «false», falls kein vollständiger Datensatz im Puffer
// steht.
////////////////////////////////////////////////////////////////////////////////////////////////////

bool IoBuffer::next()
{
    if (rp_ + 4 > rlen_) return false;		// Header vollständig?
    len_ = get3(rbuf_ + rp_ + 1);
    if (rp_ + 4 + len_ + 1 > rlen_) {		// Daten vollständig?
	if (rp_ > 0) {				// Fragment an den Pufferanfang verschieben
	    rlen_ -= rp_;
	    memmove(rbuf_,rbuf_ + rp_,rlen_);
	    rp_ = 0;
	}
	return false;
    }

    // Datensatz ist vollständig
    tag_ = rbuf_[rp_];
    data_ = rbuf_ + rp_ + 4;
    if ((rp_ += 4 + len_ + 1) >= rlen_)		// Puffer ist leer geworden
       rp_ = rlen_ = 0;
    return true;
}


////////////////////////////////////////////////////////////////////////////////////////////////////
// Datensatz zum Senden freigeben. Inhalt muß bereits besetzt sein, Header und NUL werden hier
// eingetragen.
////////////////////////////////////////////////////////////////////////////////////////////////////

void IoBuffer::commit(char tag, size_t len)
{
   wbuf_[wlen_] = tag;
   put3(wbuf_ + wlen_ + 1,len);
   wbuf_[wlen_ + 4 + len] = 0;
   wlen_ += len + 5;
   YABU_ASSERT(wlen_ <= wmax_);
   obj_.set_events(POLLOUT);
}


////////////////////////////////////////////////////////////////////////////////////////////////////
// Stellt einen Datensatz in die Warteschlange. Der Inhalt wird mit printf()-Syntax formatiert.
////////////////////////////////////////////////////////////////////////////////////////////////////

void IoBuffer::append(char tag, const char *msg, ...)
{
   reserve(wbuf_,wmax_,wlen_ + 200);
   va_list al;
   va_start(al,msg);
   int len = vsnprintf(wbuf_ + wlen_ + 4,wmax_ - wlen_ - 4,msg,al);
   if (len < 0) return;
   if ((size_t) len >= wmax_ - wlen_ - 4) {
      reserve(wbuf_,wmax_,wlen_ + len + 5);
      vsnprintf(wbuf_ + wlen_ + 4,wmax_ - wlen_ - 4,msg,al);	// 2. Versuch
   }
   commit(tag,len);
}


////////////////////////////////////////////////////////////////////////////////////////////////////
// Stellt einen Datensatz in die Warteschlange.
////////////////////////////////////////////////////////////////////////////////////////////////////

void IoBuffer::append(char tag, size_t len, char const *data)
{
   reserve(wbuf_,wmax_,wlen_ + len + 4);
   memcpy(wbuf_ + wlen_ + 4,data,len);
   commit(tag,len);
}


////////////////////////////////////////////////////////////////////////////////////////////////////
// Daten schreiben. Return wie «::write()»
////////////////////////////////////////////////////////////////////////////////////////////////////

int IoBuffer::write(int fd)
{
   int rc = 0;

   if (wp_ < wlen_) {
      rc = ::write(fd,wbuf_ + wp_,wlen_ - wp_);
      if (rc > 0 && (wp_ += rc) == wlen_)
	 wp_ = wlen_ = 0;
   }

   if (wp_ >= wlen_)
      obj_.clear_events(POLLOUT);	// Keine weiteren Daten
   return rc;
}


// vim:shiftwidth=3:cindent
