#include "http.h"
#include "httpc.h"

#include "utils.h"

// libmd5-rfc includes
#include "md5/md5.h"

#define ETAG_LENGTH 512
#define CONTENT_MD5_LENGTH 24

HTTPC httpc;
void httpGet(const char* url, u8** buf, u32* size, const bool verbose, HTTPResponseInfo* info) {
	httpcContext context;
	CHECK(httpc.OpenContext(&context, HTTPC_METHOD_GET, (char*)url, 0), "Could not open HTTP context");
	// Add User Agent field (required by Github API calls)
	CHECK(httpc.AddRequestHeaderField(&context, (char*)"User-Agent", (char*)"LUMA-UPDATER"), "Could not set User Agent");

	CHECK(httpc.BeginRequest(&context), "Could not begin request");

	u32 statuscode = 0;
	CHECK(httpc.GetResponseStatusCode(&context, &statuscode), "Could not get status code");
	if (statuscode != 200) {
		// Handle 3xx codes
		if (statuscode >= 300 && statuscode < 400) {
			char newUrl[1024];
			CHECK(httpc.GetResponseHeader(&context, (char*)"location", newUrl, 1024), "Could not get Location header for 3xx reply");
			CHECK(httpc.CloseContext(&context), "Could not close HTTP context");
			httpGet(newUrl, buf, size, verbose, info);
			return;
		}
		throw std::runtime_error(formatErrMessage("Non-200 status code", statuscode));
	}

	// Retrieve extra info if required
	if (info != nullptr) {
		char etagChr[ETAG_LENGTH] = { 0 };
		if (httpc.GetResponseHeader(&context, (char*)"ETag", etagChr, ETAG_LENGTH) == 0) {
			info->etag = std::string(etagChr);
		}
		char contentmd5Chr[CONTENT_MD5_LENGTH] = { 0 };
		if (httpc.GetResponseHeader(&context, (char*)"Content-MD5", contentmd5Chr, CONTENT_MD5_LENGTH) == 0) {
			info->contentmd5 = std::string(contentmd5Chr);
		}
	}

	u32 pos = 0;
	u32 dlstartpos = 0;
	u32 dlpos = 0;
	Result dlret = HTTPC_RESULTCODE_DOWNLOADPENDING;

	CHECK(httpc.GetDownloadSizeState(&context, &dlstartpos, size), "Could not get file size");

	*buf = (u8*)std::malloc(*size);
	if (*buf == NULL) throw std::runtime_error(formatErrMessage("Could not allocate enough memory", *size));
	std::memset(*buf, 0, *size);

	while (pos < *size && dlret == (s32)HTTPC_RESULTCODE_DOWNLOADPENDING)
	{
		u32 sz = *size - pos;
		dlret = httpc.ReceiveData(&context, *buf + pos, sz);
		CHECK(httpc.GetDownloadSizeState(&context, &dlpos, NULL), "Could not get file size");
		pos = dlpos - dlstartpos;
		if (verbose) {
			logPrintf("Download progress: %lu / %lu", dlpos, *size);
			gfxFlushBuffers();
		}
	}
	
	if (verbose) {
		logPrintf("\n");
	}

	CHECK(httpc.CloseContext(&context), "Could not close HTTP context");
}


bool httpCheckETag(std::string etag, const u8* fileData, const u32 fileSize) {
	std::string md5;
	
	// Strip quotes from either side of the etag
	if (etag[0] == '"') {
		md5 = etag.substr(1, etag.length() - 2);
	}

	return httpCheckMD5(md5, fileData, fileSize);
}

bool httpCheckContentMD5(std::string contentmd5, const u8* fileData, const u32 fileSize) {
	std::string md5;
	
	// Base64 decode Content-MD5 string
	md5 = base64_decode(contentmd5);
	
	return httpCheckMD5(md5, fileData, fileSize);
}

bool httpCheckMD5(std::string md5, const u8* fileData, const u32 fileSize) {
	// Get MD5 bytes from MD5 string
	md5_byte_t expected[MD5_DIGEST_LENGTH];
	const char* md5chr = md5.c_str();
	for (u8 i = 0; i < MD5_DIGEST_LENGTH; i++) {
		std::sscanf(md5chr + (i * 2), "%02x", &expected[i]);
	}

	// Calculate MD5 hash of downloaded archive
	md5_state_t state;
	md5_byte_t result[MD5_DIGEST_LENGTH];
	md5_init(&state);
	md5_append(&state, (const md5_byte_t *)fileData, fileSize);
	md5_finish(&state, result);

	return memcmp(expected, result, MD5_DIGEST_LENGTH) == 0;
}
