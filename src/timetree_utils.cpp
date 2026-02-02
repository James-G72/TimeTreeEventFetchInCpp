#include "nlohmann/json.hpp"
#include "../include/timetree_utils.h"
#include "curlpp/cURLpp.hpp"
#include "curlpp/Options.hpp"
#include "curlpp/Easy.hpp"
#include <list>
#include <string>
#include <sstream>
#include <random>

using json = nlohmann::json;

// Generate a 
std::string generateSimpleUUID() {
    std::string uuid = "";
    for (int i = 0; i < 32; i++) {
        uuid += std::string(random(), 16);
    }
    return uuid;
}

// Login to TimeTree using credentials and obtain a session token
char const * getTTSessionToken(char const * username, char const * password) {
    try {
        curlpp::Cleanup myCleaner;
        curlpp::Easy request;

        request.setOpt(new curlpp::options::Url(API_URL));

        std::list<std::string> headers;
        headers.push_back("uid: " + std::string(username));
        headers.push_back("password: " + std::string(password));
        headers.push_back("uuid: " + generateSimpleUUID());

        request.setOpt(new curlpp::options::HttpHeader(headers));

        std::stringstream responseBody;
        request.setOpt(new curlpp::options::WriteStream(&responseBody));

        request.perform();

        // Access the content
        std::string responseString = responseBody.str();
        std::cout << "Body: " << responseString << std::endl;

        }
    catch (curlpp::RuntimeError & e) {
        std::cerr << e.what() << std::endl;
    }
    return nullptr;
}

