#include <xcb/xcb.h>
#include <xcb/xcb_atom.h>
#include <xcb/xcb_ewmh.h>
#include <xcb/xcb_icccm.h>
#include <X11/Xlib.h>
#include <iostream>
#include <string>
#include <regex>
#include <unistd.h>
#include <cmath>
#include "INIReader.h"

using namespace std;

string pad_both(string &s, long width)
{
    if (width > 0) {
        int len = s.length();
        int remaining = width - len;
        if (remaining <= 0) {
            return s;
        }

        int pad_left = ceil(remaining / 2);
        int pad_right = floor(remaining / 2);

        s.insert(s.begin(), pad_left, ' ');
        s.insert(s.end(), pad_right, ' ');
    }

    return s;
}

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

string generate_value(xcb_connection_t* conn, xcb_window_t win, string state, uint32_t currentDesktop) {
    string name;
    uint32_t pid;
    string generated;
    xcb_ewmh_connection_t eCon= init(conn);
    xcb_ewmh_get_utf8_strings_reply_t utf8_reply{};
    uint32_t desktop;

    xcb_ewmh_get_wm_desktop_reply(&eCon, xcb_ewmh_get_wm_desktop(&eCon, win), &desktop, nullptr);
    if (desktop == currentDesktop) {
        xcb_ewmh_get_wm_name_reply(&eCon, xcb_ewmh_get_wm_name(&eCon, win), &utf8_reply, nullptr);
        xcb_ewmh_get_wm_pid_reply(&eCon, xcb_ewmh_get_wm_pid(&eCon, win), &pid, nullptr);

        name = get_reply_string(&utf8_reply).c_str();
        string state_value = config.Get("item", "state-" + state, state);
        string label = config.Get("item", "label-" + state, "%name%");

        label = std::regex_replace(label, std::regex("%name%"), name);
        label = std::regex_replace(label, std::regex("%state%"), state_value);
        label = std::regex_replace(label, std::regex("%pid%"), to_string(pid));
        label = std::regex_replace(label, std::regex("%wid%"), to_string(win));

        long min_len = config.GetInteger("item", "label-" + state + "-minlen", 0);
        long max_len = config.GetInteger("item", "label-" + state + "-maxlen", label.length());
        bool ellipsis = config.GetBoolean("item", "label-" + state + "-ellipsis", false);
        long padding = config.GetInteger("item", "label-" + state + "-padding", 0);
        string background = config.Get("item", "label-" + state + "-background", "");
        string foreground = config.Get("item", "label-" + state + "-foreground", "");
        string format = config.Get("item", "format-" + state, "<label>");
        if (label.length() < min_len) {
            label = pad_both(label, min_len);
        } else if (label.length() > max_len && ellipsis) {
            label = label.substr(0, max_len - 3) + "...";
        } else if (label.length() > max_len && !ellipsis) {
            label = label.substr(0, max_len);
        }

        if (padding > 0) {
            label = pad_both(label, padding * 2);
        }

        if (foreground != "" && background != "") {
            label = "%{B" + background + "}%{F" +  foreground + "}" + label + "${B- F-}";
        } else if (background != "") {
            label = "%{B" +  background + "}" + label + "%{B-}";
        } else if (foreground != "") {
            label = "%{F" +  background + "}" + label + "%{F-}";
        }

        generated = std::regex_replace(format, std::regex("<label>"), label);
        generated = std::regex_replace(generated, std::regex("<state>"), state_value);

        // action
        string click_left = config.Get("item", "click-left-" + state, "");
        click_left = std::regex_replace(click_left, std::regex("%name%"), name);
        click_left = std::regex_replace(click_left, std::regex("%state%"), state_value);
        click_left = std::regex_replace(click_left, std::regex("%pid%"), to_string(pid));
        click_left = std::regex_replace(click_left, std::regex("%wid%"), to_string(win));

        string click_right = config.Get("item", "click-right-" + state, "");
        click_right = std::regex_replace(click_right, std::regex("%name%"), name);
        click_right = std::regex_replace(click_right, std::regex("%state%"), state_value);
        click_right = std::regex_replace(click_right, std::regex("%pid%"), to_string(pid));
        click_right = std::regex_replace(click_right, std::regex("%wid%"), to_string(win));

        string double_click_left = config.Get("item", "double-click-left-" + state, "");
        double_click_left = std::regex_replace(double_click_left, std::regex("%name%"), name);
        double_click_left = std::regex_replace(double_click_left, std::regex("%state%"), state_value);
        double_click_left = std::regex_replace(double_click_left, std::regex("%pid%"), to_string(pid));
        double_click_left = std::regex_replace(double_click_left, std::regex("%wid%"), to_string(win));

        string double_click_right = config.Get("item", "double-click-right-" + state, "");
        double_click_right = std::regex_replace(double_click_right, std::regex("%name%"), name);
        double_click_right = std::regex_replace(double_click_right, std::regex("%state%"), state_value);
        double_click_right = std::regex_replace(double_click_right, std::regex("%pid%"), to_string(pid));
        double_click_right = std::regex_replace(double_click_right, std::regex("%wid%"), to_string(win));

        if (click_left != "") {
            generated = "%{A1:" + click_left + ":}" + generated + "%{A}";
        }

        if (click_right != "") {
            generated = "%{A3:" + click_right + ":}" + generated + "%{A}";
        }

        if (double_click_left != "") {
            generated = "%{A6:" + double_click_left + ":}" + generated + "%{A}";
        }

        if (double_click_right != "") {
            generated = "%{A8:" + double_click_right + ":}" + generated + "%{A}";
        }

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

    long max_items = config.GetInteger("main", "max-items", reply.windows_len);

    for (int i = 0; i < max_items; i++) {
        string value = "";
        bool is_active = to_string(active_window) == to_string(reply.windows[i]);
        value = generate_value(conn, reply.windows[i], is_active ? "active" : "inactive", desktop);
        if (value != "") {
            values.push_back(value);
        }
    }

    puts(std::regex_replace(
        config.GetString("main", "output", "<output>"),
        std::regex("<output>"),
        implode(config.GetString("main", "separator", ""),values)
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