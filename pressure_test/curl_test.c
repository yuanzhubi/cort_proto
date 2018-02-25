#include <stdio.h>
#include <stdlib.h>

#include <curl/curl.h>

int main(int argc, char *argv[])
{
    CURL *curl;            
    CURLcode res;          
    curl = curl_easy_init();       
    if(curl != NULL){
        curl_easy_setopt(curl, CURLOPT_URL, "http://203.205.128.137/pingd?dm=news.qq.com&url=/");  
        res = curl_easy_perform(curl);
        curl_easy_cleanup(curl);
    }
    return 0;
}