#include <cstdarg>
#include <cstdio>
#include <cstring>
#include <sstream>
#include <string>

#include <arpa/inet.h>
#include <errno.h>
#include <fcntl.h>
#include <sys/socket.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <sys/un.h>
#include <unistd.h>

#include "gpcommon.h"
#include "s3conf.h"
#include "s3log.h"
#include "s3utils.h"

#ifndef S3_STANDALONE
extern "C" {
void write_log(const char* fmt, ...) __attribute__((format(printf, 1, 2)));
}
#endif

using std::string;
using std::stringstream;

// configurable parameters
int32_t s3ext_segid = -1;
int32_t s3ext_segnum = -1;

string s3ext_logserverhost;
int32_t s3ext_loglevel = -1;
int32_t s3ext_logtype = -1;
int32_t s3ext_logserverport = -1;
int32_t s3ext_logsock_udp = -1;
struct sockaddr_in s3ext_logserveraddr;

// not thread safe
bool InitConfig(S3Params& params, const string& conf_path, const string section = "default") {
    if (conf_path == "") {
#ifndef S3_STANDALONE
        write_log("Config file is not specified\n");
#else
        S3ERROR("Config file is not specified");
#endif
        return false;
    }

    Config* s3cfg = new Config(conf_path);
    if (s3cfg == NULL || !s3cfg->Handle()) {
#ifndef S3_STANDALONE
        write_log("Failed to parse config file \"%s\", or it doesn't exist\n", conf_path.c_str());
#else
        S3ERROR("Failed to parse config file \"%s\", or it doesn't exist", conf_path.c_str());
#endif
        delete s3cfg;
        return false;
    }

    string content = s3cfg->Get(section.c_str(), "loglevel", "WARNING");
    s3ext_loglevel = getLogLevel(content.c_str());

#ifndef S3_STANDALONE_CHECKCLOUD
    content = s3cfg->Get(section.c_str(), "logtype", "INTERNAL");
    s3ext_logtype = getLogType(content.c_str());

    params.setDebugCurl(to_bool(s3cfg->Get(section.c_str(), "debug_curl", "false")));
#endif

    params.setCred(s3cfg->Get(section.c_str(), "accessid", ""),
                   s3cfg->Get(section.c_str(), "secret", ""),
                   s3cfg->Get(section.c_str(), "token", ""));

    s3ext_logserverhost = s3cfg->Get(section.c_str(), "logserverhost", "127.0.0.1");

    bool ret = s3cfg->Scan(section.c_str(), "logserverport", "%d", &s3ext_logserverport);
    if (!ret) {
        s3ext_logserverport = 1111;
    }

    int32_t scannedValue;
    ret = s3cfg->Scan(section.c_str(), "threadnum", "%d", &scannedValue);
    if (!ret) {
        S3INFO("The thread number is set to default value 4");
        params.setNumOfChunks(4);
    } else if (scannedValue > 8) {
        S3INFO("The given thread number is too big, use max value 8");
        params.setNumOfChunks(8);
    } else if (scannedValue < 1) {
        S3INFO("The given thread number is too small, use min value 1");
        params.setNumOfChunks(1);
    } else {
        params.setNumOfChunks(scannedValue);
    }

    ret = s3cfg->Scan(section.c_str(), "chunksize", "%d", &scannedValue);
    if (!ret) {
        S3INFO("The chunksize is set to default value 64MB");
        params.setChunkSize(64 * 1024 * 1024);
    } else if (scannedValue > 128 * 1024 * 1024) {
        S3INFO("The given chunksize is too large, use max value 128MB");
        params.setChunkSize(128 * 1024 * 1024);
    } else if (scannedValue < 8 * 1024 * 1024) {
        // multipart uploading requires the chunksize larger than 5MB(only the last part to upload
        // could be smaller than 5MB)
        S3INFO("The given chunksize is too small, use min value 8MB");
        params.setChunkSize(8 * 1024 * 1024);
    } else {
        params.setChunkSize(scannedValue);
    }

    ret = s3cfg->Scan(section.c_str(), "low_speed_limit", "%d", &scannedValue);
    if (!ret) {
        S3INFO("The low_speed_limit is set to default value %d bytes/s", 10240);
        params.setLowSpeedLimit(10240);
    } else {
        params.setLowSpeedLimit(scannedValue);
    }

    ret = s3cfg->Scan(section.c_str(), "low_speed_time", "%d", &scannedValue);
    if (!ret) {
        S3INFO("The low_speed_time is set to default value %d seconds", 60);
        params.setLowSpeedTime(60);
    } else {
        params.setLowSpeedTime(scannedValue);
    }

    params.setEncryption(to_bool(s3cfg->Get(section.c_str(), "encryption", "true")));

    params.setAutoCompress(to_bool(s3cfg->Get(section.c_str(), "autocompress", "true")));

#ifdef S3_STANDALONE
    s3ext_segid = 0;
    s3ext_segnum = 1;
#else
    s3ext_segid = GpIdentity.segindex;
    s3ext_segnum = GpIdentity.numsegments;
#endif

    delete s3cfg;

    return true;
}
