/***************************************************************************
 *  Copyright (C) 2026  Andreas Hanft                                      *
 *                                                                         *
 *  This program is free software: you can redistribute it and/or modify   *
 *  it under the terms of the GNU General Public License as published by   *
 *  the Free Software Foundation, either version 3 of the License, or      *
 *  (at your option) any later version.                                    *
 *                                                                         *
 *  This program is distributed in the hope that it will be useful,        *
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of         *
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the          *
 *  GNU General Public License for more details.                           *
 *                                                                         *
 *  You should have received a copy of the GNU General Public License      *
 *  along with this program.  If not, see <http://www.gnu.org/licenses/>.  *
 ***************************************************************************/

#include "json.hpp"

#include <charconv>
#include <cmath>
#include <limits>

namespace util {


namespace {

const Json g_undefined;
const std::string g_empty;
const std::vector<Json> g_noElements;
const std::map<std::string, Json> g_noMembers;

// deeper documents are refused rather than overflowing the stack
const int MaxDepth = 1024;


void appendUtf8(std::string &out, std::uint32_t c)
{
    if (c < 0x80) {
        out += char(c);
    } else if (c < 0x800) {
        out += char(0xc0 | (c >> 6));
        out += char(0x80 | (c & 0x3f));
    } else if (c < 0x10000) {
        out += char(0xe0 | (c >> 12));
        out += char(0x80 | ((c >> 6) & 0x3f));
        out += char(0x80 | (c & 0x3f));
    } else {
        out += char(0xf0 | (c >> 18));
        out += char(0x80 | ((c >> 12) & 0x3f));
        out += char(0x80 | ((c >> 6) & 0x3f));
        out += char(0x80 | (c & 0x3f));
    }
}

} // namespace


class Json::Parser
{
public:
    explicit Parser(const std::string &text) : m_text(text) {}

    bool document(Json &value)
    {
        skipSpace();
        if (m_position >= m_text.size() || (m_text[m_position] != '{' && m_text[m_position] != '['))
            return fail("an object or an array expected");
        if (!parse(value, 0))
            return false;
        skipSpace();
        if (m_position != m_text.size())
            return fail("text after the document");
        return true;
    }

    std::string error() const { return m_error + " at offset " + std::to_string(m_position); }

private:
    bool fail(const char *reason)
    {
        if (m_error.empty())
            m_error = reason;
        return false;
    }

    void skipSpace()
    {
        while (m_position < m_text.size() && (m_text[m_position] == ' ' || m_text[m_position] == '\t' || m_text[m_position] == '\n' || m_text[m_position] == '\r'))
            m_position++;
    }

    bool literal(const char *word)
    {
        const std::string expected(word);
        if (m_text.compare(m_position, expected.size(), expected) != 0)
            return fail("an unknown value");
        m_position += expected.size();
        return true;
    }

    bool parse(Json &value, int depth)
    {
        if (depth > MaxDepth)
            return fail("too deeply nested");
        skipSpace();
        if (m_position >= m_text.size())
            return fail("a value expected");
        switch (m_text[m_position]) {
        case '{': return object(value, depth);
        case '[': return array(value, depth);
        case '"': value.m_type = String; return string(value.m_string);
        case 't': value.m_type = Bool; value.m_bool = true; return literal("true");
        case 'f': value.m_type = Bool; value.m_bool = false; return literal("false");
        case 'n': value.m_type = Null; return literal("null");
        default: return number(value);
        }
    }

    bool object(Json &value, int depth)
    {
        value.m_type = Object;
        value.m_members = std::make_shared<std::map<std::string, Json>>();
        m_position++;
        skipSpace();
        if (m_position < m_text.size() && m_text[m_position] == '}') {
            m_position++;
            return true;
        }
        for (;;) {
            skipSpace();
            std::string key;
            if (m_position >= m_text.size() || m_text[m_position] != '"')
                return fail("a key expected");
            if (!string(key))
                return false;
            skipSpace();
            if (m_position >= m_text.size() || m_text[m_position] != ':')
                return fail("':' expected");
            m_position++;
            Json member;
            if (!parse(member, depth + 1))
                return false;
            (*value.m_members)[key] = std::move(member);
            skipSpace();
            if (m_position < m_text.size() && m_text[m_position] == ',') {
                m_position++;
                continue;
            }
            if (m_position < m_text.size() && m_text[m_position] == '}') {
                m_position++;
                return true;
            }
            return fail("',' or '}' expected");
        }
    }

    bool array(Json &value, int depth)
    {
        value.m_type = Array;
        value.m_elements = std::make_shared<std::vector<Json>>();
        m_position++;
        skipSpace();
        if (m_position < m_text.size() && m_text[m_position] == ']') {
            m_position++;
            return true;
        }
        for (;;) {
            Json element;
            if (!parse(element, depth + 1))
                return false;
            value.m_elements->push_back(std::move(element));
            skipSpace();
            if (m_position < m_text.size() && m_text[m_position] == ',') {
                m_position++;
                continue;
            }
            if (m_position < m_text.size() && m_text[m_position] == ']') {
                m_position++;
                return true;
            }
            return fail("',' or ']' expected");
        }
    }

    bool hex4(std::uint32_t &code)
    {
        if (m_position + 4 > m_text.size())
            return fail("a broken escape");
        code = 0;
        for (int i = 0; i < 4; i++) {
            const char c = m_text[m_position++];
            code <<= 4;
            if (c >= '0' && c <= '9')
                code |= std::uint32_t(c - '0');
            else if (c >= 'a' && c <= 'f')
                code |= std::uint32_t(c - 'a' + 10);
            else if (c >= 'A' && c <= 'F')
                code |= std::uint32_t(c - 'A' + 10);
            else
                return fail("a broken escape");
        }
        return true;
    }

    bool string(std::string &out)
    {
        m_position++;
        for (;;) {
            if (m_position >= m_text.size())
                return fail("an unterminated string");
            const char c = m_text[m_position++];
            if (c == '"')
                return true;
            // (control characters are taken as they are)
            if (c != '\\') {
                out += c;
                continue;
            }
            if (m_position >= m_text.size())
                return fail("an unterminated string");
            switch (m_text[m_position++]) {
            case '"': out += '"'; break;
            case '\\': out += '\\'; break;
            case '/': out += '/'; break;
            case 'b': out += '\b'; break;
            case 'f': out += '\f'; break;
            case 'n': out += '\n'; break;
            case 'r': out += '\r'; break;
            case 't': out += '\t'; break;
            case 'u': {
                std::uint32_t code;
                if (!hex4(code))
                    return false;
                if (code >= 0xd800 && code < 0xdc00 && m_text.compare(m_position, 2, "\\u") == 0) {
                    const std::size_t pair = m_position;
                    m_position += 2;
                    std::uint32_t low;
                    if (!hex4(low))
                        return false;
                    if (low >= 0xdc00 && low < 0xe000)
                        code = 0x10000 + ((code - 0xd800) << 10) + (low - 0xdc00);
                    else
                        m_position = pair;
                }
                // a surrogate without its other half is no character
                if (code >= 0xd800 && code < 0xe000)
                    code = 0xfffd;
                appendUtf8(out, code);
                break;
            }
            default:
                // an unknown escape: the character itself
                out += m_text[m_position - 1];
                break;
            }
        }
    }

    bool number(Json &value)
    {
        const char *first = m_text.data() + m_position;
        const std::from_chars_result result = std::from_chars(first, m_text.data() + m_text.size(), value.m_number);
        if (result.ec != std::errc() || result.ptr == first)
            return fail("an illegal number");
        value.m_type = Number;
        m_position += std::size_t(result.ptr - first);
        return true;
    }

private:
    const std::string &m_text;
    std::size_t m_position = 0;
    std::string m_error;
};


Json Json::parse(const std::string &text, std::string *error)
{
    Parser parser(text);
    Json value;
    if (parser.document(value)) {
        if (error)
            error->clear();
        return value;
    }
    if (error)
        *error = parser.error();
    return Json();
}


bool Json::toBool(bool defaultValue) const
{
    return m_type == Bool ? m_bool : defaultValue;
}


double Json::toDouble(double defaultValue) const
{
    return m_type == Number ? m_number : defaultValue;
}


int Json::toInt(int defaultValue) const
{
    if (m_type != Number || m_number < std::numeric_limits<int>::min() || m_number > std::numeric_limits<int>::max() || m_number != std::floor(m_number))
        return defaultValue;
    return int(m_number);
}


std::int64_t Json::toInt64() const
{
    // (doubles hold whole numbers exactly up to 2^53, far beyond the sizes of glTF files)
    return m_type == Number && std::fabs(m_number) < 9.2e18 ? std::int64_t(std::llround(m_number)) : 0;
}


const std::string& Json::toString() const
{
    return m_type == String ? m_string : g_empty;
}


std::size_t Json::size() const
{
    if (m_type == Array)
        return m_elements->size();
    if (m_type == Object)
        return m_members->size();
    return 0;
}


const Json& Json::operator[](std::size_t index) const
{
    return m_type == Array && index < m_elements->size() ? (*m_elements)[index] : g_undefined;
}


const Json& Json::operator[](const std::string &key) const
{
    if (m_type != Object)
        return g_undefined;
    const auto it = m_members->find(key);
    return it != m_members->end() ? it->second : g_undefined;
}


bool Json::contains(const std::string &key) const
{
    return m_type == Object && m_members->count(key) != 0;
}


const std::vector<Json>& Json::elements() const
{
    return m_type == Array ? *m_elements : g_noElements;
}


const std::map<std::string, Json>& Json::members() const
{
    return m_type == Object ? *m_members : g_noMembers;
}


} // namespace util
