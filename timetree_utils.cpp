#include "timetree_utils.hpp"
#include <stdexcept>

json selectCalendarDefinition(const json& response, const std::string& name) {
    const auto& definitions = response.at("calendars");
    if (!definitions.is_array())
        throw std::runtime_error("Expected 'calendars' to be a JSON array");
    for (const auto& definition : definitions) {
        if (definition.value("name", "") == name) return definition;
    }
    throw std::runtime_error("Calendar '" + name + "' was not found");
}
