#ifndef TIMETREE_UTILS_HPP
#define TIMETREE_UTILS_HPP
#include <functional>
#include <string>
#include "vendor/json.hpp"
using json = nlohmann::json;

// Application-supplied page loader. since == -1 requests the initial full-sync
// page; otherwise since is the cursor returned by the preceding page.
// Return parsed JSON containing events/chunk/since, or throw on failure.
using TTEventFetcher = std::function<json(long long calendar_id, long long since)>;

// Find an exact calendar name in a parsed calendar-list response.
json selectCalendarDefinition(const json& response, const std::string& name);
#endif
