#ifndef THREADED_OGG_READER
#define THREADED_OGG_READER

#define OGG_BUFFER_SIZE 20*1024*1024

// TODO: Put thread and mutex functions in osapi for abstraction


#include <boost/interprocess/sync/interprocess_mutex.hpp>
#include <boost/interprocess/sync/interprocess_semaphore.hpp>
#include <boost/thread.hpp>

#include "theora/theora.h"
#include "vorbis/codec.h"

#include "cfile/cfile.h"

//#define THREADED_READER_BUFFER (8*1024*1024)

struct THEORAFILE;

class OggReader
{
	FILE* f; //debug

	CFILE* file;
	bool isOpen;

	bool working;

	boost::interprocess::interprocess_mutex mutexVideo;
	boost::interprocess::interprocess_mutex mutexAudio;
	boost::thread thread;
	boost::interprocess::interprocess_semaphore canWrite;
	boost::interprocess::interprocess_semaphore readerSem;
	bool wantToRead;
	bool writerIsWaiting;

	bool buffering;

	THEORAFILE* movie;

	long body_returned;

	// ctor and dtor
public:
	OggReader(CFILE* f, THEORAFILE* movie);
	~OggReader();

	// public interface
	int cfread(void* buf, int elsize, int nelem);
	bool cfeof();
	int cfclose();

	int popVideoPacket(ogg_packet* op);
	int popAudioPacket(ogg_packet* op);

	void readerLock();
	void readerRelease();
	void start();
	void stop();

	void compact_if_needed();

protected:
	// thread
	static void run();

private:
	int popOggPacket(HANDLE mutex, ogg_stream_state* os, ogg_packet* op);
	bool pushVideoPage(ogg_page* op);
	bool pushAudioPage(ogg_page* op);
	bool pushOggPage(HANDLE mutex, ogg_stream_state* os, ogg_page* op);
	void writerLock();
	void writerRelease();

	void compact_now(ogg_stream_state* os);
	void compact_delay(ogg_stream_state* os) const;
	void compact_save_state(ogg_stream_state* os);
	void compact_restore_state(ogg_stream_state* os);
	bool compact_eval_state(ogg_stream_state* os);
};

#endif
