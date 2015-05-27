#ifndef URL_CHECK_H_
#define URL_CHECK_H_

#define URL_CHECK_BUF_SIZE 200
typedef int (*isUrlRealM3U8Fun)(const char* url);

typedef enum URL_CHECK_TYPE {
    URL_TYPE_NONE           =0,
    URL_TYPE_HTTPLIVE,

} URL_CHECK_TYPE;

typedef struct UrlCheckContext {
    char url[URL_CHECK_BUF_SIZE];
    URL_CHECK_TYPE url_type;
    isUrlRealM3U8Fun urlCheck;
}UrlCheckContext_t;


namespace android {

class UrlCheckHelper
{
public:
    static UrlCheckHelper *getInstance();
    int32_t isUrlRealM3U8 (const char *url);
    virtual ~UrlCheckHelper() {};

private:
    UrlCheckHelper();
    static UrlCheckHelper *singleton;
    void* mHandle;

    UrlCheckContext_t mUrlCtx;
};

}   //namespace android

#endif //URL_CHECK_H_

