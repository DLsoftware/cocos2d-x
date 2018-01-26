/****************************************************************************
Copyright (c) 2013 cocos2d-x.org

http://www.cocos2d-x.org

Permission is hereby granted, free of charge, to any person obtaining a copy
of this software and associated documentation files (the "Software"), to deal
in the Software without restriction, including without limitation the rights
to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
copies of the Software, and to permit persons to whom the Software is
furnished to do so, subject to the following conditions:

The above copyright notice and this permission notice shall be included in
all copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
THE SOFTWARE.
****************************************************************************/

#include "Updater.h"
#include "cocos2d.h"

#if (CC_TARGET_PLATFORM != CC_PLATFORM_WINRT) && (CC_TARGET_PLATFORM != CC_PLATFORM_WP8)
#include <curl/curl.h>
#include <curl/easy.h>

#include <stdio.h>
#include <vector>
#include <math.h>
#include <cstdlib>

#if (CC_TARGET_PLATFORM != CC_PLATFORM_WIN32)
#include <sys/types.h>
#include <sys/stat.h>
#include <errno.h>
#endif

#include "external/unzip/unzip.h"
#include "base/CCScriptSupport.h"
#include <io.h>
#include <iostream>
#include <fstream>

USING_NS_CC;

using namespace std;


#define KEY_OF_VERSION   "current-version-code"
#define KEY_OF_DOWNLOADED_VERSION    "downloaded-version-code"
#define TEMP_PACKAGE_FILE_NAME    "cocos2dx-update-temp-package.zip"
#define BUFFER_SIZE    8192
#define MAX_FILENAME   512
#define PACKAGE_PARIT  2

// Message type
#define UPDATER_MESSAGE_UPDATE_SUCCEED                0
#define UPDATER_MESSAGE_STATE                         1
#define UPDATER_MESSAGE_PROGRESS                      2
#define UPDATER_MESSAGE_ERROR                         3

#ifdef _WIN32
#define SHORT_SLEEP Sleep(100)
#else
#define SHORT_SLEEP usleep(100000)
#endif


static int lastPercent = 0;

// Some data struct for sending messages

struct ErrorMessage
{
	Updater::ErrorCode code;
	Updater* manager;
};

struct ProgressMessage
{
	int percent;
	Updater* manager;
};

struct StateMessage
{
	Updater::StateCode code;
	Updater* manager;
};

static size_t get_size_struct(void *ptr, size_t size, size_t nmemb, void *data)
{
	(void)ptr;
	(void)data;

	// return only the size, dump the rest
	return (size_t)(size * nmemb);
}

static double get_download_size(const char *url)
{
	CURL *curl;
	CURLcode res;
	double size = 0.0;

	curl = curl_easy_init();
	curl_easy_setopt(curl, CURLOPT_URL, url);
	curl_easy_setopt(curl, CURLOPT_NOBODY, 1L);
	curl_easy_setopt(curl, CURLOPT_HEADERFUNCTION, get_size_struct);
	res = curl_easy_perform(curl);
	res = curl_easy_getinfo(curl, CURLINFO_CONTENT_LENGTH_DOWNLOAD, &size);
	if (res != CURLE_OK)
	{
		fprintf(stderr, "curl_easy_getinfo() failed: %s\n", curl_easy_strerror(res));
	}

	curl_easy_cleanup(curl);

	return size;
}

void set_share_handle(CURL* curl_handle)
{
	static CURLSH* share_handle = nullptr;
	if (!share_handle)
	{
		share_handle = curl_share_init();
		curl_share_setopt(share_handle, CURLSHOPT_SHARE, CURL_LOCK_DATA_DNS);
	}
	curl_easy_setopt(curl_handle, CURLOPT_SHARE, share_handle);
	curl_easy_setopt(curl_handle, CURLOPT_DNS_CACHE_TIMEOUT, 60*5);
}

// Implementation of Updater

Updater::Updater()
	: _curl(NULL)
	, _tid(NULL)
	, _connectionTimeout(0)
	, _delegate(NULL)
	, _scriptHandler(0)
	, _packageTotalToDownload(0)
	, _packageCurDownloaded(0)
	, _fileUrl("")
	, _filePath("")
	, _unzipTmpDir("")
	, _updateInfoString("")
	, _resetBeforeUnZIP(true)
	, _updateType(kUpdateUndefined)
	, _downloadType(kDownloadNormal)
{
	//CCLOG("Updater::Updater()");
	_schedule = new Helper();
	CCDirector::getInstance()->getScheduler()
		->scheduleUpdate(_schedule, 0, false);
}

Updater::~Updater()
{
	//CCLOG("Updater::~Updater()");
	if (_schedule)
	{
		CCDirector::getInstance()->getScheduler()
			->unscheduleAllForTarget(_schedule);
		_schedule->release();
	}
	unregisterScriptHandler();
}


static Updater* s_pSharedUpdater = nullptr;
Updater* Updater::getInstance()
{
	if (s_pSharedUpdater == nullptr)
	{
		s_pSharedUpdater = new (std::nothrow) Updater();
	}

	return s_pSharedUpdater;
}

void Updater::checkUnZIPTmpDir()
{
	if (_unzipTmpDir.size() > 0 && _unzipTmpDir[_unzipTmpDir.size() - 1] != '/')
	{
		_unzipTmpDir.append("/");
	}
}

void Updater::clearOnSuccess()
{
	if (_updateType == kUpdateZIP)
	{
		// Delete unloaded zip file.
		if (remove(_filePath.c_str()) != 0)
		{
			CCLOG("Updater::clearOnSuccess Can not remove downloaded zip file %s", _filePath.c_str());
		}
	}
}

static size_t getUpdateInfoWriteFunc(void *ptr, size_t size, size_t nmemb, void *userdata)
{
	string *updateInfo = (string*)userdata;
	
	updateInfo->append((char*)ptr, size * nmemb);

	return (size * nmemb);
}

static size_t downloadWriteFunc(void *ptr, size_t size, size_t nmemb, void *userdata)
{
	ofstream *fp = (ofstream*)userdata;
	fp->write((char *)ptr, size*nmemb);

	return size*nmemb;
}

const char* Updater::getUpdateInfo(const char* url)
{
	CCLOG("Updater::getUpdateInfo(%s)", url);
	_curl = curl_easy_init();
	if (!_curl)
	{
		sendErrorMessage(kNetwork);
		CCLOG("Updater::getUpdateInfo(%s) Can not init curl", url);
		return "";
	}

	_updateInfoString.clear();

	CURLcode res;
	curl_easy_setopt(_curl, CURLOPT_URL, url);
	curl_easy_setopt(_curl, CURLOPT_SSL_VERIFYPEER, 0L);
	curl_easy_setopt(_curl, CURLOPT_WRITEFUNCTION, getUpdateInfoWriteFunc);
	curl_easy_setopt(_curl, CURLOPT_WRITEDATA, &_updateInfoString);
	if (_connectionTimeout) curl_easy_setopt(_curl, CURLOPT_CONNECTTIMEOUT, _connectionTimeout);
	res = curl_easy_perform(_curl);
	curl_easy_cleanup(_curl);
	if (res != 0)
	{
		sendErrorMessage(kNetwork);
		CCLOG("Can not get update info content %s, error code is %d", url, res);
		return "";
	}
	CCLOG("getUpdateInfoWriteFunc %s", _updateInfoString.c_str());
	return _updateInfoString.c_str();
}

void* updateThreadFunc(void *data)
{
	Updater* self = (Updater*)data;
	do
	{
		if (self->_downloadType == self->kDownloadNormal && !self->download(self->_fileUrl.c_str(), self->_filePath.c_str())) 
			break;
		else if (self->_downloadType == self->kDownloadMuti && !self->downloadMuti(self->_fileUrl.c_str(), self->_filePath.c_str()))
			break;

		if (self->_updateType == Updater::kUpdateZIP)
		{
			// Uncompress zip file.
			if (!self->uncompress(self->_filePath.c_str(), self->_unzipTmpDir.c_str(), self->_resetBeforeUnZIP))
			{
				self->sendErrorMessage(Updater::kUncompress);
				break;
			}
		}

		remove(self->_filePath.c_str());

		self->sendSuccMessage();
	} while (0);
	self->clearTid();
	return NULL;
}

void Updater::update(const char* zipUrl, const char* zipFile, const char* unzipTmpDir, bool resetBeforeUnZIP)
{
	if (!isAvailable()) return;

	_updateType = kUpdateZIP;

	_fileUrl.clear();
	_fileUrl.append(zipUrl);
	_filePath.clear();
	_filePath.append(zipFile);
	_unzipTmpDir.clear();
	_unzipTmpDir.append(unzipTmpDir);
	_resetBeforeUnZIP = resetBeforeUnZIP;
	lastPercent = 0;

	checkUnZIPTmpDir();

	// 1. Urls of package and version should be valid;
	// 2. Package should be a zip file.
	if (_fileUrl.size() == 0 ||
		_filePath.size() == 0 ||
		std::string::npos == _fileUrl.find(".zip"))
	{
		CCLOG("No version file url, or no package url, or the package is not a zip file");
		return;
	}

	curl_global_init(CURL_GLOBAL_ALL);

	std::thread _tid(&updateThreadFunc, this);
	_tid.detach();
}

void Updater::update(const char* fileUrl, const char* filePath)
{
	if (!isAvailable()) return;
	_updateType = kUpdateFile;
	_fileUrl.clear();
	_fileUrl.append(fileUrl);
	_filePath.clear();
	_filePath.append(filePath);

	curl_global_init(CURL_GLOBAL_ALL);

	std::thread _tid(&updateThreadFunc, this);
	_tid.detach();
}

bool Updater::uncompress(const char* zipFilePath, const char* unzipTmpDir, bool resetBeforeUnZIP)
{
	if (resetBeforeUnZIP)
	{
		// Create unzipTmpDir
		if (CCFileUtils::getInstance()->isFileExist(unzipTmpDir))
		{
			this->removeDirectory(unzipTmpDir);
		}
	}

	this->createDirectory(unzipTmpDir);

	// Open the zip file
	string outFileName = std::string(zipFilePath);
	unzFile zipfile = unzOpen(outFileName.c_str());
	if (!zipfile)
	{
		CCLOG("Can not open downloaded zip file %s", outFileName.c_str());
		return false;
	}

	// Get info about the zip file
	unz_global_info global_info;
	if (unzGetGlobalInfo(zipfile, &global_info) != UNZ_OK)
	{
		CCLOG("Can not read file global info of %s", outFileName.c_str());
		unzClose(zipfile);
		return false;
	}

	// Buffer to hold data read from the zip file
	char readBuffer[BUFFER_SIZE];

	CCLOG("Start uncompressing");

	this->sendStateMessage(kUncompressStart);

	// Loop to extract all files.
	uLong i;
	for (i = 0; i < global_info.number_entry; ++i)
	{
		// Get info about current file.
		unz_file_info fileInfo;
		char fileName[MAX_FILENAME];
		if (unzGetCurrentFileInfo(zipfile,
			&fileInfo,
			fileName,
			MAX_FILENAME,
			NULL,
			0,
			NULL,
			0) != UNZ_OK)
		{
			CCLOG("Can not read file info");
			unzClose(zipfile);
			return false;
		}

		//CCLOG("fullName:%s", fileName);
		string fullPath = std::string(unzipTmpDir) + fileName;

		//CCLOG("fullPath:%s", fullPath.c_str());
		// Check if this entry is a directory or a file.
		const size_t filenameLength = strlen(fileName);
		if (fileName[filenameLength - 1] == '/')
		{
			// get all dir
			string fileNameStr = string(fileName);
			size_t position = 0;
			while ((position = fileNameStr.find_first_of("/", position)) != string::npos)
			{
				string dirPath = unzipTmpDir + fileNameStr.substr(0, position);
				// Entry is a direcotry, so create it.
				// If the directory exists, it will failed scilently.
				if (!createDirectory(dirPath.c_str()))
				{
					CCLOG("Can not create directory %s", dirPath.c_str());
					//unzClose(zipfile);
					//return false;
				}
				position++;
			}
		}
		else
		{
			// Entry is a file, so extract it.

			// Open current file.
			if (unzOpenCurrentFile(zipfile) != UNZ_OK)
			{
				CCLOG("Can not open file %s", fileName);
				unzClose(zipfile);
				return false;
			}

			// Create a file to store current file.
			FILE *out = fopen(fullPath.c_str(), "wb");
			if (!out)
			{
				CCLOG("Can not open destination file %s", fullPath.c_str());
				unzCloseCurrentFile(zipfile);
				unzClose(zipfile);
				return false;
			}

			// Write current file content to destinate file.
			int error = UNZ_OK;
			do
			{
				error = unzReadCurrentFile(zipfile, readBuffer, BUFFER_SIZE);
				if (error < 0)
				{
					CCLOG("Can not read zip file %s, error code is %d", fileName, error);
					unzCloseCurrentFile(zipfile);
					unzClose(zipfile);
					return false;
				}

				if (error > 0)
				{
					fwrite(readBuffer, error, 1, out);
				}
			} while (error > 0);

			fclose(out);
		}

		unzCloseCurrentFile(zipfile);

		// Goto next entry listed in the zip file.
		if ((i + 1) < global_info.number_entry)
		{
			if (unzGoToNextFile(zipfile) != UNZ_OK)
			{
				CCLOG("Can not read next file");
				unzClose(zipfile);
				return false;
			}
		}
	}

	unzClose(zipfile);
	CCLOG("End uncompressing");
	this->sendStateMessage(kUncompressDone);

	return true;
}

bool Updater::removeDirectory(const char* path)
{
	int succ = -1;
#if (CC_TARGET_PLATFORM != CC_PLATFORM_WIN32)
	string command = "rm -r ";
	// Path may include space.
	command += "\"" + string(path) + "\"";
	succ = system(command.c_str());
#else
	string command = "rd /s /q ";
	// Path may include space.
	command += "\"" + string(path) + "\"";
	succ = system(command.c_str());
#endif
	if (succ != 0)
	{
		return false;
	}
	return true;
}

/*
* Create a direcotry is platform depended.
*/
bool Updater::createDirectory(const char *path)
{
#if (CC_TARGET_PLATFORM != CC_PLATFORM_WIN32)
	mode_t processMask = umask(0);
	int ret = mkdir(path, S_IRWXU | S_IRWXG | S_IRWXO);
	umask(processMask);
	if (ret != 0 && (errno != EEXIST))
	{
		return false;
	}

	return true;
#else
	BOOL ret = CreateDirectoryA(path, NULL);
	if (!ret && ERROR_ALREADY_EXISTS != GetLastError())
	{
		return false;
	}
	return true;
#endif
}


int downloadProgressFunc(void *ptr, double totalToDownload, double nowDownloaded,
	double totalToUpLoad, double nowUpLoaded)
{
	Updater* manager = static_cast<Updater *>(ptr);

	int curPercent = totalToDownload > 0 ?
		(nowDownloaded / totalToDownload * 100) : 0;

	if (curPercent <= lastPercent)
		return 0;

	lastPercent = curPercent;

	ProgressMessage *progressData = new ProgressMessage();
	progressData->percent = curPercent;
	progressData->manager = manager;

	Updater::Message *msg = new Updater::Message();
	msg->what = UPDATER_MESSAGE_PROGRESS;
	msg->obj = progressData;

	manager->_schedule->sendMessage(msg);


	return 0;
}

int downloadMutiProgressFunc(void *ptr, double totalToDownload, double nowDownloaded,
	double totalToUpLoad, double nowUpLoaded)
{
	dl_progress myProgress = static_cast<dl_progress>(ptr);
	time_t seconds;
	seconds = time(NULL);
	myProgress->totalDownloaded = nowDownloaded;
	myProgress->currentTime = seconds;

	static int lastPercent = 0;
	static int count = 0;

	Updater* manager = (Updater*)myProgress->handler;
	manager->_packageCurDownloaded = 0;
	for (int i = 0; i < PACKAGE_PARIT; i++)
	{
		manager->_packageCurDownloaded += manager->_myProgress[i].totalDownloaded;
	}
	int curPercent = manager->_packageTotalToDownload > 0 ?
				(manager->_packageCurDownloaded / manager->_packageTotalToDownload * 100) : 0;

	if (curPercent <= lastPercent)
		return 0;

	lastPercent = curPercent;
	count++;
	if (curPercent == 100)
	{ 

		CCLOG("Succeed downloading file %d", count);
	}
		
	
	ProgressMessage *progressData = new ProgressMessage();
	progressData->percent = curPercent;
	progressData->manager = manager;

	Updater::Message *msg = new Updater::Message();
	msg->what = UPDATER_MESSAGE_PROGRESS;
	msg->obj = progressData;

	manager->_schedule->sendMessage(msg);

	
	return 0;
}

void MergeFile(const string* sfilenames, const int num, const char* dfilename)
{
	ofstream outfile(dfilename, ios::binary);
	if (!outfile)
	{
		cerr << "no found " << endl;
		exit(1);
	}

	for (int i = 0; i < num; i++)
	{
		const string sfile = *sfilenames;
		ifstream infile(sfile, ios::binary);
		if (!infile)
		{
			cerr << "no found " << endl;
			exit(1);
		}

		infile.seekg(0, ios::end);      //设置文件指针到文件流的尾部
		int filesize = infile.tellg();
		infile.seekg(0, ios::beg);

		char *buffer = new char[filesize];
		infile.read(buffer, filesize);

		std::cout << infile.gcount() << " could be read";


		outfile.write(buffer, filesize);


		infile.close();

		remove(sfile.c_str());

		sfilenames++;
	}
	outfile.close();
}

bool Updater::download(const char* fileUrl, const char* filePath)
{
	// setup our output filename

	string filename(filePath);

	// Create a file to save package.
	ofstream fp;
	fp.open(filename.c_str(), ios::binary);

	_curl = curl_easy_init();
	if (!_curl)
	{
		sendErrorMessage(kNetwork);
		CCLOG("Updater::download(%s, %s) Can not init curl!", fileUrl, filePath);
		return false;
	}

	this->sendStateMessage(kDownStart);

	CURLcode res;
	curl_easy_setopt(_curl, CURLOPT_URL, fileUrl);
	curl_easy_setopt(_curl, CURLOPT_WRITEFUNCTION, downloadWriteFunc);
	curl_easy_setopt(_curl, CURLOPT_WRITEDATA, &fp);
	curl_easy_setopt(_curl, CURLOPT_NOPROGRESS, false);
	curl_easy_setopt(_curl, CURLOPT_PROGRESSFUNCTION, downloadProgressFunc);
	curl_easy_setopt(_curl, CURLOPT_PROGRESSDATA, this);
	curl_easy_setopt(_curl, CURLOPT_FRESH_CONNECT, 1);
	res = curl_easy_perform(_curl);
	curl_easy_cleanup(_curl);
	if (res != 0)
	{
		sendErrorMessage(kNetwork);
		CCLOG("Error when download package");
		fp.close();
		return false;
	}

	CCLOG("Succeed downloading file %s", fileUrl);

	fp.close();
	this->sendStateMessage(kDownDone);
	return true;
}

bool Updater::downloadMuti(const char* fileUrl, const char* filePath)
{
	double size = get_download_size(fileUrl);
	double partSize = INT(size / PACKAGE_PARIT);
	_packageTotalToDownload = size;

	_myProgress = (dl_progress)malloc(sizeof(struct progress) * PACKAGE_PARIT);
	memset(_myProgress, 0, sizeof(struct progress) * PACKAGE_PARIT);

	ofstream fileparts[PACKAGE_PARIT];
	string filePartNames[PACKAGE_PARIT];
	CURL *single_handles[PACKAGE_PARIT];
	CURLM *multi_handle;
	CURLMsg *msg;
	int msgs_left;
	int still_running = 0;
	double segLocation = 0;

	for (int i = 0; i < PACKAGE_PARIT; i++)
	{
		if (i == PACKAGE_PARIT - 1)
			partSize = size - (PACKAGE_PARIT - 1)*partSize;

		time_t seconds;
		seconds = time(NULL);
		memset(_myProgress[i].byteInterval, 0, sizeof(_myProgress[i].byteInterval));
		_myProgress[i].startTime = seconds;
		_myProgress[i].id = i;
		_myProgress[i].segmentSize = partSize;
		_myProgress[i].totalDownloaded = 0;
		_myProgress[i].handler = this;

		// setup our output filename
		char tmpFilePath[128] = { 0 };
		snprintf(tmpFilePath, sizeof(tmpFilePath), "_%d.bin", i);
		string filename(filePath);
		int len = filename.length();
		filename.replace(len - 4, len - 1, tmpFilePath);
		filePartNames[i] = filename;

		// allocate curl handle for each segment
		single_handles[i] = curl_easy_init();
		if (!single_handles[i])
		{
			sendErrorMessage(kNetwork);
			CCLOG("Updater::download(%s, %s) Can not init curl!", fileUrl, filePath);
			return false;
		}

		
		fileparts[i].open(filename.c_str(), ios::binary);

		double nextPart = 0;
		if (i == PACKAGE_PARIT - 1)
		{
			nextPart = size - 1;
		}
		else
		{
			nextPart = segLocation + partSize - 1;
		}

		char range[64] = { 0 };
		snprintf(range, sizeof(range), "%.0f-%.0f", segLocation, nextPart);
		memcpy(_myProgress[i].byteInterval, range, strlen(range));

		// set some curl options.
		curl_easy_setopt(single_handles[i], CURLOPT_URL, fileUrl);
		curl_easy_setopt(single_handles[i], CURLOPT_HEADER, 0);
		curl_easy_setopt(single_handles[i], CURLOPT_RANGE, range);  //设置range请求
		curl_easy_setopt(single_handles[i], CURLOPT_WRITEFUNCTION, downloadWriteFunc);//设置接收到文件内容回调
		curl_easy_setopt(single_handles[i], CURLOPT_WRITEDATA, &fileparts[i]); //写数据回调的最后一个参数
		curl_easy_setopt(single_handles[i], CURLOPT_NOPROGRESS, 0); //设置进度回调功能
		curl_easy_setopt(single_handles[i], CURLOPT_PROGRESSFUNCTION, downloadMutiProgressFunc); //设置进度回调函数
		curl_easy_setopt(single_handles[i], CURLOPT_PROGRESSDATA, &_myProgress[i]); //设置进度回调函数的第一个参数

		segLocation += partSize;
	}

	this->sendStateMessage(kDownStart);

	multi_handle = curl_multi_init();

	// add all individual transfers to the stack
	for (int i = 0; i < PACKAGE_PARIT; i++)
	{
		curl_multi_add_handle(multi_handle, single_handles[i]);
	}

	/*
	* 调用curl_multi_perform函数执行curl请求
	* url_multi_perform返回CURLM_CALL_MULTI_PERFORM时，表示需要继续调用该函数直到返回值不是CURLM_CALL_MULTI_PERFORM为止
	* running_handles变量返回正在处理的easy curl数量，running_handles为0表示当前没有正在执行的curl请求
	*/
	while (CURLM_CALL_MULTI_PERFORM == curl_multi_perform(multi_handle, &still_running))
	{
		CCLOG("select: ", multi_handle);
	}

	/**
	* 为了避免循环调用curl_multi_perform产生的cpu持续占用的问题，采用select来监听文件描述符
	*/
	do
	{
		if (-1 == curl_multi_select(multi_handle))
		{
			fprintf(stderr, "Could not select the error\n");
			break;
		}
		else
		{
			while (CURLM_CALL_MULTI_PERFORM == curl_multi_perform(multi_handle, &still_running))
			{
				CCLOG("select: ", multi_handle);
			}
		}
	} while (still_running);

	while ((msg = curl_multi_info_read(multi_handle, &msgs_left)))
	{
		if (msg->msg == CURLMSG_DONE)
		{
			int index, found = 0;

			for (index = 0; index < PACKAGE_PARIT; index++)
			{
				found = (msg->easy_handle == single_handles[index]);
				if (found)
					break;
			}

			if (index == PACKAGE_PARIT)
				fprintf(stderr, "curl not found\n");
		}
	}

	for (int i = 0; i < PACKAGE_PARIT; i++)
	{
		curl_multi_remove_handle(multi_handle, single_handles[i]);
	}

	// free up the curl handles
	for (int i = 0; i < PACKAGE_PARIT; i++)
	{
		curl_easy_cleanup(single_handles[i]);
		fileparts[i].close();
	}

	// clean up our multi handle
	curl_multi_cleanup(multi_handle);

	free(_myProgress);


	MergeFile(filePartNames, PACKAGE_PARIT, filePath);

	CCLOG("Succeed downloading file %s", fileUrl);
	this->sendStateMessage(kDownDone);
	return true;
}


int Updater::curl_multi_select(CURLM *multi_handle)
{
	struct timeval timeout;
	int rc;					// return code
	fd_set fdread;
	fd_set fdwrite;
	fd_set fdexcep;			// file descriptor exception
	int maxfd = -1;

	long curl_timeo = -1;

	FD_ZERO(&fdread);
	FD_ZERO(&fdwrite);
	FD_ZERO(&fdexcep);

	// set a suitable timeout to play with
	curl_multi_timeout(multi_handle, &curl_timeo);
	if (curl_timeo < 0)
		curl_timeo = 1000;

	timeout.tv_sec = curl_timeo / 1000;
	timeout.tv_usec = (curl_timeo % 1000) * 1000;

	curl_multi_fdset(multi_handle, &fdread, &fdwrite, &fdexcep, &maxfd);

	if (maxfd == -1)
	{
		SHORT_SLEEP;
		rc = 0;
	}
	else
		rc = select(maxfd + 1, &fdread, &fdwrite, &fdexcep, &timeout);

	switch (rc)
	{
	case -1:
		return -1;
		break;
	case 0:
		/* timeout */
	default:
		return 0;
		break;
	}
}



void Updater::setDelegate(UpdaterDelegateProtocol *delegate)
{
	_delegate = delegate;
}

void Updater::registerScriptHandler(int handler)
{
	unregisterScriptHandler();
	_scriptHandler = handler;
}

void Updater::unregisterScriptHandler(void)
{
	ScriptEngineManager::getInstance()->getScriptEngine()->
		removeScriptHandler(_scriptHandler);
	_scriptHandler = 0;
}

void Updater::setConnectionTimeout(unsigned int timeout)
{
	_connectionTimeout = timeout;
}

unsigned int Updater::getConnectionTimeout()
{
	return _connectionTimeout;
}

void Updater::sendSuccMessage()
{
	// Record updated version and remove downloaded zip file
	Message *msg = new Message();
	msg->what = UPDATER_MESSAGE_UPDATE_SUCCEED;
	msg->obj = this;

	_schedule->sendMessage(msg);
	_updateType = kUpdateUndefined;
}

void Updater::sendErrorMessage(Updater::ErrorCode code)
{
	Message *msg = new Message();
	msg->what = UPDATER_MESSAGE_ERROR;

	ErrorMessage *errorMessage = new ErrorMessage();
	errorMessage->code = code;
	errorMessage->manager = this;
	msg->obj = errorMessage;

	_schedule->sendMessage(msg);
}

void Updater::sendStateMessage(Updater::StateCode code)
{
	Message *msg = new Message();
	msg->what = UPDATER_MESSAGE_STATE;

	StateMessage *stateMessage = new StateMessage();
	stateMessage->code = code;
	stateMessage->manager = this;
	msg->obj = stateMessage;

	_schedule->sendMessage(msg);
}

void Updater::clearTid()
{
	if (_tid)
	{
		delete _tid;
		_tid = NULL;
	}
}

bool Updater::isAvailable()
{
	if (_tid) return false;
	if (_updateType != kUpdateUndefined)
	{

		CCLOG("Update is running!");
		return false;
	}
	return true;
}

void Updater::addProgressHandler(ccProgressCallback callBack)
{
	_progressCallBack = callBack;
}

// Implementation of UpdaterHelper

Updater::Helper::Helper()
{
	//CCLOG("Updater::Helper::Helper()");
	_messageQueue = new list<Message*>();
}

Updater::Helper::~Helper()
{
	//TODO 2014-08-19 zrong
	// The Helper::update lost the lastest message in MacOS.
	// But in XCode debug mode, the message is correct.
	// Add the CCLOG in here can fix it .
	// It's a stonger bug. Perhaps about multi-thread or lua.
	CCLOG("Updater::Helper::~Helper()");
	delete _messageQueue;
}

void Updater::Helper::sendMessage(Message *msg)
{
	_messageQueueMutex.lock();
	_messageQueue->push_back(msg);
	_messageQueueMutex.unlock();
}

void Updater::Helper::update(float dt)
{
	Message *msg = NULL;

	// Returns quickly if no message
	_messageQueueMutex.lock();
	//CCLOG("_messageQueue:%d", _messageQueue->size());
	if (0 == _messageQueue->size())
	{
		_messageQueueMutex.unlock();
		return;
	}

	// Gets message
	msg = *(_messageQueue->begin());
	_messageQueue->pop_front();
	_messageQueueMutex.unlock();

	switch (msg->what) {
	case UPDATER_MESSAGE_UPDATE_SUCCEED:
		handleUpdateSucceed(msg);
		break;
	case UPDATER_MESSAGE_STATE:
		handlerState(msg);
		break;
	case UPDATER_MESSAGE_PROGRESS:
		handlerProgress(msg);
		break;
	case UPDATER_MESSAGE_ERROR:
		handlerError(msg);
		break;
	default:
		break;
	}

	delete msg;
}

void Updater::Helper::handleUpdateSucceed(Message *msg)
{
	Updater* manager = (Updater*)msg->obj;
	CCLOG("Updater::helper::handlerUpdateSuccessed pointer %u handler %u", manager, manager->_scriptHandler);
	if (manager)
	{
		manager->clearOnSuccess();

		if (manager->_delegate)
		{
			manager->_delegate->onSuccess();
		}
		if (manager->_scriptHandler)
		{
			//CCScriptEngineManager::sharedManager()
			//	->getScriptEngine()
			//	->executeEvent(
			//	manager->_scriptHandler,
			//	"success",
			//	CCString::create("success"),
			//	"CCString"
			//	);
		}
	}
	else
	{
		CCLOG("Updater::Helper::handlerUpdateSuccessed Can not get the Update instance!");
	}
}

void Updater::Helper::handlerState(Message *msg)
{
	StateMessage* stateMsg = (StateMessage*)msg->obj;
	if (stateMsg->manager->_delegate)
	{
		stateMsg->manager->_delegate->onState(stateMsg->code);
	}
	if (stateMsg->manager->_scriptHandler)
	{
		std::string stateMessage;
		switch ((StateCode)stateMsg->code)
		{
		case kDownStart:
			stateMessage = "downloadStart";
			break;

		case kDownDone:
			stateMessage = "downloadDone";
			break;

		case kUncompressStart:
			stateMessage = "uncompressStart";
			break;
		case kUncompressDone:
			stateMessage = "uncompressDone";
			break;

		default:
			stateMessage = "stateUnknown";
		}

		//ScriptEngineManager::getInstance()
		//	->getScriptEngine()
		//	->executeEvent(
		//	stateMsg->manager->_scriptHandler,
		//	"state",
		//	CCString::create(stateMessage.c_str()),
		//	"CCString");
	}

	delete ((StateMessage*)msg->obj);
}

void Updater::Helper::handlerError(Message* msg)
{
	ErrorMessage* errorMsg = (ErrorMessage*)msg->obj;
	if (errorMsg->manager->_delegate)
	{
		errorMsg->manager->_delegate
			->onError(errorMsg->code);
	}
	if (errorMsg->manager->_scriptHandler)
	{
		std::string errorMessage;
		switch (errorMsg->code)
		{
		case kCreateFile:
			errorMessage = "errorCreateFile";
			break;

		case kNetwork:
			errorMessage = "errorNetwork";
			break;

		case kUncompress:
			errorMessage = "errorUncompress";
			break;

		default:
			errorMessage = "errorUnknown";
		}

		/*CCScriptEngineManager::sharedManager()
			->getScriptEngine()
			->executeEvent(
			errorMsg->manager->_scriptHandler,
			"error",
			CCString::create(errorMessage.c_str()),
			"CCString"
			);*/
	}

	delete ((ErrorMessage*)msg->obj);
}

void Updater::Helper::handlerProgress(Message* msg)
{
	ProgressMessage* progMsg = (ProgressMessage*)msg->obj;
	if (progMsg->manager->_delegate)
	{
		progMsg->manager->_delegate
			->onProgress(progMsg->percent);
	}
	if (progMsg->manager->_scriptHandler)
	{
		//char buff[10];
		//sprintf(buff, "%d", ((ProgressMessage*)msg->obj)->percent);
		//CCScriptEngineManager::sharedManager()
		//	->getScriptEngine()
		//	->executeEvent(
		//	progMsg->manager->_scriptHandler,
		//	"progress",
		//	CCInteger::create(progMsg->percent),
		//	"CCInteger"
		//	);
	}

	if (progMsg->manager->_progressCallBack)
	{
		progMsg->manager->_progressCallBack(progMsg->percent);
	}

	delete (ProgressMessage*)msg->obj;
}

#endif // CC_PLATFORM_WINRT
