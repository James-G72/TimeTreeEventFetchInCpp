# TimeTreeHandlerCpp

A small C++17 library that follows the existing Python implementation in
`C:\dev\TimeTreeEventFetch`. The original source-file layout is retained:

- `timetree_utils`: HTTP transport, login, session cookie, raw calendar JSON.
- `timetree_structs`: calendar metadata and millisecond timestamps.
- `main.cpp`: optional command-line example, excluded from the library.

## First working build on this laptop

The installed curl headers and library are MSYS builds under `C:\msys64\usr`.
Use the MSYS compiler with them. The MinGW compiler under `mingw32` needs its
own matching curl package; do not mix it with the MSYS library. curlpp is not
required: `timetree_utils.cpp` wraps libcurl directly behind the existing functions.

From PowerShell in this directory:

```powershell
& C:\msys64\usr\bin\make.exe CXX=C:/msys64/usr/bin/g++.exe AR=C:/msys64/usr/bin/ar.exe
```

This builds `libtimetree.a` and `timetree_example.exe` without running either.
Keep `C:\msys64\usr\bin` on PATH when running these MSYS executables.
For a native Windows parent project, build both this library and curl using the
same native toolchain as the parent instead. The MSYS archive is not a drop-in
library for an MSVC or MinGW parent.

## Try HTTP, then TimeTree

```powershell
.\timetree_example.exe --get https://example.com

$env:TIMETREE_USERNAME = 'your-email'
$env:TIMETREE_PASSWORD = 'your-password'
.\timetree_example.exe
Remove-Item Env:TIMETREE_PASSWORD
```

The first command tests HTTP independently. The second logs in, parses the
response using `vendor/json.hpp`, selects the calendar named `Ruth`, constructs
a single `TTCalendar` and prints its name, ID and label count. It reports an error
if `Ruth` is not found. It then fetches Ruth's events and prints the event and
recurring-series counts, followed by a live table refreshed every 10 seconds.
The table covers yesterday through the next seven days, including recurring
occurrences, and shows start/end times (UTC), title, label, author and recurring
status. Each update prints a timestamp, update number and tracked deletions.
Press **Ctrl+C** to stop. A failed refresh reports the error and retries after
10 seconds. The interval is measured after each fetch/display completes.
Credentials are read from the process environment; avoid saving
real passwords in scripts or source control. The old embedded password was
removed and should be rotated if still valid.

Requests match the Python code:

- Base URL: `https://timetreeapp.com/api/v1`
- Login: PUT `/auth/email/signin`, JSON `uid`, `password`, `uuid`.
- Header: `X-Timetreea: web/2.1.0/en`.
- User-Agent: `TimeTreeHandlerCpp/0.1`. Omitting it caused the live login to
  return HTTP 400 with API error code -401, even with valid credentials.
- Authentication: `_session_id` cookie from the login response.
- Calendars: GET `/calendars?since=0`.

The raw calendar response contains a `calendars` array. To construct objects:

```cpp
#include "timetree_structs.hpp"

UserCredentials login{username, password};
auto session = getTTSessionToken(login.username, login.password);
auto response = requestCalendars(session);
std::vector<TTCalendar> calendars;
for (const auto& definition : response.at("calendars"))
    calendars.emplace_back(session, definition, login);
```

Transport failures throw `std::runtime_error`. Non-2xx responses throw
`TTHttpError`, a `std::runtime_error` subclass with `status` and `response_body`
members for programmatic diagnosis. The body is not automatically logged.
Malformed JSON
throws a nlohmann JSON exception. Catch `std::exception` at the application
boundary, as in `main.cpp`. TLS verification stays enabled. Requests have a
10-second connection timeout and a 30-second total timeout. No passwords,
session tokens or server response bodies are included in error messages.

## Import into a CMake parent

```cmake
add_subdirectory(path/to/TimeTreeHandlerCpp)
target_link_libraries(your_application PRIVATE timetree)
```

CMake finds curl using `find_package(CURL REQUIRED)`. Public headers, C++17 and
curl link requirements propagate to the parent. The example defaults to OFF;
set `TIMETREE_BUILD_EXAMPLE=ON` to include it. Configure using a toolchain and
curl installation matching the parent project.

For this laptop's existing MSYS tools and CMake 3.10:

```powershell
New-Item -ItemType Directory -Force build | Out-Null
Push-Location build
cmake -G 'MSYS Makefiles' -DCMAKE_CXX_COMPILER=C:/msys64/usr/bin/g++.exe -DCMAKE_MAKE_PROGRAM=C:/msys64/usr/bin/make.exe -DCURL_INCLUDE_DIR=C:/msys64/usr/include -DCURL_LIBRARY=C:/msys64/usr/lib/libcurl.dll.a -DTIMETREE_BUILD_EXAMPLE=ON ..
cmake --build .
Pop-Location
```

## Local verification

These tests use dummy credentials and a local HTTP server. They do not contact
TimeTree or read the Python project's credential file.

```powershell
& C:\msys64\usr\bin\g++.exe -std=c++17 -Wall -Wextra -I. tests/integration.cpp libtimetree.a -lcurl -o timetree_integration.exe
python tests/local_api.py .\timetree_integration.exe
& C:\msys64\usr\bin\g++.exe -std=c++17 -Wall -Wextra -I. tests/recurrence.cpp libtimetree.a -lcurl -o timetree_recurrence.exe
.\timetree_recurrence.exe
python tests/live_view.py .\timetree_example.exe
```

The test covers GET, login method/payload/header, UUID format, cookie extraction,
authenticated calendar JSON, model member initialization, HTTP 401, invalid JSON,
event pagination, duplicate IDs, recurring series, all-day ends, repeated cursors,
retention of the old snapshot after a failed fetch, millisecond conversion and
month-end clamping. It also covers expired-session renewal, deletion history,
reappearing events, fetch bounds and events changing between ordinary and recurring.
Recurrence tests cover intervals, exclusions, UNTIL in both formats, leap years,
month-end behavior, invalid dates/rules, inclusive boundaries and full-day queries.
The live-view test takes about 23 seconds and verifies timed refreshes, recovery
after an HTTP failure, title edits, deletions, and author/label display.
`TIMETREE_BASE_URL` overrides the
base URL for local testing.

## Fetch events into a calendar

```cpp
calendar.fetch_events();
for (const auto& [id, event] : calendar.events) {
    std::cout << event.title << " " << event.start.to_string() << '\n';
}
```

`fetch_events()` requests `/calendar/{id}/events/sync` and follows the `since`
cursor while `chunk` is true, matching Python. It performs a full refresh each
time and replaces the previous snapshot only after every page parses successfully.

`calendar.events` holds ordinary events; `calendar.recur_events` holds recurring
series. Both are `std::map<std::string, TTEvent>` keyed by the API's string event
ID, so iteration is by ID rather than start time. `TTEvent` contains title,
calendar/author/label IDs, timestamps, all-day status, duration, and recurrence
strings. Author and label IDs may be absent (`std::optional`). Its `definition`
member preserves the original JSON, including unmodeled fields such as notes,
locations and attendees. All-day ends follow the Python convention of including
the final day through 23:59:59.

`fetch_events(since, until)` optionally filters by event start, using inclusive
bounds. Like Python, omitted bounds default to calendar creation minus 52 weeks
and now plus 1,000 weeks. Earlier recurring seeds are retained so later
occurrences can still be generated. `calendar.bounds` records the chosen window.

## Recurring occurrences and refreshes

```cpp
const auto start = TTTime::from_string("20261018");
const auto finish = TTTime::from_string("20261025");
for (const auto& occurrence : calendar.events_between_dates(start, finish, true)) {
    std::cout << occurrence.start.to_string() << " " << occurrence.title << '\n';
}
calendar.refresh_events();
```

`TTEvent::recur_within_dates()` expands DAILY, WEEKLY, MONTHLY and YEARLY series,
with INTERVAL (default 1), UNTIL (UTC timestamp or date), and EXDATE exclusions.
It uses the same repeated `relativedelta`-style steps as Python: January 31 becomes
February 28 and then March 28, for example. Date-only UNTIL includes its final day.
Like Python, BYDAY and WKST are stored but do not alter those steps; other RFC
5545 scheduling features such as COUNT are not implemented. This is the Python
model's recurrence scope, not a complete iCalendar engine. Calculations use UTC
throughout, without local timezone or daylight-saving adjustments.

`events_between_dates(start, end, full_day)` merges ordinary events with expanded
series, sorted by start time, and selects occurrences whose **start** lies in the
window. It returns `std::vector<TTEventRecur>` for both kinds of result, with
`parent_id` identifying the source event, `recurring` indicating a generated
occurrence, and copies of the title, author/label IDs and start/end timestamps.
The returned objects do not hold pointers into the calendar and remain valid
after a refresh. Full-day mode covers both endpoint dates in UTC.

`refresh_events()` keeps the fetch bounds and performs another full paginated
sync. Missing events and series are copied to `deleted_events` with `deleted`
and `deleted_time` set; a reappearing ID removes its deletion marker. Filtering
an event out of the window does not mark it deleted. HTTP 401/403 during an event
fetch triggers reauthentication, using the new session cookie on the retry
(at most three attempts per page).

`known_users` maps user IDs to names; `label_data` maps label IDs to their name
and colour. The existing `event_labels` vector remains available.

## Remaining scope

The endpoints and payloads were matched to the working Python source and tested
locally. Live login, Ruth's event fetch and recurrence expansion succeeded.
The C++ version intentionally corrects several Python edge cases: it enforces
UNTIL during expansion, includes occurrences exactly at the window end, advances
the full-day upper bound, retains earlier recurring seeds, handles empty
refreshes, follows every refresh page and detects removals from both event maps.
Refreshes compare full snapshots rather than relying on the newest timestamp,
so edits sharing a timestamp are retained. Unlike Python's mixed result list,
date-window results use a single C++ occurrence value type.

TTTime accepts milliseconds, not microseconds. Month/year additions clamp to the
last valid day, preserve the subsecond component and use UTC rather than local
time. Use ordinary modern calendar dates within the platform clock range.
