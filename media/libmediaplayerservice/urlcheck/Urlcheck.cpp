/*
 * Copyright (C) 2009 The Android Open Source Project
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *      http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

//#define LOG_NDEBUG 0
#define LOG_TAG "UrlCheck"

#include <utils/Log.h>
#include <dlfcn.h>
#include <pthread.h>
#include "Urlcheck.h"

#define URL_CHECK_DEBUG 1

#if URL_CHECK_DEBUG
#define URL_CECK_LOG ALOGI
#else
#define URL_CECK_LOG ALOGI
#endif

#define URL_CHECK_ERROR ALOGE

static pthread_mutex_t url_chk_mutex = PTHREAD_RECURSIVE_MUTEX_INITIALIZER;

namespace android {

UrlCheckHelper *UrlCheckHelper::singleton = NULL;

UrlCheckHelper *UrlCheckHelper::getInstance()
{
    pthread_mutex_lock(&url_chk_mutex);
    if (singleton == NULL) {
        singleton = new UrlCheckHelper();
    }
    pthread_mutex_unlock(&url_chk_mutex);
    return singleton;
}


int32_t UrlCheckHelper::isUrlRealM3U8 (const char *url)
{
    pthread_mutex_lock(&url_chk_mutex);

    URL_CECK_LOG("isUrlRealM3U8 in, ptr: %p, url: %s", this, url);
    int32_t reallyM3U8 = 0;
    UrlCheckContext_t *p =&mUrlCtx;
    int32_t len =0;

    if (url ==NULL) {
        return 0;
    }

    len = strlen(url);
    len = len >URL_CHECK_BUF_SIZE ? URL_CHECK_BUF_SIZE : len;
    URL_CECK_LOG("UrlCheckHelper ptr: %p, url_type: %d", this, p->url_type);
    if (mHandle && p->urlCheck && (p->url_type !=URL_TYPE_NONE)) {
        if (!strncmp(p->url, url, len)) {
            reallyM3U8 = (p->url_type ==URL_TYPE_HTTPLIVE);

            URL_CECK_LOG("current url has been checked, reallyM3U8: %d", reallyM3U8);
            goto IS_URL_M3U8_OUT;
        } else {
            URL_CECK_LOG("url is not the same as last");
        }
    }

    if (mHandle ==NULL) {
        mHandle = dlopen("libstagefright.so", RTLD_NOW);
        if (mHandle ==NULL) {
            URL_CHECK_ERROR("open libstagefright lib fail");
            goto IS_URL_M3U8_OUT;
        }
    }
    if (p->urlCheck ==NULL) {
        p->urlCheck =
            (isUrlRealM3U8Fun)dlsym(
                    mHandle, "_ZN7android19AwesomePlayerHelper13isUrlRealM3U8EPKc");

        if (p->urlCheck ==NULL) {
            URL_CHECK_ERROR("dlsym from libstagefright lib fail");
            goto IS_URL_M3U8_OUT;
        }
    }

    if (p->urlCheck && (reallyM3U8 = p->urlCheck(url)) >0) {
        p->url_type =URL_TYPE_HTTPLIVE;
    } else {
        p->url_type =URL_TYPE_NONE;
    }

    memcpy(p->url, url, len);
    URL_CECK_LOG("url checked, reallyM3U8: %d", reallyM3U8);

IS_URL_M3U8_OUT:
    pthread_mutex_unlock(&url_chk_mutex);
    return reallyM3U8;
}

UrlCheckHelper::UrlCheckHelper()
{
    memset(&mUrlCtx, 0, sizeof(UrlCheckContext_t));
    mUrlCtx.urlCheck = NULL;
    mHandle = NULL;
}

}

