#include <xcb/xcb.h>
#include <xcb/xcb_atom.h>
#include <xcb/xcb_ewmh.h>
#include <xcb/xcb_icccm.h>
#include <X11/Xlib.h>
#include <iostream>
#include <string>
#include <regex>
#include <unistd.h>
#include "INIReader.h"

using namespace std;

class ewmh_i {
public:
    xcb_ewmh_connection_t connection{nullptr};
};

class Config {
public:
    bool initialized = false;
};


;
ewmh_i* ewmh_conn = new ewmh_i;
INIReader config("taskbar.ini");

xcb_ewmh_connection_t init(xcb_connection_t* conn) {
    if (!ewmh_conn->connection.connection) {
        xcb_intern_atom_cookie_t* cookie = xcb_ewmh_init_atoms(conn, &ewmh_conn->connection);
        xcb_ewmh_init_atoms_replies(&ewmh_conn->connection, cookie, nullptr);
    }

    return ewmh_conn->connection;
}

string get_reply_string(xcb_ewmh_get_utf8_strings_reply_t *reply) {
    string str;
    if (reply) {
        str = string(reply->strings, reply->strings_len);
        xcb_ewmh_get_utf8_strings_reply_wipe(reply);
    }
    return str;
}

string generate_value(xcb_connection_t* conn, xcb_window_t win, string format, uint32_t currentDesktop) {
    string name;
    string generated;
    xcb_ewmh_connection_t eCon= init(conn);
    xcb_ewmh_get_utf8_strings_reply_t utf8_reply{};
    uint32_t desktop;

    xcb_ewmh_get_wm_desktop_reply(&eCon, xcb_ewmh_get_wm_desktop(&eCon, win), &desktop, nullptr);
    if (desktop == currentDesktop) {
        xcb_ewmh_get_wm_name_reply(&eCon, xcb_ewmh_get_wm_name(&eCon, win), &utf8_reply, nullptr);

        name = get_reply_string(&utf8_reply).c_str();
        generated = std::regex_replace(format, std::regex("<name>"), name);
        generated = std::regex_replace(generated, std::regex("<id>"), to_string(win));

        return generated;
    } else {
        return "";
    }
}


string implode( const string &glue, const vector<string> &pieces )
{
    string a;
    int leng=pieces.size();
    for(int i=0; i<leng; i++)
    {
        a+= pieces[i];
        if (  i < (leng-1) )
            a+= glue;
    }
    return a;
}

void print_child(xcb_connection_t *conn) {
    xcb_ewmh_connection_t eCon= init(conn);
    xcb_get_property_cookie_t clientCookie = xcb_ewmh_get_client_list(&eCon, 0);
    xcb_ewmh_get_windows_reply_t reply;
    xcb_ewmh_get_client_list_reply(&eCon, clientCookie, &reply, nullptr);

    xcb_get_property_cookie_t desktopCookie = xcb_ewmh_get_current_desktop(&eCon, 0);
    uint32_t desktop;
    xcb_ewmh_get_current_desktop_reply(&eCon, desktopCookie, &desktop, nullptr);

    xcb_window_t active_window;
    xcb_ewmh_get_active_window_reply(&eCon, xcb_ewmh_get_active_window(&eCon, 0), &active_window, nullptr);

    vector<string> values;

    for (int i = 0; i < reply.windows_len; i++) {
        string value = "";
        if (to_string(active_window) == to_string(reply.windows[i])) {
            value = generate_value(conn, reply.windows[i], config.GetString("format", "active", "<id>:<name>"), desktop);
        } else {
            value = generate_value(conn, reply.windows[i], config.GetString("format", "item", "<id>:<name>"), desktop);
        }
        if (value != "") {
            values.push_back(value);
        }
    }

    puts(std::regex_replace(
        config.GetString("format", "output", "<output>"),
        std::regex("<output>"),
        implode(config.GetString("format", "separator", ""),values)
    ).c_str());
}

void initialize_config(string config_path) {
    config = INIReader(config_path.c_str());
    if (config.ParseError() < 0) {
        throw "Can't load";
    }
}

int main (int argc, char* argv[]) {
    initialize_config(argv[1]);

    const xcb_setup_t   *setup;
    xcb_connection_t    *conn;
    xcb_screen_t        *screen;

    conn = xcb_connect(NULL, NULL);
    if (xcb_connection_has_error(conn)) {
        puts("Error connecting to display");
        return 1;
    }
    setup = xcb_get_setup(conn);
    screen = xcb_setup_roots_iterator(setup).data;
    string listen = "0";
    if (argc >= 3) {
        listen = argv[2];
    }
    if (listen == "0") {
        print_child(conn);
    } else {
        Display* display = XOpenDisplay(0);
        XSetWindowAttributes attributes;
        attributes.event_mask = StructureNotifyMask | PropertyChangeMask;
        Window win = XDefaultRootWindow(display);
        XChangeWindowAttributes(display, win, CWEventMask, &attributes);

        while (true) {
            XEvent event;
            XNextEvent(display, &event);
            if ( (event.xcreatewindow.parent == win)) {
                switch (event.type) {
                    case CreateNotify:
                    case DestroyNotify:
                    case PropertyNotify:
                        print_child(conn);
                        break;
                }
            }
         }
    }

    free(ewmh_conn);

    xcb_disconnect(conn);


    return 0;
}