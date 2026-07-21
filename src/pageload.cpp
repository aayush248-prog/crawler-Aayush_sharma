#include <iostream>
#include <fstream>
#include <string>
#include <stdexcept>
#include <unistd.h>
#include <thread>
#include <sstream>
#include <atomic>
#include <cstring>
#include <cstdint>
#include <cstdlib>
#include <functional>
#include <mutex>
#include <condition_variable>
#include <map>
#include <chrono>
#include <vector>
#include <set>
#include <regex>
#include <algorithm>
#include <cctype>

#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <netdb.h>
#include <sys/stat.h>   // for mkdir()

#include <curl/curl.h>
#include <nlohmann/json.hpp>

using namespace std;
using json = nlohmann::json;


static string base64_encode(const uint8_t* data, size_t len)
{
    static const char table[] =
        "ABCDEFGHIJKLMNOPQRSTUVWXYZ"
        "abcdefghijklmnopqrstuvwxyz"
        "0123456789+/";

    string out;
    out.reserve(((len + 2) / 3) * 4);

    size_t i = 0;
    while (i + 3 <= len)
    {
        uint32_t n = (data[i] << 16) | (data[i + 1] << 8) | data[i + 2];
        out.push_back(table[(n >> 18) & 0x3F]);
        out.push_back(table[(n >> 12) & 0x3F]);
        out.push_back(table[(n >> 6) & 0x3F]);
        out.push_back(table[n & 0x3F]);
        i += 3;
    }

    size_t rem = len - i;
    if (rem == 1)
    {
        uint32_t n = data[i] << 16;
        out.push_back(table[(n >> 18) & 0x3F]);
        out.push_back(table[(n >> 12) & 0x3F]);
        out.push_back('=');
        out.push_back('=');
    }
    else if (rem == 2)
    {
        uint32_t n = (data[i] << 16) | (data[i + 1] << 8);
        out.push_back(table[(n >> 18) & 0x3F]);
        out.push_back(table[(n >> 12) & 0x3F]);
        out.push_back(table[(n >> 6) & 0x3F]);
        out.push_back('=');
    }

    return out;
}


// =========================
// Minimal WebSocket client
// (raw sockets, RFC6455 framing — no Boost, no websocketpp)
// =========================

class MinimalWebSocket
{

private:

    int sockfd = -1;

    atomic<bool> running{false};


    static bool readAll(int fd, void *buf, size_t len)
    {
        uint8_t *p = (uint8_t *)buf;
        size_t got = 0;

        while (got < len)
        {
            ssize_t n = ::read(fd, p + got, len - got);

            if (n <= 0)
                return false;

            got += (size_t)n;
        }

        return true;
    }


    static bool parseWsUrl(
        const string &url,
        string &host,
        int &port,
        string &path
    )
    {

        string prefix = "ws://";

        if (url.compare(0, prefix.size(), prefix) != 0)
            return false;

        string rest = url.substr(prefix.size());

        size_t slashPos = rest.find('/');

        string hostPort =
            (slashPos == string::npos) ? rest : rest.substr(0, slashPos);

        path =
            (slashPos == string::npos) ? "/" : rest.substr(slashPos);

        size_t colonPos = hostPort.find(':');

        if (colonPos == string::npos)
        {
            host = hostPort;
            port = 80;
        }
        else
        {
            host = hostPort.substr(0, colonPos);
            port = stoi(hostPort.substr(colonPos + 1));
        }

        return true;
    }


    bool doHandshake(
        const string &host,
        int port,
        const string &path
    )
    {

        uint8_t keyBytes[16];

        for (int i = 0; i < 16; i++)
            keyBytes[i] = (uint8_t)(rand() % 256);

        string secKey = base64_encode(keyBytes, 16);

        ostringstream req;

        req << "GET " << path << " HTTP/1.1\r\n"
            << "Host: " << host << ":" << port << "\r\n"
            << "Upgrade: websocket\r\n"
            << "Connection: Upgrade\r\n"
            << "Sec-WebSocket-Key: " << secKey << "\r\n"
            << "Sec-WebSocket-Version: 13\r\n"
            << "\r\n";

        string reqStr = req.str();

        if (::write(sockfd, reqStr.data(), reqStr.size()) < 0)
            return false;

        // Read response headers until the blank line
        string resp;
        char c;

        while (true)
        {
            ssize_t n = ::read(sockfd, &c, 1);

            if (n <= 0)
                return false;

            resp.push_back(c);

            if (resp.size() >= 4 &&
                resp.compare(resp.size() - 4, 4, "\r\n\r\n") == 0)
                break;
        }

        // Expect HTTP/1.1 101 Switching Protocols
        if (resp.find(" 101 ") == string::npos)
        {
            cout << "WebSocket handshake failed:\n" << resp << endl;
            return false;
        }

        return true;
    }


    bool readFrame(uint8_t &opcode, bool &fin, string &payload)
    {

        uint8_t hdr[2];

        if (!readAll(sockfd, hdr, 2))
            return false;

        fin = (hdr[0] & 0x80) != 0;
        opcode = hdr[0] & 0x0F;

        bool masked = (hdr[1] & 0x80) != 0;
        uint64_t len = hdr[1] & 0x7F;

        if (len == 126)
        {
            uint8_t ext[2];

            if (!readAll(sockfd, ext, 2))
                return false;

            len = ((uint64_t)ext[0] << 8) | ext[1];
        }
        else if (len == 127)
        {
            uint8_t ext[8];

            if (!readAll(sockfd, ext, 8))
                return false;

            len = 0;

            for (int i = 0; i < 8; i++)
                len = (len << 8) | ext[i];
        }

        uint8_t maskKey[4] = {0, 0, 0, 0};

        if (masked)
        {
            if (!readAll(sockfd, maskKey, 4))
                return false;
        }

        payload.resize(len);

        if (len > 0)
        {
            if (!readAll(sockfd, &payload[0], len))
                return false;
        }

        if (masked)
        {
            for (uint64_t i = 0; i < len; i++)
                payload[i] = (char)((uint8_t)payload[i] ^ maskKey[i % 4]);
        }

        return true;
    }


    void writeFrame(uint8_t opcode, const string &payload)
    {

        string frame;

        frame.push_back((char)(0x80 | opcode)); // FIN=1

        uint64_t len = payload.size();
        uint8_t maskBit = 0x80; // client->server frames must be masked

        if (len <= 125)
        {
            frame.push_back((char)(maskBit | len));
        }
        else if (len <= 0xFFFF)
        {
            frame.push_back((char)(maskBit | 126));
            frame.push_back((char)((len >> 8) & 0xFF));
            frame.push_back((char)(len & 0xFF));
        }
        else
        {
            frame.push_back((char)(maskBit | 127));

            for (int i = 7; i >= 0; i--)
                frame.push_back((char)((len >> (8 * i)) & 0xFF));
        }

        uint8_t maskKey[4];

        for (int i = 0; i < 4; i++)
            maskKey[i] = (uint8_t)(rand() % 256);

        frame.append((char *)maskKey, 4);

        string maskedPayload = payload;

        for (uint64_t i = 0; i < len; i++)
            maskedPayload[i] =
                (char)((uint8_t)maskedPayload[i] ^ maskKey[i % 4]);

        frame += maskedPayload;

        ::write(sockfd, frame.data(), frame.size());
    }


public:

    function<void()> onOpen;
    function<void(const string &)> onMessage;

    thread readerThread;


    bool connect(const string &url)
    {

        string host, path;
        int port = 80;

        if (!parseWsUrl(url, host, port, path))
        {
            cout << "Invalid WebSocket URL: " << url << endl;
            return false;
        }

        sockfd = ::socket(AF_INET, SOCK_STREAM, 0);

        if (sockfd < 0)
        {
            cout << "Socket creation failed\n";
            return false;
        }

        struct hostent *server = gethostbyname(host.c_str());

        if (!server)
        {
            cout << "DNS resolution failed for " << host << endl;
            return false;
        }

        struct sockaddr_in serv_addr{};
        serv_addr.sin_family = AF_INET;
        memcpy(
            &serv_addr.sin_addr.s_addr,
            server->h_addr,
            server->h_length
        );
        serv_addr.sin_port = htons(port);

        if (::connect(
                sockfd,
                (struct sockaddr *)&serv_addr,
                sizeof(serv_addr)
            ) < 0)
        {
            cout << "TCP connect failed\n";
            return false;
        }

        if (!doHandshake(host, port, path))
            return false;

        running = true;

        if (onOpen)
            onOpen();

        readerThread = thread(&MinimalWebSocket::readLoop, this);

        return true;
    }


    void readLoop()
    {

        string messageBuffer;
        bool assembling = false;

        while (running)
        {

            uint8_t opcode;
            bool fin;
            string payload;

            if (!readFrame(opcode, fin, payload))
            {
                running = false;
                break;
            }

            if (opcode == 0x8) // close
            {
                running = false;
                break;
            }

            if (opcode == 0x9) // ping -> respond pong
            {
                writeFrame(0xA, payload);
                continue;
            }

            if (opcode == 0xA) // pong
            {
                continue;
            }

            if (opcode == 0x1 || opcode == 0x2) // text/binary start
            {
                messageBuffer = payload;
                assembling = !fin;
            }
            else if (opcode == 0x0) // continuation
            {
                messageBuffer += payload;
                assembling = !fin;
            }

            if (fin && onMessage)
                onMessage(messageBuffer);
        }
    }


    void send(const string &data)
    {
        if (running)
            writeFrame(0x1, data);
    }


    void close()
    {

        if (running)
        {
            running = false;

            // best-effort close frame; ignore errors, socket is going away
            writeFrame(0x8, "");
        }

        if (sockfd >= 0)
        {
            ::shutdown(sockfd, SHUT_RDWR);
            ::close(sockfd);
            sockfd = -1;
        }

        if (readerThread.joinable())
            readerThread.join();
    }


    ~MinimalWebSocket()
    {
        close();
    }

};


// =========================
// WebSocket Client (CDP-facing wrapper)
// =========================

class WebSocketClient
{

private:

    MinimalWebSocket ws;

    atomic<bool> connected{false};

    atomic<int> idCounter{1};

    mutex responseMutex;
    condition_variable responseCV;
    map<int, json> responses;

    mutex eventMutex;
    map<string, function<void(json)>> eventHandlers;


    void handleMessage(const string &payload)
    {

        json msg;

        try
        {
            msg = json::parse(payload);
        }
        catch (exception &e)
        {
            cout << "Failed to parse CDP message: " << e.what() << endl;
            return;
        }

        if (msg.contains("id"))
        {
            int id = msg["id"].get<int>();

            {
                lock_guard<mutex> lock(responseMutex);
                responses[id] = msg;
            }

            responseCV.notify_all();
        }
        else if (msg.contains("method"))
        {
            string method = msg["method"].get<string>();

            function<void(json)> handler;

            {
                lock_guard<mutex> lock(eventMutex);
                auto it = eventHandlers.find(method);

                if (it != eventHandlers.end())
                    handler = it->second;
            }

            if (handler)
                handler(msg.value("params", json::object()));
        }
    }


public:


    bool connect(string url)
    {

        ws.onOpen = [&]()
        {
            cout << "WebSocket connected\n";
            connected = true;
        };

        ws.onMessage = [&](const string &payload)
        {
            handleMessage(payload);
        };

        if (!ws.connect(url))
        {
            cout << "Connection error\n";
            return false;
        }

        while (!connected)
        {
            usleep(10000);
        }

        return true;
    }


    void disconnect()
    {
        ws.close();
        connected = false;

        lock_guard<mutex> lock(responseMutex);
        responses.clear();
    }


    void onEvent(const string &method, function<void(json)> handler)
    {
        lock_guard<mutex> lock(eventMutex);
        eventHandlers[method] = handler;
    }


    int send(
        string method,
        json params = {}
    )
    {

        if (!connected)
        {
            cout << "Socket not connected\n";
            return -1;
        }

        int id = idCounter++;

        json request;

        request["id"] = id;
        request["method"] = method;

        if (!params.empty())
            request["params"] = params;

        ws.send(request.dump());

        return id;
    }


    json sendAndWait(
        string method,
        json params = {},
        int timeoutMs = 10000
    )
    {

        int id = send(method, params);

        if (id < 0)
            return json::object();

        unique_lock<mutex> lock(responseMutex);

        bool got = responseCV.wait_for(
            lock,
            chrono::milliseconds(timeoutMs),
            [&]() { return responses.find(id) != responses.end(); }
        );

        if (!got)
        {
            cout << "Timed out waiting for response to " << method << endl;
            return json::object();
        }

        json response = responses[id];
        responses.erase(id);

        if (response.contains("error"))
        {
            cout << "CDP error for " << method << ": "
                 << response["error"].dump() << endl;
            return json::object();
        }

        return response.value("result", json::object());
    }


    ~WebSocketClient()
    {
        ws.close();
    }

};


// =========================
// Browser Controller
// =========================


class Browser
{


private:


    string websocketURL;

    WebSocketClient socket;

    bool chromeLaunched = false;

    bool socketOpen = false;


    static size_t WriteCallback(
        void *contents,
        size_t size,
        size_t nmemb,
        string *output
    )
    {

        size_t total = size * nmemb;

        output->append(
            (char *)contents,
            total
        );

        return total;

    }


    bool hasFrameworkRoot(const string &html)
    {
        vector<string> patterns = {
            "id=\"root\"",
            "id='root'",
            "id=\"app\"",
            "<app-root>"
        };

        for (auto &p : patterns)
        {
            if (html.find(p) != string::npos)
                return true;
        }

        return false;
    }


public:


    Browser()
    {

        system("pkill -f 'remote-debugging-port=9222' 2>/dev/null");
        sleep(1);

        // NOTE: this path is macOS-only. On Linux, use something like
        // "google-chrome" or "chromium-browser"; on Windows, the full
        // path to chrome.exe. Consider making this configurable
        // (env var or constructor argument) rather than hardcoded.
        string command =
    "/Applications/Google\\ Chrome.app/Contents/MacOS/Google\\ Chrome "
    "--headless=new "
    "--remote-debugging-port=9222 "
    "--disable-gpu "
    "--disable-extensions "
    "--disable-background-networking "
    "--disable-background-mode "
    "--disable-component-update "
    "--disable-domain-reliability "
    "--disable-crash-reporter "
    "--no-first-run "
    "--no-default-browser-check "
    "--noerrdialogs "
    "about:blank >/dev/null 2>&1 &";

        int result =
            system(command.c_str());

        if (result == 0)
        {
            cout << "Chrome launch command issued\n";
            chromeLaunched = true;
        }
        else
        {
            cout << "Chrome failed to launch\n";
            chromeLaunched = false;
        }

        sleep(2);

    }


    bool getSocketURL()
    {

        if (!chromeLaunched)
        {
            cout << "Chrome was never launched; cannot fetch debug targets\n";
            return false;
        }

        CURL *curl =
            curl_easy_init();

        if (!curl)
        {
            cout << "curl_easy_init failed\n";
            return false;
        }

        string response;

        curl_easy_setopt(
            curl,
            CURLOPT_URL,
            "http://localhost:9222/json"
        );

        curl_easy_setopt(
            curl,
            CURLOPT_WRITEFUNCTION,
            WriteCallback
        );

        curl_easy_setopt(
            curl,
            CURLOPT_WRITEDATA,
            &response
        );

        CURLcode res =
            curl_easy_perform(curl);

        curl_easy_cleanup(curl);

        if (res != CURLE_OK)
        {

            cout << "Curl error "
                 << curl_easy_strerror(res)
                 << endl;

            return false;

        }


        try
        {

            json data =
                json::parse(response);

            if (data.empty())
            {
                cout << "No debug targets returned by Chrome\n";
                return false;
            }

            cout << "\nAvailable debug targets:\n";

            for (auto &target : data)
            {
                cout << "  type=" << target.value("type", "?")
                     << "  url=" << target.value("url", "?")
                     << endl;
            }

            bool found = false;

            for (auto &target : data)
            {
                if (target.value("type", "") == "page")
                {
                    websocketURL = target["webSocketDebuggerUrl"];
                    found = true;
                    break;
                }
            }

            if (!found)
            {
                cout << "No target with type=='page' found; "
                        "falling back to first entry\n";

                websocketURL =
                    data[0]["webSocketDebuggerUrl"];
            }

            cout << "\nUsing debugger URL:\n"
                 << websocketURL
                 << endl;

        }

        catch (exception &e)
        {

            cout << "JSON error "
                 << e.what()
                 << endl;

            return false;

        }

        return true;

    }


    bool openPage(string url)
    {

        if (socketOpen)
        {
            socket.disconnect();
            socketOpen = false;
        }

        if (!socket.connect(websocketURL))
        {
            cout << "Failed to connect to debugger websocket\n";
            return false;
        }

        socketOpen = true;

        socket.sendAndWait("Page.enable");

        socket.sendAndWait(
            "Page.setLifecycleEventsEnabled",
            {
                {
                    "enabled",
                    true
                }
            }
        );

        mutex loadMutex;
        condition_variable loadCV;
        bool settled = false;

        socket.onEvent("Page.lifecycleEvent", [&](json params)
        {
            string name = params.value("name", "");

            if (name == "networkIdle")
            {
                {
                    lock_guard<mutex> lock(loadMutex);
                    settled = true;
                }
                loadCV.notify_all();
            }
        });

        socket.sendAndWait(
            "Page.navigate",
            {
                {
                    "url",
                    url
                }
            }
        );

        bool fired;

        {
            unique_lock<mutex> lock(loadMutex);

            fired = loadCV.wait_for(
                lock,
                chrono::seconds(20),
                [&]() { return settled; }
            );

            if (!fired)
                cout << "Warning: networkIdle not observed within "
                        "timeout, reading DOM anyway\n";
        }

        socket.onEvent("Page.lifecycleEvent", nullptr);

        usleep(500000);

        return true;

    }


    string getHtml()
    {

        if (!socketOpen)
        {
            cout << "getHtml() called with no open debugger connection\n";
            return "";
        }

        socket.sendAndWait("Runtime.enable");

        json result = socket.sendAndWait(
            "Runtime.evaluate",
            {
                {
                    "expression",
                    "document.documentElement.outerHTML"
                },

                {
                    "returnByValue",
                    true
                }
            }
        );

        try
        {
            return result["result"]["value"].get<string>();
        }
        catch (exception &e)
        {
            cout << "Failed to extract HTML: " << e.what() << endl;
            return "";
        }

    }


    string fetch(string url)
    {
        string html;

        CURL *curl = curl_easy_init();

        if (!curl)
        {
            cout << "curl_easy_init failed\n";
            return "";
        }

        curl_easy_setopt(curl, CURLOPT_URL, url.c_str());
        curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, WriteCallback);
        curl_easy_setopt(curl, CURLOPT_WRITEDATA, &html);
        curl_easy_setopt(curl, CURLOPT_FOLLOWLOCATION, 1L);

        CURLcode res = curl_easy_perform(curl);

        curl_easy_cleanup(curl);

        if (res != CURLE_OK)
        {
            cout << "Curl error " << curl_easy_strerror(res) << endl;
            return "";
        }

        bool flag = hasFrameworkRoot(html);

        if (flag == false)
        {
            cout << "static file\n";
            return html;
        }

        cout << "dynamic site\n";

        if (!getSocketURL())
        {
            cout << "Failed to get debugger socket URL\n";
            return "";
        }

        if (!openPage(url))
        {
            cout << "Failed to open page via debugger\n";
            return "";
        }

        return getHtml();
    }

};


// =========================
// Page load / save / link-extract wrapper
// =========================

class pageload
{

public:

    Browser browser;

    int pagecount;

    string lastHtml;


    pageload()
    {
        pagecount = 0;
    }


    void getpage(string path1)
    {
        string path = "./include/pages/" + path1 + ".txt";
        ifstream file(path);

        if (!file.is_open())
        {
            cout << "File not found: " << path << endl;
            return;
        }

        string text;

        while (getline(file, text))
        {
            cout << text << endl;
        }

        file.close();
    }


    bool loadpage(string url)
    {
        try
        {
            lastHtml = browser.fetch(url);

            if (lastHtml.empty())
            {
                cout << "Failed to fetch: " << url << endl;
                return false;
            }

            return true;
        }
        catch (exception &err)
        {
            cout << err.what() << endl;
            return false;
        }
    }


    string savepage(string name)
    {
        try
        {
            if (lastHtml.empty())
            {
                throw runtime_error(
                    "No page loaded — call loadpage() successfully first"
                );
            }

            mkdir("./include", 0755);
            mkdir("./include/pages", 0755);

            string path = "./include/pages/" + name + ".txt";

            ofstream file(path);

            if (!file.is_open())
            {
                throw runtime_error("File cannot open: " + path);
            }

            pagecount++;

            file << lastHtml;

            file.close();

            return path;
        }
        catch (exception &err)
        {
            cout << err.what() << endl;
            return "";
        }
    }


    int pages()
    {
        return pagecount;
    }


    // FIX: was a plain <a> extractor with no filtering, which on a
    // real page like this one pulls in href="" (dropdown toggles),
    // href="#" (placeholders), tel:/mailto: links, and duplicates —
    // none of which are real crawlable pages. Now filters those out
    // and dedupes, matching extractLinks.cpp.
    vector<string> linktag()
    {
        vector<string> links;
        set<string> seen;

        regex pattern(R"re(<a\s[^>]*href\s*=\s*"([^"]*)")re", regex::icase);

        sregex_iterator begin(lastHtml.begin(), lastHtml.end(), pattern);
        sregex_iterator end;

        for (auto it = begin; it != end; ++it)
        {
            string href = (*it)[1].str();

            if (isIgnoredHref(href))
                continue;

            string trimmed = trim(href);

            if (seen.count(trimmed))
                continue;

            seen.insert(trimmed);
            links.push_back(trimmed);
        }

        return links;
    }


private:

    static string trim(const string &s)
    {
        size_t start = s.find_first_not_of(" \t\r\n");
        if (start == string::npos)
            return "";
        size_t stop = s.find_last_not_of(" \t\r\n");
        return s.substr(start, stop - start + 1);
    }

    // Returns true for href values that aren't real, fetchable page
    // links: empty, fragment-only (#...), tel:, mailto:, javascript:.
    static bool isIgnoredHref(const string &href)
    {
        string trimmed = trim(href);

        if (trimmed.empty())
            return true;

        if (trimmed[0] == '#')
            return true;

        auto startsWithCI = [&](const string &prefix) {
            if (trimmed.size() < prefix.size())
                return false;
            return equal(prefix.begin(), prefix.end(), trimmed.begin(),
                         [](char a, char b) { return tolower(a) == tolower(b); });
        };

        if (startsWithCI("tel:"))
            return true;
        if (startsWithCI("mailto:"))
            return true;
        if (startsWithCI("javascript:"))
            return true;

        return false;
    }

};






int main()
{
    pageload pg;

    if (pg.loadpage("https://login.salesforce.com/?locale=in"))
    {
        pg.savepage("salesforce");
    }

    cout << "Pages saved: " << pg.pages() << endl;

    vector<string> s = pg.linktag();
    cout << "Extracted " << s.size() << " links:\n";
    for (string st : s)
    {
        cout << st << endl;
    }
}