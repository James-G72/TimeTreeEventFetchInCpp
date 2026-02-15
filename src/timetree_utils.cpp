#include "timetree_utils.h"
#include "curlpp/cURLpp.hpp"
#include "curlpp/Options.hpp"
#include "curlpp/Easy.hpp"
#include <list>
#include <string>
#include <sstream>
#include <random>
using namespace std;

// Generate a UUID string of 32 hex characters.
string generateSimpleUUID() {
    string uuid = "";
    for (int i = 0; i < 32; i++) {
        uuid += string(random(), 16);
    }
    return uuid;
}

// Login to TimeTree using credentials and obtain a session token
char const * getTTSessionToken(char const * username, char const * password) {
    try {
        cout << "Fetching TimeTree session for user: " << username << endl;

        curlpp::Cleanup myCleaner;
        curlpp::Easy request;

        request.setOpt(new curlpp::options::Url(string(API_URL) + "/auth/email/signin"));

        string payload = "{uid: " + string(username) +
                         "password: " + string(password)+
                         "uuid: " + string(generateSimpleUUID()) + "}";
        request.setOpt(new curlpp::options::PostFields(payload));
        request.setOpt(new curlpp::options::PostFieldSize(payload.length()));

        list<std::string> headers;
        headers.push_back("Content-Type: application/json");
        headers.push_back("X-Timetreea:" + string(API_AGENT));
        request.setOpt(new curlpp::options::HttpHeader(headers));

        std::stringstream responseBody;
        request.setOpt(new curlpp::options::WriteStream(&responseBody));

        request.perform();

        // Access the content
        string responseString = responseBody.str();
        cout << "Body: " << responseString << endl;

        }
    catch (curlpp::RuntimeError & e) {
        cerr << e.what() << endl;
    }
    return nullptr;
}

