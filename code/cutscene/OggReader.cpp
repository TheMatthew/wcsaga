#include "oggplayer.h"
#include "OggReader.h"

#include "io/timer.h"

OggReader::OggReader(CFILE* f, THEORAFILE* movie)
{
	this->f = fopen("debug.txt", "wb");

	this->file = f;
	this->movie = movie;
	this->isOpen = true;
	this->mutexVideo(boost::interprocess::open_or_create, "MutexVideo");
	this->mutexAudio(boost::interprocess::open_or_create, "MutexAudio");
	this->canWrite = CreateSemaphore(0, 1, 1, NULL);
	this->readerSem = CreateSemaphore(0, 0, 1, NULL);

	this->wantToRead = false;
	this->writerIsWaiting = false;

	this->buffering = false;

	this->body_returned = 0;
}

OggReader::~OggReader()
{
	if (this->file && isOpen)
	{
		::cfclose(this->file);
	}

	stop();

	if (this->mutexVideo)
	{
		// close handle
		boost::interprocess::remove("MutexVideo");
	}

	if (this->mutexAudio)
	{
		// close handle
		boost::interprocess::remove("MutexAudio");
	}

	if (this->canWrite)
	{
		// close handle
		boost::interprocess::remove("canWrite");
	}

	if (this->readerSem)
	{
		// close handle
		boost::interprocess::remove("readerSem");
	}

	fclose(f);
}

DWORD OggReader::run(LPVOID ptr)
{
	OggReader* reader = (OggReader*)ptr;

	while (reader->working && reader->isOpen)
	{
		// get maximum read size given available sync buffer space
		int size = reader->movie->osyncstate.fill - reader->movie->osyncstate.returned;
		if (size < 0)
			size = reader->movie->osyncstate.storage - size;
		size = reader->movie->osyncstate.storage - size;

		if (size > 4096 && !reader->cfeof())
		{
			char* buffer = ogg_sync_buffer(&reader->movie->osyncstate, size);
			int bytes = reader->cfread(buffer, 1, size);
			ogg_sync_wrote(&reader->movie->osyncstate, bytes);
		}

		while (reader->working)
		{
			size = reader->movie->t_osstate.body_storage - reader->movie->t_osstate.body_fill;

			if (size <= 1024 * 1024)
				break;

			reader->buffering = true;

			if (ogg_sync_pageout(&reader->movie->osyncstate, &reader->movie->opage) > 0)
			{
				reader->writerLock();

				bool success = reader->pushVideoPage(&reader->movie->opage);

				if (!success)
				{
					if (reader->movie->vorbis_p)
					{
						reader->pushAudioPage(&reader->movie->opage);
					}
				}
				reader->writerRelease();
			}
			else
			{
				break;
			}

			// the trick: let's try not to starve any other threads in the game that may be running
			::Sleep(1);

		}

		reader->buffering = false;

		::Sleep(5);
	}

	return 0;
}

void OggReader::start()
{
	this->working = true;
	this->body_returned = movie->t_osstate.body_returned;
	this->thread = CreateThread(0, 0, (LPTHREAD_START_ROUTINE)OggReader::run, this, 0, 0);
}

void OggReader::stop()
{
	if (this->thread)
	{
		// wait for thread to end
		working = false;

		// in case the thread is stopped at the semaphore
		::canWrite.post();

		if (WAIT_FAILED == WaitForSingleObject(this->thread, INFINITE))
		{
			fprintf(f, "WARNING:: Error on thread Join\r\n");
		}

		// close handle
		if (!CloseHandle(this->thread))
		{
			fprintf(f, "WARNING:: Error closing thread handle, check if we don't have a handle leak\r\n");
		}
		this->thread = NULL;
	}
}

int OggReader::cfread(void* buf, int elsize, int nelem)
{
	return ::cfread(buf, elsize, nelem, file);
}

bool OggReader::cfeof()
{
	return ::cfeof(this->file) != 0 && (movie->osyncstate.fill == movie->osyncstate.returned);
}

int OggReader::cfclose()
{
	return ::cfclose(file);
}

int OggReader::popVideoPacket(ogg_packet* op)
{
	int ret = 0;
	ogg_stream_state* os = &movie->t_osstate;

	compact_restore_state(os);

	ret = popOggPacket(mutexVideo, &movie->t_osstate, op);
	fprintf(f, "%ld %ld %ld %ld\r\n", os->body_fill, os->body_returned, movie->v_osstate.body_fill,
		movie->v_osstate.body_returned);

	compact_save_state(os);

	return ret;
}

int OggReader::popAudioPacket(ogg_packet* op)
{
	return popOggPacket(mutexAudio, &movie->v_osstate, op);
}

int OggReader::popOggPacket(boost::interprocess::interprocess_mutex mutex, ogg_stream_state* os, ogg_packet* op)
{
	int ret = 0;

	ret = ogg_stream_packetout(os, op);

	return ret;
}

bool OggReader::pushVideoPage(ogg_page* op)
{
	return pushOggPage(mutexAudio, &movie->t_osstate, op);
}

bool OggReader::pushAudioPage(ogg_page* op)
{
	return pushOggPage(mutexAudio, &movie->v_osstate, op);
}

bool OggReader::pushOggPage(boost::interprocess::interprocess_mutex mutex, ogg_stream_state* os, ogg_page* op)
{
	int ret = 0;

	ret = ogg_stream_pagein(os, op);

	return ret == 0;
}

void OggReader::readerLock()
{
	mutexVideo.wait();
	wantToRead = true;
	mutexVideo.post();

	canWrite.wait();
}

void OggReader::readerRelease()
{
	mutexVideo.wait();
	wantToRead = false;
	if (writerIsWaiting)
	{
		readerSem.post();
	}
	mutexVideo.post();

	canWrite.post();
}

void OggReader::writerLock()
{
	mutexVideo.wait();
	if (wantToRead)
	{
		writerIsWaiting = true;
		mutexVideo.post();

		WaitForSingleObject(readerSem, INFINITE); // wait for reader to finish	

		mutexVideo.wait();
		writerIsWaiting = false;
		mutexVideo.post();

		canWrite.wait();
	}
	else
	{
		mutexVideo.post();
		canWrite.wait();
	}
}

void OggReader::writerRelease()
{
	canWrite.post();
}

void OggReader::compact_now(ogg_stream_state* os)
{
	// huge ammounts of evil hackery
	memmove(os->body_data, os->body_data + os->body_returned, os->body_fill - os->body_returned);
	os->body_fill -= os->body_returned;
	os->body_returned = 0;
	this->body_returned = 0;
}

void OggReader::compact_delay(ogg_stream_state* os) const
{
	os->body_returned = 0;
}

void OggReader::compact_save_state(ogg_stream_state* os)
{
	this->body_returned = os->body_returned;
}

void OggReader::compact_restore_state(ogg_stream_state* os)
{
	os->body_returned = this->body_returned;
}

bool OggReader::compact_eval_state(ogg_stream_state* os)
{
	bool readFarFromEnding = (os->body_storage - os->body_returned >= 2 * 1024 * 1024);	// try to minimize compact operation, but...
	bool hasAudio = (ogg_stream_packetpeek(&movie->v_osstate, NULL) > 0);			// ... if we run out of audio, don't risk audio glitches!
	bool pastMinimumReturn = (os->body_returned > 3 * 1024 * 1024);

	return !readFarFromEnding || (!hasAudio && movie->vorbis_p /*&& !::cfeof(this->file) && !buffering **/ &&
								  pastMinimumReturn);
}

void OggReader::compact_if_needed()
{
	ogg_stream_state* os = &movie->t_osstate;

	if (!compact_eval_state(os))
	{
		compact_delay(os);
	}
	else
	{
		compact_now(os);
	}

}
