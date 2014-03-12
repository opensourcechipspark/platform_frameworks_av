#define LOG_TAG "TvpadSource"

//#include "datumtunel.h"
//#include "datumclient.h"
#include <TvpadSource.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>
#include <fcntl.h>
#include <stdio.h>
#include <media/stagefright/foundation/ADebug.h>
#include <dlfcn.h>
namespace android {

int logNum = 0;
int logNumT = 0;
int (*client_usedatabuf)();
int (*client_init)();
int (*client_start)(char *);
int (*client_stop)();
int (*client_uninit)();
int (*client_read188align)(char *);
TvpadSource::TvpadSource(char *uri)
	:file_end(false),
	 temp_flag(false),
	 total_buf_size(0),
	 total_buf_offset(0),
	 mFinalResult(1){
	mUri = (char*)malloc(strlen(uri));
	if(mUri == NULL)
		ALOGD("can't not alloc mUri spcace");
	else
		strcpy(mUri,uri);
	inWrite = true;
	copy_buf = (char *) malloc (6 * 188);
	read_buf = (char*) malloc(20 * 188);
	temp_buf = (char*) malloc(10 * 188);

	libhandle = dlopen("libclientdata.so", RTLD_NOW);

	if (libhandle != NULL) {
		*(void **) (&client_usedatabuf) = dlsym(libhandle, "datumclient_preset_usedatabuf");
		*(void **) (&client_init) = dlsym(libhandle, "datumclient_init");
		*(void **) (&client_start) = dlsym(libhandle, "datumclient_start");
		*(void **) (&client_stop) = dlsym(libhandle, "datumclient_stop");
		*(void **) (&client_uninit) = dlsym(libhandle, "datumclient_uninit");
		*(void **) (&client_read188align) = dlsym(libhandle, "datumclient_read188align");

		if ((*client_usedatabuf)() != 0) {
			ALOGI("datumclient preset error");
		}

		if ((*client_init)() != 0) {
			ALOGI("datumclient init error");
		}
	}

	//testFd = open("/data/socket/hisense_ts_rec.ts", O_CREAT | O_LARGEFILE | O_APPEND | O_RDWR);//O_TRUNC | O_RDWR);

}

TvpadSource::~TvpadSource() {
	
	ALOGI("Hello, tvpadsource will quit mUri %s",mUri);

	file_end = true;
	free(mUri);
	if (temp_buf != NULL) {
		free(temp_buf);
	}
	if (read_buf != NULL) {
		free(read_buf);
	}
	if (libhandle != NULL) {
		(*client_stop)();
		(*client_uninit)();
		dlclose(libhandle);
	}

	//close(testFd);

}

/* check it is init ok */
status_t TvpadSource::initCheck() const {
	if(mUri == NULL)
	{
		ALOGD("mUri == NULL in init");
		return -1;
	}

	if (libhandle != NULL && (*client_start)(mUri) != 0) {
                ALOGI("datumclient start error");
        }
	return OK;

}

ssize_t TvpadSource::readAt(off64_t offset, void *data, size_t size) {

	char temp[12] = "TVPAD";
	if (size == 11) {
		memcpy(data, temp, 12);
		return size;
	}
	int readCount = 0;
	if(libhandle == NULL || mFinalResult == OK)
	{
		ALOGD("TvpadSource::readAt libhandle is NULL ? mFinalResult %d ",mFinalResult);
		return -1111;
	}

	if (size == 1 && total_buf_size == 0) {
		readCount = feedBuffer(offset);
		if (readCount == -1 || readCount == -2)
			return -2111;
	}

	if (size == 188 && offset > (total_buf_offset + total_buf_size - 1)) {//7 * 188 - 1)) {
		total_buf_size = 0;
		memset(copy_buf, 0, 188);
		readCount = (*client_read188align)(copy_buf);
		if (readCount < 0 || readCount == -1 || readCount == -2)
			return -2111;
		else if (readCount > 0) {// && readCount == 188) {
			//memcpy(data, copy_buf, readCount);
			logNum ++;
			if (logNum % 6000 == 0) {
				//logNum = 0;
				//ALOGI("size = 188 and return readCount = %d, logNum = %d, logNumT = %d\n", readCount, logNum, logNumT);
			}
			memcpy(data, copy_buf, 188);//readCount);
			if (false) {
				write(testFd, copy_buf, readCount);
			}
		} else {
			logNumT ++;
			if (logNumT % 60000 == 0) {
			//	logNumT = 0;
				//ALOGI("size = 188 and return readCount = %d, logNum = %d, logNumT = %d\n", readCount, logNum, logNumT);
			}
			return -2111;
		}
		return 188;//readCount;
	} else if (offset > total_buf_offset  ||  offset == total_buf_offset || offset < (total_buf_offset + total_buf_size)){//7 * 188 )){
		memcpy(data, &read_buf[offset - total_buf_offset], size);
		return size;
	}
	return -2111;
}

int TvpadSource::feedBuffer(off64_t offset) {
	total_buf_offset = offset;
	int readNum = 0;
	memset(read_buf, 0, 188);
	for (int i =0; i < 1; i ++) {
		readNum = (*client_read188align)(&read_buf[total_buf_size]);
                if (readNum > 0) {
			logNum ++;
			if (logNum % 6000 == 0) {
//				logNum = 0;
				//ALOGI("size = 188 and return readCount = %d , logNum = %d, logNumT = %d\n", readNum, logNum, logNumT);
			}
                        if (false) {
                                write(testFd, &read_buf[total_buf_size], 188);
                        }
                        total_buf_size = total_buf_size + readNum;
                } else if (readNum == 0) {
			logNumT ++;
			if (logNumT % 60000 == 0) {
//				logNumT = 0;
				//ALOGI("size = 188 and return readCount = %d, logNum = %d, logNumT = %d\n", readNum, logNum, logNumT);
			}
		} else if (readNum == -1 || readNum == -2) {

		}
        }
	return readNum;
}

/* read from file or the share memory and send to service*/
void TvpadSource::readThread() {

	/* Hisense interface *//*
	file_end = false;
	while (!file_end) {
		if (temp_flag && total_buf_size < 5 * 188) {
			int readNum = datumclient_read(copy_buf, 1);
			if (readNum > 0) {
				memcpy(&read_buf[total_buf_size], copy_buf, readNum);
				total_buf_size += readNum;
				if (total_buf_size == 5 * 188)
					temp_flag = false;
				ALOGI("readThread  total_buf_size = %d\n", total_buf_size);
			}
		}
		usleep(30000);
	}*/

}
status_t TvpadSource::getSize(off64_t *size) {
	/* Nothing need to do */
	return OK;
}

void* TvpadSource::SendBufferThread(void *me) {

	TvpadSource *memSource = static_cast<TvpadSource *> (me);
	memSource->readThread();
	return NULL;

}
void TvpadSource::queueEOS(status_t finalResult) {
    ALOGD("TvpadSource::queueEOS finalResult %d",finalResult);
	CHECK_NE(finalResult, (status_t)OK);

    if(finalResult == INFO_TIME_OUT)
    {
		finalResult = OK;
    }
    mFinalResult = finalResult;
}

void TvpadSource::startThread() {

	pthread_attr_t sendAttr;
        pthread_attr_init(&sendAttr);
        pthread_attr_setdetachstate(&sendAttr, PTHREAD_CREATE_JOINABLE);
	pthread_create(&ReadThread, &sendAttr, SendBufferThread, this);
        pthread_attr_destroy(&sendAttr);

}

}
