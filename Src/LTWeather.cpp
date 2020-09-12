/// @file       LTWeather.cpp
/// @brief      Fetch real weather information from AWC
/// @see        https://www.aviationweather.gov/dataserver/example?datatype=metar
/// @see        Example request: Latest weather 25 statute miles around EDLE, limited to the fields we are interested in:
///             https://www.aviationweather.gov/adds/dataserver_current/httpparam?dataSource=metars&requestType=retrieve&format=xml&radialDistance=100;-118.9385,33.4036&hoursBeforeNow=2&mostRecent=true&fields=raw_text,station_id,latitude,longitude,altim_in_hg
/// @details    Example response:
///                 @code{.xml}
///                     <response xmlns:xsd="http://www.w3.org/2001/XMLSchema" xmlns:xsi="http://www.w3.org/2001/XML-Schema-instance" version="1.2" xsi:noNamespaceSchemaLocation="http://aviationweather.gov/adds/schema/metar1_2.xsd">
///                     <request_index>71114711</request_index>
///                     <data_source name="metars"/>
///                     <request type="retrieve"/>
///                     <errors/>
///                     <warnings/>
///                     <time_taken_ms>249</time_taken_ms>
///                     <data num_results="1">
///                     <METAR>
///                     <raw_text>KL18 222035Z AUTO 23009G16KT 10SM CLR A2990 RMK AO2</raw_text>
///                     <station_id>KL18</station_id>
///                     <latitude>33.35</latitude>
///                     <longitude>-117.25</longitude>
///                     <altim_in_hg>29.899607</altim_in_hg>
///                     </METAR>
///                     </data>
///                     </response>
///                 @endcode
///
///             Example empty response (no weather reports found):
///                 @code{.xml}
///                 <response xmlns:xsd="http://www.w3.org/2001/XMLSchema" xmlns:xsi="http://www.w3.org/2001/XML-Schema-instance" version="1.2" xsi:noNamespaceSchemaLocation="http://aviationweather.gov/adds/schema/metar1_2.xsd">
///                 <request_index>60222216</request_index>
///                 <data_source name="metars"/>
///                 <request type="retrieve"/>
///                 <errors/>
///                 <warnings/>
///                 <time_taken_ms>7</time_taken_ms>
///                 <data num_results="0"/>
///                 </response>
///                 @endcode
///
///             Example error response:
///                 @code{.xml}
///                 <response xmlns:xsd="http://www.w3.org/2001/XMLSchema" xmlns:xsi="http://www.w3.org/2001/XML-Schema-instance" version="1.2" xsi:noNamespaceSchemaLocation="http://aviationweather.gov/adds/schema/metar1_2.xsd">
///                 <request_index>59450188</request_index>
///                 <data_source name="metars"/>
///                 <request type="retrieve"/>
///                 <errors>
///                 <error>Query must be constrained by time</error>
///                 </errors>
///                 <warnings/>
///                 <time_taken_ms>0</time_taken_ms>
///                 </response>
///                 @endcode
///
/// @author     Birger Hoppe
/// @copyright  (c) 2018-2020 Birger Hoppe
/// @copyright  Permission is hereby granted, free of charge, to any person obtaining a
///             copy of this software and associated documentation files (the "Software"),
///             to deal in the Software without restriction, including without limitation
///             the rights to use, copy, modify, merge, publish, distribute, sublicense,
///             and/or sell copies of the Software, and to permit persons to whom the
///             Software is furnished to do so, subject to the following conditions:\n
///             The above copyright notice and this permission notice shall be included in
///             all copies or substantial portions of the Software.\n
///             THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
///             IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
///             FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
///             AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
///             LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
///             OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
///             THE SOFTWARE.

#include "LiveTraffic.h"

//
// MARK: Weather Network Request handling
//

/// The request URL, parameters are in this order: radius, longitude, latitude
const char* WEATHER_URL="https://www.aviationweather.gov/adds/dataserver_current/httpparam?dataSource=metars&requestType=retrieve&format=xml&radialDistance=%.f;%.2f,%.2f&hoursBeforeNow=2&mostRecent=true&fields=raw_text,station_id,latitude,longitude,altim_in_hg";

/// Maximum search radius [nm]
constexpr float MAX_WEATHER_RADIUS_NM = 100.0f;

// Error messages
#define ERR_WEATHER_REQU        "Could not request weather from aviationweather.gov: HTTP return code %d"
#define ERR_WEATHER_ERROR       "Weather request returned with error: %s"
#define WARN_NO_WEATHER         "Found no weather in a %.fnm radius"

/// return the value between two xml tags
std::string GetXMLValue (const std::string& _r, const std::string& _tag,
                         std::string::size_type& pos)
{
    // find the tag
    std::string::size_type p = _r.find(_tag, pos);
    if (p == std::string::npos)         // didn't find it
        return "";
    
    // find the beginning of the _next_ tag (we don't validate any further)
    const std::string::size_type startPos = p + _tag.size();
    pos = _r.find('<', startPos);       // where the end tag begins
    if (pos != std::string::npos)
        return _r.substr(startPos, pos-startPos);
    else {
        pos = 0;                        // we overwrite pos with npos...reset to buffer's beginning for next search
        return "";
    }
}

/// @brief Process the response from aviationweather.com
/// @details Response is in XML format. (JSON is not available.)
///          We aren't doing a full XML parse here but rely on the
///          fairly static structure:
///          We straight away search for:
///            `<error>` Indicates just that and stops interpretation.\n
///            `<station_id>`, `<raw_text>`, `<latitude>`, `<longitude>`,
///            and`<altim_in_hg>` are the values we are interested in.
bool WeatherProcessResponse (const std::string& _r)
{
    float lat = NAN;
    float lon = NAN;
    float hPa = NAN;
    std::string stationId;
    std::string METAR;
    
    // Any error?
    std::string::size_type pos = 0;
    std::string val = GetXMLValue(_r, "<error>", pos);
    if (!val.empty()) {
        LOG_MSG(logERR, ERR_WEATHER_ERROR, val.c_str());
        return false;
    }
    
    // find the pressure
    val = GetXMLValue(_r, "<altim_in_hg>", pos);
    if (!val.empty()) {
        hPa = std::stof(val) * (float)HPA_per_INCH;
        
        // We fetch the other fields in order of appearance, but need to start once again from the beginning of the buffer
        pos = 0;
        // Try fetching METAR and station_id
        METAR = GetXMLValue(_r, "<raw_text>", pos);
        stationId = GetXMLValue(_r, "<station_id>", pos);

        // then let's see if we also find the weather station's location
        val = GetXMLValue(_r, "<latitude>", pos);
        if (!val.empty())
            lat = std::stof(val);
        val = GetXMLValue(_r, "<longitude>", pos);
        if (!val.empty())
            lon = std::stof(val);
        
        // tell ourselves what we found
        dataRefs.SetWeather(hPa, lat, lon, stationId, METAR);
        return true;
    }

    // didn't find weather!
    return false;
}

/// CURL callback just adding up data
size_t WeatherFetchCB(char *ptr, size_t, size_t nmemb, void* userdata)
{
    // copy buffer to our std::string
    std::string& readBuf = *reinterpret_cast<std::string*>(userdata);
    readBuf.append(ptr, nmemb);
    
    // all consumed
    return nmemb;
}

// check on X-Plane.org what version's available there
// This function would block. Idea is to call it in a thread like with std::async
bool WeatherFetch (float _lat, float _lon, float _radius_nm)
{
    // This is a thread main function, set thread's name
    SET_THREAD_NAME("LT_Weather");
    
    bool bRet = false;
    try {
        char curl_errtxt[CURL_ERROR_SIZE];
        char url[255];
        std::string readBuf;
        
        // initialize the CURL handle
        CURL *pCurl = curl_easy_init();
        if (!pCurl) {
            LOG_MSG(logERR,ERR_CURL_EASY_INIT);
            return false;
        }

        // Loop in case we need to re-do a request with larger radius
        bool bRepeat = false;
        do {
            bRepeat = false;

            // put together the URL, convert nautical to statute miles
            snprintf(url, sizeof(url), WEATHER_URL, _radius_nm / 1.151f, _lon, _lat);

            // prepare the handle with the right options
            readBuf.reserve(CURL_MAX_WRITE_SIZE);
            curl_easy_setopt(pCurl, CURLOPT_NOSIGNAL, 1);
            curl_easy_setopt(pCurl, CURLOPT_TIMEOUT, dataRefs.GetNetwTimeout());
            curl_easy_setopt(pCurl, CURLOPT_ERRORBUFFER, curl_errtxt);
            curl_easy_setopt(pCurl, CURLOPT_WRITEFUNCTION, WeatherFetchCB);
            curl_easy_setopt(pCurl, CURLOPT_WRITEDATA, &readBuf);
            curl_easy_setopt(pCurl, CURLOPT_USERAGENT, HTTP_USER_AGENT);
            curl_easy_setopt(pCurl, CURLOPT_URL, url);

            // perform the HTTP get request
            CURLcode cc = CURLE_OK;
            if ((cc = curl_easy_perform(pCurl)) != CURLE_OK)
            {
                // problem with querying revocation list?
                if (LTOnlineChannel::IsRevocationError(curl_errtxt)) {
                    // try not to query revoke list
                    curl_easy_setopt(pCurl, CURLOPT_SSL_OPTIONS, CURLSSLOPT_NO_REVOKE);
                    LOG_MSG(logWARN, ERR_CURL_DISABLE_REV_QU, LT_DOWNLOAD_CH);
                    // and just give it another try
                    cc = curl_easy_perform(pCurl);
                }

                // if (still) error, then log error
                if (cc != CURLE_OK)
                    LOG_MSG(logERR, ERR_CURL_NOVERCHECK, cc, curl_errtxt);
            }

            if (cc == CURLE_OK)
            {
                // CURL was OK, now check HTTP response code
                long httpResponse = 0;
                curl_easy_getinfo(pCurl, CURLINFO_RESPONSE_CODE, &httpResponse);

                // not HTTP_OK?
                if (httpResponse != HTTP_OK) {
                    LOG_MSG(logERR, ERR_WEATHER_REQU, (int)httpResponse);
                }
                else {
                    // Success: Process data
                    bRet = WeatherProcessResponse(readBuf);
                    // Not found weather yet?
                    if (!bRet) {
                        LOG_MSG(logWARN, WARN_NO_WEATHER, _radius_nm);
                        if (_radius_nm < MAX_WEATHER_RADIUS_NM) {
                            _radius_nm = MAX_WEATHER_RADIUS_NM;
                            bRepeat = true;
                        }
                    }
                }
            }
        } while (bRepeat);
        
        // cleanup CURL handle
        curl_easy_cleanup(pCurl);
    }
    catch (const std::exception& e) {
        LOG_MSG(logERR, "Fetching weather failed with exception %s", e.what());
    }
    catch (...) {
        LOG_MSG(logERR, "Fetching weather failed with exception");
    }
    
    // done
    return bRet;
}


//
// MARK: Global functions
//

/// Is currently an async operation running to refresh the airports from apt.dat?
static std::future<bool> futWeather;

// Asynchronously, fetch fresh weather information
bool WeatherUpdate (const positionTy& pos, float radius_nm)
{
    // does only make sense in a certain latitude range
    // (During XP startup irregular values >80 show up)
    if (pos.lat() >= 80.0)
        return false;
    
    // a request still underway?
    if (futWeather.valid() &&
        futWeather.wait_for(std::chrono::seconds(0)) != std::future_status::ready)
            // then stop here
            return false;

    // start another thread with the weather request
    futWeather = std::async(std::launch::async,
                            WeatherFetch, (float)pos.lat(), (float)pos.lon(), radius_nm);
    return true;
}
