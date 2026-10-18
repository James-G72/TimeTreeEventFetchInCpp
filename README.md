# TimeTreeHandlerCpp

Shared C++17 calendar, event and recurrence code, with a desktop live viewer.
It follows the Python project in `C:\dev\TimeTreeEventFetch`.

- `timetree_structs.hpp/.cpp`: models, pagination, snapshots, recurrence and date queries.
- `timetree_utils.hpp/.cpp`: the JSON page-callback type and calendar selection.
- `vendor/json.hpp`: nlohmann/json 3.12.0, with its license alongside it.
- `main.cpp`: **all HTTP, libcurl, login, credentials, cookies, retries and printing**.

The shared library has no curl, Wi-Fi, environment-variable or HTTP dependencies.
`main.cpp` is excluded from the library target and is guarded with `#ifndef ARDUINO`.

## Desktop build on this laptop

Use the MSYS compiler with the installed MSYS curl library; do not mix those
with the separate MinGW compiler.

```powershell
& C:\msys64\usr\bin\make.exe CXX=C:/msys64/usr/bin/g++.exe AR=C:/msys64/usr/bin/ar.exe
$env:TIMETREE_USERNAME = 'your-email'
$env:TIMETREE_PASSWORD = 'your-password'
.\timetree_example.exe
```

The example logs in, selects `Ruth`, fetches events, and prints a live table every
10 seconds. It displays yesterday through seven days ahead, with start/end times
in UTC, title, label, author, recurring status and deletion history. Press Ctrl+C
to stop. Failed updates report the error and retry after 10 seconds.
The interval is measured after each request/display completes.

For an independent HTTP check:

```powershell
.\timetree_example.exe --get https://example.com
```

The desktop example reads credentials from its process environment and does not
log passwords or session cookies. `TIMETREE_BASE_URL` overrides the API base for
local HTTP testing. Keep `C:\msys64\usr\bin` on PATH for these MSYS executables.
The generated MSYS `.a` file is for that toolchain; ESP32 builds compile the sources.

## Import the shared code into the ESP32 project

Copy these files into your project's library/source directory, preserving the
`vendor` subdirectory:

```text
timetree_structs.hpp
timetree_structs.cpp
timetree_utils.hpp
timetree_utils.cpp
vendor/json.hpp
vendor/LICENSE.MIT
```

Use C++17 or later and enable C++ exceptions. The code uses STL containers,
`std::optional`, `std::function`, `std::chrono` and UTC time conversion helpers.
No ESP32 toolchain or hardware build has been verified in this workspace yet.
Wi-Fi, clock synchronization, TLS certificate configuration, credentials and
memory/PSRAM configuration belong to your ESP32 application.

Your application logs in and obtains the calendar-list JSON. It then supplies a
function or lambda with this signature:

```cpp
json fetchEventPage(long long calendar_id, long long since);
```

The callback performs the HTTP request and returns parsed JSON. For `since == -1`,
request the initial full-sync page. Otherwise use the supplied cursor. Throw on
transport/HTTP failure so the library can preserve the previous snapshot.
Reauthentication, cookies and request retries are handled inside your callback.
The desktop implementation in `main.cpp` is the reference for the exact requests.

For example, once your application has `calendar_response`:

```cpp
#include "timetree_structs.hpp"

TTCalendar calendar(selectCalendarDefinition(calendar_response, "Ruth"), fetchEventPage);
calendar.fetch_events();
calendar.refresh_events();

const auto start = TTTime::from_string("20261018");
const auto finish = TTTime::from_string("20261025");
auto occurrences = calendar.events_between_dates(start, finish, true);
```

A calendar can also be constructed without a callback for offline parsing:
`TTCalendar calendar(definition);`. Configure one later using
`calendar.set_event_fetcher(fetchEventPage)` before fetching events. State captured
by reference in a callback must outlive the calendar.

## Page contract and HTTP details

The library callback returns this shape:

```json
{"events": [], "chunk": true, "since": 1735776000000}
```

`events` must be an array. A true `chunk` requests another page with `since`;
false finishes the snapshot and does not require a cursor. Repeated or negative
continuation cursors fail. The library replaces its maps only after every page
has been fetched and parsed successfully.

The desktop HTTP code matches Python:

- Base URL: `https://timetreeapp.com/api/v1`.
- PUT `/auth/email/signin` with `uid`, `password` and a UUID4 hex string.
- Headers: `Content-Type: application/json`, `X-Timetreea: web/2.1.0/en`,
  and a nonempty `User-Agent` (`TimeTreeHandlerCpp/0.1` on desktop).
- Send the login response's `_session_id` cookie on authenticated requests.
- GET `/calendars?since=0` for calendar definitions.
- GET `/calendar/{id}/events/sync` for the first event page;
  append `?since={cursor}` for subsequent pages.

Missing User-Agent caused live login to return HTTP 400 despite valid credentials.
The desktop code keeps TLS verification enabled, uses a 10-second connect timeout
and 30-second total timeout, and renews sessions on HTTP 401/403, with at most
three attempts per event page. Its `TTHttpError` retains status and response body
for diagnosis, while the exception message omits the response body.

## Calendar behavior

`events`, `recur_events` and `deleted_events` are maps keyed by string event ID.
`known_users` and `label_data` provide author-name and label lookups. Event models
retain their original JSON in `definition`; memory use still needs measuring on
ESP32, especially while both old and new snapshots coexist during refreshes.
No automatic PSRAM allocator is configured by this change.

`fetch_events(since, until)` optionally filters by event start, inclusively.
Defaults match Python: calendar creation minus 52 weeks through now plus 1,000
weeks. Filtering happens after downloading. Earlier recurring seeds are retained
so they can generate occurrences in later windows. `refresh_events()` preserves
those bounds and performs a full paginated sync. Removed events are recorded with
`deleted` and `deleted_time`; reappearing IDs clear the deletion marker.

`events_between_dates(start, end, full_day)` returns chronological
`std::vector<TTEventRecur>` values for ordinary events and generated occurrences.
Each contains source `parent_id`, title, start/end, author/label IDs and a
`recurring` flag. Results select events whose **start** is in the requested window.
Full-day mode covers both endpoint dates in UTC.

Recurrence supports DAILY, WEEKLY, MONTHLY and YEARLY steps, INTERVAL, UNTIL and
EXDATE. Like Python, BYDAY and WKST are stored without changing the stepping;
COUNT and other advanced iCalendar rules are not implemented. Monthly/yearly
steps clamp repeatedly (January 31 to February 28 to March 28). Date-only UNTIL
includes its final day. Calculations use UTC without DST adjustments.

The C++ version fixes Python's UNTIL cutoff, full-day end rounding, exact end
boundary, missing earlier recurrence seeds, empty refreshes and pagination on
refresh. All-day ends follow Python's inclusive 23:59:59 convention.

## Builds without curl

```powershell
& C:\msys64\usr\bin\make.exe CXX=C:/msys64/usr/bin/g++.exe AR=C:/msys64/usr/bin/ar.exe library test
```

Or import with CMake:

```cmake
add_subdirectory(path/to/TimeTreeHandlerCpp)
target_link_libraries(your_application PRIVATE timetree)
```

The `timetree` target needs no curl. Enable `TIMETREE_BUILD_EXAMPLE` only for the
desktop viewer; curl is then linked solely to that executable. Enable
`TIMETREE_BUILD_TESTS` for the offline integration and recurrence CTest targets.

This laptop has CMake 3.10, so configure from a build directory:

```powershell
New-Item -ItemType Directory -Force build | Out-Null
Push-Location build
cmake -G 'MSYS Makefiles' -DCMAKE_CXX_COMPILER=C:/msys64/usr/bin/g++.exe -DCMAKE_MAKE_PROGRAM=C:/msys64/usr/bin/make.exe -DCURL_INCLUDE_DIR=C:/msys64/usr/include -DCURL_LIBRARY=C:/msys64/usr/lib/libcurl.dll.a -DTIMETREE_BUILD_EXAMPLE=ON -DTIMETREE_BUILD_TESTS=ON ..
cmake --build .
ctest --output-on-failure
Pop-Location
```

## Verification

`make test` runs the core tests without curl or any network service. They cover
callback configuration, page cursors, duplicate IDs, failed-fetch preservation,
deletion/reappearance, empty responses, bounds, metadata and recurrence edges.

```powershell
python tests/live_view.py .\timetree_example.exe
```

This uses dummy credentials and a local HTTP server for about 23 seconds. It checks
the desktop login payload, required headers, cookie handling, session renewal,
live edits/deletions and recovery after an HTTP error. It never contacts TimeTree.
