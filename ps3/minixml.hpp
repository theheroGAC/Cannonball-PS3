#pragma once

#include <cctype>
#include <fstream>
#include <locale>
#include <optional>
#include <sstream>
#include <stdexcept>
#include <string>
#include <vector>

namespace minixml {

struct xml_writer_settings {
    char indent_char;
    int indent_count;
    xml_writer_settings(char c = '\t', int n = 1) : indent_char(c), indent_count(n) {}
};

namespace xml_parser {
    static const int trim_whitespace = 1;
}

class ptree {
public:
    typedef std::pair<std::string, ptree> value_type;
    typedef std::vector<value_type>::iterator iterator;
    typedef std::vector<value_type>::const_iterator const_iterator;

    std::string data;
    std::vector<value_type> children;

    ptree() {}
    explicit ptree(const std::string& d) : data(d) {}

    iterator begin() { return children.begin(); }
    iterator end() { return children.end(); }
    const_iterator begin() const { return children.begin(); }
    const_iterator end() const { return children.end(); }
    bool empty() const { return children.empty() && data.empty(); }
    void clear() { data.clear(); children.clear(); }

    ptree* find_child_ptr(const std::string& key) {
        for (size_t i = 0; i < children.size(); i++) {
            if (children[i].first == key)
                return &children[i].second;
        }
        return nullptr;
    }

    const ptree* find_child_ptr(const std::string& key) const {
        for (size_t i = 0; i < children.size(); i++) {
            if (children[i].first == key)
                return &children[i].second;
        }
        return nullptr;
    }

    ptree& get_or_create_child(const std::string& key) {
        ptree* c = find_child_ptr(key);
        if (c) return *c;
        children.push_back(std::make_pair(key, ptree()));
        return children.back().second;
    }

    const ptree* resolve_path(const std::string& path) const {
        if (path.empty()) return this;
        const ptree* cur = this;
        size_t pos = 0;
        while (pos <= path.size()) {
            size_t dot = path.find('.', pos);
            std::string seg = (dot == std::string::npos) ? path.substr(pos) : path.substr(pos, dot - pos);
            cur = cur->find_child_ptr(seg);
            if (!cur) return nullptr;
            if (dot == std::string::npos) break;
            pos = dot + 1;
        }
        return cur;
    }

    ptree& ensure_path(const std::string& path) {
        if (path.empty()) return *this;
        ptree* cur = this;
        size_t pos = 0;
        while (pos <= path.size()) {
            size_t dot = path.find('.', pos);
            std::string seg = (dot == std::string::npos) ? path.substr(pos) : path.substr(pos, dot - pos);
            cur = &cur->get_or_create_child(seg);
            if (dot == std::string::npos) break;
            pos = dot + 1;
        }
        return *cur;
    }

    template <typename T>
    static std::optional<T> convert_from_string(const std::string& s);

    template <typename T>
    std::optional<T> get_optional(const std::string& path) const {
        const ptree* node = resolve_path(path);
        if (!node) return std::nullopt;
        return convert_from_string<T>(node->data);
    }

    template <typename T>
    T get(const std::string& path, const T& def) const {
        std::optional<T> v = get_optional<T>(path);
        return v.has_value() ? *v : def;
    }

    std::string get(const std::string& path, const char* def) const {
        std::optional<std::string> v = get_optional<std::string>(path);
        return v.has_value() ? *v : std::string(def ? def : "");
    }

    template <typename T>
    void put(const std::string& path, const T& val) {
        std::ostringstream ss;
        ss << val;
        ensure_path(path).data = ss.str();
    }

    void put(const std::string& path, const std::string& val) {
        ensure_path(path).data = val;
    }

    void put(const std::string& path, const char* val) {
        ensure_path(path).data = std::string(val ? val : "");
    }

    void put(const std::string& path, bool val) {
        ensure_path(path).data = val ? "true" : "false";
    }
};

template <>
inline std::optional<std::string> ptree::convert_from_string<std::string>(const std::string& s) {
    return s;
}

template <>
inline std::optional<int> ptree::convert_from_string<int>(const std::string& s) {
    if (s.empty()) return std::nullopt;
    std::istringstream ss(s);
    int v = 0;
    ss >> v;
    if (ss.fail()) return std::nullopt;
    return v;
}

template <>
inline std::optional<uint16_t> ptree::convert_from_string<uint16_t>(const std::string& s) {
    if (s.empty()) return std::nullopt;
    std::istringstream ss(s);
    unsigned int v = 0;
    ss >> v;
    if (ss.fail()) return std::nullopt;
    return static_cast<uint16_t>(v);
}

template <>
inline std::optional<float> ptree::convert_from_string<float>(const std::string& s) {
    if (s.empty()) return std::nullopt;
    std::istringstream ss(s);
    float v = 0.0f;
    ss >> v;
    if (ss.fail()) return std::nullopt;
    return v;
}

template <>
inline std::optional<bool> ptree::convert_from_string<bool>(const std::string& s) {
    if (s.empty()) return std::nullopt;
    if (s == "1" || s == "true" || s == "TRUE" || s == "yes") return true;
    if (s == "0" || s == "false" || s == "FALSE" || s == "no") return false;
    return std::nullopt;
}

inline std::string trim_str(const std::string& s) {
    size_t a = 0;
    while (a < s.size() && std::isspace(static_cast<unsigned char>(s[a]))) a++;
    size_t b = s.size();
    while (b > a && std::isspace(static_cast<unsigned char>(s[b - 1]))) b--;
    return s.substr(a, b - a);
}

inline std::string decode_entities(const std::string& s) {
    std::string out;
    out.reserve(s.size());
    for (size_t i = 0; i < s.size(); i++) {
        if (s[i] == '&') {
            if (s.compare(i, 5, "&amp;") == 0) { out.push_back('&'); i += 4; continue; }
            if (s.compare(i, 4, "&lt;") == 0) { out.push_back('<'); i += 3; continue; }
            if (s.compare(i, 4, "&gt;") == 0) { out.push_back('>'); i += 3; continue; }
            if (s.compare(i, 6, "&quot;") == 0) { out.push_back('"'); i += 5; continue; }
            if (s.compare(i, 6, "&apos;") == 0) { out.push_back('\''); i += 5; continue; }
        }
        out.push_back(s[i]);
    }
    return out;
}

inline std::string encode_entities(const std::string& s) {
    std::string out;
    out.reserve(s.size());
    for (size_t i = 0; i < s.size(); i++) {
        char c = s[i];
        if (c == '&') out += "&amp;";
        else if (c == '<') out += "&lt;";
        else if (c == '>') out += "&gt;";
        else if (c == '"') out += "&quot;";
        else out.push_back(c);
    }
    return out;
}

inline void parse_element(const std::string& src, size_t& pos, ptree& parent, bool trim_ws) {
    while (pos < src.size()) {
        size_t lt = src.find('<', pos);
        if (lt == std::string::npos) {
            std::string tail = src.substr(pos);
            std::string dec = decode_entities(trim_ws ? trim_str(tail) : tail);
            if (!dec.empty()) parent.data += dec;
            pos = src.size();
            return;
        }
        if (lt > pos) {
            std::string txt = src.substr(pos, lt - pos);
            std::string dec = decode_entities(trim_ws ? trim_str(txt) : txt);
            if (!dec.empty()) parent.data += dec;
        }
        pos = lt;
        if (src.compare(pos, 4, "<!--") == 0) {
            size_t end = src.find("-->", pos + 4);
            pos = (end == std::string::npos) ? src.size() : end + 3;
            continue;
        }
        if (src.compare(pos, 2, "<?") == 0) {
            size_t end = src.find("?>", pos + 2);
            pos = (end == std::string::npos) ? src.size() : end + 2;
            continue;
        }
        if (src.compare(pos, 9, "<![CDATA[") == 0) {
            size_t end = src.find("]]>", pos + 9);
            if (end == std::string::npos) { pos = src.size(); return; }
            parent.data += src.substr(pos + 9, end - (pos + 9));
            pos = end + 3;
            continue;
        }
        if (src.compare(pos, 2, "</") == 0) {
            size_t gt = src.find('>', pos + 2);
            pos = (gt == std::string::npos) ? src.size() : gt + 1;
            return;
        }
        pos++;
        size_t tag_start = pos;
        while (pos < src.size() && !std::isspace(static_cast<unsigned char>(src[pos])) && src[pos] != '>' && src[pos] != '/')
            pos++;
        std::string tag = src.substr(tag_start, pos - tag_start);
        ptree child;
        ptree* attr_node = nullptr;
        bool self_close = false;
        while (pos < src.size()) {
            while (pos < src.size() && std::isspace(static_cast<unsigned char>(src[pos]))) pos++;
            if (pos >= src.size()) break;
            if (src.compare(pos, 2, "/>") == 0) {
                self_close = true;
                pos += 2;
                break;
            }
            if (src[pos] == '>') {
                pos++;
                break;
            }
            size_t an_start = pos;
            while (pos < src.size() && src[pos] != '=' && !std::isspace(static_cast<unsigned char>(src[pos])) && src[pos] != '>' && src[pos] != '/')
                pos++;
            std::string attr_name = src.substr(an_start, pos - an_start);
            while (pos < src.size() && (std::isspace(static_cast<unsigned char>(src[pos])) || src[pos] == '=')) pos++;
            std::string attr_val;
            if (pos < src.size() && (src[pos] == '"' || src[pos] == '\'')) {
                char q = src[pos++];
                size_t av_start = pos;
                while (pos < src.size() && src[pos] != q) pos++;
                attr_val = decode_entities(src.substr(av_start, pos - av_start));
                if (pos < src.size()) pos++;
            }
            if (!attr_name.empty()) {
                if (!attr_node) {
                    child.children.push_back(std::make_pair(std::string("<xmlattr>"), ptree()));
                    attr_node = &child.children.back().second;
                }
                attr_node->children.push_back(std::make_pair(attr_name, ptree(attr_val)));
            }
        }
        if (!self_close)
            parse_element(src, pos, child, trim_ws);
        if (!tag.empty())
            parent.children.push_back(std::make_pair(tag, child));
    }
}

inline void read_xml(const std::string& filename, ptree& pt, int flags = xml_parser::trim_whitespace) {
    std::ifstream ifs(filename.c_str(), std::ios::in | std::ios::binary);
    if (!ifs.is_open())
        throw std::runtime_error("Cannot open XML file: " + filename);
    std::ostringstream ss;
    ss << ifs.rdbuf();
    std::string content = ss.str();
    if (content.size() >= 3 &&
        static_cast<unsigned char>(content[0]) == 0xEF &&
        static_cast<unsigned char>(content[1]) == 0xBB &&
        static_cast<unsigned char>(content[2]) == 0xBF) {
        content = content.substr(3);
    }
    pt.clear();
    size_t pos = 0;
    parse_element(content, pos, pt, (flags & xml_parser::trim_whitespace) != 0);
}

inline void write_node(std::ostream& os, const std::string& name, const ptree& node, int depth, const xml_writer_settings& settings) {
    for (int i = 0; i < depth * settings.indent_count; i++) os << settings.indent_char;
    os << "<" << name;
    const ptree* attrs = node.find_child_ptr("<xmlattr>");
    if (attrs) {
        for (size_t i = 0; i < attrs->children.size(); i++) {
            os << " " << attrs->children[i].first << "=\"" << encode_entities(attrs->children[i].second.data) << "\"";
        }
    }
    bool has_elem_children = false;
    for (size_t i = 0; i < node.children.size(); i++) {
        if (node.children[i].first != "<xmlattr>") {
            has_elem_children = true;
            break;
        }
    }
    if (!has_elem_children && node.data.empty()) {
        os << "/>\n";
        return;
    }
    os << ">";
    if (!has_elem_children) {
        os << encode_entities(node.data) << "</" << name << ">\n";
        return;
    }
    os << "\n";
    for (size_t i = 0; i < node.children.size(); i++) {
        if (node.children[i].first == "<xmlattr>") continue;
        write_node(os, node.children[i].first, node.children[i].second, depth + 1, settings);
    }
    for (int i = 0; i < depth * settings.indent_count; i++) os << settings.indent_char;
    os << "</" << name << ">\n";
}

inline void write_xml(const std::string& filename, const ptree& pt, const std::locale& = std::locale(), const xml_writer_settings& settings = xml_writer_settings()) {
    std::ofstream ofs(filename.c_str(), std::ios::out | std::ios::binary | std::ios::trunc);
    if (!ofs.is_open())
        throw std::runtime_error("Cannot write XML file: " + filename);
    ofs << "<?xml version=\"1.0\" encoding=\"utf-8\"?>\n";
    for (size_t i = 0; i < pt.children.size(); i++) {
        if (pt.children[i].first == "<xmlattr>") continue;
        write_node(ofs, pt.children[i].first, pt.children[i].second, 0, settings);
    }
    if (!ofs.good())
        throw std::runtime_error("Failed writing XML file: " + filename);
}

}
