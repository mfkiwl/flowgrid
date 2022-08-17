#include "SVGDevice.h"

#include <sstream>
#include <map>
#include "fmt/core.h"
#include "../../Helper/File.h"
#include "../../Helper/String.h"

using namespace fmt;

bool scaledSVG = false; // Draw scaled SVG files

static string xml_sanitize(const string &name) {
    static std::map<char, string> replacements{{'<', "&lt;"}, {'>', "&gt;"}, {'\'', "&apos;"}, {'"', "&quot;"}, {'&', "&amp;"}};

    auto replaced_name = name;
    for (const auto &[c, replacement]: replacements) {
        replaced_name = replace(replaced_name, c, replacement);
    }
    return replaced_name;
}

SVGDevice::SVGDevice(string file_name, float width, float height) : file_name(std::move(file_name)) {
    static const float scale = 0.5;

    stream << format(R"(<svg xmlns="http://www.w3.org/2000/svg" viewBox="0 0 {} {}")", width, height);
    stream << (scaledSVG ? R"( width="100%" height="100%">)" : format(R"( width="{}mm" height="{}mm">)", width * scale, height * scale));
}

SVGDevice::~SVGDevice() {
    stream << "</svg>\n";
    FileIO::write(file_name, stream.str());
}

void SVGDevice::rect(const ImVec4 &rect, const string &color, const string &link) {
    if (!link.empty()) stream << format(R"(<a href="{}">)", xml_sanitize(link)); // open the optional link tag
    const auto [x, y, w, h] = rect;
    stream << format(R"(<rect x="{}" y="{}" width="{}" height="{}" rx="0" ry="0" style="stroke:none;fill:{};"/>)", x, y, w, h, color);
    if (!link.empty()) stream << "</a>"; // close the optional link tag
}

void SVGDevice::triangle(const ImVec2 &pos, const ImVec2 &size, const string &color, int orientation, const string &link) {
    if (!link.empty()) stream << format(R"(<a href="{}">)", xml_sanitize(link)); // open the optional link tag
    const auto [x, y] = pos;
    const auto [l, h] = size;

    static const float radius = 1.5;
    float x0, x1, x2;
    if (orientation == kLeftRight) {
        x0 = x;
        x1 = x + l - 2 * radius;
        x2 = x + l - radius;
    } else {
        x0 = x + l;
        x1 = x + 2 * radius;
        x2 = x + radius;
    }
    // triangle + circle
    stream << format(R"(<polygon fill="{}" stroke="black" stroke-width=".25" points="{},{} {},{} {},{}"/>)", color, x0, y, x1, y + h / 2.0, x0, y + h);
    stream << format(R"(<circle  fill="{}" stroke="black" stroke-width=".25" cx="{}" cy="{}" r="{}"/>)", color, x2, y + h / 2.0, radius);
}

void SVGDevice::circle(const ImVec2 &pos, float radius) {
    stream << format(R"(<circle cx="{}" cy="{}" r="{}"/>)", pos.x, pos.y, radius);
}

string rotate_line(const Line &line, float rx, float ry, float rz) {
    const auto &[start, end] = line;
    return format(R"lit(<line x1="{}" y1="{}" x2="{}" y2="{}" transform="rotate({},{},{})" style="stroke: black; stroke-width:0.25;"/>)lit", start.x, start.y, end.x, end.y, rx, ry, rz);
}

void SVGDevice::arrow(const ImVec2 &pos, float rotation, int orientation) {
    static const float dx = 3, dy = 1;
    const auto [x, y] = pos;
    const auto x1 = orientation == kLeftRight ? x - dx : x + dx;
    stream << rotate_line({{x1, y - dy}, pos}, rotation, x, y);
    stream << rotate_line({{x1, y + dy}, pos}, rotation, x, y);
}

void SVGDevice::line(const Line &line) {
    const auto &[start, end] = line;
    stream << format(R"(<line x1="{}" y1="{}" x2="{}" y2="{}"  style="stroke:black; stroke-linecap:round; stroke-width:0.25;"/>)", start.x, start.y, end.x, end.y);
}

void SVGDevice::dasharray(const Line &line) {
    const auto &[start, end] = line;
    stream << format(R"(<line x1="{}" y1="{}" x2="{}" y2="{}"  style="stroke: black; stroke-linecap:round; stroke-width:0.25; stroke-dasharray:3,3;"/>)", start.x, start.y, end.x, end.y);
}

void SVGDevice::text(const ImVec2 &pos, const char *name, const string &link) {
    if (!link.empty()) stream << format(R"(<a href="{}">)", xml_sanitize(link)); // open the optional link tag
    stream << format(R"(<text x="{}" y="{}" font-family="Arial" font-size="7" text-anchor="middle" fill="#FFFFFF">{}</text>)", pos.x, pos.y + 2, xml_sanitize(name));
    if (!link.empty()) stream << "</a>"; // close the optional link tag
}

void SVGDevice::label(const ImVec2 &pos, const char *name) {
    stream << format(R"(<text x="{}" y="{}" font-family="Arial" font-size="7">{}</text>)", pos.x, pos.y + 2, xml_sanitize(name));
}

void SVGDevice::dot(const ImVec2 &pos, int orientation) {
    const float offset = orientation == kLeftRight ? 2 : -2;
    stream << format(R"(<circle cx="{}" cy="{}" r="1"/>)", pos.x + offset, pos.y + offset);
}
